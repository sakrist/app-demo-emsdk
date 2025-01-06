if (navigator.gpu) {
    console.log("WebGPU detected")
    WebGPUInit();
  } else {
    console.error("No WebGPU support.")
  }

  async function WebGPUInit() {
    const adapter = await navigator.gpu.requestAdapter();
    if (!adapter) {
        console.error("No WebGPU support.")
        return
    }
    const device = await adapter.requestDevice();
    Module["preinitializedWebGPUDevice"] = device;
    console.log(adapter, device);
  }