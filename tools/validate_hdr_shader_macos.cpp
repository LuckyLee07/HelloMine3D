// Offscreen CGL checks of production HDR GLSL; not Ogre FBO/UI/gameplay evidence.
// clang++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations \
//   tools/validate_hdr_shader_macos.cpp -framework OpenGL -o /tmp/hdr-shader-gpu
// /tmp/hdr-shader-gpu <repository-or-package-resource-root> <new-output-directory>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
using Pixel = std::array<float, 4>;
int checks = 0, failures = 0;
fs::path evidence;
std::ofstream samples;
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
std::string read(const fs::path& path) {
    std::ifstream input(path);
    require(input.good(), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}
void write(const fs::path& path, const std::string& text) {
    std::ofstream output(path);
    require(output.good(), "Cannot write " + path.string());
    output << text;
}
void check(const std::string& name, bool ok, double actual = 0, double expected = 0) {
    ++checks; failures += !ok;
    std::cout << "[HDR_GPU] " << (ok ? "PASS " : "FAIL ") << name << '\n';
    samples << name << '\t' << (ok ? "PASS" : "FAIL") << '\t'
            << std::setprecision(12) << actual << '\t' << expected << '\n';
}
void closeTo(const std::string& name, double actual, double expected, double tolerance = 0.00004) {
    check(name, std::isfinite(actual) && std::abs(actual - expected) <= tolerance, actual, expected);
}
double decode(double authored) {
    const double c = std::max(authored, 0.0);
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
double encode(double linear) {
    const double c = std::max(linear, 0.0);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
}
double resolveReference(double linear, double exposure) {
    const double x = std::max(0.0, linear * exposure);
    return encode(std::clamp((x * (2.51 * x + 0.03)) /
                             (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0));
}
std::string function(const std::string& source, const std::string& signature) {
    const auto start = source.find(signature), brace = source.find('{', start);
    require(start != std::string::npos && brace != std::string::npos,
            "Missing production function: " + signature);
    int depth = 0;
    for (auto i = brace; i < source.size(); ++i) {
        if (source[i] == '{') ++depth;
        if (source[i] == '}' && --depth == 0) return source.substr(start, i - start + 1);
    }
    throw std::runtime_error("Unterminated production function: " + signature);
}
GLuint shader(GLenum kind, const std::string& source) {
    const GLuint object = glCreateShader(kind); const char* pointer = source.c_str();
    glShaderSource(object, 1, &pointer, nullptr); glCompileShader(object);
    GLint ok = 0; char log[16384]{};
    glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
    glGetShaderInfoLog(object, sizeof(log), nullptr, log);
    require(ok == GL_TRUE, "Shader compile failed: " + std::string(log));
    return object;
}
GLuint program(const std::string& vertex, const std::string& fragment) {
    const GLuint object = glCreateProgram();
    const GLuint vs = shader(GL_VERTEX_SHADER, vertex), fs = shader(GL_FRAGMENT_SHADER, fragment);
    glAttachShader(object, vs); glAttachShader(object, fs); glLinkProgram(object);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0; char log[16384]{};
    glGetProgramiv(object, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(object, sizeof(log), nullptr, log);
    require(ok == GL_TRUE, "Program link failed: " + std::string(log));
    return object;
}
GLint location(GLuint object, const char* name) {
    const GLint result = glGetUniformLocation(object, name);
    require(result >= 0, std::string("Missing active uniform: ") + name); return result;
}
void uniform(GLuint object, const char* name, float value) {
    glUniform1f(location(object, name), value);
}
void sampler(GLuint object, const char* name) { glUniform1i(location(object, name), 0); }
const std::string fullscreen = R"GLSL(#version 150
uniform float probeDepth;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, probeDepth * 2.0 - 1.0, 1.0);
}
)GLSL";
const std::string resolveVertex = R"GLSL(#version 150
out vec2 postUv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); postUv = p;
}
)GLSL";
GLuint colourFixture(const std::string& productionFunction, bool earlyClamp = false) {
    return program(fullscreen, "#version 150\nuniform float linearHdrMode;\n"
        "uniform vec4 probe; uniform float probeGain; out vec4 fragmentColour;\n" +
        productionFunction + "\nvoid main() { vec3 radiance = sceneColour(probe.rgb) * probeGain;"
        "fragmentColour = vec4(" + (earlyClamp ? std::string("clamp(radiance, 0.0, 1.0)") :
                                              std::string("radiance")) + ", probe.a); }\n");
}
struct Context {
    CGLContextObj object = nullptr;
    Context() {
        CGLPixelFormatAttribute attributes[]{kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),
            kCGLPFAAccelerated, static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format = nullptr; GLint count = 0;
        require(CGLChoosePixelFormat(attributes, &format, &count) == kCGLNoError && format,
                "No accelerated CGL pixel format");
        const auto status = CGLCreateContext(format, nullptr, &object);
        CGLDestroyPixelFormat(format);
        require(status == kCGLNoError && object, "No CGL context");
        require(CGLSetCurrentContext(object) == kCGLNoError, "Cannot activate CGL context");
    }
    ~Context() { CGLSetCurrentContext(nullptr); if (object) CGLDestroyContext(object); }
};
struct Target {
    GLuint fbo = 0, colour = 0, depth = 0;
    explicit Target(GLenum format) {
        glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &colour); glBindRenderbuffer(GL_RENDERBUFFER, colour);
        glRenderbufferStorage(GL_RENDERBUFFER, format, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colour);
        glGenRenderbuffers(1, &depth); glBindRenderbuffer(GL_RENDERBUFFER, depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, 1, 1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete float FBO");
        GLint actual = 0; glBindRenderbuffer(GL_RENDERBUFFER, colour);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &actual);
        require(actual == static_cast<GLint>(format), "Float FBO format silently downgraded");
    }
    void bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, 1, 1); }
    void clear(const Pixel& background = Pixel{0, 0, 0, 0}) const {
        bind(); glClearBufferfv(GL_COLOR, 0, background.data());
        const float z = 1; glClearBufferfv(GL_DEPTH, 0, &z);
    }
    Pixel pixel() const {
        bind(); Pixel result{}; glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, result.data()); return result;
    }
    float depthValue() const {
        bind(); float result = 0; glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &result); return result;
    }
    ~Target() { glDeleteRenderbuffers(1, &colour); glDeleteRenderbuffers(1, &depth); glDeleteFramebuffers(1, &fbo); }
};
GLuint texture(const Pixel& value) {
    GLuint object = 0; glGenTextures(1, &object);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, object);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_FLOAT, value.data());
    return object;
}
Pixel sceneSample(const Target& target, GLuint object, const Pixel& value, float mode, float gain = 1) {
    target.clear(); glUseProgram(object); glUniform4fv(location(object, "probe"), 1, value.data());
    uniform(object, "linearHdrMode", mode); uniform(object, "probeGain", gain);
    uniform(object, "probeDepth", 0.37f); glDrawArrays(GL_TRIANGLES, 0, 3); return target.pixel();
}
Pixel resolveSample(const Target& target, GLuint object, const Pixel& input, float exposure) {
    target.clear(); glUseProgram(object); const GLuint tex = texture(input);
    sampler(object, "sceneTexture"); uniform(object, "exposure", exposure);
    glDrawArrays(GL_TRIANGLES, 0, 3); const Pixel result = target.pixel(); glDeleteTextures(1, &tex); return result;
}
void summary(const std::string& renderer, const std::string& version, const std::string& fatal = "") {
    const bool pass = failures == 0 && fatal.empty();
    std::string result = "status=" + std::string(pass ? "PASS" : "FAIL") +
        "\nchecks=" + std::to_string(checks) + "\nfailures=" + std::to_string(failures) +
        "\nrenderer=" + renderer + "\nversion=" + version +
        "\nscope=offscreen CGL: extracted production colour functions, full resolve, particle, cave mask and caster fragments\n"
        "native_ogre_fbo=NOT_RUN\nui_composition=NOT_RUN\nnormal_gameplay=NOT_RUN\n";
    if (!fatal.empty()) result += "fatal=" + fatal + "\n";
    write(evidence / "summary.txt", result);
    std::cout << "[HDR_GPU] status=" << (pass ? "PASS" : "FAIL") << " checks=" << checks
              << " failures=" << failures << " evidence=" << evidence << '\n';
}
} // namespace

