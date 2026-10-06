// Production water GLSL in an offscreen CGL fixture. This is shader evidence,
// not native Ogre RTT, resident scene, UI, streaming or gameplay acceptance.
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
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Pixel = std::array<float, 4>;
using Point = std::array<float, 3>;
int checks = 0, failures = 0;
std::ofstream samples;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
std::string read(const std::filesystem::path& path) {
    std::ifstream in(path); require(in.good(), "Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(in), {}};
}
void check(const std::string& name, bool ok, float actual = 0, float expected = 0) {
    ++checks; failures += !ok;
    samples << name << '\t' << (ok ? "PASS" : "FAIL") << '\t' << std::setprecision(10) << actual << '\t' << expected << '\n';
    std::cout << "[PLANAR_WATER_GPU] " << (ok ? "PASS " : "FAIL ") << name << '\n';
}
GLuint shader(GLenum type, const std::string& source) {
    GLuint id = glCreateShader(type); const char* text = source.c_str();
    glShaderSource(id, 1, &text, nullptr); glCompileShader(id);
    GLint ok = 0; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[8192] = {}; glGetShaderInfoLog(id, sizeof(log), nullptr, log); glDeleteShader(id); throw std::runtime_error(log); }
    return id;
}
GLuint program(const std::string& fragment) {
    const std::string vertex = R"GLSL(#version 150
out vec3 waterWorldPosition; out vec3 waterWorldNormal; out float waterLight;
out vec2 waterLightSources; out float waterDistance; out vec2 waterSurfaceData; out vec2 waterSurfaceDrift;
uniform vec3 fixturePosition; uniform vec3 fixtureNormal; uniform float fixtureDepth;
void main() {
    vec2 p = gl_VertexID == 0 ? vec2(-1.0,-1.0) : gl_VertexID == 1 ? vec2(3.0,-1.0) : vec2(-1.0,3.0);
    gl_Position = vec4(p,0.0,1.0);
    waterWorldPosition=fixturePosition; waterWorldNormal=fixtureNormal; waterLight=1.0;
    waterLightSources=vec2(1.0,0.0); waterDistance=0.0;
    waterSurfaceData=vec2(fixtureDepth,0.0); waterSurfaceDrift=vec2(0.0);
})GLSL";
    GLuint v = shader(GL_VERTEX_SHADER, vertex), f = shader(GL_FRAGMENT_SHADER, fragment), id = glCreateProgram();
    glAttachShader(id, v); glAttachShader(id, f); glBindFragDataLocation(id, 0, "fragmentColour"); glLinkProgram(id);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok = 0; glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) { char log[8192] = {}; glGetProgramInfoLog(id, sizeof(log), nullptr, log); glDeleteProgram(id); throw std::runtime_error(log); }
    return id;
}
void scalar(GLuint p, const char* name, float x) { glUniform1f(glGetUniformLocation(p, name), x); }
void vec3(GLuint p, const char* name, Point x) { glUniform3fv(glGetUniformLocation(p, name), 1, x.data()); }
const float projection[16] = {1,0,0,0, 0,0,1,0, 0,1,0,0, 0,0,0,1};
struct Fixture {
    GLuint fbo = 0, colour = 0, reflection = 0, vao = 0;
    Fixture() {
        glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenTextures(1, &colour); glBindTexture(GL_TEXTURE_2D, colour);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour, 0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Fixture framebuffer incomplete");
        glGenTextures(1, &reflection); glBindTexture(GL_TEXTURE_2D, reflection);
        std::vector<Pixel> pixels(64 * 64);
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x)
            pixels[std::size_t(y) * 64 + x] = { .18f + float(x) / 64, 2.f + float(y) / 32, 8.f, .2f };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 64, 64, 0, GL_RGBA, GL_FLOAT, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    }
    ~Fixture() { glDeleteVertexArrays(1, &vao); glDeleteTextures(1, &reflection); glDeleteTextures(1, &colour); glDeleteFramebuffers(1, &fbo); }
    Pixel draw(GLuint p, bool enabled, Point pos = {0,0,0}, Point eye = {0,1,10}, Point normal = {0,1,0}, float depth = 8,
               float detail = 0, float hdr = 1, const float* matrix = projection) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0,0,4,4); glDisable(GL_FRAMEBUFFER_SRGB); glDisable(GL_BLEND);
        glUseProgram(p); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, reflection);
        vec3(p,"fixturePosition",pos); vec3(p,"fixtureNormal",normal); scalar(p,"fixtureDepth",depth);
        scalar(p,"linearHdrMode",hdr); scalar(p,"environmentLight",1); scalar(p,"fogDensity",0);
        scalar(p,"waterDetailStrength",detail); scalar(p,"sunIntensity",0); vec3(p,"sunDirection",{0,1,0});
        vec3(p,"cameraPosition",eye); vec3(p,"waterShallowColour",{.12f,.43f,.53f}); vec3(p,"waterDeepColour",{.018f,.15f,.24f});
        vec3(p,"skyHorizonColour",{.7f,.8f,.9f}); vec3(p,"skyZenithColour",{.2f,.4f,.7f});
        scalar(p,"planarReflectionEnabled",enabled ? 1.f : 0.f); scalar(p,"planarReflectionPlaneY",0);
        glUniform1i(glGetUniformLocation(p,"planarReflectionTexture"),0);
        glUniform2f(glGetUniformLocation(p,"planarReflectionTexelSize"),1.f/64,1.f/64);
        glUniformMatrix4fv(glGetUniformLocation(p,"planarReflectionViewProj"),1,GL_FALSE,matrix);
        glDrawArrays(GL_TRIANGLES,0,3);
        Pixel pixel{}; glReadPixels(1,1,1,1,GL_RGBA,GL_FLOAT,pixel.data()); return pixel;
    }
};
float length(Point p) { return std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]); }
float amount(Point pos, Point eye, Point normal, float depth) {
    Point delta{eye[0]-pos[0],eye[1]-pos[1],eye[2]-pos[2]};
    float facing = std::clamp((normal[0]*delta[0]+normal[1]*delta[1]+normal[2]*delta[2])/(length(normal)*length(delta)),0.f,1.f);
    float f = .08f + .82f * std::pow(1.f-facing,3.2f);
    return f * .72f * (.55f + .45f * (1.f-std::exp(-depth*.32f)));
}
void identical(const std::string& name, Pixel a, Pixel b) {
    for (int c=0;c<4;++c) check(name+"-c"+std::to_string(c), std::isfinite(a[c]) && std::abs(a[c]-b[c])<.00001f, a[c],b[c]);
}
std::string replace(std::string source, const std::string& from, const std::string& to) {
    const auto at=source.find(from); require(at!=std::string::npos,"Fault anchor absent"); source.replace(at,from.size(),to); return source;
}
}
int main(int argc,char** argv) {
    CGLContextObj context=nullptr; CGLPixelFormatObj format=nullptr;
    try {
        require(argc==3,"Usage: validator <repo-or-resources-root> <new-evidence-dir>");
        auto evidence=std::filesystem::path(argv[2]); require(!std::filesystem::exists(evidence),"Evidence directory already exists");
        std::filesystem::create_directories(evidence); samples.open(evidence/"samples.tsv");
        const auto sourcePath=std::filesystem::path(argv[1])/"media/ogre/HelloMine3DWater.frag";
        auto source=read(sourcePath); std::ofstream(evidence/"HelloMine3DWater.frag") << source;
        CGLPixelFormatAttribute attrs[]={kCGLPFAAccelerated,kCGLPFAOpenGLProfile,static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),static_cast<CGLPixelFormatAttribute>(0)};
        GLint count=0; require(CGLChoosePixelFormat(attrs,&format,&count)==kCGLNoError && format,"CGL format unavailable");
        require(CGLCreateContext(format,nullptr,&context)==kCGLNoError && context,"CGL context unavailable");
        CGLDestroyPixelFormat(format); format=nullptr; require(CGLSetCurrentContext(context)==kCGLNoError,"CGL current failed");
        std::ofstream driver(evidence/"driver.txt"); driver << glGetString(GL_VENDOR)<<'\n'<<glGetString(GL_RENDERER)<<'\n'<<glGetString(GL_VERSION)<<'\n';
        {
            Fixture fixture; GLuint p=program(source);
            const Point pos{0,0,0}, eye{0,1,10}, normal{0,1,0};
            const auto base=fixture.draw(p,false), reflected=fixture.draw(p,true);
            const float blend=amount(pos,eye,normal,8); const Pixel sample{.18f+31.5f/64,2.f+31.5f/32,8.f,.2f};
            for (int c=0;c<3;++c) {
                const float expected=base[c]*(1-blend)+sample[c]*blend;
                check("linear-radiance-c"+std::to_string(c),std::abs(reflected[c]-expected)<.006f,reflected[c],expected);
            }
            check("radiance-over-one-survives",reflected[2]>3.f,reflected[2],3.f);
            check("alpha-is-not-reflection-alpha",std::abs(reflected[3]-base[3])<.00001f,reflected[3],base[3]);
            for (const float depth : {1.f,3.f,8.f}) {
                const auto b=fixture.draw(p,false,pos,eye,normal,depth), r=fixture.draw(p,true,pos,eye,normal,depth);
                const float a=amount(pos,eye,normal,depth), expected=b[2]*(1-a)+8*a;
                check("resident-depth-"+std::to_string(int(depth)),std::abs(r[2]-expected)<.006f,r[2],expected);
            }
            identical("other-level",fixture.draw(p,true,{0,1,0}),fixture.draw(p,false,{0,1,0}));
            identical("underwater",fixture.draw(p,true,pos,{0,-1,10}),fixture.draw(p,false,pos,{0,-1,10}));
            identical("crossing",fixture.draw(p,true,pos,{0,.1f,10}),fixture.draw(p,false,pos,{0,.1f,10}));
            identical("legacy",fixture.draw(p,true,pos,eye,normal,8,0,0),fixture.draw(p,false,pos,eye,normal,8,0,0));
            identical("out-of-uv",fixture.draw(p,true,{2,0,0}),fixture.draw(p,false,{2,0,0}));
            identical("edge-guard",fixture.draw(p,true,{-.99f,0,0}),fixture.draw(p,false,{-.99f,0,0}));
            float behind[16]; std::copy(std::begin(projection),std::end(projection),behind); behind[15]=-1;
            identical("behind-camera",fixture.draw(p,true,pos,eye,normal,8,0,1,behind),base);
            float invalid[16]; std::copy(std::begin(projection),std::end(projection),invalid); invalid[15]=std::nanf("");
            identical("nonfinite-projection",fixture.draw(p,true,pos,eye,normal,8,0,1,invalid),base);
            const Point tilted{1,1,0};
            const auto staticWarp=fixture.draw(p,true,pos,eye,tilted,8,0), waveWarp=fixture.draw(p,true,pos,eye,tilted,8,1);
            const float shiftExpected=.012f*amount(pos,eye,tilted,8);
            check("bounded-wave-sampling",std::abs((waveWarp[0]-staticWarp[0])-shiftExpected)<.002f,waveWarp[0]-staticWarp[0],shiftExpected);
            GLuint clamped=program(replace(source,"max(radiance, vec3(0.0))","clamp(radiance, vec3(0.0), vec3(1.0))"));
            auto bad=fixture.draw(clamped,true); check("fault-early-clamp-rejected",std::abs(bad[2]-reflected[2])>2.f,bad[2],reflected[2]); glDeleteProgram(clamped);
            GLuint decoded=program(replace(source,"max(radiance, vec3(0.0))","sceneColour(max(radiance, vec3(0.0)))"));
            bad=fixture.draw(decoded,true); check("fault-double-decode-rejected",std::abs(bad[2]-reflected[2])>10.f,bad[2],reflected[2]); glDeleteProgram(decoded);
            GLuint level=program(replace(source,"abs(waterWorldPosition.y - planarReflectionPlaneY) > 0.16","false"));
            bad=fixture.draw(level,true,{0,1,0}); auto other=fixture.draw(p,false,{0,1,0});
            check("fault-multiple-levels-rejected",std::abs(bad[2]-other[2])>.1f,bad[2],other[2]); glDeleteProgram(level);
            glDeleteProgram(p);
            check("no-gl-errors",glGetError()==GL_NO_ERROR);
        }
        std::ofstream(evidence/"result.txt") << "checks="<<checks<<" failures="<<failures<<" scope=production-water-GLSL-offscreen-CGL\n"
            << "native-Ogre-RTT=NOT_RUN normal-gameplay=NOT_RUN resident-streaming=NOT_RUN\n";
        CGLSetCurrentContext(nullptr); CGLDestroyContext(context); context=nullptr;
        std::cout << "[PLANAR_WATER_GPU] checks="<<checks<<" failures="<<failures<<'\n'; return failures ? 1:0;
    } catch (const std::exception& error) {
        if (context) { CGLSetCurrentContext(nullptr); CGLDestroyContext(context); }
        if (format) CGLDestroyPixelFormat(format);
        std::cerr << "[PLANAR_WATER_GPU] ERROR " << error.what() << '\n'; return 2;
    }
}
