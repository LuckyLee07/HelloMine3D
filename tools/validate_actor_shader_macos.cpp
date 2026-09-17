// Production actor shader checks: shape lighting, facing, windup and shadow parity.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_actor_shader_macos.cpp \
//   -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation -o /tmp/actor-gpu
// /tmp/actor-gpu <repository or package resource root> <new output directory>
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
GLuint program(const std::filesystem::path& root, bool shadow) {
    const std::string base = shadow ? "HelloMine3DActorShadow" : "HelloMine3DActor";
    GLuint p=glCreateProgram();
    for (const auto& entry : {std::pair<GLenum,const char*>{GL_VERTEX_SHADER,".vert"},
                             {GL_FRAGMENT_SHADER,".frag"}}) {
        std::string source=read(root/"media/ogre"/(base+entry.second));
        GLuint shader=glCreateShader(entry.first); const char* text=source.c_str();
        glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
        GLint ok=0;char log[4096]{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
        glGetShaderInfoLog(shader,sizeof(log),nullptr,log);
        require(ok,std::string("Actor compile: ")+log);
        glAttachShader(p,shader);glDeleteShader(shader);
    }
    glLinkProgram(p);GLint ok=0;char log[4096]{};
    glGetProgramiv(p,GL_LINK_STATUS,&ok);glGetProgramInfoLog(p,sizeof(log),nullptr,log);
    require(ok,std::string("Actor link: ")+log);return p;
}
Pixels render(GLuint p,float role,float windup=0,float front=-.5f,float enabled=1,float light=1) {
    glUseProgram(p);
    auto scalar=[&](const char* key,float v){glUniform1f(glGetUniformLocation(p,key),v);};
    const float identity[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const float projection[]{2,0,0,0,0,2,0,0,0,0,0,0,0,0,0,1};
    for(const char* key:{"world","worldView"})
        glUniformMatrix4fv(glGetUniformLocation(p,key),1,GL_FALSE,identity);
    glUniformMatrix4fv(glGetUniformLocation(p,"worldViewProj"),1,GL_FALSE,projection);
    glUniform4f(glGetUniformLocation(p,"actorTint"),.25f,.47f,.51f,1);
    glUniform4f(glGetUniformLocation(p,"actorPartData"),role,1,windup,0);
    scalar("actorSurfaceStrength",enabled);scalar("environmentLight",light);
    scalar("fogDensity",0);scalar("directionalShadowEnabled",0);scalar("directionalShadowStrength",0);
    glUniform3f(glGetUniformLocation(p,"sunDirection"),0,1,0);
    const float vertices[]{-.5f,-.5f,front,.5f,-.5f,front,.5f,.5f,front,
                          .5f,.5f,front,-.5f,.5f,front,-.5f,-.5f,front};
    GLuint vbo;glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    const GLint position=glGetAttribLocation(p,"vertex");
    glEnableVertexAttribArray(position);glVertexAttribPointer(position,3,GL_FLOAT,GL_FALSE,0,nullptr);
    glDrawArrays(GL_TRIANGLES,0,6);glDisableVertexAttribArray(position);glDeleteBuffers(1,&vbo);
    Pixels pixels(Edge*Edge*4);glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR,"Actor draw/read failure");return pixels;
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
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"Usage: actor-gpu <root> <new output>");
        const std::filesystem::path root(argv[1]),output(argv[2]);
        require(!std::filesystem::exists(output),"Output must be new");
        std::filesystem::create_directories(output);
        const CGLPixelFormatAttribute attributes[]{kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),kCGLPFAAccelerated,
            static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format;GLint count;CGLContextObj context;
        require(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError&&format,"No OpenGL context");
        require(CGLCreateContext(format,nullptr,&context)==kCGLNoError,"Cannot create context");
        CGLDestroyPixelFormat(format);CGLSetCurrentContext(context);
        std::cout<<"renderer="<<glGetString(GL_RENDERER)<<'\n';
        GLuint vao,fbo,colour;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenRenderbuffers(1,&colour);glBindRenderbuffer(GL_RENDERBUFFER,colour);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,Edge,Edge);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colour);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete framebuffer");
        glViewport(0,0,Edge,Edge);
        const GLuint normal=program(root,false),shadow=program(root,true);
        int checks=0;
        auto check=[&](const char* name,bool ok){++checks;require(ok,name);std::cout<<"[ACTOR_GPU] PASS "<<name<<'\n';};
        for(float role:{0,1,2,3,5,8}) {
            check("normal-shadow-disabled-parity",render(normal,role)==render(shadow,role));
            check("fallback-is-flat-tint",render(normal,role,1,.5f,0)==render(normal,0,0,-.5f,0));
        }
        const auto head=render(normal,2),windup=render(normal,2,1);
        check("front-face-differs-from-back",difference(head,render(normal,2,0,.5f))>1);
        check("windup-eyes-change-colour",difference(head,windup)>.1);
        check("torso-and-legs-distinct",difference(render(normal,1),render(normal,5))>10);
        check("night-face-retains-contrast",difference(render(normal,2,0,-.5f,1,.18f),render(normal,2,0,.5f,1,.18f))>.4);
        const auto nightWindup=render(normal,2,1,-.5f,1,0);
        const int eye=(140*Edge+69)*4;
        check("night-windup-eyes-retain-bounded-light",nightWindup[eye]>=160 && nightWindup[eye]<=170 && nightWindup[eye+1]<60);
        check("night-windup-shadow-disabled-parity",nightWindup==render(shadow,2,1,-.5f,1,0));
        png(output/"head-night-windup.png",nightWindup);
        png(output/"head-idle.png",head);png(output/"head-windup.png",windup);
        png(output/"torso.png",render(normal,1));png(output/"legs.png",render(normal,5));
        glDeleteProgram(normal);glDeleteProgram(shadow);glDeleteRenderbuffers(1,&colour);
        glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);CGLSetCurrentContext(nullptr);CGLDestroyContext(context);
        std::cout<<"[ACTOR_GPU] checks="<<checks<<" failures=0\n";return 0;
    }catch(const std::exception& error){std::cerr<<"[ACTOR_GPU] FAIL "<<error.what()<<'\n';return 1;}
}
