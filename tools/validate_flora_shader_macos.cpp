// Run the production GLSL and the time binding declared in HelloMine3D.program.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_flora_shader_macos.cpp \
//   -framework OpenGL -o /tmp/validate-flora-shader
// /tmp/validate-flora-shader media/ogre
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#include <string>

namespace {
using Vector = std::array<float, 3>;
using Matrix = std::array<float, 16>;

void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}

std::string read(const std::string &path)
{
    std::ifstream input(path);
    require(input.good(), "Cannot read " + path);
    return {std::istreambuf_iterator<char>(input), {}};
}

Matrix translation(float x, float y, float z)
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1};
}

float difference(const Vector &a, const Vector &b)
{
    float result = 0;
    for (int i = 0; i < 3; ++i)
        result = std::max(result, std::abs(a[i] - b[i]));
    return result;
}

Vector offset(const Vector &position, float x, float y, float z)
{
    return {position[0] - x, position[1] - y, position[2] - z};
}

struct Shader {
    GLuint program = 0;
    GLint vertex = -1, repeat = -1, clock = -1;
    bool repeatingTime = false;
    float timeParameter = 1.0f;

    Shader(const std::string &directory, const std::string &name)
    {
        const std::string declarations = read(directory + "/HelloMine3D.program");
        const auto start = declarations.find("vertex_program HelloMine3D/" + name + " glsl");
        require(start != std::string::npos, "Missing program " + name);
        const auto next = declarations.find("\nvertex_program ", start + 1);
        const std::string block = declarations.substr(start, next - start);
        std::smatch binding;
        require(std::regex_search(block, binding, std::regex(
                    "param_named_auto\\s+globalTime\\s+(time_0_x|time)\\s+([0-9.]+)")),
                "Missing time binding for " + name);
        repeatingTime = binding[1] == "time_0_x";
        timeParameter = std::stof(binding[2]);
        require(timeParameter > 0, "Invalid time parameter");

        const std::string source = read(directory + "/HelloMine3D" +
            (name == "FloraVertex" ? "Flora.vert" : "FloraShadow.vert"));
        const GLuint shader = glCreateShader(GL_VERTEX_SHADER);
        const char *text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        char log[4096] = {};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        require(ok == GL_TRUE, name + " compile failed: " + log);
        program = glCreateProgram();
        glAttachShader(program, shader);
        const char *varying = "terrainWorldPosition";
        glTransformFeedbackVaryings(program, 1, &varying, GL_INTERLEAVED_ATTRIBS);
        glLinkProgram(program);
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        require(ok == GL_TRUE, name + " link failed: " + log);
        glDeleteShader(shader);
        vertex = glGetAttribLocation(program, "vertex");
        repeat = glGetAttribLocation(program, "uv1");
        clock = glGetUniformLocation(program, "globalTime");
        require(vertex >= 0 && clock >= 0, "Missing active vertex/time input");
    }

    Vector sample(float time, const Matrix &world, float x, float y, float z,
                  float repeatV) const
    {
        glUseProgram(program);
        glUniform1f(clock, repeatingTime ? std::fmod(time, timeParameter)
                                        : time * timeParameter);
        for (const char *name : {"world", "worldView", "worldViewProj", "shadowWorldViewProj"})
            glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, world.data());
        glVertexAttrib4f(vertex, x, y, z, 1);
        if (repeat >= 0) glVertexAttrib2f(repeat, 0, repeatV);
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, 1);
        glEndTransformFeedback();
        Vector value{};
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(value), value.data());
        const GLenum error = glGetError();
        require(error == GL_NO_ERROR, "OpenGL sampling failed: " + std::to_string(error));
        for (float v : value) require(std::isfinite(v), "Non-finite shader output");
        return value;
    }
};
}

