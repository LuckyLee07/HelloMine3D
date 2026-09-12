// Execute the production GLSL vertex shader, including its actual matrices.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_water_shader_macos.cpp \
//   -framework OpenGL -o /tmp/validate-water-shader
// /tmp/validate-water-shader media/ogre/HelloMine3DWater.vert
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

#include "../src/HelloMine3D/World/WorldConstants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
using Matrix = std::array<float, 16>;
using Sample = std::array<float, 6>; // world position, world normal

Matrix translation(float x, float y, float z)
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1};
}

void require(bool condition, const std::string &message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}
}

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Usage: validate-water-shader <Water.vert>");
        std::ifstream input(argv[1]);
        require(input.good(), "Cannot read water vertex shader");
        const std::string source{std::istreambuf_iterator<char>(input), {}};

        const CGLPixelFormatAttribute attributes[] = {
            kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),
            kCGLPFAAccelerated, static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format = nullptr;
        GLint count = 0;
        require(CGLChoosePixelFormat(attributes, &format, &count) == kCGLNoError &&
                    format != nullptr,
                "No macOS OpenGL core pixel format");
        CGLContextObj context = nullptr;
        const CGLError created = CGLCreateContext(format, nullptr, &context);
        CGLDestroyPixelFormat(format);
        require(created == kCGLNoError && context != nullptr,
                "Cannot create macOS OpenGL context");
        require(CGLSetCurrentContext(context) == kCGLNoError,
                "Cannot activate OpenGL context");
        std::cout << "renderer=" << glGetString(GL_RENDERER) << '\n';

        const GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        const char *text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        char log[4096] = {};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        require(ok == GL_TRUE, std::string("Shader compile failed: ") + log);

        const GLuint program = glCreateProgram();
        glAttachShader(program, shader);
        const char *varyings[] = {"waterWorldPosition", "waterWorldNormal"};
        glTransformFeedbackVaryings(program, 2, varyings, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        require(ok == GL_TRUE, std::string("Shader link failed: ") + log);
        glUseProgram(program);

        // A CGL context has no default drawable. Draw calls still require a
        // complete framebuffer even when rasterization is discarded.
        GLuint framebuffer = 0, colour = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenRenderbuffers(1, &colour);
        glBindRenderbuffer(GL_RENDERBUFFER, colour);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, colour);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "Incomplete diagnostic framebuffer");

        GLuint vao = 0, buffer = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(Sample), nullptr,
                     GL_STREAM_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
        glEnable(GL_RASTERIZER_DISCARD);

        const GLint vertex = glGetAttribLocation(program, "vertex");
        require(vertex >= 0, "Missing vertex input");
        const GLint globalTime = glGetUniformLocation(program, "globalTime");
        require(globalTime >= 0, "Missing wave time input");
        const auto sample = [&](const Matrix &world, float x, float y, float z) {
            for (const char *name : {"world", "worldView", "worldViewProj"}) {
                glUniformMatrix4fv(glGetUniformLocation(program, name), 1,
                                   GL_FALSE, world.data());
            }
            glVertexAttrib4f(vertex, x, y, z, 1.0f);
            glBeginTransformFeedback(GL_POINTS);
            glDrawArrays(GL_POINTS, 0, 1);
            glEndTransformFeedback();
            Sample result{};
            glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                               sizeof(result), result.data());
            const GLenum error = glGetError();
            require(error == GL_NO_ERROR,
                    "OpenGL sampling failed: " + std::to_string(error));
            for (float value : result) {
                require(std::isfinite(value), "Non-finite shader output");
            }
            return result;
        };

        float maxPositionDelta = 0.0f, maxNormalDelta = 0.0f;
        std::size_t pairs = 0;
        // Both representations of a shared edge must produce the same output.
        // Include both horizontal axes, negative coordinates, the origin,
        // four-section corners, and multiple animation phases.
        for (float time : {0.0f, 0.25f, 0.75f, 1.0f, 8.0f}) {
            glUniform1f(globalTime, time);
            for (int boundary : {-1024, -256, -16, 0, 16, 256, 1024}) {
                for (int along : {0, 7, CHUNK_SIZE}) {
                    for (int axis = 0; axis < 2; ++axis) {
                        const Matrix a = axis == 0
                            ? translation(boundary - CHUNK_SIZE, 48, -256)
                            : translation(-256, 48, boundary - CHUNK_SIZE);
                        const Matrix b = axis == 0
                            ? translation(boundary, 48, -256)
                            : translation(-256, 48, boundary);
                        const Sample left = sample(a,
                            axis == 0 ? CHUNK_SIZE : along, 16,
                            axis == 0 ? along : CHUNK_SIZE);
                        const Sample right = sample(b,
                            axis == 0 ? 0 : along, 16,
                            axis == 0 ? along : 0);
                        for (int i = 0; i < 3; ++i) {
                            maxPositionDelta = std::max(maxPositionDelta,
                                std::abs(left[i] - right[i]));
                            maxNormalDelta = std::max(maxNormalDelta,
                                std::abs(left[i + 3] - right[i + 3]));
                        }
                        // Preserve the existing mean level and bounded waves.
                        require(left[1] >= 63.84f - 0.00001f &&
                                    left[1] <= 63.96f + 0.00001f,
                                "Water displacement exceeded its bounds");
                        ++pairs;
                    }
                }
            }
        }
        std::cout << "pairs=" << pairs
                  << " max_position_delta=" << maxPositionDelta
                  << " max_normal_delta=" << maxNormalDelta << '\n';
        glDeleteBuffers(1, &buffer);
        glDeleteVertexArrays(1, &vao);
        glDeleteRenderbuffers(1, &colour);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteProgram(program);
        glDeleteShader(shader);
        CGLSetCurrentContext(nullptr);
        CGLDestroyContext(context);
        require(maxPositionDelta < 0.00001f && maxNormalDelta < 0.00001f,
                "Adjacent chunk water vertices disagree");
        std::cout << "[WATER_SHADER] PASS\n";
        return 0;
    }
    catch (const std::exception &error) {
        std::cerr << "[WATER_SHADER] FAIL " << error.what() << '\n';
        return 1;
    }
}
