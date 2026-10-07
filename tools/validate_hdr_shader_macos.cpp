// Offscreen CGL checks of production HDR GLSL; not Ogre FBO/UI/gameplay evidence.
// clang++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations \
//   tools/validate_hdr_shader_macos.cpp -framework OpenGL -o /tmp/hdr-shader-gpu
// /tmp/hdr-shader-gpu <repository-or-package-resource-root> <new-output-directory>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

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
// Actual full CaveBoundary VS/FS samples. These prove supplied world/range
// inputs and shader output; Ogre's live logical-centre binding is separate.
const std::array<float,16> CaveIdentity{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
struct CaveCase {
    std::string name;
    std::array<float,16> world = CaveIdentity;
    std::array<float,2> range{{8,14}}, centre{{0,0}};
    float strength=1, mode=0, mask=0.6f;
};
CaveCase caveCase(const std::string& name, float x, float z, float strength=1,
                  float mode=0, std::array<float,2> centre={{0,0}}) {
    CaveCase value; value.name=name; value.world[12]=x; value.world[13]=64;
    value.world[14]=z; value.strength=strength; value.mode=mode; value.centre=centre;
    return value;
}
struct CaveSample {
    Pixel before{}, after{};
    float depthBefore=0, depthAfter=0;
    std::array<float,16> world{}, wvp{};
    std::array<float,2> range{}, centre{};
    std::array<float,3> fog{};
    float strength=0, mode=0;
    GLint maskUnit=-1;
    bool uniforms=false;
};
bool caveDepthExact(const CaveSample& value) {
    return std::memcmp(&value.depthBefore,&value.depthAfter,sizeof(float))==0;
}
CaveSample caveSample(const Target& target, GLuint object, GLuint buffer,
                      const CaveCase& input, bool writeDepth=false,
                      const Pixel& background=Pixel{.18f,1,2,.35f}) {
    struct Vertex {float x,y,z,u,v;};
    const std::array<Vertex,3> vertices{{{-1,-1,0,.5f,.5f},{3,-1,0,.5f,.5f},{-1,3,0,.5f,.5f}}};
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices.data(),GL_DYNAMIC_DRAW);
    glUseProgram(object);
    const GLint position=glGetAttribLocation(object,"vertex"),uv=glGetAttribLocation(object,"uv0");
    require(position>=0 && uv>=0,"Cave production vertex/UV interface inactive");
    glEnableVertexAttribArray(position); glEnableVertexAttribArray(uv);
    glVertexAttribPointer(position,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glVertexAttribPointer(uv,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(3*sizeof(float)));
    // Model translation is real; the supplied WVP compensates the camera so
    // this target's pixel centre is local (0,0,0), hence world=(x,64,z).
    glUniformMatrix4fv(location(object,"worldViewProj"),1,GL_FALSE,CaveIdentity.data());
    const GLint world=glGetUniformLocation(object,"world"),range=glGetUniformLocation(object,"viewRange");
    const GLint centre=glGetUniformLocation(object,"viewRangeCentre"),strength=glGetUniformLocation(object,"viewRangeStrength");
    const GLint fog=glGetUniformLocation(object,"fogColour");
    if(world>=0) glUniformMatrix4fv(world,1,GL_FALSE,input.world.data());
    if(range>=0) glUniform2fv(range,1,input.range.data());
    if(centre>=0) glUniform2fv(centre,1,input.centre.data());
    if(strength>=0) glUniform1f(strength,input.strength);
    if(fog>=0) glUniform3f(fog,.6f,.7f,.8f);
    uniform(object,"linearHdrMode",input.mode); sampler(object,"caveBoundaryMask");
    const GLuint tex=texture({input.mask,0,0,1});
    CaveSample result;
    glGetUniformfv(object,location(object,"linearHdrMode"),&result.mode);
    glGetUniformfv(object,location(object,"worldViewProj"),result.wvp.data());
    glGetUniformiv(object,location(object,"caveBoundaryMask"),&result.maskUnit);
    if(world>=0 && range>=0 && centre>=0 && strength>=0 && fog>=0) {
        glGetUniformfv(object,world,result.world.data()); glGetUniformfv(object,range,result.range.data());
        glGetUniformfv(object,centre,result.centre.data()); glGetUniformfv(object,strength,&result.strength);
        glGetUniformfv(object,fog,result.fog.data());
        result.uniforms=result.world==input.world && result.wvp==CaveIdentity && result.range==input.range &&
            result.centre==input.centre && result.strength==input.strength && result.mode==input.mode &&
            result.fog==std::array<float,3>{{.6f,.7f,.8f}} && result.maskUnit==0;
    }
    glDepthMask(GL_TRUE); glDisable(GL_BLEND); target.clear(background);
    result.before=target.pixel(); result.depthBefore=target.depthValue();
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDepthMask(writeDepth?GL_TRUE:GL_FALSE);
    glDrawArrays(GL_TRIANGLES,0,3);
    result.after=target.pixel(); result.depthAfter=target.depthValue();
    glDeleteTextures(1,&tex); glDepthMask(GL_TRUE); glDepthFunc(GL_ALWAYS);
    require(glGetError()==GL_NO_ERROR,"Cave actual draw/uniform/readback GL error");
    return result;
}
double caveCoverage(const CaveCase& input) {
    if(input.strength<=0 || input.range[1]<=input.range[0]) return 1;
    const double distance=std::max(std::abs(double(input.world[12])-input.centre[0]),
                                   std::abs(double(input.world[14])-input.centre[1]));
    const double t=std::clamp((distance-input.range[0])/(input.range[1]-input.range[0]),0.0,1.0);
    return 1-std::clamp(double(input.strength),0.0,1.0)*t*t*(3-2*t);
}
bool caveReferenceMatches(const CaveSample& actual, const CaveCase& input, double tolerance) {
    if(!actual.uniforms || !caveDepthExact(actual)) return false;
    const double coverage=caveCoverage(input);
    const std::array<float,3> fog{{.6f,.7f,.8f}},dark{{.035f,.043f,.054f}};
    for(int c=0;c<4;++c) {
        double expected=actual.before[c];
        if(input.mask>=.5f && coverage>0)
            expected=c==3 ? 1 : (input.mode>=.5f ? decode(fog[c])*(1-coverage)+decode(dark[c])*coverage :
                                                   fog[c]*(1-coverage)+dark[c]*coverage);
        if(!std::isfinite(actual.after[c]) || std::abs(actual.after[c]-expected)>tolerance) return false;
    }
    return true;
}
bool caveOldExact(const CaveSample& actual, const CaveSample& old) {
    return actual.uniforms && caveDepthExact(actual) && caveDepthExact(old) &&
        std::memcmp(actual.after.data(),old.after.data(),sizeof(Pixel))==0 &&
        std::memcmp(actual.before.data(),old.before.data(),sizeof(Pixel))==0 &&
        std::memcmp(&actual.depthBefore,&old.depthBefore,sizeof(float))==0;
}
void caveReadback(const std::string& format, const CaveCase& input, const CaveSample& actual,
                  const std::string& variant="production") {
    fs::create_directories(evidence/"cave-range-raw");
    const auto path=evidence/"cave-range-raw"/(format+"-"+input.name+"-"+variant);
    std::ofstream raw(path.string()+".readback.f32",std::ios::binary);
    const std::array<float,10> values{{actual.before[0],actual.before[1],actual.before[2],actual.before[3],
        actual.after[0],actual.after[1],actual.after[2],actual.after[3],actual.depthBefore,actual.depthAfter}};
    raw.write(reinterpret_cast<const char*>(values.data()),sizeof(values)); require(raw.good(),"Cannot save cave range pixels");
    std::ofstream facts(path.string()+".facts.txt"); facts<<std::setprecision(9);
    facts<<"scope=actual-production-VS-FS-supplied-world-1x1\nformat="<<format
        <<"\nreadback_type=GL_FLOAT\nvariant="<<variant<<"\nuniform_readback="<<actual.uniforms
        <<"\ndepth_exact="<<caveDepthExact(actual)<<"\nworld_actual=";
    for(float v:actual.world) facts<<v<<',';
    facts<<"\nworld_input="; for(float v:input.world) facts<<v<<',';
    facts<<"\nwvp_actual="; for(float v:actual.wvp) facts<<v<<',';
    facts<<"\nrange_actual="<<actual.range[0]<<','<<actual.range[1]<<"\ncentre_actual="<<actual.centre[0]<<','<<actual.centre[1]
        <<"\nstrength_actual="<<actual.strength<<"\nmode_actual="<<actual.mode<<"\nfog_actual=";
    for(float v:actual.fog) facts<<v<<',';
    facts<<"\nmask_unit_actual="<<actual.maskUnit<<"\nmask_input="<<input.mask
        <<"\nexpected_coverage="<<caveCoverage(input)<<"\nOgre_live_binding=NOT_RUN\nnormal_gameplay=NOT_RUN\n";
    require(facts.good(),"Cannot save cave uniform readback");
}
std::string caveMutate(std::string source, const std::string& from, const std::string& to) {
    const auto at=source.find(from); require(at!=std::string::npos && source.find(from,at+from.size())==std::string::npos,
                                           "Missing/duplicate actual cave fault seam: "+from);
    source.replace(at,from.size(),to); return source;
}
void caveRangeChecks(const Target& precise, const Target& hdr, const std::string& vertex,
                     const std::string& fragment, const std::string& oldFragment) {
    const GLuint actual=program(vertex,fragment),old=program(vertex,oldFragment);
    GLint callerVao=0,callerArrayBuffer=0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&callerVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&callerArrayBuffer);
    // Attribute pointers capture VBOs in their VAO. Keep these production-VS
    // inputs out of the caller's empty gl_VertexID VAO used by later gates.
    GLuint caveVao=0,buffer=0;
    glGenVertexArrays(1,&caveVao); glBindVertexArray(caveVao);
    glGenBuffers(1,&buffer);
    // Preserve the original mask threshold/decoded-colour gates with actual VS.
    for(float mask:{.4f,.6f}) {
        auto input=caveCase("original-mask",0,0,0,1); input.mask=mask;
        const auto sample=caveSample(precise,actual,buffer,input,true,Pixel{0,0,0,0});
        check("cave/actual-mask-data/"+std::to_string(mask),mask<.5f ? sample.after[3]==0 : sample.after[3]==1);
        if(mask>.5f) closeTo("cave/actual-unlit-colour-decode",sample.after[0],decode(.035));
    }
    const auto wrongCoverage=caveMutate(fragment,"float coverage = 1.0 - smoothstep(viewRange.x, viewRange.y, edgeDistance);",
        "float coverage = 1.0 - 0.5 * smoothstep(viewRange.x, viewRange.y, edgeDistance);");
    const auto noDiscard=caveMutate(fragment,"if (coverage <= 0.0) discard;","if (coverage < -1.0) discard;");
    write(evidence/"faults/cave-half-range-coverage.frag",wrongCoverage);
    write(evidence/"faults/cave-no-range-discard.frag",noDiscard);
    write(evidence/"inputs/cave-pre-range-baseline.frag",oldFragment);
    const GLuint coverageFault=program(vertex,wrongCoverage),discardFault=program(vertex,noDiscard);
    for(const auto& format:std::array<std::pair<const char*,const Target*>,2>{{{"RGBA32F",&precise},{"RGBA16F",&hdr}}}) {
        const double tolerance=format.second==&hdr ? .0003 : .00004;
        const std::string prefix="cave/range/"+std::string(format.first)+"/";
        for(float mode:{0.f,1.f}) {
            const auto off=caveCase("strength0-far-mode"+std::to_string(int(mode)),100,-100,0,mode);
            const auto near=caveCase("strength1-near-mode"+std::to_string(int(mode)),2,3,1,mode);
            for(const auto& input:{off,near}) {
                const auto current=caveSample(*format.second,actual,buffer,input),baseline=caveSample(*format.second,old,buffer,input);
                caveReadback(format.first,input,current); caveReadback(format.first,input,baseline,"pre-range-baseline");
                check(prefix+input.name+"-old-pixels-depth-exact",caveOldExact(current,baseline));
            }
            for(const auto& input:{caveCase("cutoff-mode"+std::to_string(int(mode)),14,0,1,mode),
                caveCase("outside-mode"+std::to_string(int(mode)),16,0,1,mode),
                caveCase("partial-mode"+std::to_string(int(mode)),11,0,1,mode)}) {
                const auto current=caveSample(*format.second,actual,buffer,input); caveReadback(format.first,input,current);
                check(prefix+input.name+"-independent-colour-depth",caveReferenceMatches(current,input,tolerance));
            }
        }
        std::vector<CaveCase> cases{caveCase("negative-partial",-43,-48,1,1,{{-32,-48}}),
            caveCase("diagonal-chebyshev",11,11,1,1),caveCase("logical-centre",139,-85,1,1,{{128,-96}}),
            caveCase("partial-strength",11,0,.25f,1)};
        auto masked=caveCase("mask-discard-partial",11,0,1,1); masked.mask=.4f; cases.push_back(masked);
        auto invalid=caveCase("invalid-range",100,-100,1,1); invalid.range={{14,8}}; cases.push_back(invalid);
        for(const auto& input:cases) {
            const auto current=caveSample(*format.second,actual,buffer,input); caveReadback(format.first,input,current);
            check(prefix+input.name+"-independent-colour-depth",caveReferenceMatches(current,input,tolerance));
        }
        const auto far=caveCase("fault-far",16,0,1,1);
        const auto badCoverage=caveSample(*format.second,coverageFault,buffer,far),badDiscard=caveSample(*format.second,discardFault,buffer,far);
        caveReadback(format.first,far,badCoverage,"half-range-coverage"); caveReadback(format.first,far,badDiscard,"no-range-discard");
        check(prefix+"reject-half-range-coverage-at-far-pixels",badCoverage.uniforms && caveDepthExact(badCoverage) && !caveReferenceMatches(badCoverage,far,tolerance));
        check(prefix+"reject-no-range-discard-at-far-pixels",badDiscard.uniforms && caveDepthExact(badDiscard) && !caveReferenceMatches(badDiscard,far,tolerance));
    }
    glDeleteProgram(coverageFault); glDeleteProgram(discardFault); glDeleteProgram(actual); glDeleteProgram(old);
    glBindVertexArray(static_cast<GLuint>(callerVao));
    glBindBuffer(GL_ARRAY_BUFFER,static_cast<GLuint>(callerArrayBuffer));
    glDeleteVertexArrays(1,&caveVao); glDeleteBuffers(1,&buffer);
    GLint restoredVao=0,restoredArrayBuffer=0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&restoredVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&restoredArrayBuffer);
    const bool callerRestored=restoredVao==callerVao && restoredArrayBuffer==callerArrayBuffer;
    check("cave/range/restores-caller-VAO-and-array-buffer",callerRestored);
    std::ofstream state(evidence/"cave-range-caller-state.txt");
    state<<"scope=actual-glGetIntegerv-before-after-own-VAO-destruction\ncaller_vao_before="<<callerVao
        <<"\ncaller_array_buffer_before="<<callerArrayBuffer<<"\nprivate_cave_vao="<<caveVao
        <<"\nprivate_cave_buffer="<<buffer<<"\ncaller_vao_after="<<restoredVao
        <<"\ncaller_array_buffer_after="<<restoredArrayBuffer<<"\ncaller_state_restored="<<callerRestored<<'\n';
    require(state.good(),"Cannot save actual caller VAO/buffer state");
}

