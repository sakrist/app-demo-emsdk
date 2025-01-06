#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES2/gl2.h>
#include <cstdio>

GLuint program;
GLuint vao;

// Simple shader sources
const char* vertexShaderSource = R"(
    attribute vec4 position;
    void main() {
        gl_Position = position;
    }
)";

const char* fragmentShaderSource = R"(
    precision mediump float;
    void main() {
        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
)";

GLuint createShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

void renderFrame() {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

int main() {
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = emscripten_webgl_create_context("#canvas", &attrs);
    emscripten_webgl_make_context_current(context);

    // Create shaders
    GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    // Create and link program
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Clean up shaders
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Set up vertex data
    float vertices[] = {
        0.0f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f
    };

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Set up vertex attributes
    GLint posAttrib = glGetAttribLocation(program, "position");
    glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(posAttrib);

    // Set up the render loop
    emscripten_set_main_loop(renderFrame, 0, true);

    return 0;
}

