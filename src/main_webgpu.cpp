#include <emscripten.h>
#include <webgpu/webgpu.h>
#include <emscripten/html5_webgpu.h>
#include <emscripten/html5.h>
#include <emscripten/websocket.h>
#include <emscripten/bind.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <cassert>
#include <vector>

#include "webgpu-utils.h"

WGPUInstance instance;
WGPUDevice device;
WGPUQueue queue;
WGPUSwapChain swapChain;
WGPURenderPipeline pipeline;

// WGSL shaders

void renderFrame() {
    WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
    if (!nextTexture) {
        printf("Cannot acquire next swap chain texture\n");
        return;
    }


    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = nextTexture;
    colorAttachment.depthSlice = UINT32_MAX;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.label = "Render-Pass";
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;

    WGPUCommandEncoderDescriptor encoderDesc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &command);

    wgpuCommandBufferRelease(command);
    wgpuCommandEncoderRelease(encoder);
}
WGPUShaderModule createShaderModule(WGPUDevice device, std::string const &source, std::string const &name) {
    WGPUShaderModuleWGSLDescriptor wgslDesc = {
            .chain = WGPUChainedStruct{
                    .next = nullptr,
                    .sType = WGPUSType_ShaderModuleWGSLDescriptor,
            },
            .code = source.c_str(),
    };
    WGPUShaderModuleDescriptor desc = {
            .nextInChain = &wgslDesc.chain,
            .label = name.c_str(),
    };

    return wgpuDeviceCreateShaderModule(device, &desc);
}

bool initWebGPU() {
	WGPUInstanceDescriptor desc = {};
	desc.nextInChain = nullptr;

	WGPUInstance instance = wgpuCreateInstance(nullptr);

	if (!instance) {
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return false;
	}

	std::cout << "WGPU instance: " << instance << std::endl;

	std::cout << "Requesting adapter..." << std::endl;
	WGPURequestAdapterOptions adapterOpts = {};
	adapterOpts.nextInChain = nullptr;
	WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;

	inspectAdapter(adapter);

	// We no longer need to use the instance once we have the adapter
	wgpuInstanceRelease(instance);

	std::cout << "Requesting device..." << std::endl;
	WGPUDeviceDescriptor deviceDesc = {};
	deviceDesc.nextInChain = nullptr;
	deviceDesc.label = "My Device"; // anything works here, that's your call
	deviceDesc.requiredFeatureCount = 0; // we do not require any specific feature
	deviceDesc.requiredLimits = nullptr; // we do not require any specific limit
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	// A function that is invoked whenever the device stops being available.
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
		std::cout << "Device lost: reason " << reason;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	device = requestDeviceSync(adapter, &deviceDesc);
	std::cout << "Got device: " << device << std::endl;

	// A function that is invoked whenever there is an error in the use of the device
	auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData */) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /* pUserData */);

	// We no longer need to access the adapter once we have the device
	wgpuAdapterRelease(adapter);

	// Display information about the device
	inspectDevice(device);
    return true;
}

void initWebGPUPipeline() {
    queue = wgpuDeviceGetQueue(device);

    WGPUInstanceDescriptor instanceDesc{};
    instance = wgpuCreateInstance(nullptr);

    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc = {};
    canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvasDesc.selector = "canvas";

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = &canvasDesc.chain;

    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfDesc);

    WGPURequestAdapterOptions adapterOpts{};
    adapterOpts.compatibleSurface = surface;

    WGPUSwapChainDescriptor swapChainDesc = {
            .nextInChain = nullptr,
            .label = "swapchain",
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = WGPUTextureFormat_BGRA8Unorm,
            .width = 800,
            .height = 600,
            .presentMode = WGPUPresentMode_Fifo,
    };

    swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);



    auto vertexShaderModule = createShaderModule(device, R"(
    @vertex fn main(
        @builtin(vertex_index) VertexIndex : u32
    ) -> @builtin(position) vec4<f32> {
        var pos = array<vec2<f32>, 3>(
            vec2<f32>( 0.0,  0.5),
            vec2<f32>(-0.5, -0.5),
            vec2<f32>( 0.5, -0.5)
        );
        return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
    })", "vertex-shader");


    auto fragmentShaderModule = createShaderModule(device, R"(
    @fragment fn main() -> @location(0) vec4<f32> {
        return vec4<f32>(1.0, 0.0, 0.0, 1.0);
    })", "fragment-shader");

    WGPUBlendState blend = {
            .color = WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_One,
                    .dstFactor = WGPUBlendFactor_One,
            },
            .alpha = WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_One,
                    .dstFactor = WGPUBlendFactor_One,
            },
    };
    WGPUColorTargetState colorTarget{};
    colorTarget.format = WGPUTextureFormat_BGRA8Unorm;
    colorTarget.blend = &blend;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment{};
    fragment.module = fragmentShaderModule;
    fragment.entryPoint = "main";
    fragment.targetCount = 1;
    fragment.targets = &colorTarget;

    WGPUVertexState vertex{};
    vertex.module = vertexShaderModule;
    vertex.entryPoint = "main";

    const uint32_t whiteColor = 0xFFFFFFFF;

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    WGPUPrimitiveState primitiveState{};
    primitiveState.topology = WGPUPrimitiveTopology_TriangleList;
    primitiveState.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitiveState.frontFace = WGPUFrontFace_CCW;
    primitiveState.cullMode = WGPUCullMode_None;

    WGPUMultisampleState multisampleState{};
    multisampleState.count = 1;
    multisampleState.mask = whiteColor;

    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.label = "render-pipeline";
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex = vertex;
    pipelineDesc.fragment = &fragment;
    pipelineDesc.primitive = primitiveState;
    pipelineDesc.multisample = multisampleState;

    pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

}

int main() {
    if (initWebGPU()) {
        initWebGPUPipeline();
        emscripten_set_main_loop(renderFrame, 0, true); 
    }
    
    return 0;
}