int main(int argc, char **argv)
{
    try {
        require(argc == 2, "Usage: validate-flora-shader <shader-directory>");
        const CGLPixelFormatAttribute attributes[] = {
            kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),
            kCGLPFAAccelerated, static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format = nullptr;
        GLint count = 0;
        require(CGLChoosePixelFormat(attributes, &format, &count) == kCGLNoError && format,
                "No macOS OpenGL core pixel format");
        CGLContextObj context = nullptr;
        const auto created = CGLCreateContext(format, nullptr, &context);
        CGLDestroyPixelFormat(format);
        require(created == kCGLNoError && context, "Cannot create OpenGL context");
        require(CGLSetCurrentContext(context) == kCGLNoError, "Cannot activate OpenGL context");
        std::cout << "renderer=" << glGetString(GL_RENDERER) << '\n';

        GLuint framebuffer = 0, colour = 0, vao = 0, buffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glGenRenderbuffers(1, &colour);
        glBindRenderbuffer(GL_RENDERBUFFER, colour);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colour);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                "Incomplete diagnostic framebuffer");
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glGenBuffers(1, &buffer);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buffer);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(Vector), nullptr, GL_STREAM_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer);
        glEnable(GL_RASTERIZER_DISCARD);

        Shader normal(argv[1], "FloraVertex"), shadow(argv[1], "FloraShadowVertex");
        const Matrix world = translation(0, 48, 0);
        float rootDrift = 0, jump = 0, passMismatch = 0, spatialMismatch = 0;
        float cropScaleError = 0, maxDisplacement = 0, oneSecondDifference = 0;
        float neighbourDifference = 0, frameStep = 0;
        std::size_t samples = 0;
        for (int step = 0; step <= 960; ++step) {
            const float time = step / 120.0f;
            const Vector root = normal.sample(time, world, 4, 8, 4, 1);
            const Vector tip = normal.sample(time, world, 4, 9, 4, 0);
            const Vector small = normal.sample(time, world, 4, 8.25f, 4, 0);
            const Vector displacement = offset(tip, 4, 57, 4);
            const Vector smallDisplacement = offset(small, 4, 56.25f, 4);
            const Vector expectedSmall{displacement[0] * .25f, 0, displacement[2] * .25f};
            rootDrift = std::max(rootDrift, difference(root, {4, 56, 4}));
            cropScaleError = std::max(cropScaleError, difference(smallDisplacement, expectedSmall));
            maxDisplacement = std::max(maxDisplacement, difference(displacement, {}));
            passMismatch = std::max(passMismatch,
                difference(tip, shadow.sample(time, world, 4, 9, 4, 0)));
            oneSecondDifference = std::max(oneSecondDifference,
                difference(tip, normal.sample(time + 1, world, 4, 9, 4, 0)));
            const Vector neighbour = normal.sample(time, world, 11, 9, 11, 0);
            neighbourDifference = std::max(neighbourDifference,
                difference(displacement, offset(neighbour, 11, 57, 11)));
            if (step > 0) frameStep = std::max(frameStep,
                difference(tip, normal.sample(time - 1.0f/120.0f, world, 4, 9, 4, 0)));
            ++samples;
        }
        for (int second = 1; second <= 8; ++second) {
            jump = std::max(jump, difference(
                normal.sample(second - .0001f, world, 4, 9, 4, 0),
                normal.sample(second + .0001f, world, 4, 9, 4, 0)));
        }
        for (int boundary : {-1024, -16, 0, 16, 1024}) {
            for (float time : {.0f, .37f, 2.4f, 19.0f, 3600.0f}) {
                spatialMismatch = std::max(spatialMismatch, difference(
                    normal.sample(time, translation(boundary - 16, 48, 0), 16, 9, 4, 0),
                    normal.sample(time, translation(boundary, 48, 0), 0, 9, 4, 0)));
                spatialMismatch = std::max(spatialMismatch, difference(
                    normal.sample(time, translation(0, 48, boundary - 16), 4, 9, 16, 0),
                    normal.sample(time, translation(0, 48, boundary), 4, 9, 0, 0)));
            }
        }

        int failures = 0;
        const auto check = [&](const char *name, bool passed, float value) {
            std::cout << "[FLORA_SHADER] " << (passed ? "PASS " : "FAIL ")
                      << name << " value=" << value << '\n';
            failures += !passed;
        };
        check("continuous-time-bindings", !normal.repeatingTime && !shadow.repeatingTime &&
              normal.timeParameter == 1 && shadow.timeParameter == 1, normal.repeatingTime);
        check("anchored-roots", rootDrift < .00001f, rootDrift);
        check("proportional-young-crop-bend", cropScaleError < .00002f, cropScaleError);
        check("bounded-tip-displacement", maxDisplacement < .09f && maxDisplacement > .005f, maxDisplacement);
        check("continuous-second-boundaries", jump < .0002f, jump);
        check("smooth-120hz-motion", frameStep < .004f, frameStep);
        check("not-a-one-second-loop", oneSecondDifference > .01f, oneSecondDifference);
        check("spatial-motion-variation", neighbourDifference > .005f, neighbourDifference);
        check("section-independent-wind", spatialMismatch < .0002f, spatialMismatch);
        check("shadow-receiver-motion-matches", passMismatch < .00001f, passMismatch);
        std::cout << "timeline_samples=" << samples << " checks=10 failures=" << failures << '\n';
        glDeleteProgram(normal.program);
        glDeleteProgram(shadow.program);
        glDeleteBuffers(1, &buffer);
        glDeleteVertexArrays(1, &vao);
        glDeleteRenderbuffers(1, &colour);
        glDeleteFramebuffers(1, &framebuffer);
        CGLSetCurrentContext(nullptr);
        CGLDestroyContext(context);
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception &error) {
        std::cerr << "[FLORA_SHADER] FAIL " << error.what() << '\n';
        return 1;
    }
}
