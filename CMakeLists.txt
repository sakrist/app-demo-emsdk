cmake_minimum_required(VERSION 3.21.0)

# set the project name
project(app-demo DESCRIPTION "Demo project for app")

# configure C++
set(CMAKE_CXX_STANDARD 17)


# set(SOURCES src/main_webgl.cpp)
set(SOURCES src/main_webgpu.cpp
            src/webgpu-utils.cpp)

if (EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set(EMSCRIPTEN_FLAGS " -s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_WEBGPU=1 \
    -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -sASSERTIONS=1 -sASYNCIFY\
    -s SAFE_HEAP=1  -s DISABLE_EXCEPTION_CATCHING=0 \
    -s FORCE_FILESYSTEM=1 --bind  --emrun \
    -lidbfs.js --shell-file ${CMAKE_SOURCE_DIR}/app-demo.html \
    --pre-js ${CMAKE_SOURCE_DIR}/src/pre.js ")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(EMSCRIPTEN_FLAGS "${EMSCRIPTEN_FLAGS}  -s ASSERTIONS=1 -s STACK_OVERFLOW_CHECK=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O0 -g -fexceptions ")
    add_definitions(-DDEBUG)
    add_compile_options(-g)
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -O3 -fexceptions ")
endif()


add_executable(${PROJECT_NAME} ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "${EMSCRIPTEN_FLAGS}" )