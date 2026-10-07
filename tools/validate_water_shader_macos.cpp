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
#include <cstring>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
using Matrix = std::array<float, 16>;
using Sample = std::array<float, 11>; // world position, normal, clip position, distance

Matrix translation(float x, float y, float z)
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1};
}

Matrix multiply(const Matrix& a, const Matrix& b)
{
    Matrix result{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                result[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
    return result;
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
out vec2 waterLightSources;
uniform vec2 diagnosticLightSources;
out float waterDistance;
out vec2 waterSurfaceData;
out vec2 waterSurfaceDrift;
uniform vec2 depthAndShore;
uniform vec2 diagnosticDrift;
uniform vec2 diagnosticOrigin;
uniform float diagnosticScale;
uniform float diagnosticDistance;
void main() {
    vec2 position = gl_VertexID == 0 ? vec2(-1,-1) :
        (gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3));
    gl_Position = vec4(position, 0, 1);
    vec2 worldXZ = diagnosticOrigin + position * diagnosticScale;
    waterWorldPosition = vec3(worldXZ.x, 0, worldXZ.y);
    waterWorldNormal = vec3(0,1,0);
    waterLight = 1;
    waterLightSources = diagnosticLightSources;
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
    // The reference fragment adds a 2D reflection sampler even when its
    // dynamic feature flag is off. Supply a complete disabled-path texture;
    // zero/unloadable driver substitution would make this fixture misleading.
    const GLint reflectionSampler = glGetUniformLocation(program, "planarReflectionTexture");
    require(reflectionSampler >= 0, "Missing compiled planar reflection sampler");
    GLuint disabledReflectionTexture = 0;
    glGenTextures(1, &disabledReflectionTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, disabledReflectionTexture);
    const std::array<unsigned char, 4> disabledReflectionPixel{0, 0, 0, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, disabledReflectionPixel.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glUniform1i(reflectionSampler, 0);
    scalar("planarReflectionEnabled", 0.f);
    GLint samplerUnit = -1, textureWidth = 0, textureHeight = 0, textureFormat = 0;
    glGetUniformiv(program, reflectionSampler, &samplerUnit);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &textureWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &textureHeight);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &textureFormat);
    require(samplerUnit == 0 && textureWidth == 1 && textureHeight == 1 && textureFormat == GL_RGBA8 &&
        glGetError() == GL_NO_ERROR, "Disabled planar reflection fixture binding is incomplete");
    std::cout << "[WATER_SHADER] disabled_planar_sampler_complete=1 PASS\n";
    glUniform2f(glGetUniformLocation(program,"diagnosticLightSources"),-1,-1);
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
    const auto rangeUniform = glGetUniformLocation(program,"viewRange");
    scalar("viewRangeStrength",1);
    const auto rangeOrigin = glGetUniformLocation(program,"diagnosticOrigin");
    glUniform2f(rangeOrigin,122,0);
    const auto fullRange=sample(1,12,0,0);
    glUniform2f(rangeUniform,118,126);
    const auto halfRange=sample(1,12,0,0);
    require(std::abs(int(halfRange[3])*2-int(fullRange[3]))<=1,
        "Water range transition does not halve coverage at midpoint");
    for(int component=0;component<3;++component)
        require(halfRange[component]==fullRange[component],"Water range changes surface colour instead of coverage");
    glUniform2f(rangeOrigin,110,110);
    const auto diagonal=sample(1,12,0,0);
    glUniform2f(rangeUniform,0,0);
    require(diagonal==sample(1,12,0,0),"Water range removes interior diagonal coverage");
    glUniform2f(rangeUniform,118,126);
    glUniform2f(rangeOrigin,130,0);
    require(sample(1,12,0,0)[3]==0,"Water survives outside view range");
    scalar("viewRangeStrength",0);
    const auto enclosed=sample(1,12,0,0);
    glUniform2f(rangeUniform,0,0);
    require(enclosed==sample(1,12,0,0),"Water range changes enclosed-space pixels");
    scalar("viewRangeStrength",1);
    glUniform2f(rangeUniform,118,126);
    glUniform2f(glGetUniformLocation(program,"viewRangeCentre"),130,0);
    const auto centred=sample(1,12,0,0);
    glUniform2f(rangeUniform,0,0);
    require(centred==sample(1,12,0,0),"Water range ignores logical centre");
    glUniform2f(glGetUniformLocation(program,"viewRangeCentre"),0,0);
    glUniform2f(rangeUniform,118,126);
    glUniform2f(rangeOrigin,122,0);
    int previousAlpha=256;
    for(float distance:{116.f,118.f,120.f,122.f,124.f,126.f}) {
        glUniform2f(glGetUniformLocation(program,"viewRangeCentre"),122-distance,0);
        const int alpha=sample(1,12,0,0)[3];
        require(alpha<=previousAlpha,"Water range coverage is not monotonic");
        previousAlpha=alpha;
    }
    glUniform2f(glGetUniformLocation(program,"viewRangeCentre"),0,0);
    glUniform2f(rangeOrigin,0,0);
    const auto nearRange=sample(1,12,0,0);
    glUniform2f(rangeUniform,0,0);
    require(nearRange==sample(1,12,0,0),"Water range changes near pixels");
    std::cout << "[WATER_SHADER] view_range_checks=10 PASS\n";
    const auto shallow = sample(1, 12, 0, 0), deep = sample(8, 12, 0, 0);
    require(shallow[1] > deep[1] && shallow[2] > deep[2] && shallow[3] < deep[3],
        "Water depth does not increase colour absorption and opacity");
    require(shallow == sample(1, 150, 0, 0), "Depth incorrectly depends on camera distance");
    require(deep == sample(80, 12, 0, 0), "Depth does not saturate at the sampling bound");
    const auto shoreA = sample(1, 12, .4f, 0);
    int shoreRange=0;
    for(int tick=1;tick<=40;++tick) {
        const auto b=sample(1,12,.4f,tick*.15f);
        for(int k=0;k<3;++k)shoreRange=std::max(shoreRange,std::abs(int(b[k])-int(shoreA[k])));
    }
    require(shoreRange>=2, "Shore ripple does not visibly animate through a complete period");
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
    // A coarse world footprint must remove time-varying subpixel streaks and
    // shore ripples. Removing the production derivative filter fails here.
    scalar("diagnosticScale", .5f);
    const auto farDetail=sample(1,12,.4f,0);
    bool filtered=true;
    for(int tick=1;tick<=20;++tick)filtered &= sample(1,12,.4f,tick*.3f)==farDetail;
    require(filtered,"Far-water detail still aliases at a one-metre pixel footprint");
    scalar("diagnosticScale",0);
    const GLint origin=glGetUniformLocation(program,"diagnosticOrigin");
    const auto linearSample=[&](float x,float z,float time) {
        glUniform2f(origin,x,z);vector("cameraPosition",x,10,z);(void)sample(1,12,0,time);
        std::array<float,4> value{};glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,value.data());return value;
    };
    // Track an advected feature through production fragment output. Sampling
    // the later frame downstream must align better than sampling upstream.
    double forward=0,backward=0;
    for(const auto velocity:{std::array<float,2>{.6f,0.f},std::array<float,2>{-.36f,.48f}}) {
        glUniform2f(driftUniform,velocity[0],velocity[1]);
        for(int z=-8;z<=8;++z)for(int x=-8;x<=8;++x) {
            const float px=x*.37f,pz=z*.41f,dx=velocity[0]*.027f,dz=velocity[1]*.027f;
            const auto a=linearSample(px,pz,1),b=linearSample(px+dx,pz+dz,1.1f),c=linearSample(px-dx,pz-dz,1.1f);
            for(int k=0;k<3;++k){forward+=std::pow(a[k]-b[k],2);backward+=std::pow(a[k]-c[k],2);}
        }
    }
    std::cout<<"[WATER_SHADER] advected_forward_error="<<forward<<" backward_error="<<backward<<'\n';
    require(forward<backward*.95,"Surface features do not travel with the supplied downstream vector");
    glUniform2f(origin,0,0);glUniform2f(driftUniform,0,0);
    vector("sunColour",1,1,1);scalar("sunIntensity",0);const auto unlit=linearSample(0,0,0);
    scalar("sunIntensity",1);const auto lit=linearSample(0,0,0);
    for(int k=0;k<3;++k)require(lit[k]-unlit[k]>.15f && lit[k]-unlit[k]<=.205f,"Sun reflection exceeds its restrained highlight budget");
    scalar("sunIntensity",0);
    scalar("waterDetailStrength", 0);
    require(sample(1, 12, .4f, 0) == sample(1, 12, .4f, 1), "Fallback shoreline still animates");
    vector("cameraPosition", 0, -1, 0);
    require(sample(1, 12, 0, 0)[3] >= 225, "Underwater surface loses opacity");
    // Read the production fragment output while crossing the surface. A hard
    // medium switch used to change an entire clipped water sheet in one frame.
    // Nonzero horizontal offset keeps the view direction defined at height 0.
    int maxCrossingDelta = 0;
    std::size_t crossingSamples = 0;
    bool nearClipClear = true;
    for (float detailAmount : {0.f, 1.f}) {
        scalar("waterDetailStrength", detailAmount);
        for (float depth : {1.f, 8.f}) {
            std::array<unsigned char, 4> previous{};
            for (int millimetres = -1200; millimetres <= 800; ++millimetres) {
                vector("cameraPosition", 2, millimetres * .001f, 1);
                const auto current = sample(depth, 12, .4f, 1);
                if (millimetres > -1200) {
                    // Compare the visible contribution over a bright sky and
                    // dark bed, as well as alpha. Uncovered RGB is irrelevant.
                    for (float background : {24.f, 190.f}) {
                        for (int channel = 0; channel < 3; ++channel) {
                            const auto composite = [&](const auto& pixel) {
                                return int(std::round((pixel[channel] * pixel[3] +
                                    background * (255 - pixel[3])) / 255.f));
                            };
                            maxCrossingDelta = std::max(maxCrossingDelta,
                                std::abs(composite(current) - composite(previous)));
                        }
                    }
                    maxCrossingDelta = std::max(maxCrossingDelta,
                        std::abs(int(current[3]) - int(previous[3])));
                }
                if (std::abs(millimetres) <= 100)
                    nearClipClear = nearClipClear && current[3] == 0;
                previous = current;
                ++crossingSamples;
            }
        }
    }
    std::cout << "[WATER_SHADER] waterline_samples=" << crossingSamples
              << " max_composited_delta=" << maxCrossingDelta << '\n';
    require(maxCrossingDelta <= 3, "Waterline colour/opacity changes discontinuously");
    require(nearClipClear, "Near-eye water sheet is still opaque at the clipping plane");
    std::cout << "[WATER_SHADER] PASS depth-absorption distance-invariance depth-bound shore-motion drift-motion drift-continuity fallback underwater\n";
    vector("cameraPosition", 2, 4, 1); scalar("waterDetailStrength",1);
    vector("sunColour",1,.8f,.4f); scalar("sunIntensity",1);
    vector("skyHorizonColour",.5f,.6f,.7f); vector("skyZenithColour",.3f,.6f,1);
    glUniform2f(glGetUniformLocation(program,"diagnosticLightSources"),0,.8f);
    // WorldEnvironment::evaluate already tints these water palette uniforms
    // for time of day. V09's original fixture varied only exposure and sky,
    // missing this second daylight dependency in a torch-lit chamber.
    vector("waterShallowColour",.12f,.43f,.53f);
    vector("waterDeepColour",.018f,.15f,.24f);
    scalar("environmentLight",1); const auto caveDay=sample(4,12,.4f,1);
    scalar("environmentLight",0); scalar("sunIntensity",0);
    vector("skyHorizonColour",.01f,.01f,.02f); vector("skyZenithColour",.02f,.02f,.04f);
    vector("waterShallowColour",.020f,.075f,.13f);
    vector("waterDeepColour",.005f,.024f,.060f);
    require(caveDay==sample(4,12,.4f,1), "Enclosed lit pool changes with sky colour, sun, daylight or water palette");
    std::size_t enclosedPalettePairs = 0;
    for (float eyeHeight : {-1.f, 4.f}) {
        vector("cameraPosition",2,eyeHeight,1);
        for (float detailAmount : {0.f,1.f}) {
            scalar("waterDetailStrength",detailAmount);
            for (float depth : {.25f,4.f,8.f}) {
                for (float shore : {0.f,.4f,1.f}) {
                    vector("waterShallowColour",.12f,.43f,.53f);
                    vector("waterDeepColour",.018f,.15f,.24f);
                    const auto dayPalette=sample(depth,12,shore,1);
                    vector("waterShallowColour",.020f,.075f,.13f);
                    vector("waterDeepColour",.005f,.024f,.060f);
                    require(dayPalette==sample(depth,12,shore,1),
                        "Enclosed pool body, ripple or underwater tint follows outdoor palette");
                    ++enclosedPalettePairs;
                }
            }
        }
    }
    std::cout << "[WATER_SHADER] enclosed_palette_pairs=" << enclosedPalettePairs << '\n';
    vector("cameraPosition",2,4,1); scalar("waterDetailStrength",1);
    glUniform2f(glGetUniformLocation(program,"diagnosticLightSources"),0,0);
    const auto caveDark=sample(4,12,.4f,1);
    require(caveDark[1]<caveDay[1],"Unlit pool is as bright as a lit pool");
    glUniform2f(glGetUniformLocation(program,"diagnosticLightSources"),1,0);
    const auto skyNight=sample(4,12,.4f,1); scalar("environmentLight",1);
    require(skyNight!=sample(4,12,.4f,1),"Exposed pool ignores daylight");
    std::cout << "[WATER_SHADER] PASS enclosed-pool-stable no-underground-sky-reflection local-light-response exposed-pool-daylight\n";
    glDeleteTextures(1, &disabledReflectionTexture);
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
        // Accept a frozen legacy shader as a negative control. A new shader
        // must also have the real Ogre auto-constant bindings, not just uniforms
        // supplied by this diagnostic. Types are from local OgreGpuProgramParams.
        if (source.find("uniform mat4 view;") != std::string::npos) {
            const auto start = declarations.find("vertex_program HelloMine3D/WaterVertex glsl");
            const auto block = declarations.substr(start, declarations.find("\n}", start) - start);
            for (const char* binding : {"param_named_auto view view_matrix",
                    "param_named_auto projection projection_matrix",
                    "param_named_auto cameraPosition camera_position"})
                require(block.find(binding) != std::string::npos,
                    std::string("Missing camera-relative water binding: ") + binding);
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
        const char *varyings[] = {"waterWorldPosition", "waterWorldNormal", "gl_Position", "waterDistance"};
        glTransformFeedbackVaryings(program, 4, varyings, GL_INTERLEAVED_ATTRIBS);
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
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, 1, 1);
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
        const GLint velocity=glGetAttribLocation(program,"uv0");
        require(velocity>=0,"Water velocity does not affect wave geometry");
        const GLint globalTime = glGetUniformLocation(program, "globalTime");
        require(globalTime >= 0, "Missing wave time input");
        const auto draw = [&](const Matrix &world, const Matrix& view, const Matrix& projection,
                const std::array<float, 3>& camera, const Matrix& legacyWorldView,
                const Matrix& legacyWorldViewProj, float x, float y, float z) {
            const auto matrix = [&](const char* name, const Matrix& value) {
                glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, value.data());
            };
            matrix("world", world); matrix("view", view); matrix("projection", projection);
            matrix("worldView", legacyWorldView); matrix("worldViewProj", legacyWorldViewProj);
            glUniform3f(glGetUniformLocation(program, "cameraPosition"), camera[0], camera[1], camera[2]);
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
        const Matrix identity = translation(0, 0, 0);
        const auto sample = [&](const Matrix &world, float x, float y, float z) {
            return draw(world, identity, identity, {0, 0, 0}, world, world, x, y, z);
        };

        // Frozen original V06i standard-r2 frame170 inputs, not a new native
        // Ogre draw: world/worldView/worldViewProj/globalTime/camera are from
        // frame-170.json, and the eight shared uv0 pairs from its two native
        // VBOs (44B stride). Source hashes below allow an independent check.
        // frame-170-native-op-0.vbo0.bin: 6d33971e2ee2700cb05faee0ddafe19e493084456c1badd2b83349982ece4f2a
        // frame-170-native-op-1.vbo0.bin: 89bddd6975b19c06d29c53e98935549ea2ba4bb0843036cbb982bef744f294ee
        const Matrix frozenWorld[2] = {translation(208,64,-224), translation(208,64,-240)};
        const Matrix frozenWorldView[2] = {
            {1,0,0,0, 0,.7660444379f,.6427876949f,0, 0,-.6427876949f,.7660444379f,0, -8,.08639526367f,-10.37075806f,1},
            {1,0,0,0, 0,.7660444379f,.6427876949f,0, 0,-.6427876949f,.7660444379f,0, -8,10.3710022f,-22.62747192f,1}};
        const Matrix frozenWorldViewProj[2] = {
            {.5625f,0,0,0, 0,.7660444379f,-.6428005099f,-.6427876949f,
             0,-.6427876949f,-.7660596967f,-.7660444379f, -4.5f,.08639526367f,10.17096233f,10.37075806f},
            {.5625f,0,0,0, 0,.7660444379f,-.6428005099f,-.6427876949f,
             0,-.6427876949f,-.7660596967f,-.7660444379f, -4.5f,10.3710022f,22.42791939f,22.62747192f}};
        const Matrix frozenView = {1,0,0,0, 0,.7660444379f,.6427876949f,0,
            0,-.6427876949f,.7660444379f,0, 0,0,0,1};
        // Derive one common projection from op0's actual WVP/WV. The V06i
        // observer did not read native projection separately; this reconstruction
        // is explicitly fixture input, not a claim of current client binding.
        Matrix frozenProjection{};
        frozenProjection[0] = .5625f; frozenProjection[5] = 1;
        frozenProjection[10] = frozenWorldViewProj[0][10] / frozenWorldView[0][10];
        frozenProjection[11] = -1;
        frozenProjection[14] = frozenWorldViewProj[0][14] -
            frozenProjection[10] * frozenWorldView[0][14];
        const std::array<float,3> frozenCamera = {216,70.59999847f,-216};
        const std::array<std::array<float,2>,8> frozenVelocity = {{{0,.24943915009498596f},
            {.04237174242734909f,.5068253874778748f}, {.043656233698129654f,.5220351815223694f},
            {.04482452571392059f,.5358467102050781f}, {.04587234556674957f,.5482105016708374f},
            {.04679712653160095f,.5590968728065491f}, {.04759809374809265f,.5684980750083923f},
            {.048276450484991074f,.5764297246932983f}}};
        glUniform1f(globalTime, 5.343996048f);
        float frozenClipDelta = 0;
        std::size_t frozenMismatches = 0, frozenPairs = 0;
        for (float detailAmount : {0.f,1.f}) {
            glUniform1f(detail, detailAmount);
            for (int corner = 0; corner < 8; ++corner) {
                glVertexAttrib2f(velocity, frozenVelocity[corner][0], frozenVelocity[corner][1]);
                const auto a = draw(frozenWorld[0], frozenView, frozenProjection, frozenCamera,
                    frozenWorldView[0], frozenWorldViewProj[0], float(corner+9),1,0);
                const auto b = draw(frozenWorld[1], frozenView, frozenProjection, frozenCamera,
                    frozenWorldView[1], frozenWorldViewProj[1], float(corner+9),1,16);
                if (std::memcmp(a.data()+6, b.data()+6, sizeof(float)*4) != 0) ++frozenMismatches;
                for (int k = 6; k < 10; ++k)
                    frozenClipDelta = std::max(frozenClipDelta, std::abs(a[k]-b[k]));
                require(std::abs(a[10]-b[10]) < .00001f,
                    "Frozen V06i shared-point camera distance differs");
                ++frozenPairs;
            }
        }
        std::cout << "[WATER_SHADER] frozen_V06i_clip_pairs=" << frozenPairs
            << " mismatches=" << frozenMismatches << " max_clip_delta=" << frozenClipDelta << '\n';
        require(frozenMismatches == 0, "Frozen V06i shared-edge clip coordinates disagree");

        // Different vertical section-local Y must not round the wave offset
        // differently. Large camera/world coordinates must retain the small
        // offset in clip space after subtracting the shared camera position.
        std::size_t relativePairs = 0;
        const Matrix projection = {.5625f,0,0,0, 0,1,0,0,
            0,0,-1.00002f,-1, 0,0,-.200002f,0};
        for (float farOrigin : {-1048576.f,-4096.f,0.f,4096.f,1048576.f}) {
            const std::array<float,3> camera = {farOrigin+8,farOrigin+7,farOrigin+8};
            const Matrix view = translation(-camera[0],-camera[1],-camera[2]);
            const Matrix aWorld = translation(farOrigin,farOrigin,farOrigin);
            const Matrix bWorld = translation(farOrigin,farOrigin-16,farOrigin-16);
            glVertexAttrib2f(velocity, -.36f,.48f);
            float minimumClipY = 1000, maximumClipY = -1000;
            for (float detailAmount : {0.f,1.f}) {
                glUniform1f(detail, detailAmount);
                for (int tick = 0; tick < 64; ++tick) {
                    glUniform1f(globalTime, float(tick)*.125f);
                    const auto a = draw(aWorld, view, projection, camera, multiply(view,aWorld),
                        multiply(projection,multiply(view,aWorld)), 12,1,0);
                    const auto b = draw(bWorld, view, projection, camera, multiply(view,bWorld),
                        multiply(projection,multiply(view,bWorld)), 12,17,16);
                    require(std::memcmp(a.data()+6,b.data()+6,sizeof(float)*4) == 0,
                        "Camera-relative shared corner loses clip invariance across local Y or large coordinates");
                    require(std::abs(a[10]-b[10]) < .00001f,
                        "Camera-relative shared-point distance differs");
                    if (detailAmount > 0) {
                        minimumClipY = std::min(minimumClipY,a[7]);
                        maximumClipY = std::max(maximumClipY,a[7]);
                    }
                    ++relativePairs;
                }
            }
            require(maximumClipY-minimumClipY > .01f,
                "Large camera coordinates erase the animated water offset in clip space");
        }
        std::cout << "[WATER_SHADER] camera_relative_clip_pairs=" << relativePairs << " PASS\n";

        // The merged shader must keep new eighth-grid cut vertices on their
        // actual boundary without disturbing the shared projection or the
        // complete legacy mesh path. These are exact dyadic input positions;
        // no independent 32F whole-shader bit-pressure suite is needed here.
        const GLint shore = glGetAttribLocation(program, "uv1");
        const GLint pin = glGetAttribLocation(program, "uv3");
        const GLint boundaryGuard = glGetUniformLocation(program, "waterBoundaryPinsV1");
        require(shore >= 0 && pin >= 0 && boundaryGuard >= 0,
            "Missing active water boundary pin interface");
        const Matrix cutWorldA = translation(-256, 64, -256);
        const Matrix cutWorldB = translation(-272, 48, -272);
        glVertexAttrib2f(velocity, -.36f, .48f);
        std::size_t cutCases = 0, guardOffPairs = 0, ordinaryShoreCases = 0;
        for (float detailAmount : {0.f, 1.f}) {
            glUniform1f(detail, detailAmount);
            for (float time : {0.f, 7.125f}) {
                glUniform1f(globalTime, time);
                for (float rawShore : {0.f, .25f}) {
                    glVertexAttrib2f(shore, 4.f, rawShore);
                    for (float height : {.125f, .5f, .875f}) {
                        glUniform1f(boundaryGuard, 1.f);
                        glVertexAttrib1f(pin, 1.f);
                        const auto a = sample(cutWorldA, 4, height, 4);
                        const auto b = sample(cutWorldB, 20, 16 + height, 20);
                        require(a[1] == 64 + height && b[1] == 64 + height,
                            "Pinned water cut does not retain its exact eighth-grid height");
                        require(std::memcmp(a.data() + 6, b.data() + 6, sizeof(float) * 4) == 0,
                            "Pinned shared corner loses clip invariance across section-local Y");
                        ++cutCases;

                        glUniform1f(boundaryGuard, 0.f);
                        const auto disabledPin = sample(cutWorldA, 4, height, 4);
                        glVertexAttrib1f(pin, 0.f);
                        const auto legacy = sample(cutWorldA, 4, height, 4);
                        require(disabledPin == legacy,
                            "Disabled boundary guard changes the complete legacy water path");
                        require(std::abs(legacy[1] - (64 + height)) > .039f,
                            "Legacy water lost its original nonzero surface offset");
                        ++guardOffPairs;
                    }
                }
                glUniform1f(boundaryGuard, 1.f);
                glVertexAttrib1f(pin, 0.f);
                for (float rawShore : {.25f, .5f, 1.f}) {
                    glVertexAttrib2f(shore, 4.f, rawShore);
                    const auto ordinary = sample(cutWorldA, 4, 1, 4);
                    require(std::abs(ordinary[1] - 64.9f) < .00001f && ordinary[4] == 1.f,
                        "Ordinary opaque-shore corner lost its original -0.10 level or flat normal");
                    ++ordinaryShoreCases;
                }
            }
        }
        std::cout << "[WATER_SHADER] boundary_pin_cut_cases=" << cutCases
            << " guard_off_pairs=" << guardOffPairs
            << " ordinary_shore_cases=" << ordinaryShoreCases << " PASS\n";
        // Restore the old open-water fixture for all existing checks below.
        glUniform1f(boundaryGuard, 0.f);
        glVertexAttrib1f(pin, 0.f);
        glVertexAttrib2f(shore, 0.f, 0.f);
        glUniform1f(detail, 1.f);

        float maxPositionDelta = 0.0f, maxNormalDelta = 0.0f;
        std::size_t pairs = 0;
        // Both representations of a shared edge must produce the same output.
        // Include both horizontal axes, negative coordinates, the origin,
        // four-section corners, and multiple animation phases.
        const std::array<std::array<float,2>,4> profiles{{{{.04f,.03f}},{{.16f,.12f}},{{-.36f,.48f}},{{.8f,.6f}}}};
        for(const auto profile:profiles) {
          glVertexAttrib2f(velocity,profile[0],profile[1]);
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
        }
        float previousAmplitude=0;
        for(const auto profile:profiles) {
            glVertexAttrib2f(velocity,profile[0],profile[1]);float amplitude=0;
            for(int tick=0;tick<128;++tick) {
                glUniform1f(globalTime,tick*.125f);
                amplitude=std::max(amplitude,std::abs(sample(translation(0,48,0),4,16,4)[1]-63.9f));
            }
            std::cout<<"[WATER_SHADER] speed="<<std::hypot(profile[0],profile[1])<<" wave_amplitude="<<amplitude<<'\n';
            require(amplitude>previousAmplitude*1.5f && amplitude<=.06001f,"Waterbody wave profiles do not separate wetland/lake/river/sea");
            if(previousAmplitude==0)require(amplitude<.003f,"Wetland surface is not sheltered");
            previousAmplitude=amplitude;
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