int main(int argc, char** argv) {
    std::string renderer, version;
    try {
        require(argc == 3, "Usage: hdr-shader-gpu <resource-root> <new-output-directory>");
        const fs::path root = fs::absolute(argv[1]); evidence = fs::absolute(argv[2]);
        require(!fs::exists(evidence), "Refusing to replace previous evidence: " + evidence.string());
        fs::create_directories(evidence / "inputs"); fs::create_directories(evidence / "faults");
        samples.open(evidence / "samples.tsv"); require(samples.good(), "Cannot create sample evidence");
        samples << "check\tstatus\tactual\texpected\n";
        std::map<std::string, std::string> sources;
        auto source = [&](const std::string& name) -> const std::string& {
            auto found = sources.find(name);
            if (found == sources.end()) {
                const auto text = read(root / "media/ogre" / name);
                write(evidence / "inputs" / name, text); found = sources.emplace(name, text).first;
            }
            return found->second;
        };
        Context context;
        renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        std::cout << "[HDR_GPU] renderer=" << renderer << " version=" << version << '\n';
        GLuint vao = 0; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
        glDisable(GL_FRAMEBUFFER_SRGB); glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
        glEnable(GL_DEPTH_TEST); glDepthFunc(GL_ALWAYS); glDepthMask(GL_TRUE);
        Target precise(GL_RGBA32F), hdr(GL_RGBA16F);
        const std::array<std::pair<const char*, const char*>, 9> sceneShaders{{
            {"HelloMine3DTerrain.frag", "HelloMine3DTerrain.vert"},
            {"HelloMine3DTerrainShadow.frag", "HelloMine3DTerrainShadow.vert"},
            {"HelloMine3DActor.frag", "HelloMine3DActor.vert"},
            {"HelloMine3DActorShadow.frag", "HelloMine3DActorShadow.vert"},
            {"HelloMine3DWater.frag", "HelloMine3DWater.vert"},
            {"HelloMine3DSkybox.frag", "HelloMine3DSkybox.vert"},
            {"HelloMine3DBlockFeedback.frag", "HelloMine3DTerrain.vert"},
            {"HelloMine3DBlockParticle.frag", "HelloMine3DBlockParticle.vert"},
            {"HelloMine3DCaveBoundary.frag", "HelloMine3DCaveBoundary.vert"}
        }};
        for (const auto& pair : sceneShaders) {
            const std::string name(pair.first);
            const GLuint full = program(source(pair.second), source(pair.first));
            check(name + "/complete-production-program-links", true); glDeleteProgram(full);
            const GLuint object = colourFixture(function(source(pair.first), "vec3 sceneColour(vec3 authored)"));
            for (float input : {-0.2f, 0.f, 0.003f, 0.04045f, 0.04046f, 0.18f, 0.5f, 1.f, 1.1f}) {
                const std::string suffix = "/" + std::to_string(input);
                const Pixel value{input, input * 0.7f, input * 0.3f, 0.35f};
                const Pixel legacy = sceneSample(precise, object, value, 0), linear = sceneSample(precise, object, value, 1);
                for (int c = 0; c < 3; ++c) {
                    closeTo(name + "/legacy-identity" + suffix + "/" + std::to_string(c), legacy[c], value[c]);
                    closeTo(name + "/decode-reference" + suffix + "/" + std::to_string(c), linear[c], decode(value[c]));
                }
                closeTo(name + "/alpha-data-unchanged" + suffix, linear[3], value[3]);
                closeTo(name + "/depth-data-unchanged" + suffix, precise.depthValue(), 0.37);
            }
            for (float intensity : {0.18f, 1.f, 2.f, 8.f}) {
                const Pixel sample = sceneSample(hdr, object, {1, 1, 1, 0.35f}, 1, intensity);
                closeTo(name + "/float16-radiance/" + std::to_string(intensity), sample[0], intensity, 0.0003);
            }
            glDeleteProgram(object);
        }
        const std::string resolveSource = source("HelloMine3DHdrResolve.frag");
        const GLuint completeResolve = program(source("HelloMine3DPostProcess.vert"), resolveSource);
        check("resolve/complete-production-program-links", true); glDeleteProgram(completeResolve);
        const GLuint resolve = program(resolveVertex, resolveSource); glDisable(GL_DEPTH_TEST);
        for (float exposure : {0.25f, 1.f, 2.f}) {
            float previous = -1;
            for (float intensity : {0.f, 0.00001f, 0.0031308f, 0.018f, 0.18f, 0.5f, 1.f, 2.f, 8.f, 16.f}) {
                const Pixel result = resolveSample(precise, resolve, {intensity, intensity, intensity, 0.35f}, exposure);
                const std::string name = "resolve/exposure-" + std::to_string(exposure) + "/radiance-" + std::to_string(intensity);
                closeTo(name + "/independent-tone-srgb-reference", result[0], resolveReference(intensity, exposure), 0.0005);
                check(name + "/monotonic", result[0] >= previous - 0.00001f, result[0], previous);
                closeTo(name + "/alpha-unchanged", result[3], 0.35, 0.0003); previous = result[0];
            }
        }
        const Pixel chroma = resolveSample(precise, resolve, {0.18f, 1.f, 8.f, 0.75f}, 1);
        for (int c = 0; c < 3; ++c) closeTo("resolve/channel-reference/" + std::to_string(c), chroma[c],
            resolveReference(Pixel{0.18f, 1.f, 8.f, 0.75f}[c], 1), 0.0005);
        closeTo("resolve/alpha-not-tone-mapped", chroma[3], 0.75);
        const std::string particleVertex = R"GLSL(#version 150
out vec2 particleTileUv; out vec2 particleUv; out vec4 particleColour; out float particleDistance;
uniform float probeDepth;
void main() {
    vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
    gl_Position=vec4(p*2.0-1.0,probeDepth*2.0-1.0,1.0);
    particleTileUv=vec2(0); particleUv=vec2(0.5); particleColour=vec4(0.6,0.9,0.3,0.7); particleDistance=0;
}
)GLSL";
        const Pixel texel{0.8f, 0.5f, 0.18f, 0.6f};
        auto particleSample = [&](GLuint object, float mode, bool blending) {
            precise.clear({0.18f, 1.f, 2.f, 0.8f}); glUseProgram(object); const GLuint tex = texture(texel);
            sampler(object, "terrainAtlas"); uniform(object, "linearHdrMode", mode);
            uniform(object, "atlasPixels", 1); uniform(object, "tilePixels", 1); uniform(object, "tilesPerRow", 1);
            uniform(object, "environmentLight", 1); uniform(object, "probeDepth", 0.37f);
            glUniform3f(location(object, "fogColour"), 0, 0, 0); uniform(object, "fogDensity", 0);
            if (blending) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
            else glDisable(GL_BLEND);
            glDrawArrays(GL_TRIANGLES, 0, 3); const Pixel result = precise.pixel();
            glDisable(GL_BLEND); glDeleteTextures(1, &tex); return result;
        };
        const GLuint particle = program(particleVertex, source("HelloMine3DBlockParticle.frag"));
        glEnable(GL_DEPTH_TEST);
        const Pixel particleLinear = particleSample(particle, 1, false), particleLegacy = particleSample(particle, 0, false);
        const std::array<double, 3> tint{0.6, 0.9, 0.3};
        for (int c = 0; c < 3; ++c) {
            closeTo("particle/actual-colour-decode/" + std::to_string(c), particleLinear[c], decode(texel[c]) * decode(tint[c]), 0.0004);
            closeTo("particle/actual-legacy-product/" + std::to_string(c), particleLegacy[c], texel[c] * tint[c], 0.0004);
        }
        closeTo("particle/actual-alpha-product-raw", particleLinear[3], 0.6 * 0.7, 0.0003);
        closeTo("particle/actual-depth-raw", precise.depthValue(), 0.37);
        const Pixel blended = particleSample(particle, 1, true), background{0.18f, 1.f, 2.f, 0.8f};
        for (int c = 0; c < 3; ++c) closeTo("particle/actual-linear-blend/" + std::to_string(c), blended[c],
            particleLinear[c] * particleLinear[3] + background[c] * (1 - particleLinear[3]));
        const Pixel wrongBlend = particleSample(particle, 0, true);
        check("fault/nonlinear-particle-blend-rejected", std::abs(wrongBlend[0] - blended[0]) > 0.05);
        std::string gammaAlpha = source("HelloMine3DBlockParticle.frag");
        const auto alphaAt = gammaAlpha.find("texel.a * particleColour.a");
        require(alphaAt != std::string::npos, "Missing particle alpha expression for fault fixture");
        gammaAlpha.replace(alphaAt, std::string("texel.a * particleColour.a").size(), "pow(texel.a * particleColour.a, 2.4)");
        write(evidence / "faults/gamma-alpha.frag", gammaAlpha);
        const GLuint badAlpha = program(particleVertex, gammaAlpha);
        check("fault/gamma-alpha-rejected", std::abs(particleSample(badAlpha, 1, false)[3] - particleLinear[3]) > 0.1);
        glDeleteProgram(badAlpha); glDeleteProgram(particle);
        const std::string caveVertex = R"GLSL(#version 150
out vec2 boundaryUV;
void main() {vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);boundaryUV=vec2(0.5);}
)GLSL";
        const GLuint cave = program(caveVertex, source("HelloMine3DCaveBoundary.frag"));
        for (float mask : {0.4f, 0.6f}) {
            precise.clear(); glUseProgram(cave); const GLuint tex = texture({mask, 0, 0, 1});
            sampler(cave, "caveBoundaryMask"); uniform(cave, "linearHdrMode", 1); glDrawArrays(GL_TRIANGLES, 0, 3);
            const Pixel result = precise.pixel();
            check("cave/actual-mask-data/" + std::to_string(mask), mask < 0.5f ? result[3] == 0 : result[3] == 1);
            if (mask > 0.5f) closeTo("cave/actual-unlit-colour-decode", result[0], decode(0.035));
            glDeleteTextures(1, &tex);
        }
        glDeleteProgram(cave);
        const std::string casterVertex = R"GLSL(#version 150
