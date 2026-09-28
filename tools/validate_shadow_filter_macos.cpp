// Execute the production PCF function in a controlled GPU texture fixture.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_shadow_filter_macos.cpp \
//   -framework OpenGL -o /tmp/shadow-filter
// /tmp/shadow-filter <repository or packaged resource root>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
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
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
GLuint compile(GLenum type, const std::string& text) {
    GLuint shader=glCreateShader(type); const char* data=text.c_str();
    glShaderSource(shader,1,&data,nullptr); glCompileShader(shader);
    GLint ok=0; char log[4096]{}; glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    glGetShaderInfoLog(shader,sizeof(log),nullptr,log); require(ok,log); return shader;
}
GLuint program(const std::filesystem::path& path, const std::string& prefix, bool brokenPlane=false) {
    std::ifstream file(path); require(file.good(),"Missing shader");
    std::string source{std::istreambuf_iterator<char>(file),{}};
    const auto start=source.find("float directionalShadowVisibility()");
    const auto end=source.find("\nvec3 directionalFogColour",start);
    require(start!=std::string::npos && end!=std::string::npos,"Missing production function");
    std::string function=source.substr(start,end-start);
    if(brokenPlane) {
        const auto at=function.find("float pcfVisibility = 0.0;");
        require(at!=std::string::npos,"Missing filter for plane negative fixture");
        function.insert(at,"receiverDepthGradient = vec2(0.0);\n    ");
    }
    const std::string fragment="#version 150\nuniform vec4 probeShadowPosition;\nvec4 "+prefix+"ShadowPosition;\n"
        "uniform vec2 probeDepthSlope; uniform float probePixelSpan, probeShear, probeCompression;\n"
        "vec3 "+prefix+"WorldPosition; uniform mat4 directionalShadowViewProj;\n"
        "uniform float "+prefix+"Distance;\nuniform sampler2D directionalShadowMap;\n"
        "uniform float directionalShadowEnabled, directionalShadowStrength, directionalShadowBias;\n"
        "uniform float directionalShadowFadeStart, directionalShadowFadeEnd;\nout vec4 colour;\n"+
        function+"\nvoid main(){vec2 shift=mat2(1,0,probeShear,probeCompression)*(gl_FragCoord.xy-vec2(0.5))*probePixelSpan;"+
        prefix+"WorldPosition=vec3(1648,128,668)+vec3(shift*128.0,0);"+
        prefix+"ShadowPosition=probeShadowPosition+vec4(shift,2.0*dot(shift,probeDepthSlope),0);"+
        "colour=vec4(directionalShadowVisibility());}\n";
    GLuint p=glCreateProgram();
    GLuint v=compile(GL_VERTEX_SHADER,"#version 150\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);}");
    GLuint f=compile(GL_FRAGMENT_SHADER,fragment);glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);require(ok,"Filter link failed");
    glDeleteShader(v);glDeleteShader(f);return p;
}
float sample(GLuint p,const std::string& prefix,float x,float y=.5f,float z=0.f,
             float enabled=1.f,float strength=1.f,float distance=1.f,
             float bias=.008f,float slopeX=0.f,float slopeY=0.f,float pixelSpan=0.f,float shear=0.f,float compression=1.f) {
    glUseProgram(p);
    auto scalar=[&](const char* key,float value){glUniform1f(glGetUniformLocation(p,key),value);};
    scalar("directionalShadowEnabled",enabled);scalar("directionalShadowStrength",strength);
    scalar("directionalShadowBias",bias);scalar("directionalShadowFadeStart",48);scalar("directionalShadowFadeEnd",64);
    scalar("probePixelSpan",pixelSpan);scalar("probeShear",shear);scalar("probeCompression",compression);
    const float lightProjection[]{1.f/128,0,2*slopeX/128,0, 0,1.f/128,2*slopeY/128,0,
                                  0,0,1,0, .5f,.5f,0,1};
    glUniformMatrix4fv(glGetUniformLocation(p,"directionalShadowViewProj"),1,GL_FALSE,lightProjection);
    glUniform2f(glGetUniformLocation(p,"probeDepthSlope"),slopeX,slopeY);
    scalar((prefix+"Distance").c_str(),distance);
    glUniform1i(glGetUniformLocation(p,"directionalShadowMap"),0);
    glUniform4f(glGetUniformLocation(p,"probeShadowPosition"),x,y,z,1);
    glDrawArrays(GL_TRIANGLES,0,3);float result[4];glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,result);
    require(glGetError()==GL_NO_ERROR,"Filter GPU error");return result[0];
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Usage: shadow-filter <root>");
        CGLPixelFormatAttribute attributes[]{kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core), kCGLPFAAccelerated,
            static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format;GLint count;CGLContextObj context;
        require(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError && format,"No pixel format");
        require(CGLCreateContext(format,nullptr,&context)==kCGLNoError,"No context");
        CGLDestroyPixelFormat(format);CGLSetCurrentContext(context);
        std::cout<<"renderer="<<glGetString(GL_RENDERER)<<'\n';
        GLuint vao,fbo,colour,map;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenRenderbuffers(1,&colour);glBindRenderbuffer(GL_RENDERBUFFER,colour);glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA32F,1,1);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colour);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete FBO");glViewport(0,0,1,1);
        glGenTextures(1,&map);glBindTexture(GL_TEXTURE_2D,map);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        int checks=0;auto check=[&](const char* name,bool ok){++checks;require(ok,name);std::cout<<"[SHADOW_FILTER] PASS "<<name<<'\n';};
        for(const std::string prefix:{"terrain","actor"}) {
            auto path=std::filesystem::path(argv[1])/"media/ogre"/(prefix=="terrain"?"HelloMine3DTerrainShadow.frag":"HelloMine3DActorShadow.frag");
            GLuint p=program(path,prefix),broken=program(path,prefix,true);
            for(int size:{1024,2048}) {
                std::vector<float> depth(size*size,1.f);
                for(int y=0;y<size;++y)for(int x=0;x<size/2;++x)depth[y*size+x]=.3f;
                glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,size,size,0,GL_RED,GL_FLOAT,depth.data());
                check("occluder-remains-dark",sample(p,prefix,.25f)<.0001f);
                check("open-region-remains-lit",sample(p,prefix,.75f)>.9999f);
                check("off-exactly-lit",sample(p,prefix,.25f,.5f,0,0)==1.f);
                check("night-zero-strength",sample(p,prefix,.25f,.5f,0,1,0)==1.f);
                check("far-fades-to-lit",sample(p,prefix,.25f,.5f,0,1,1,64)==1.f);
                check("outside-map-lit",sample(p,prefix,-.01f)==1.f);
                check("outside-depth-lit",sample(p,prefix,.25f,.5f,2.f)==1.f);
                float previous=sample(p,prefix,.5f-1.f/size);bool smooth=true,monotonic=true;
                for(int i=1;i<=64;++i) {
                    float current=sample(p,prefix,.5f+(-1.f+i/32.f)/size);
                    smooth &= std::abs(current-previous) < .032f;
                    monotonic &= current>=previous-.0001f;previous=current;
                }
                check("subtexel-edge-no-brightness-jumps",smooth);
                check("edge-monotonic-and-reaches-lit",monotonic && previous>.999f);
                check("strength-bounded",std::abs(sample(p,prefix,.25f,.5f,0,1,.42f)-.58f)<.0001f);

                // Production profile depths: Medium clips at 0.5..128 m,
                // High at 0.5..192 m. A nearby blocker must not disappear
                // inside the old 0.51/0.38 m constant bias gap.
                const float farDepth=size==1024?128.f:192.f;
                const float bias=size==1024?.004f:.002f;
                const float gap=.08f/(farDepth-.5f);
                std::fill(depth.begin(),depth.end(),.5f-gap);
                glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,size,size,0,GL_RED,GL_FLOAT,depth.data());
                const float contact=sample(p,prefix,.5f,.5f,0,1,1,1,bias,0,0,1.f/size);
                std::cout<<"[SHADOW_FILTER] contact-8cm visibility="<<contact<<" size="<<size<<'\n';
                check("eight-centimetre-contact-blocker-visible",contact<.0001f);

                // Real linear receiver depth varies across the PCF footprint.
                // Use raster derivatives, not a uniform coordinate fixture.
                for(const auto slope : {std::pair<float,float>{0,0},{1.8f,1.2f},{-1.6f,.9f}}) {
                    for(int y=0;y<size;++y)for(int x=0;x<size;++x)
                        depth[y*size+x]=.5f+slope.first*((x+.5f)/size-.5f)+
                            slope.second*((y+.5f)/size-.5f);
                    glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,size,size,0,GL_RED,GL_FLOAT,depth.data());
                    bool stable=true;
                    for(int step=-16;step<=16;++step) {
                        const float delta=step/(16.f*size);
                        stable &= sample(p,prefix,.5f+delta,.5f,2*delta*slope.first,1,1,1,
                                         bias,slope.first,slope.second,1.f/size)>.9999f;
                    }
                    check("sloped-receiver-moving-across-texels-has-no-acne",stable);
                }
                check("removed-plane-correction-negative-detected",
                      sample(broken,prefix,.5f,.5f,0,1,1,1,bias,-1.6f,.9f,1.f/size)<.9f);

                // Dawn faces project to thin, skewed footprints. Differencing
                // translated shadow coordinates used to turn an unobstructed
                // plane into half-shadow at subtexel screen spans. Keep world
                // coordinates far from zero as in the production snow scene.
                for(const auto slope : {std::pair<float,float>{16,5},{32,6}}) {
                    for(int y=0;y<size;++y)for(int x=0;x<size;++x)
                        depth[y*size+x]=.5f+slope.first*((x+.5f)/size-.5f)+
                            slope.second*((y+.5f)/size-.5f);
                    glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,size,size,0,GL_RED,GL_FLOAT,depth.data());
                    for(float span:{.01f,.05f,.25f})for(float compression:{.05f,1.f}) {
                        float minimum=1;
                        for(int step=-4;step<=4;++step) {
                            const float delta=step/(4.f*size);
                            minimum=std::min(minimum,sample(p,prefix,.5f+delta,.5f,2*delta*slope.first,
                                1,1,1,bias,slope.first,slope.second,span/size,.8f,compression));
                        }
                        std::cout<<"[SHADOW_FILTER] grazing slope="<<slope.first<<","<<slope.second
                            <<" span="<<span<<" compression="<<compression<<" minimum="<<minimum<<'\n';
                        check("grazing-subtexel-receiver-remains-lit",minimum>.9999f);
                    }
                }
            }
            glDeleteProgram(p);glDeleteProgram(broken);
        }
        std::cout<<"checks="<<checks<<" failures=0\n";
        glDeleteTextures(1,&map);glDeleteRenderbuffers(1,&colour);glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);
        CGLSetCurrentContext(nullptr);CGLDestroyContext(context);return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
