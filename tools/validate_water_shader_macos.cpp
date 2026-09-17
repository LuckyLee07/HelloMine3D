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
#include <filesystem>
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

void checkDepthFragment(const std::filesystem::path& directory)
{
    std::ifstream input(directory / "HelloMine3DWater.frag");
    require(input.good(), "Cannot read water fragment shader");
    const std::string fragment{std::istreambuf_iterator<char>(input), {}};
    const std::string vertex = R"GLSL(#version 150
out vec3 waterWorldPosition;
out vec3 waterWorldNormal;
out float waterLight;
out float waterDistance;
out vec2 waterSurfaceData;
out vec2 waterSurfaceDrift;
uniform vec2 depthAndShore;
uniform vec2 diagnosticDrift;
uniform float diagnosticDistance;
void main() {
    vec2 position = gl_VertexID == 0 ? vec2(-1,-1) :
        (gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3));
    gl_Position = vec4(position, 0, 1);
    waterWorldPosition = vec3(0);
    waterWorldNormal = vec3(0,1,0);
    waterLight = 1;
    waterDistance = diagnosticDistance;
    waterSurfaceData = depthAndShore;
    waterSurfaceDrift = diagnosticDrift;
})GLSL";
    const GLuint program = glCreateProgram();
    for (const auto& entry : {std::pair<GLenum, const std::string*>{GL_VERTEX_SHADER, &vertex},
                             {GL_FRAGMENT_SHADER, &fragment}}) {
        const GLuint shader = glCreateShader(entry.first);
        const char* source = entry.second->c_str();
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint ok = 0; char log[4096]{};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        require(ok, std::string("Depth fragment compile: ") + log);
        glAttachShader(program, shader); glDeleteShader(shader);
    }
    glLinkProgram(program);
    GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
    require(ok, "Depth fragment link failed");
    glUseProgram(program);
    const auto scalar = [&](const char* name, float value) {
        glUniform1f(glGetUniformLocation(program, name), value);
    };
    const auto vector = [&](const char* name, float x, float y, float z) {
        glUniform3f(glGetUniformLocation(program, name), x, y, z);
    };
    scalar("environmentLight", 1); scalar("waterDetailStrength", 1);
    vector("waterShallowColour", .15f, .5f, .6f);
    vector("waterDeepColour", .02f, .1f, .18f);
    vector("sunDirection", 0, 1, 0); vector("cameraPosition", 0, 10, 0);
    glDisable(GL_RASTERIZER_DISCARD); glDisable(GL_BLEND);
    glViewport(0, 0, 1, 1);
    const auto sample = [&](float depth, float distance, float shore, float time) {
        glUniform2f(glGetUniformLocation(program, "depthAndShore"), depth, shore);
        scalar("diagnosticDistance", distance); scalar("globalTime", time);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::array<unsigned char, 4> pixel{};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        require(glGetError() == GL_NO_ERROR, "Depth fragment draw failed");
        return pixel;
    };
    const auto shallow = sample(1, 12, 0, 0), deep = sample(8, 12, 0, 0);
    require(shallow[1] > deep[1] && shallow[2] > deep[2] && shallow[3] < deep[3],
        "Water depth does not increase colour absorption and opacity");
    require(shallow == sample(1, 150, 0, 0), "Depth incorrectly depends on camera distance");
    require(deep == sample(80, 12, 0, 0), "Depth does not saturate at the sampling bound");
    const auto shoreA = sample(1, 12, .4f, 0), shoreB = sample(1, 12, .4f, 1);
    require(shoreA != shoreB, "Shore ripple does not animate");
    const GLint driftUniform = glGetUniformLocation(program, "diagnosticDrift");
    require(driftUniform >= 0, "Missing surface drift input");
    require(sample(1, 12, 0, 0) == sample(1, 12, 0, 4), "Zero drift still moves surface streaks");
    glUniform2f(driftUniform, .8f, .6f);
    const auto driftStart = sample(1, 12, 0, 0);
    bool driftMoves = false;
    for (int tick = 1; tick <= 40; ++tick)
        driftMoves = driftMoves || sample(1, 12, 0, tick * .15f) != driftStart;
    require(driftMoves, "Actual surface drift does not animate");
    for (float boundary : {0.5f / .15f, 1.f / .15f, 1200.f}) {
        const auto a = sample(1, 12, 0, boundary - .0002f);
        const auto b = sample(1, 12, 0, boundary + .0002f);
        for (std::size_t component = 0; component < a.size(); ++component)
            require(std::abs(int(a[component]) - int(b[component])) <= 1,
                    "Surface drift jumps at its phase boundary");
    }
    scalar("waterDetailStrength", 0);
    require(sample(1, 12, .4f, 0) == sample(1, 12, .4f, 1), "Fallback shoreline still animates");
    vector("cameraPosition", 0, -1, 0);
    require(sample(1, 12, 0, 0)[3] >= 225, "Underwater surface loses opacity");
    std::cout << "[WATER_SHADER] PASS depth-absorption distance-invariance depth-bound shore-motion drift-motion drift-continuity fallback underwater\n";
    glDeleteProgram(program);
}
}

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Usage: validate-water-shader <Water.vert>");
        std::ifstream input(argv[1]);
        require(input.good(), "Cannot read water vertex shader");
        const std::string source{std::istreambuf_iterator<char>(input), {}};
        const auto directory = std::filesystem::path(argv[1]).parent_path();
        std::ifstream declarationsInput(directory / "HelloMine3D.program");
        const std::string declarations{std::istreambuf_iterator<char>(declarationsInput), {}};
        for (const char* name : {"WaterVertex", "WaterFragment"}) {
            const auto start = declarations.find(std::string("program HelloMine3D/") + name + " glsl");
            require(start != std::string::npos, "Missing water program declaration");
            const auto end = declarations.find("\n}", start);
            const auto block = declarations.substr(start, end - start);
            require(block.find("param_named_auto globalTime time 1.0") != std::string::npos &&
                    block.find("time_0_x") == std::string::npos, "Water animation time resets");
        }

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
        const GLint detail = glGetUniformLocation(program, "waterDetailStrength");
        require(detail >= 0, "Missing water detail fallback");
        glUniform1f(detail, 1.f);

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
        glUniform1f(globalTime, 0.9999f);
        const auto before = sample(translation(0,48,0), 4,16,4);
        glUniform1f(globalTime, 1.0001f);
        const auto after = sample(translation(0,48,0), 4,16,4);
        require(std::abs(before[1] - after[1]) < .0001f, "Water jumps at a second boundary");
        glUniform1f(detail, 0.f);
        const auto fallback = sample(translation(0,48,0), 4,16,4);
        require(std::abs(fallback[1] - 63.9f) < .00001f && fallback[4] == 1.f,
                "Water fallback is not level");
        checkDepthFragment(directory);
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