flat out vec3 casterNaturalTreeRoot; uniform float probeDepth;
void main() {vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,probeDepth*2.0-1.0,1);casterNaturalTreeRoot=vec3(0);}
)GLSL";
        const GLuint caster = program(casterVertex, source("HelloMine3DDirectionalShadowCaster.frag"));
        for (float depth : {0.18f, 0.37f, 0.75f}) {
            precise.clear(); glUseProgram(caster); uniform(caster, "probeDepth", depth); glDrawArrays(GL_TRIANGLES, 0, 3);
            closeTo("caster/actual-depth-output-data/" + std::to_string(depth), precise.pixel()[0], depth);
            closeTo("caster/actual-depth-attachment-data/" + std::to_string(depth), precise.depthValue(), depth);
        }
        glDeleteProgram(caster); glDisable(GL_DEPTH_TEST);
        const auto sceneFunction = function(source("HelloMine3DTerrain.frag"), "vec3 sceneColour(vec3 authored)");
        const GLuint earlyClamp = colourFixture(sceneFunction, true);
        check("fault/early-HDR-clamp-rejected", std::abs(sceneSample(hdr, earlyClamp, {1, 1, 1, 1}, 1, 8)[0] - 8) > 1);
        glDeleteProgram(earlyClamp);
        std::string noTone = resolveSource;
        const auto tone = function(resolveSource, "vec3 toneMap(vec3 radiance)");
        noTone.replace(noTone.find(tone), tone.size(), "vec3 toneMap(vec3 radiance) { return clamp(radiance * exposure, 0.0, 1.0); }");
        write(evidence / "faults/missing-tone-map.frag", noTone);
        const GLuint badTone = program(resolveVertex, noTone);
        check("fault/missing-tone-map-rejected", std::abs(resolveSample(precise, badTone, {0.18f, 0.18f, 0.18f, 1}, 1)[0] - resolveReference(0.18, 1)) > 0.05);
        glDeleteProgram(badTone);
        std::string doubleEncode = resolveSource;
        const auto mainEnd = doubleEncode.rfind('}');
        require(mainEnd != std::string::npos, "Missing resolve main body");
        doubleEncode.insert(mainEnd, "fragmentColour.rgb = pow(fragmentColour.rgb, vec3(1.0 / 2.2));\n");
        write(evidence / "faults/double-display-encode.frag", doubleEncode);
        const GLuint badEncode = program(resolveVertex, doubleEncode);
        check("fault/double-display-encode-rejected", std::abs(resolveSample(precise, badEncode, {0.18f, 0.18f, 0.18f, 1}, 1)[0] - resolveReference(0.18, 1)) > 0.05);
        glDeleteProgram(badEncode); glDeleteProgram(resolve);
        check("all-operations/GL-error-zero", glGetError() == GL_NO_ERROR);
        glDeleteVertexArrays(1, &vao); summary(renderer, version); return failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[HDR_GPU] ERROR " << error.what() << '\n';
        if (!evidence.empty() && samples.is_open()) summary(renderer, version, error.what());
        return 1;
    }
}