void summary(const std::string& renderer, const std::string& version, const std::string& fatal = "") {
    const bool pass = failures == 0 && fatal.empty();
    std::string result = "status=" + std::string(pass ? "PASS" : "FAIL") +
        "\nchecks=" + std::to_string(checks) + "\nfailures=" + std::to_string(failures) +
        "\nrenderer=" + renderer + "\nversion=" + version +
        "\nscope=offscreen CGL: extracted production colour functions, full resolve, particle, full cave mask/range VS/FS with supplied world/uniform readback, and caster fragments\n"
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
        // Frozen pre-range production FS at 2a448fdab39523f797f4339f154c824054d720ea.
        const std::string oldCaveFragment=R"BASELINE(#version 150

// The legacy branch keeps authored display colours untouched. HDR scene
// shaders decode colour inputs before lighting/blending; alpha/data stay raw.
uniform float linearHdrMode;
vec3 sceneColour(vec3 authored)
{
    if (linearHdrMode < 0.5) return authored;
    vec3 c = max(authored, vec3(0.0));
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), c));
}


in vec2 boundaryUV;
uniform sampler2D caveBoundaryMask;
out vec4 fragColour;

void main()
{
    if (texture(caveBoundaryMask, boundaryUV).r < 0.5)
        discard;
    // The same unlit underground background used by the terrain fog.
    fragColour = vec4(sceneColour(vec3(0.035, 0.043, 0.054)), 1.0);
}
)BASELINE";
        caveRangeChecks(precise,hdr,source("HelloMine3DCaveBoundary.vert"),
                        source("HelloMine3DCaveBoundary.frag"),oldCaveFragment);
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
