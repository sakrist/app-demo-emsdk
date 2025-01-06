#include <emscripten.h>
#include <webgpu/webgpu.h>
#include <emscripten/html5_webgpu.h>
#include <emscripten/html5.h>
#include <emscripten/websocket.h>
#include <emscripten/bind.h>
#include <cstdlib>
#include <cstdio>

WGPUInstance ctx;
WGPUDevice device;
WGPUQueue queue;
WGPUSwapChain swapChain;
WGPURenderPipeline pipeline;
WGPUTexture depthTexture;  // Add this
WGPUTextureView depthTextureView;  // Add this

// WGSL shaders
const char* shaderSource = R"(
    struct VertexOut {
        @builtin(position) position : vec4f,
    }

    @vertex
    fn vs_main(@location(0) position : vec3f) -> VertexOut {
        var out: VertexOut;
        out.position = vec4f(position, 1.0);
        return out;
    }

    @fragment
    fn fs_main() -> @location(0) vec4f {
        return vec4f(1.0, 0.0, 0.0, 1.0);
    }
)";

void renderFrame() {
    WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
    if (!nextTexture) {
        printf("Cannot acquire next swap chain texture\n");
        return;
    }

    WGPUCommandEncoderDescriptor encoderDesc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = nextTexture;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;

    // Add depth stencil attachment
    WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = depthTextureView;
    depthStencilAttachment.depthClearValue = 1.0f;
    depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthStencilAttachment.stencilLoadOp = WGPULoadOp_Undefined;
    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Undefined;
    
    renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);

    wgpuTextureViewRelease(nextTexture);

    WGPUCommandBufferDescriptor cmdBufferDesc = {};
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    wgpuQueueSubmit(queue, 1, &command);

    wgpuCommandBufferRelease(command);
    wgpuCommandEncoderRelease(encoder);
}

void initWebGPU() {
    ctx = wgpuCreateInstance(NULL);
    assert(ctx);
    if ((device = emscripten_webgpu_get_device())) {
        queue = wgpuDeviceGetQueue(device);

        // Fix: Use correct type for canvas surface descriptor
        WGPUSurfaceDescriptorFromCanvasHTMLSelector canvDesc = {};
        canvDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
        canvDesc.selector = "#canvas";

        WGPUSurfaceDescriptor surfDesc = {};
        surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvDesc);

        WGPUSurface surface = wgpuInstanceCreateSurface(nullptr, &surfDesc);

        WGPUSwapChainDescriptor swapChainDesc = {};
        swapChainDesc.usage = WGPUTextureUsage_RenderAttachment;
        swapChainDesc.format = WGPUTextureFormat_BGRA8Unorm;
        swapChainDesc.width = 800;
        swapChainDesc.height = 600;
        swapChainDesc.presentMode = WGPUPresentMode_Fifo;
        swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);

        // Create depth texture
        WGPUTextureDescriptor depthTextureDesc = {};
        depthTextureDesc.dimension = WGPUTextureDimension_2D;
        depthTextureDesc.format = WGPUTextureFormat_Depth24Plus;
        depthTextureDesc.mipLevelCount = 1;
        depthTextureDesc.sampleCount = 1;
        depthTextureDesc.size = {800, 600, 1};
        depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
        depthTextureDesc.viewFormatCount = 1;
        depthTextureDesc.viewFormats = &depthTextureDesc.format;
        
        depthTexture = wgpuDeviceCreateTexture(device, &depthTextureDesc);
        depthTextureView = wgpuTextureCreateView(depthTexture, nullptr);

        // Create shader module
        WGPUShaderModuleDescriptor shaderDesc = {};
        WGPUShaderModuleWGSLDescriptor wgslDesc = {};
        wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
        wgslDesc.code = shaderSource;
        shaderDesc.nextInChain = &wgslDesc.chain;
        WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

        // Create pipeline
        WGPURenderPipelineDescriptor pipelineDesc = {};
        
        // Setup vertex state
        pipelineDesc.vertex.module = shaderModule;
        pipelineDesc.vertex.entryPoint = "vs_main";
        
        // Add vertex buffer layout
        WGPUVertexBufferLayout vertexBufferLayout = {};
        vertexBufferLayout.arrayStride = 3 * sizeof(float);
        vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
        vertexBufferLayout.attributeCount = 1;

        // Setup vertex attributes
        WGPUVertexAttribute attribute = {};
        attribute.format = WGPUVertexFormat_Float32x3;
        attribute.offset = 0;
        attribute.shaderLocation = 0;
        vertexBufferLayout.attributes = &attribute;

        // Assign vertex buffer layout to pipeline
        pipelineDesc.vertex.bufferCount = 1;
        pipelineDesc.vertex.buffers = &vertexBufferLayout;

        // Setup fragment state
        WGPUFragmentState fragmentState = {};
        fragmentState.module = shaderModule;
        fragmentState.entryPoint = "fs_main";
        fragmentState.targetCount = 1;
        WGPUColorTargetState colorTarget = {};
        colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
        colorTarget.writeMask = WGPUColorWriteMask_All;
        fragmentState.targets = &colorTarget;
        
        // Add primitive state
        WGPUPrimitiveState primitiveState = {};
        primitiveState.topology = WGPUPrimitiveTopology_TriangleList;
        primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
        primitiveState.frontFace = WGPUFrontFace_CCW;
        primitiveState.cullMode = WGPUCullMode_None;
        pipelineDesc.primitive = primitiveState;

        // Add multisample state
        WGPUMultisampleState multisample = {};
        multisample.count = 1;
        multisample.mask = ~0u;
        multisample.alphaToCoverageEnabled = false;
        pipelineDesc.multisample = multisample;

        // Add depth stencil state before creating pipeline
        WGPUDepthStencilState depthStencilState = {};
        depthStencilState.format = WGPUTextureFormat_Depth24Plus;
        depthStencilState.depthWriteEnabled = true;
        depthStencilState.depthCompare = WGPUCompareFunction_Less;
        pipelineDesc.depthStencil = &depthStencilState;

        // Assign fragment state to pipeline
        pipelineDesc.fragment = &fragmentState;

        pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

        wgpuShaderModuleRelease(shaderModule);
    } else {
        printf("WebGPU is not supported\n");
    }
}

int main() {
    initWebGPU();
    emscripten_set_main_loop(renderFrame, 0, true);
    
    // Add cleanup
    if (depthTextureView) wgpuTextureViewRelease(depthTextureView);
    if (depthTexture) wgpuTextureRelease(depthTexture);
    return 0;
}

