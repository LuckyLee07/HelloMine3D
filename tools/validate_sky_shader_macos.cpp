// Render the production sky fragment shader; these are diagnostic GPU samples.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_sky_shader_macos.cpp \
//   -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation -o /tmp/sky-gpu
// /tmp/sky-gpu <candidate Skybox.frag> <baseline Skybox.frag> <new output directory>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int Edge = 256;
using Pixels = std::vector<unsigned char>;
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string read(const std::filesystem::path& path) {
    std::ifstream input(path);
    require(input.good(), "Missing shader " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}
GLuint program(const std::string& fragment) {
    const std::string vertex = R"GLSL(#version 150
out vec3 vDirection;
uniform float viewPitch;
void main() {
    vec2 p = gl_VertexID == 0 ? vec2(-1,-1) :
        (gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3));
    gl_Position = vec4(p,0,1);
    vDirection = vec3(p.x * 1.5, p.y * .75 + viewPitch, -1);
})GLSL";
    const GLuint p = glCreateProgram();
    for (const auto& entry : {std::pair<GLenum,const std::string*>{GL_VERTEX_SHADER,&vertex},
                             {GL_FRAGMENT_SHADER,&fragment}}) {
        const GLuint shader = glCreateShader(entry.first);
        const char* source = entry.second->c_str();
        glShaderSource(shader,1,&source,nullptr); glCompileShader(shader);
        GLint ok=0; char log[4096]{};
        glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        glGetShaderInfoLog(shader,sizeof(log),nullptr,log);
        require(ok,std::string("Sky compile: ")+log);
        glAttachShader(p,shader); glDeleteShader(shader);
    }
    glLinkProgram(p); GLint ok=0; char log[4096]{};
    glGetProgramiv(p,GL_LINK_STATUS,&ok);
    glGetProgramInfoLog(p,sizeof(log),nullptr,log);
    require(ok,std::string("Sky link: ")+log);
    return p;
}
Pixels render(GLuint p, float time, bool enabled=true, bool night=false,
              float height=108, float x=168, float lightX=0,
              bool cloudLightingProbe=false, float lightBalance=-1,
              float viewPitch=.4f) {
    glUseProgram(p);
    const auto scalar=[&](const char* key,float value) {
        glUniform1f(glGetUniformLocation(p,key),value);
    };
    const auto vector=[&](const char* key,float x,float y,float z) {
        glUniform3f(glGetUniformLocation(p,key),x,y,z);
    };
    scalar("viewPitch",viewPitch);
    vector("skyZenithColour",night?.006f:.12f,night?.014f:.36f,night?.05f:.68f);
    vector("skyHorizonColour",night?.025f:.56f,night?.04f:.70f,night?.085f:.8f);
    vector("fogSunwardColour",.78f,.7f,.59f);
    scalar("fogDirectionalStrength",cloudLightingProbe?0:(night?.04f:.12f));
    vector("sunDirection",lightX,night?-1:1,0); vector("sunColour",1,.92f,.72f);
    scalar("sunIntensity",night?0:1); scalar("moonIntensity",night?1:0); scalar("starIntensity",night?1:0);
    if(lightBalance>=0) {
        scalar("sunIntensity",1-lightBalance); scalar("moonIntensity",lightBalance);
    }
    vector("cloudLightColour",night?.1f:.90f,night?.14f:.93f,night?.22f:.95f);
    vector("cloudShadowColour",night?.018f:.42f,night?.032f:.53f,night?.075f:.60f);
    scalar("cloudCoverage",.44f); scalar("cloudLayerEnabled",enabled?1:0);
    scalar("cloudBaseHeight",168); scalar("cloudThickness",24); scalar("cloudHorizontalScale",92);
    glUniform2f(glGetUniformLocation(p,"cloudVelocity"),1.6f,.55f);
    scalar("cloudMaxDistance",2400); vector("cameraPosition",x,height,552);
    scalar("globalTime",time); scalar("legacyTime",std::fmod(time,1.f));
    glDrawArrays(GL_TRIANGLES,0,3);
    Pixels result(Edge*Edge*4);
    glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,result.data());
    require(glGetError()==GL_NO_ERROR,"Sky draw/read failure");
    return result;
}
void png(const std::filesystem::path& path,const Pixels& pixels) {
    Pixels flipped(pixels.size());
    for(int y=0;y<Edge;++y) std::copy_n(pixels.data()+y*Edge*4,Edge*4,
        flipped.data()+(Edge-1-y)*Edge*4);
    auto space=CGColorSpaceCreateDeviceRGB();
    auto provider=CGDataProviderCreateWithData(nullptr,flipped.data(),flipped.size(),nullptr);
    auto image=CGImageCreate(Edge,Edge,8,32,Edge*4,space,kCGBitmapByteOrder32Big|kCGImageAlphaLast,
        provider,nullptr,false,kCGRenderingIntentDefault);
    auto url=CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8*>(path.c_str()),path.string().size(),false);
    auto dest=CGImageDestinationCreateWithURL(url,CFSTR("public.png"),1,nullptr);
    require(dest&&image,"PNG creation failed"); CGImageDestinationAddImage(dest,image,nullptr);
    require(CGImageDestinationFinalize(dest),"PNG encode failed");
    CFRelease(dest); CFRelease(url); CGImageRelease(image); CGDataProviderRelease(provider); CGColorSpaceRelease(space);
}
double difference(const Pixels& a,const Pixels& b) {
    double total=0; for(std::size_t i=0;i<a.size();++i) total+=std::abs(int(a[i])-int(b[i]));
    return total/a.size();
}
bool equalRows(const Pixels& a,const Pixels& b,int first,int last) {
    return std::equal(a.begin()+first*Edge*4,a.begin()+last*Edge*4,
                      b.begin()+first*Edge*4);
}
}
int main(int argc,char** argv) {
    try {
        require(argc==4,"Usage: sky-gpu <candidate> <baseline> <new output>");
        const std::filesystem::path output(argv[3]);
        require(!std::filesystem::exists(output),"Output must be new");
        std::filesystem::create_directories(output);
        const CGLPixelFormatAttribute attributes[]{kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),kCGLPFAAccelerated,
            static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format; GLint count; CGLContextObj context;
        require(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError&&format,"No OpenGL context");
        require(CGLCreateContext(format,nullptr,&context)==kCGLNoError,"Cannot create context");
        CGLDestroyPixelFormat(format); CGLSetCurrentContext(context);
        std::cout<<"renderer="<<glGetString(GL_RENDERER)<<'\n';
        GLuint vao,fbo,colour;
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenRenderbuffers(1,&colour);glBindRenderbuffer(GL_RENDERBUFFER,colour);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,Edge,Edge);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colour);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete framebuffer");
        glViewport(0,0,Edge,Edge);
        const GLuint current=program(read(argv[1])),baseline=program(read(argv[2]));
        int checks=0;
        auto check=[&](const char* name,bool ok) { ++checks; require(ok,name); std::cout<<"[SKY_GPU] PASS "<<name<<'\n'; };
        for(bool night:{false,true}) {
            check(night?"legacy-night-preserved":"legacy-day-preserved",
                render(current,4,false,night)==render(baseline,4,false,night));
            const auto a=render(current,.999f,true,night),b=render(current,1.001f,true,night);
            check(night?"night-continuity":"day-continuity",difference(a,b)<.05);
            check(night?"night-motion":"day-motion",difference(a,render(current,21,true,night))>.03);
            png(output/(night?"night.png":"day.png"),a);
        }
        const auto day=render(current,5);
        const auto old=render(baseline,5);
        check("bounded-cloud-composition-changes",difference(day,old)>.1);
        check("world-space-parallax",difference(day,render(current,5,true,false,108,208))>.1);
        check("cloud-layer-height-parallax",difference(day,render(current,5,true,false,168))>.1);
        check("day-and-night-readable",difference(day,render(current,5,true,true))>20);
        check("sun-moon-handoff-continuity",
              difference(render(current,5,true,false,108,168,1,true,.4999f),
                         render(current,5,true,false,108,168,1,true,.5001f))<.002);
        check("cloud-bottom-crossing-continuity",
              difference(render(current,5,true,false,155.999f),
                         render(current,5,true,false,156.001f))<.1);
        check("cloud-top-crossing-continuity",
              difference(render(current,5,true,false,179.999f,168,0,false,-1,-.4f),
                         render(current,5,true,false,180.001f,168,0,false,-1,-.4f))<.1);
        png(output/"cloud-inside.png",render(current,5,true,false,168));
        png(output/"cloud-above.png",render(current,5,true,false,210,168,0,false,-1,-.4f));
        // This view excludes both discs/halos. Fog is disabled for the probe:
        // only cloud shading may respond to the horizontal light direction.
        for(bool night:{false,true}) {
            const auto left=render(current,5,true,night,108,168,-1,true);
            const auto right=render(current,5,true,night,108,168,1,true);
            check(night?"moon-lights-cloud-form":"sun-lights-cloud-form",
                  difference(left,right)>.01);
            check(night?"night-clear-horizon-preserved":"day-clear-horizon-preserved",
                  equalRows(render(current,5,true,night),
                            render(baseline,5,true,night),0,56));
            check(night?"night-no-cloud-above-outward-ray":"day-no-cloud-above-outward-ray",
                  equalRows(render(current,5,true,night,210),
                            render(baseline,5,true,night,210),128,Edge));
            png(output/(night?"moon-left.png":"sun-left.png"),left);
            png(output/(night?"moon-right.png":"sun-right.png"),right);
            auto previous=render(current,5,true,night);
            double maximumStep=0;
            for(int frame=1;frame<=120;++frame) {
                const auto next=render(current,5+frame/60.f,true,night);
                maximumStep=std::max(maximumStep,difference(previous,next));
                previous=next;
            }
            check(night?"night-continuous-drift":"day-continuous-drift",maximumStep<.05);
            std::cout<<"[SKY_GPU] max-step-60hz="<<maximumStep<<'\n';
        }
        png(output/"day-before.png",old);png(output/"day-after.png",day);
        glDeleteProgram(current);glDeleteProgram(baseline);glDeleteRenderbuffers(1,&colour);
        glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);
        CGLSetCurrentContext(nullptr);CGLDestroyContext(context);
        std::cout<<"[SKY_GPU] checks="<<checks<<" failures=0\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"[SKY_GPU] FAIL "<<error.what()<<'\n';return 1;}
}
