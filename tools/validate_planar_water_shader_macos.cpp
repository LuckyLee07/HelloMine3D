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
GLuint program(const std::string& fragment, const std::string& suppliedVertex = {}) {
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
    GLuint v = shader(GL_VERTEX_SHADER, suppliedVertex.empty() ? vertex : suppliedVertex), f = shader(GL_FRAGMENT_SHADER, fragment), id = glCreateProgram();
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

// A second fixture supplies actual per-pixel shore/world varyings. Its RGBA32F
// storage separates integration error from the original FP16 RTT checks above.
const std::string shoreVertex = R"GLSL(#version 150
out vec3 waterWorldPosition; out vec3 waterWorldNormal; out float waterLight;
out vec2 waterLightSources; out float waterDistance; out vec2 waterSurfaceData; out vec2 waterSurfaceDrift;
uniform vec2 fixtureDimensions; uniform vec3 fixtureShore; uniform vec3 fixtureWorld; uniform int fixtureQuadMode;
void main() {
    vec2 p = gl_VertexID == 0 ? vec2(-1,-1) : gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3);
    float cornerShore=0;
    if(fixtureQuadMode>0) {
        const vec2 corners[4]=vec2[4](vec2(-1,1),vec2(1,1),vec2(1,-1),vec2(-1,-1));
        const int diagonal02[6]=int[6](0,1,2,2,3,0);
        const int diagonal13[6]=int[6](0,1,3,1,2,3);
        int corner=fixtureQuadMode==1 ? diagonal02[gl_VertexID] : diagonal13[gl_VertexID];
        p=corners[corner]; cornerShore=corner==2 ? .25 : 0;
    }
    gl_Position=vec4(p,0,1);
    vec2 pixel=(p*.5+.5)*fixtureDimensions-fixtureDimensions*.5;
    waterWorldPosition=vec3(fixtureWorld.x*pixel.x,0,fixtureWorld.y*pixel.y);
    waterWorldNormal=vec3(0,1,0); waterLight=1; waterLightSources=vec2(1,0); waterDistance=0;
    waterSurfaceData=vec2(3,fixtureQuadMode>0 ? cornerShore : fixtureShore.x+dot(fixtureShore.yz,pixel));
    waterSurfaceDrift=vec2(0);
})GLSL";
struct ShoreCase {
    std::string name;
    int width, height;
    float shore, dx, dy, worldDx, worldDy, time;
    int quadMode=0;
};
struct ShoreGrid {
    GLuint fbo=0, colour=0, vao=0, dummy=0;
    ShoreGrid() {
        glGenFramebuffers(1,&fbo); glGenTextures(1,&colour); glGenVertexArrays(1,&vao);
        glGenTextures(1,&dummy); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,dummy);
        const Pixel zero{}; glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,zero.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    ~ShoreGrid() { glDeleteTextures(1,&dummy); glDeleteVertexArrays(1,&vao); glDeleteTextures(1,&colour); glDeleteFramebuffers(1,&fbo); }
    std::vector<Pixel> draw(GLuint p, const ShoreCase& c, float hdr=1, GLint storage=GL_RGBA32F) {
        glBindFramebuffer(GL_FRAMEBUFFER,fbo); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,colour);
        glTexImage2D(GL_TEXTURE_2D,0,storage,c.width,c.height,0,GL_RGBA,GL_FLOAT,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Shore grid framebuffer incomplete");
        glBindVertexArray(vao); glViewport(0,0,c.width,c.height); glDisable(GL_BLEND); glDisable(GL_FRAMEBUFFER_SRGB); glUseProgram(p);
        glUniform2f(glGetUniformLocation(p,"fixtureDimensions"),float(c.width),float(c.height));
        glUniform1i(glGetUniformLocation(p,"fixtureQuadMode"),c.quadMode);
        vec3(p,"fixtureShore",{c.shore,c.dx,c.dy}); vec3(p,"fixtureWorld",{c.worldDx,c.worldDy,0});
        scalar(p,"linearHdrMode",hdr); scalar(p,"globalTime",c.time); scalar(p,"environmentLight",1); scalar(p,"fogDensity",0);
        scalar(p,"waterDetailStrength",1); scalar(p,"sunIntensity",0); vec3(p,"sunDirection",{0,1,0});
        vec3(p,"cameraPosition",{0,100,10}); vec3(p,"waterShallowColour",{.12f,.43f,.53f}); vec3(p,"waterDeepColour",{.018f,.15f,.24f});
        vec3(p,"skyHorizonColour",{.7f,.8f,.9f}); vec3(p,"skyZenithColour",{.2f,.4f,.7f});
        scalar(p,"planarReflectionEnabled",0); scalar(p,"planarReflectionPlaneY",0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,dummy);
        glUniform1i(glGetUniformLocation(p,"planarReflectionTexture"),0);
        glDrawArrays(GL_TRIANGLES,0,c.quadMode>0 ? 6 : 3);
        std::vector<Pixel> pixels(std::size_t(c.width)*c.height);
        glReadPixels(0,0,c.width,c.height,GL_RGBA,GL_FLOAT,pixels.data()); return pixels;
    }
};
double positiveRipple(double phase) {
    const double a=std::max(std::sin(phase),0.0); return std::pow(a,12.0);
}
double smoothShore(double raw) {
    const double t=std::clamp((raw-.04)/.58,0.0,1.0); return t*t*(3.0-2.0*t);
}
double denseRipple(const ShoreCase& c, double x, double y, bool linearPhase=false) {
    // Independent original signal sampled across the complete square pixel.
    // No production primitive, derivative, blend or antialias code is reused.
    constexpr int n=96; double sum=0;
    for(int sy=0;sy<n;++sy) for(int sx=0;sx<n;++sx) {
        const double px=x+(sx+.5)/n-.5, py=y+(sy+.5)/n-.5;
        double raw=double(c.shore)+double(c.dx)*px+double(c.dy)*py;
        if(c.quadMode>0) {
            const double u=px/c.width+.5,v=py/c.height+.5;
            raw=.25*(c.quadMode==1 ? std::min(u,1-v) : std::max(u-v,0.0));
        }
        if(linearPhase) sum+=positiveRipple(raw);
        else {
            const double shore=smoothShore(raw);
            const double phase=shore*22.0-double(c.time)*2.4+double(c.worldDx)*px*.12+double(c.worldDy)*py*.08;
            sum+=positiveRipple(phase)*shore*(1.0-shore)*.35;
        }
    }
    return sum/(n*n);
}
float maxPixelDifference(const std::vector<Pixel>& a,const std::vector<Pixel>& b) {
    require(a.size()==b.size(),"Grid sizes differ"); float result=0;
    for(std::size_t i=0;i<a.size();++i)for(int channel=0;channel<4;++channel)result=std::max(result,std::abs(a[i][channel]-b[i][channel]));
    return result;
}
std::string originalRippleSource(std::string source) {
    const auto call=source.find("ripple = filteredShoreRipple(ripplePhase)");
    require(call!=std::string::npos,"Production HDR ripple call absent");
    const auto start=source.rfind("if (linearHdrMode > 0.5)",call), end=source.find('}',call);
    require(start!=std::string::npos && end!=std::string::npos,"HDR ripple block absent");
    source.erase(start,end-start+1); return source;
}
void shoreChecks(const std::string& source,const std::filesystem::path& evidence) {
    const auto before=originalRippleSource(source);
    std::ofstream(evidence/"HelloMine3DWater-original-ripple.frag")<<before;
    GLuint filtered=program(source,shoreVertex), unfiltered=program(before,shoreVertex);
    // Disable only the ripple radiance term, preserving every other whole-water
    // computation to isolate the signal through real production output.
    GLuint base=program(replace(source,"* ripple * 0.38;","* 0.0;"),shoreVertex);
    ShoreGrid grid; std::ofstream quality(evidence/"shore-quality.tsv");
    quality<<"case\twidth\theight\toracle_subsamples\tfiltered_mae\toriginal_mae\tfiltered_mean\toracle_mean\tmax_error\n";
    const double shallow=std::pow((.43+.055)/1.055,2.4);
    const double white=std::pow((.85+.055)/1.055,2.4);
    const double coefficient=.38*(shallow*.35+white*.65);
    double totalFiltered=0,totalOriginal=0; int totalPixels=0;
    const std::vector<ShoreCase> cases={
        {"x-bank",31,17,.33f,.010f,0,.02f,.02f,.4f},
        {"y-bank",17,31,.33f,0,.010f,.02f,.02f,1.1f},
        {"diagonal-bank",23,19,.33f,.011f,.008f,.022f,.016f,1.7f},
        {"negative-gradient",29,13,.33f,-.013f,.006f,.026f,.012f,2.3f},
        {"strong-footprint",17,11,.33f,.026f,.013f,.052f,.026f,3.1f},
        // Decoded production top quad at (205,66,-181): corners [0,0,.25,0].
        // Exercise both production mesh diagonals; independent area samples
        // cross the genuine continuous, piecewise-affine shore field.
        {"bridge-corner-diagonal02",17,13,0,0,0,1.f/17,1.f/13,.4f,1},
        {"bridge-corner-diagonal13",17,13,0,0,0,1.f/17,1.f/13,.4f,2},
    };
    for(const auto& c:cases) {
        const auto a=grid.draw(filtered,c), b=grid.draw(unfiltered,c), z=grid.draw(base,c);
        double mae=0,oldMae=0,mean=0,oracleMean=0,maxError=0; bool finite=true;
        for(int y=0;y<c.height;++y)for(int x=0;x<c.width;++x) {
            const auto i=std::size_t(y)*c.width+x;
            const double actual=(a[i][1]-z[i][1])/coefficient, old=(b[i][1]-z[i][1])/coefficient;
            const double expected=denseRipple(c,x+.5-c.width*.5,y+.5-c.height*.5);
            finite&=std::isfinite(actual) && std::isfinite(old);
            for(int channel=0;channel<4;++channel)finite&=std::isfinite(a[i][channel]) && std::isfinite(b[i][channel]);
            mae+=std::abs(actual-expected);oldMae+=std::abs(old-expected);mean+=actual;oracleMean+=expected;maxError=std::max(maxError,std::abs(actual-expected));
        }
        const double count=c.width*c.height;mae/=count;oldMae/=count;mean/=count;oracleMean/=count;
        quality<<c.name<<'\t'<<c.width<<'\t'<<c.height<<"\t96x96\t"<<std::setprecision(12)<<mae<<'\t'<<oldMae<<'\t'<<mean<<'\t'<<oracleMean<<'\t'<<maxError<<'\n';
        check(c.name+"-finite-and-alpha-preserved",finite && std::equal(a.begin(),a.end(),b.begin(),[](const Pixel& l,const Pixel& r){return l[3]==r[3];}));
        check(c.name+"-independent-area-error",mae<.002, float(mae),.002f);
        check(c.name+"-area-error-improves",mae<oldMae*.8,float(mae),float(oldMae*.8));
        check(c.name+"-average-energy",std::abs(mean-oracleMean)<.0015,float(mean),float(oracleMean));
        // The disabled implementation is judged by the same per-case quality
        // gate, independently of whether a different case improves aggregate.
        const double improvementLimit=oldMae*.8;
        check(c.name+"-disabled-filter-same-quality-gate-rejected",!(oldMae<.002 && oldMae<improvementLimit),float(oldMae),float(improvementLimit));
        totalFiltered+=mae*count;totalOriginal+=oldMae*count;totalPixels+=int(count);
        const auto legacy=grid.draw(filtered,c,0), oldLegacy=grid.draw(unfiltered,c,0);
        check(c.name+"-legacy-exact-rgba32f",legacy==oldLegacy,maxPixelDifference(legacy,oldLegacy));
        const auto legacyWindow=grid.draw(filtered,c,0,GL_RGBA8),oldLegacyWindow=grid.draw(unfiltered,c,0,GL_RGBA8);
        check(c.name+"-legacy-window-rgba8-exact",legacyWindow==oldLegacyWindow,maxPixelDifference(legacyWindow,oldLegacyWindow));
        const auto legacyHalf=grid.draw(filtered,c,0,GL_RGBA16F),oldLegacyHalf=grid.draw(unfiltered,c,0,GL_RGBA16F);
        check(c.name+"-legacy-fp16-rgba-exact",legacyHalf==oldLegacyHalf,maxPixelDifference(legacyHalf,oldLegacyHalf));
    }
    quality<<"aggregate\t"<<totalPixels<<"\t1\t96x96\t"<<totalFiltered/totalPixels<<'\t'<<totalOriginal/totalPixels<<"\t0\t0\t0\n";
    const ShoreCase low{"low-footprint",19,11,.33f,.0002f,.00015f,.0004f,.0003f,.7f};
    const auto lowA=grid.draw(filtered,low),lowB=grid.draw(unfiltered,low);
    check("low-footprint-exact-original-rgba",lowA==lowB,maxPixelDifference(lowA,lowB));
    // Calibrate the actual production helper with affine phase independent of
    // the whole-shader nonlinear shore mapping and envelope approximation.
    auto helper=replace(source,"void main()","void productionWaterMain()");
    helper+="\nvoid main() { fragmentColour=vec4(filteredShoreRipple(waterSurfaceData.y),0,0,1); }\n";
    GLuint linear=program(helper,shoreVertex);
    for(const ShoreCase& c:std::vector<ShoreCase>{
        {"linear-tiny",7,5,1.45f,.02f,.01f,0,0,0},
        {"linear-x",11,7,1.2f,.7f,0,0,0,0},
        {"linear-y",7,11,1.8f,0,1.1f,0,0,0},
        {"linear-two-axes",13,9,.8f,.8f,1.7f,0,0,0},
        {"linear-wide",11,7,1.1f,4.3f,3.2f,0,0,0},
        {"linear-negative",13,7,1.5f,-1.1f,.4f,0,0,0},
        {"linear-minor-axis",11,7,1.2f,1.7f,.01f,0,0,0},
        {"linear-negative-y",7,13,1.5f,.4f,-1.1f,0,0,0},
        {"linear-long-session",11,7,1000.f,.7f,1.1f,0,0,0},
        {"linear-zero-width",3,5,1.2f,0,0,0,0,0},
    }) {
        const auto pixels=grid.draw(linear,c);double mae=0,maxError=0;bool bounded=true;
        for(int y=0;y<c.height;++y)for(int x=0;x<c.width;++x) {
            const double expected=denseRipple(c,x+.5-c.width*.5,y+.5-c.height*.5,true);
            const double actual=pixels[std::size_t(y)*c.width+x][0], error=std::abs(actual-expected);
            mae+=error;maxError=std::max(maxError,error);bounded&=std::isfinite(actual)&&actual>=0&&actual<=1;
        }
        mae/=c.width*c.height;
        quality<<c.name<<'\t'<<c.width<<'\t'<<c.height<<"\t96x96\t"<<mae<<"\t0\t0\t0\t"<<maxError<<'\n';
        check(c.name+"-dense-area-calibration",bounded && mae<.002 && maxError<.003,float(mae),.002f);
    }
    constexpr float pi=3.14159265358979323846f;
    const ShoreCase energy{"linear-full-period-energy",64,5,pi,2*pi/64,1.1f,0,0,0};
    const auto energyPixels=grid.draw(linear,energy);double mean=0;
    for(const auto& pixel:energyPixels)mean+=pixel[0];mean/=energyPixels.size();
    check("linear-full-period-mean-energy",std::abs(mean-.11279296875)<.0005,float(mean),.11279296875f);
    glDeleteProgram(linear);glDeleteProgram(base);glDeleteProgram(unfiltered);glDeleteProgram(filtered);
    check("shore-fixture-no-gl-errors",glGetError()==GL_NO_ERROR);
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
            shoreChecks(source,evidence);
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
