# WebGL Demo with Emscripten

A simple WebGL application compiled to WebAssembly using Emscripten.

## Prerequisites

Install Emscripten SDK (emsdk)

For detailed installation instructions, visit: https://emscripten.org/docs/getting_started/downloads.html

## Building the Project

1. Create a build directory and navigate to it:
```bash
mkdir build
cd build
```

2. Configure with CMake:
```bash
emcmake cmake ..
```

3. Build the project:
```bash
cmake --build .
```

## Running the Application

After building, you can run the application using:
```bash
emrun app-demo.html
```

Or serve the files using any web server. The following files are needed:
- app-demo.html
- app-demo.js
- app-demo.wasm

## Project Structure

- `src/main.cpp` - Main application code with WebGL setup and rendering
- `src/app-demo.html` - HTML template for the application
- `CMakeLists.txt` - CMake build configuration

## What it Does

This demo creates a WebGL context and renders a simple red triangle using WebAssembly. It demonstrates:
- WebGL context creation with Emscripten
- Basic shader setup
- Vertex buffer creation and rendering
- Main loop implementation for web


## References 
 * https://github.com/jspdown/webgpu-starter
 * https://developer.chrome.com/docs/web-platform/webgpu/build-app