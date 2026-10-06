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
    // The old fixture supplied a constant position, so it had no surface
    // derivatives. This is a real horizontal plane. At the original readback
    // pixel (1,1) in a 4x4 target, p=(-.25,-.25): its world centre and all
    // existing radiance/guard/depth expectations remain fixturePosition.
    vec2 offset=(p+vec2(.25))*.004;
    waterWorldPosition=fixturePosition+vec3(offset.x,0,offset.y);
    waterWorldNormal=fixtureNormal; waterLight=1.0;
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
// Geometric reflection eligibility is exercised with actual smooth positions,
// triangle coverage and reciprocal-W interpolation. This fixture never invents
// a surface from gl_FragCoord or uses the old upward shading normal as geometry.
const std::string surfaceVertex = R"GLSL(#version 150
out vec3 waterWorldPosition; out vec3 waterWorldNormal; out float waterLight;
out vec2 waterLightSources; out float waterDistance; out vec2 waterSurfaceData; out vec2 waterSurfaceDrift;
uniform vec3 fixtureCentre, fixtureU, fixtureV;
uniform float fixtureWave, fixturePerspective;
uniform int fixtureDiagonal, fixtureReverse;
void main() {
    const vec2 corners[4]=vec2[4](vec2(-1,1),vec2(1,1),vec2(1,-1),vec2(-1,-1));
    const int diagonal02[6]=int[6](0,1,2,2,3,0);
    const int diagonal13[6]=int[6](0,1,3,1,2,3);
    int index=gl_VertexID;
    if(fixtureReverse!=0 && index%3!=0) index=index%3==1 ? index+1 : index-1;
    int corner=fixtureDiagonal==0 ? diagonal02[index] : diagonal13[index];
    vec2 p=corners[corner];
    float w=1+fixturePerspective*(.25*p.x+.15*p.y);
    gl_Position=vec4(p*w,0,w);
    waterWorldPosition=fixtureCentre+fixtureU*p.x+fixtureV*p.y;
    waterWorldPosition.y+=fixtureWave*(.3*p.x-.2*p.y+.5*p.x*p.y);
    waterWorldNormal=normalize(vec3(.025,1,-.02));
    waterLight=1; waterLightSources=vec2(1,0); waterDistance=0;
    waterSurfaceData=vec2(3,.25); waterSurfaceDrift=vec2(.12,.09);
})GLSL";
struct SurfaceCase {
    std::string name;
    Point centre, u, v;
    float plane=66.9f, wave=0, perspective=0;
    int diagonal=0, reverse=0;
    bool horizontal=false;
};
struct SurfaceGrid {
    static constexpr int width=31, height=19;
    GLuint fbo=0, colour=0, reflection=0, vao=0, vertices=0;
    SurfaceGrid() {
        glGenFramebuffers(1,&fbo); glGenTextures(1,&colour);
        glGenTextures(1,&reflection); glBindTexture(GL_TEXTURE_2D,reflection);
        const Pixel radiance{.5f,2.f,8.f,.2f};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,1,1,0,GL_RGBA,GL_FLOAT,radiance.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glGenVertexArrays(1,&vao); glGenBuffers(1,&vertices);
    }
    ~SurfaceGrid() { glDeleteBuffers(1,&vertices); glDeleteVertexArrays(1,&vao); glDeleteTextures(1,&reflection); glDeleteTextures(1,&colour); glDeleteFramebuffers(1,&fbo); }
    std::vector<Pixel> draw(GLuint p,const SurfaceCase& c,bool enabled,float hdr=1,GLint storage=GL_RGBA16F,bool productionVertex=false) {
        glBindFramebuffer(GL_FRAMEBUFFER,fbo); glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D,colour);
        glTexImage2D(GL_TEXTURE_2D,0,storage,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Surface framebuffer incomplete");
        glBindVertexArray(vao); glViewport(0,0,width,height); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_FRAMEBUFFER_SRGB);
        glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT); glUseProgram(p);
        vec3(p,"fixtureCentre",c.centre); vec3(p,"fixtureU",c.u); vec3(p,"fixtureV",c.v);
        scalar(p,"fixtureWave",c.wave); scalar(p,"fixturePerspective",c.perspective);
        glUniform1i(glGetUniformLocation(p,"fixtureDiagonal"),c.diagonal); glUniform1i(glGetUniformLocation(p,"fixtureReverse"),c.reverse);
        scalar(p,"linearHdrMode",hdr); scalar(p,"globalTime",4); scalar(p,"environmentLight",1); scalar(p,"fogDensity",0);
        scalar(p,"waterDetailStrength",0); scalar(p,"sunIntensity",0); vec3(p,"sunDirection",{0,1,0});
        vec3(p,"cameraPosition",{c.centre[0],c.plane+2,c.centre[2]+10});
        vec3(p,"waterShallowColour",{.12f,.43f,.53f}); vec3(p,"waterDeepColour",{.018f,.15f,.24f});
        vec3(p,"skyHorizonColour",{.7f,.8f,.9f}); vec3(p,"skyZenithColour",{.2f,.4f,.7f});
        scalar(p,"planarReflectionEnabled",enabled ? 1.f:0.f); scalar(p,"planarReflectionPlaneY",c.plane);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,reflection); glUniform1i(glGetUniformLocation(p,"planarReflectionTexture"),0);
        glUniform2f(glGetUniformLocation(p,"planarReflectionTexelSize"),1.f/64,1.f/64);
        // Local reflection coordinates keep every test patch within the guard;
        // the texture is constant, so the independent radiance oracle needs no
        // shader UV reconstruction or texture-filter implementation.
        float reflectionMatrix[16]={.5f,0,0,0, 0,0,1,0, 0,.5f,0,0, -.5f*c.centre[0],-.5f*c.centre[2],0,1};
        glUniformMatrix4fv(glGetUniformLocation(p,"planarReflectionViewProj"),1,GL_FALSE,reflectionMatrix);
        if(productionVertex) {
            const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            glUniformMatrix4fv(glGetUniformLocation(p,"world"),1,GL_FALSE,identity);
            glUniformMatrix4fv(glGetUniformLocation(p,"worldView"),1,GL_FALSE,identity);
            scalar(p,"waterBoundaryPinsV1",1);
            float matrix[16]={}; matrix[15]=1;
            double uu=0,vv=0; for(int k=0;k<3;++k) { uu+=c.u[k]*c.u[k]; vv+=c.v[k]*c.v[k]; }
            require(uu>0&&vv>0,"Production geometry basis degenerate");
            for(int k=0;k<3;++k) { matrix[k*4]=float(c.u[k]/uu); matrix[k*4+1]=float(c.v[k]/vv); matrix[12]-=matrix[k*4]*c.centre[k]; matrix[13]-=matrix[k*4+1]*c.centre[k]; }
            glUniformMatrix4fv(glGetUniformLocation(p,"worldViewProj"),1,GL_FALSE,matrix);
            const std::array<std::array<float,2>,4> corners{{{{-1,1}},{{1,1}},{{1,-1}},{{-1,-1}}}};
            const int diagonals[2][6]={{0,1,2,2,3,0},{0,1,3,1,2,3}};
            std::array<std::array<float,11>,6> data{};
            for(int i=0;i<6;++i) {
                int index=i; if(c.reverse && i%3!=0) index=i%3==1 ? i+1:i-1;
                const auto q=corners[std::size_t(diagonals[c.diagonal][index])];
                auto& vertex=data[std::size_t(i)];
                for(int k=0;k<3;++k) vertex[std::size_t(k)]=c.centre[k]+c.u[k]*q[0]+c.v[k]*q[1];
                const bool pin=!c.horizontal&&q[1]<0;
                if(!pin) vertex[1]+=.1f;
                vertex[3]=.12f; vertex[4]=.09f; vertex[5]=3; vertex[6]=.25f;
                vertex[7]=1; vertex[8]=1; vertex[9]=1; vertex[10]=pin ? 1.f:0.f;
            }
            glBindBuffer(GL_ARRAY_BUFFER,vertices); glBufferData(GL_ARRAY_BUFFER,sizeof(data),data.data(),GL_STREAM_DRAW);
            const char* names[]={"vertex","uv0","uv1","uv2","uv3"}; const int sizes[]={3,2,2,3,1}, offsets[]={0,3,5,7,10};
            for(int a=0;a<5;++a) {
                const GLint location=glGetAttribLocation(p,names[a]); require(location>=0,"Production water attribute absent");
                glEnableVertexAttribArray(GLuint(location)); glVertexAttribPointer(GLuint(location),sizes[a],GL_FLOAT,GL_FALSE,44,reinterpret_cast<void*>(std::size_t(offsets[a])*sizeof(float)));
            }
        }
        glDrawArrays(GL_TRIANGLES,0,6);
        std::vector<Pixel> result(width*height); glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,result.data()); return result;
    }
};
bool finitePixels(const std::vector<Pixel>& pixels) {
    return std::all_of(pixels.begin(),pixels.end(),[](const Pixel& p){return std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v);});});
}
std::string previousGeometrySource(std::string source) {
    const auto start=source.find("    // Derivatives must precede the per-fragment early returns below.");
    const auto end=source.find("    // A single mean plane serves only its animated sheet.",start);
    require(start!=std::string::npos&&end!=std::string::npos,"Geometry guard anchors absent");
    source.erase(start,end-start); return source;
}
void surfaceChecks(const std::string& source,const std::string& vertex,const std::filesystem::path& evidence) {
    const auto before=previousGeometrySource(source);
    std::ofstream(evidence/"HelloMine3DWater-before-surface-guard.frag")<<before;
    auto helper=replace(source,"void main()","void productionWaterMain()");
    helper+="\nvoid main(){fragmentColour=vec4(planarReflection(vec3(.1,.2,.3),normalize(waterWorldNormal),.72,.8,0.),.42);}\n";
    GLuint current=program(source,surfaceVertex), previous=program(before,surfaceVertex), isolated=program(helper,surfaceVertex);
    GLuint actualVertex=program(source,vertex), previousActualVertex=program(before,vertex);
    check("surface-whole-production-vs-fs-linked",actualVertex!=0);
    const std::string oldGeometry="vec3 geometricNormal = cross(dFdx(waterWorldPosition), dFdy(waterWorldPosition));";
    GLuint badShading=program(replace(helper,oldGeometry,"vec3 geometricNormal = normal;"),surfaceVertex);
    GLuint badHeightOnly=program(previousGeometrySource(helper),surfaceVertex);
    GLuint badSigned=program(replace(helper,"abs(geometricNormal.y) < 0.5 * geometricLength","geometricNormal.y < 0.5 * geometricLength"),surfaceVertex);
    GLuint badAllOff=program(replace(helper,"abs(geometricNormal.y) < 0.5 * geometricLength","true"),surfaceVertex);
    SurfaceGrid grid; std::ofstream quality(evidence/"surface-geometry.tsv");
    quality<<"case\thorizontal\tdiagonal\treverse\tperspective\tside_on_off_max\thelper_double_max\told_height_only_max\n";
    std::vector<SurfaceCase> sides;
    for(int face=0;face<4;++face)for(int diagonal=0;diagonal<2;++diagonal)for(int reverse=0;reverse<2;++reverse)for(int perspective=0;perspective<2;++perspective) {
        const Point u=face<2 ? Point{face==0 ? .5f:-.5f,0,0}:Point{0,0,face==2 ? .5f:-.5f};
        sides.push_back({"vertical-"+std::to_string(face)+"-d"+std::to_string(diagonal)+"-r"+std::to_string(reverse)+"-p"+std::to_string(perspective),{192,66.7f,-183},u,{0,.2f,0},66.9f,0,float(perspective),diagonal,reverse,false});
    }
    for(const auto& c:sides) {
        const auto on=grid.draw(current,c,true),off=grid.draw(current,c,false),h=grid.draw(isolated,c,true,1,GL_RGBA32F);
        check(c.name+"-whole-rgba16f-on-off-exact",on==off,maxPixelDifference(on,off));
        check(c.name+"-finite-and-covered",finitePixels(on)&&std::all_of(on.begin(),on.end(),[](const Pixel& p){return p[3]>0;}));
        float error=0; for(const auto& p:h)for(int k=0;k<4;++k)error=std::max(error,std::abs(p[k]-Pixel{.1f,.2f,.3f,.42f}[k]));
        check(c.name+"-independent-vertical-plane-fallback",finitePixels(h)&&error<.000002f,error,.000002f);
        const auto old=grid.draw(badHeightOnly,c,true,1,GL_RGBA32F),up=grid.draw(badShading,c,true,1,GL_RGBA32F);
        const float oldError=maxPixelDifference(h,old),upError=maxPixelDifference(h,up);
        check(c.name+"-height-only-hard-boundary-negative-rejected",oldError>.5f,oldError,.5f);
        // The removed height-only rule genuinely split the same vertical
        // primitive: the lower row kept approximation, the upper row sampled
        // the HDR reflection. Check both, rather than only a maximum delta.
        bool lowerApprox=true,upperReflected=true;
        for(int x=0;x<SurfaceGrid::width;++x) {
            lowerApprox &= std::abs(old[std::size_t(x)][2]-.3f)<.000002f;
            upperReflected &= old[std::size_t(SurfaceGrid::height-1)*SurfaceGrid::width+x][2]>1.f;
        }
        check(c.name+"-height-only-internal-step-upper-and-lower-negative",lowerApprox&&upperReflected);
        check(c.name+"-up-shading-normal-negative-rejected",upError>.5f,upError,.5f);
        const auto previousWhole=grid.draw(previous,c,true);
        check(c.name+"-alpha-unmodified",std::equal(on.begin(),on.end(),previousWhole.begin(),[](const Pixel& a,const Pixel& b){return a[3]==b[3];}));
        for(const GLint storage:{GL_RGBA8,GL_RGBA16F}) {
            const auto legacy=grid.draw(current,c,true,0,storage),oldLegacy=grid.draw(previous,c,true,0,storage);
            check(c.name+(storage==GL_RGBA8 ? "-legacy-rgba8-exact-before":"-legacy-rgba16f-exact-before"),legacy==oldLegacy,maxPixelDifference(legacy,oldLegacy));
        }
        quality<<c.name<<"\t0\t"<<c.diagonal<<'\t'<<c.reverse<<'\t'<<c.perspective<<'\t'<<maxPixelDifference(on,off)<<'\t'<<error<<'\t'<<oldError<<'\n';
    }
    const double blend=.72*.72*(.55+.45*.8);
    const Pixel expected{float(.1*(1-blend)+.5*blend),float(.2*(1-blend)+2*blend),float(.3*(1-blend)+8*blend),.42f};
    for(int sign:{-1,1})for(int diagonal=0;diagonal<2;++diagonal)for(int reverse=0;reverse<2;++reverse) {
        const SurfaceCase c{"horizontal-s"+std::to_string(sign)+"-d"+std::to_string(diagonal)+"-r"+std::to_string(reverse),{0,66.9f,0},{.5f,0,0},{0,0,sign*.5f},66.9f,.025f,1,diagonal,reverse,true};
        const auto on=grid.draw(current,c,true),old=grid.draw(previous,c,true),h=grid.draw(isolated,c,true,1,GL_RGBA32F),allOff=grid.draw(badAllOff,c,true,1,GL_RGBA32F);
        check(c.name+"-whole-horizontal-exact-before",on==old,maxPixelDifference(on,old));
        check(c.name+"-hdr-radiance-over-one",std::any_of(on.begin(),on.end(),[](const Pixel& p){return p[2]>1;})&&finitePixels(on));
        float error=0;for(const auto& p:h)for(int k=0;k<4;++k)error=std::max(error,std::abs(p[k]-expected[k]));
        check(c.name+"-independent-double-planar-blend",error<.000002f,error,.000002f);
        check(c.name+"-blanket-disable-negative-rejected",maxPixelDifference(h,allOff)>.5f);
        if(sign>0)check(c.name+"-signed-normal-negative-rejected",maxPixelDifference(h,grid.draw(badSigned,c,true,1,GL_RGBA32F))>.5f);
        for(const GLint storage:{GL_RGBA8,GL_RGBA16F}) {
            const auto a=grid.draw(current,c,true,0,storage),z=grid.draw(previous,c,true,0,storage);
            check(c.name+(storage==GL_RGBA8 ? "-legacy-rgba8-exact-before":"-legacy-rgba16f-exact-before"),a==z,maxPixelDifference(a,z));
        }
        quality<<c.name<<"\t1\t"<<diagonal<<'\t'<<reverse<<"\t1\t0\t"<<error<<"\t0\n";
    }
    for(int face=0;face<4;++face) {
        const auto& c=sides[std::size_t(face)*8];
        const auto on=grid.draw(actualVertex,c,true,1,GL_RGBA16F,true),off=grid.draw(actualVertex,c,false,1,GL_RGBA16F,true);
        check(c.name+"-actual-production-pinned-side-on-off-exact",on==off,maxPixelDifference(on,off));
        check(c.name+"-actual-production-pinned-side-covered-finite",finitePixels(on)&&std::all_of(on.begin(),on.end(),[](const Pixel& p){return p[3]>0;}));
    }
    for(int face=0;face<4;++face)for(int diagonal=0;diagonal<2;++diagonal) {
        auto c=sides[std::size_t(face)*8]; c.centre[1]=66.8875f; c.v[1]=.0125f;
        c.diagonal=diagonal; c.reverse=1; c.name="maximum-eighth-cut-"+std::to_string(face)+"-d"+std::to_string(diagonal);
        const auto on=grid.draw(actualVertex,c,true,1,GL_RGBA16F,true),off=grid.draw(actualVertex,c,false,1,GL_RGBA16F,true);
        check(c.name+"-actual-production-thin-side-on-off-exact",on==off,maxPixelDifference(on,off));
        check(c.name+"-thin-exposed-side-covered-finite",finitePixels(on)&&std::all_of(on.begin(),on.end(),[](const Pixel& p){return p[3]>0;}));
    }
    const SurfaceCase flat{"production-horizontal",{0,66.9f,0},{.5f,0,0},{0,0,.5f},66.9f,0,0,0,0,true};
    const auto productionTop=grid.draw(actualVertex,flat,true,1,GL_RGBA16F,true),productionBase=grid.draw(actualVertex,flat,false,1,GL_RGBA16F,true);
    check("actual-production-top-retains-planar-radiance",maxPixelDifference(productionTop,productionBase)>1.f&&finitePixels(productionTop));
    const auto oldProductionTop=grid.draw(previousActualVertex,flat,true,1,GL_RGBA16F,true);
    check("actual-production-top-rgba16f-exact-before",productionTop==oldProductionTop,maxPixelDifference(productionTop,oldProductionTop));
    // Verify the replacement for the historical constant-position fixture:
    // its old 4x4 readback centre is exact, while neighbours now span X/Z.
    auto centreSource=replace(source,"void main()","void productionWaterMain()");
    centreSource+="\nvoid main(){fragmentColour=vec4(waterWorldPosition,1); }\n";
    GLuint centreProgram=program(centreSource);
    { Fixture oldChecksFixture;
      const auto centre=oldChecksFixture.draw(centreProgram,false,{.25f,.125f,-.125f});
      samples<<"# nonzero_fixture_centre_rgba16f="<<std::setprecision(10)<<centre[0]<<','<<centre[1]<<','<<centre[2]<<','<<centre[3]<<'\n';
      check("historical-fixture-nonzero-world-centre-exact-stress",centre==Pixel{.25f,.125f,-.125f,1},std::abs(centre[0]-.25f),0);
      std::array<Pixel,16> pixels{};glReadPixels(0,0,4,4,GL_RGBA,GL_FLOAT,pixels.data());
      check("historical-fixture-real-horizontal-varyings",pixels[0][0]!=pixels[3][0]&&pixels[0][2]!=pixels[12][2]&&std::all_of(pixels.begin(),pixels.end(),[](const Pixel& p){return p[1]==.125f;}));
      glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,oldChecksFixture.colour);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,4,4,0,GL_RGBA,GL_FLOAT,nullptr);
      require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Centre diagnostic framebuffer incomplete");
      const auto fullPrecision=oldChecksFixture.draw(centreProgram,false,{.25f,.125f,-.125f});
      samples<<"# nonzero_fixture_centre_rgba32f="<<std::setprecision(10)<<fullPrecision[0]<<','<<fullPrecision[1]<<','<<fullPrecision[2]<<','<<fullPrecision[3]<<'\n';
    }
    { Fixture oldChecksFixture;
      const auto origin=oldChecksFixture.draw(centreProgram,false,{0,0,0});
      check("historical-original-radiance-fixture-origin-exact",origin==Pixel{0,0,0,1});
    }
    glDeleteProgram(centreProgram);
    for(const SurfaceCase& c:std::vector<SurfaceCase>{
        {"zero-derivative",{0,66.9f,0},{0,0,0},{0,0,0}},
        {"underflow-derivative",{0,0,0},{1e-30f,0,0},{0,0,1e-30f},0},
        {"nonfinite-derivative",{0,66.9f,0},{std::nanf(""),0,0},{0,0,.5f}},
        {"infinite-derivative",{0,66.9f,0},{INFINITY,0,0},{0,0,.5f}},
    }) {
        const auto h=grid.draw(isolated,c,true,1,GL_RGBA32F);float error=0;
        for(const auto& p:h)for(int k=0;k<4;++k)error=std::max(error,std::abs(p[k]-Pixel{.1f,.2f,.3f,.42f}[k]));
        check(c.name+"-bounded-helper-fallback",finitePixels(h)&&error<.000002f,error,.000002f);
    }
    const SurfaceCase farTop{"far-origin-top",{100000,66.9f,-100000},{.5f,0,0},{0,0,.5f},66.9f,0,1,1,1,true};
    const SurfaceCase farSide{"far-origin-side",{100000,66.7f,-100000},{.5f,0,0},{0,.2f,0},66.9f,0,1,1,1,false};
    check("far-origin-horizontal-still-reflects",maxPixelDifference(grid.draw(current,farTop,true),grid.draw(current,farTop,false))>1.f);
    const auto farOn=grid.draw(current,farSide,true),farOff=grid.draw(current,farSide,false);
    check("far-origin-vertical-whole-fallback-exact",farOn==farOff&&finitePixels(farOn),maxPixelDifference(farOn,farOff));
    check("surface-fixture-no-gl-errors",glGetError()==GL_NO_ERROR);
    for(GLuint p:{current,previous,isolated,actualVertex,previousActualVertex,badShading,badHeightOnly,badSigned,badAllOff})glDeleteProgram(p);
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
            const auto vertex=read(std::filesystem::path(argv[1])/"media/ogre/HelloMine3DWater.vert");
            std::ofstream(evidence/"HelloMine3DWater.vert")<<vertex;
            surfaceChecks(source,vertex,evidence);
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
