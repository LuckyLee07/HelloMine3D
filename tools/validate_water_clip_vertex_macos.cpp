// clang++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations tools/validate_water_clip_vertex_macos.cpp -framework OpenGL -o /private/tmp/hellomine-water-clip-gpu
// Usage: <repo/resources root> <new evidence directory> <frozen old Water.vert>
// Actual production VS + FS link and GPU transform feedback. Independent double
// wave/matrix oracle, section-rebase equality and executable shader faults.
// This does not prove native Ogre VAO upload, clipping mesh or normal gameplay.
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using Vec3 = std::array<float, 3>;
using Matrix = std::array<float, 16>;
using Output = std::array<float, 18>;
struct Vertex { Vec3 position; std::array<float,2> drift; std::array<float,2> depthShore; Vec3 lightSources; float pin; };
static_assert(sizeof(Vertex) == 44, "Existing vertex attributes must remain 44 bytes");
std::ofstream samples;
int checks=0, failures=0, precisionStressFailures=0;
void need(bool ok,const std::string& why) { if(!ok) throw std::runtime_error(why); }
void check(const std::string& name,bool ok,double actual=0,double expected=0) {
    ++checks; failures+=!ok;
    if (!ok && (name.find("old-exact")!=std::string::npos)) ++precisionStressFailures;
    samples<<name<<'\t'<<(ok?"PASS":"FAIL")<<'\t'<<std::setprecision(12)<<actual<<'\t'<<expected<<'\n';
}
std::string read(const std::filesystem::path& p) { std::ifstream f(p);need(f.good(),"Cannot read "+p.string());return {std::istreambuf_iterator<char>(f),{}}; }
std::string replace(std::string s,const std::string& a,const std::string& b) { auto i=s.find(a);need(i!=std::string::npos,"Missing fault anchor "+a);s.replace(i,a.size(),b);return s; }
GLuint compile(GLenum type,const std::string& source) {
    GLuint s=glCreateShader(type);const char* p=source.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
    if(!ok) { char log[8192]={};glGetShaderInfoLog(s,sizeof(log),nullptr,log);glDeleteShader(s);throw std::runtime_error(log); }return s;
}
GLuint link(const std::string& vertex,const std::string& fragment) {
    GLuint vs=compile(GL_VERTEX_SHADER,vertex),fs=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();glAttachShader(p,vs);glAttachShader(p,fs);glBindFragDataLocation(p,0,"fragmentColour");
    const char* outputs[]={"waterWorldPosition","waterWorldNormal","waterLight","waterLightSources","waterDistance","waterSurfaceData","waterSurfaceDrift","gl_Position"};
    glTransformFeedbackVaryings(p,8,outputs,GL_INTERLEAVED_ATTRIBS);glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok) {char log[8192]={};glGetProgramInfoLog(p,sizeof(log),nullptr,log);glDeleteProgram(p);throw std::runtime_error(log);}return p;
}
Matrix identity() { return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; }
Matrix translate(Vec3 p) { auto m=identity();for(int a=0;a<3;++a)m[12+a]=p[a];return m; }
std::array<double,4> transform(const Matrix& m,const std::array<double,4>& v) {
    std::array<double,4> r{};for(int a=0;a<4;++a)for(int k=0;k<4;++k)r[a]+=double(m[k*4+a])*v[k];return r;
}
float maxDifference(const Output& a,const Output& b) {float d=0;for(std::size_t i=0;i<a.size();++i)d=std::max(d,std::abs(a[i]-b[i]));return d;}
struct Fixture {
    GLuint vao=0,input=0,feedback=0,fbo=0,colour=0,dummy=0;
    Fixture() {
        glGenVertexArrays(1,&vao);glGenBuffers(1,&input);glGenBuffers(1,&feedback);
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&colour);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,colour);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
        need(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Transform feedback draw framebuffer incomplete");
        glGenTextures(1,&dummy);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,dummy);
        const float zero[4]={0,0,0,0};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,1,1,0,GL_RGBA,GL_FLOAT,zero);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }
    ~Fixture() {glDeleteBuffers(1,&input);glDeleteBuffers(1,&feedback);glDeleteVertexArrays(1,&vao);glDeleteTextures(1,&colour);glDeleteTextures(1,&dummy);glDeleteFramebuffers(1,&fbo);}
    Output draw(GLuint program,const Vertex& v,Vec3 origin,float time,float detail,float hdr=1,bool bindPin=true,float guard=1) {
        glBindFramebuffer(GL_FRAMEBUFFER,fbo);glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,input);glBufferData(GL_ARRAY_BUFFER,sizeof(v),&v,GL_STREAM_DRAW);
        struct Attribute {const char* name;int count;std::size_t offset;};
        for(const auto& a:std::array<Attribute,5>{{{"vertex",3,offsetof(Vertex,position)},{"uv0",2,offsetof(Vertex,drift)},{"uv1",2,offsetof(Vertex,depthShore)},{"uv2",3,offsetof(Vertex,lightSources)},{"uv3",1,offsetof(Vertex,pin)}}}) {
            GLint index=glGetAttribLocation(program,a.name);if(index<0)continue;
            if(!bindPin && std::string(a.name)=="uv3") {glDisableVertexAttribArray(GLuint(index));glVertexAttrib1f(GLuint(index),0);continue;}
            glEnableVertexAttribArray(GLuint(index));glVertexAttribPointer(GLuint(index),a.count,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(a.offset));
        }
        const auto world=translate(origin);const auto wvp=identity();const auto view=identity();
        glUniformMatrix4fv(glGetUniformLocation(program,"world"),1,GL_FALSE,world.data());glUniformMatrix4fv(glGetUniformLocation(program,"worldViewProj"),1,GL_FALSE,wvp.data());glUniformMatrix4fv(glGetUniformLocation(program,"worldView"),1,GL_FALSE,view.data());
        glUniform1f(glGetUniformLocation(program,"globalTime"),time);glUniform1f(glGetUniformLocation(program,"waterDetailStrength"),detail);glUniform1f(glGetUniformLocation(program,"linearHdrMode"),hdr);glUniform1f(glGetUniformLocation(program,"waterBoundaryPinsV1"),guard);
        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,feedback);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,sizeof(Output),nullptr,GL_STREAM_READ);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,feedback);
        glEnable(GL_RASTERIZER_DISCARD);glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
        Output result{};glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(result),result.data());return result;
    }
};
Output oracle(const Vertex& v,Vec3 origin,float time,float detail,float guard=1) {
    // Independent double maths for translated actual production worlds. This
    // deliberately does not call or extract any production wave helper.
    const double bx=double(v.position[0])+origin[0],bz=double(v.position[2])+origin[2];
    const double motion=std::clamp(std::hypot(double(v.drift[0]),double(v.drift[1])),0.0,1.0);
    const double t=std::clamp((motion-.04)/.96,0.0,1.0);
    const double scale=guard>.5f && v.depthShore[1]>0?0:.025+.975*t*t*(3-2*t);
    const double a=double(time)*.78+bx*.66+bz*.21,b=double(time)*.53+bz*.82-bx*.17;
    const double wave=(std::sin(a)*.035+std::cos(b)*.025)*scale*detail;
    const double y=double(v.position[1])+(v.pin>.5f && guard>.5f?0:wave-.10);
    double nx=-(std::cos(a)*.035*.66+std::sin(b)*.025*.17)*scale*detail;
    double nz=-(std::cos(a)*.035*.21-std::sin(b)*.025*.82)*scale*detail;
    const double n=std::sqrt(nx*nx+1+nz*nz);nx/=n;nz/=n;
    const auto pos=transform(translate(origin),{v.position[0],y,v.position[2],1});
    const auto clip=transform(identity(),{v.position[0],y,v.position[2],1});
    Output r{};for(int k=0;k<3;++k)r[k]=float(pos[k]);r[3]=float(nx);r[4]=float(1/n);r[5]=float(nz);r[6]=v.lightSources[0];
    r[7]=v.lightSources[2]>=1?v.lightSources[1]:-1;r[8]=v.lightSources[2]>=1?v.lightSources[2]-1:-1;
    r[9]=float(std::sqrt(double(v.position[0])*v.position[0]+y*y+double(v.position[2])*v.position[2]));
    r[10]=v.depthShore[0];r[11]=v.depthShore[1];r[12]=v.drift[0];r[13]=v.drift[1];for(int k=0;k<4;++k)r[14+k]=float(clip[k]);return r;
}
bool close(const Output& actual,const Output& expected,float tolerance=.00005f) {for(std::size_t i=0;i<actual.size();++i)if(!std::isfinite(actual[i]) || std::abs(actual[i]-expected[i])>tolerance)return false;return true;}
}
int main(int argc,char** argv) {
    CGLContextObj context=nullptr;CGLPixelFormatObj format=nullptr;
    try {
        need(argc==4,"Usage: <repo/resources root> <new evidence directory> <frozen old Water.vert>");const std::filesystem::path root=argv[1],evidence=argv[2];need(!std::filesystem::exists(evidence),"Evidence already exists");std::filesystem::create_directories(evidence);samples.open(evidence/"samples.tsv");
        const auto source=read(root/"media/ogre/HelloMine3DWater.vert"),fragment=read(root/"media/ogre/HelloMine3DWater.frag"),before=read(argv[3]);std::ofstream(evidence/"HelloMine3DWater.vert")<<source;std::ofstream(evidence/"HelloMine3DWater.frag")<<fragment;std::ofstream(evidence/"Water-before-pin.vert")<<before;
        CGLPixelFormatAttribute attributes[]={kCGLPFAAccelerated,kCGLPFAOpenGLProfile,static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),static_cast<CGLPixelFormatAttribute>(0)};GLint count=0;need(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError && format,"CGL format unavailable");need(CGLCreateContext(format,nullptr,&context)==kCGLNoError && context,"CGL context unavailable");CGLDestroyPixelFormat(format);format=nullptr;need(CGLSetCurrentContext(context)==kCGLNoError,"CGL current unavailable");
        std::ofstream(evidence/"driver.txt")<<glGetString(GL_VENDOR)<<'\n'<<glGetString(GL_RENDERER)<<'\n'<<glGetString(GL_VERSION)<<'\n';
        {
            Fixture fixture;GLuint p=link(source,fragment),old=link(before,fragment);check("whole-production-VS-FS-link",true);GLint attribute=glGetAttribLocation(p,"uv3");check("active-pin-attribute",attribute>=0);
            GLint attributeCount=0;glGetProgramiv(p,GL_ACTIVE_ATTRIBUTES,&attributeCount);bool typed=false;
            for(GLint i=0;i<attributeCount;++i) {char name[64]={};GLsizei len=0;GLint size=0;GLenum type=0;glGetActiveAttrib(p,GLuint(i),64,&len,&size,&type,name);if(std::string(name)=="uv3")typed=type==GL_FLOAT && size==1;}
            check("pin-single-float-existing-layout",typed);
            Vertex v{{.125f,2.5f,9},{.12f,.09f},{3,0},{.8f,.7f,1.2f},0};const Vec3 origin{192,64,-192};float oracleWorst=0,oldWorst=0;
            for(float time:{0.f,4.f,37.5f,123.75f})for(float detail:{0.f,1.f})for(float speed:{0.f,.15f,1.f})for(float pin:{0.f,1.f})for(float hdr:{0.f,1.f}) {
                v.drift={speed*.8f,speed*.6f};v.pin=pin;const auto actual=fixture.draw(p,v,origin,time,detail,hdr),expected=oracle(v,origin,time,detail);oracleWorst=std::max(oracleWorst,maxDifference(actual,expected));
                const std::string label="oracle-t"+std::to_string(time)+"-d"+std::to_string(detail)+"-s"+std::to_string(speed)+"-pin"+std::to_string(pin)+"-hdr"+std::to_string(hdr);
                check(label,close(actual,expected),maxDifference(actual,expected),.00005);
                if(pin==1)check(label+"-worldY-exact",actual[1]==66.5f,actual[1],66.5);
                else {const auto prior=fixture.draw(old,v,origin,time,detail,hdr);oldWorst=std::max(oldWorst,maxDifference(actual,prior));check(label+"-unpinned-old-exact",std::memcmp(actual.data(),prior.data(),sizeof(Output))==0,maxDifference(actual,prior),0);}
            }
            GLint uniformCount=0;glGetProgramiv(p,GL_ACTIVE_UNIFORMS,&uniformCount);bool guardTyped=false;
            for(GLint i=0;i<uniformCount;++i) {char name[64]={};GLint size=0;GLenum type=0;glGetActiveUniform(p,GLuint(i),64,nullptr,&size,&type,name);if(std::string(name)=="waterBoundaryPinsV1")guardTyped=type==GL_FLOAT && size==1;}
            check("active-uniform-guard",glGetUniformLocation(p,"waterBoundaryPinsV1")>=0 && guardTyped);
            check("old-complete-vertex-optional-interface-absent",glGetUniformLocation(old,"waterBoundaryPinsV1")==-1 && glGetAttribLocation(old,"uv3")==-1);
            for(float pin:{0.f,1.f})for(float time:{0.f,4.f,37.5f,123.75f}) {
                v.pin=pin;v.drift={.12f,.09f};v.depthShore[1]=.25f;const auto off=fixture.draw(p,v,origin,time,1,1,true,0),prior=fixture.draw(old,v,origin,time,1);
                check("guard-off-old-exact-pin"+std::to_string(pin)+"-t"+std::to_string(time),std::memcmp(off.data(),prior.data(),sizeof(Output))==0,maxDifference(off,prior),0);
                check("guard-off-double-oracle-pin"+std::to_string(pin)+"-t"+std::to_string(time),close(off,oracle(v,origin,time,1,0)),maxDifference(off,oracle(v,origin,time,1,0)),.00005);
            }
            v.drift={.12f,.09f};v.depthShore[1]=0;v.pin=1;
            const auto pinnedA=fixture.draw(p,v,origin,4,1),pinnedB=fixture.draw(p,v,origin,37.5f,1);check("mixed-cut-lower-stays-fixed",pinnedA[1]==pinnedB[1] && pinnedA[1]==66.5f,pinnedB[1],66.5);
            v.position[1]=3;v.pin=0;v.drift={.8f,.6f};const auto topA=fixture.draw(p,v,origin,78.02f,1),topB=fixture.draw(p,v,origin,178.73f,1);check("mixed-original-top-keeps-wave",std::abs(topA[1]-topB[1])>.10f,topB[1]-topA[1],.10);
            // Real legal 7/8 opaque boundary: the unmodified old top can
            // sink below a fixed new cut at full motion without shore contact.
            // Keep the ordering gate and reject the no-contact-wave fault below.
            Vertex highCut{{.125f,2.875f,9},{.8f,.6f},{3,.25f},{.8f,.7f,1.2f},1};
            Vertex highTop=highCut;highTop.position[1]=3;highTop.pin=0;
            const auto highBottom=fixture.draw(p,highCut,origin,78.02f,1);
            const auto highSurface=fixture.draw(p,highTop,origin,78.02f,1);
            check("high-cut-7of8-independent-oracle",close(highBottom,oracle(highCut,origin,78.02f,1)) && close(highSurface,oracle(highTop,origin,78.02f,1)),maxDifference(highSurface,oracle(highTop,origin,78.02f,1)),.00005);
            check("high-cut-7of8-strip-noninverted",highSurface[1]>=highBottom[1],highSurface[1],highBottom[1]);
            std::ofstream(evidence/"high-cut-risk.txt")<<std::setprecision(12)<<"worldXZ=192.125,-183 motion=1 time=78.02 cut="<<highBottom[1]<<" top="<<highSurface[1]<<" top-minus-cut="<<highSurface[1]-highBottom[1]<<" actual-GPU-transform-feedback=1\n";
            for(float time:{0.f,4.f,78.02f,178.73f})for(float shore:{.03125f,.125f,.25f,.5f,1.f})for(float hdr:{0.f,1.f}) {
                Vertex contactTop=highTop;contactTop.depthShore[1]=shore;
                const auto top=fixture.draw(p,contactTop,origin,time,1,hdr);
                check("shore-contact-top-fixed-t"+std::to_string(time)+"-raw"+std::to_string(shore)+"-hdr"+std::to_string(hdr),top[1]==66.9f && top[3]==0 && top[4]==1 && top[5]==0 && close(top,oracle(contactTop,origin,time,1)),top[1],66.9);
                for(float cut:{.125f,.25f,.375f,.5f,.625f,.75f,.875f}) {
                    Vertex boundary=contactTop;boundary.position[1]=2+cut;boundary.pin=1;
                    const auto b=fixture.draw(p,boundary,origin,time,1,hdr);
                    check("eighth-cut-noninverted-t"+std::to_string(time)+"-raw"+std::to_string(shore)+"-cut"+std::to_string(cut)+"-hdr"+std::to_string(hdr),b[1]==64+boundary.position[1] && top[1]-b[1]>=.025f && close(b,oracle(boundary,origin,time,1)),top[1]-b[1],.025);
                }
            }
            // Both a new top midpoint and the separate original top face have
            // the same world-space Y, also when the side's bottom shore is 0.
            Vertex middle=highTop;middle.position[0]=.5f;middle.depthShore[1]=.25f;
            Vertex internal=middle;internal.position[1]=2.5f;internal.depthShore[1]=.125f;internal.pin=1;
            const auto mid=fixture.draw(p,middle,origin,78.02f,1),interior=fixture.draw(p,internal,origin,78.02f,1);
            check("top-midpoint-contact-no-wave",mid[1]==66.9f && mid[3]==0 && mid[5]==0,mid[1],66.9);
            check("barycentric-internal-contact-pinned",interior[1]==66.5f && interior[3]==0 && interior[5]==0,interior[1],66.5);
            for(float pin:{0.f,1.f})for(float time:{0.f,4.f,37.5f,123.75f}) {
                Vertex a{{16,2.5f,9},{.12f,.09f},{3,.5f},{1,1,1},pin},b=a;b.position[0]=0;
                const auto x=fixture.draw(p,a,{176,64,-192},time,1),y=fixture.draw(p,b,{192,64,-192},time,1);
                bool equal=true;for(int k=0;k<9;++k)equal &= x[k]==y[k];check("section-shared-position-normal-pin"+std::to_string(pin)+"-t"+std::to_string(time),equal,maxDifference({x[0],x[1],x[2],x[3],x[4],x[5]},{y[0],y[1],y[2],y[3],y[4],y[5]}),0);
            }
            v.position={.125f,2.5f,9};v.drift={.12f,.09f};v.depthShore[1]=.25f;v.pin=1;const auto expected=oracle(v,origin,4,1);
            const auto ignored=replace(source,"bool fixedBoundary = waterBoundaryPinsV1 > 0.5 && uv3 > 0.5;","bool fixedBoundary = false;");GLuint fault=link(ignored,fragment);auto bad=fixture.draw(fault,v,origin,4,1);check("fault-ignore-pin-rejected",!close(bad,expected),bad[1],expected[1]);glDeleteProgram(fault);
            const auto stillLowered=replace(source,"    vec4 worldPosition = world * animatedVertex;","    if (fixedBoundary) animatedVertex.y -= 0.10;\n    vec4 worldPosition = world * animatedVertex;");fault=link(stillLowered,fragment);bad=fixture.draw(fault,v,origin,4,1);check("fault-pin-still-lowered-rejected",!close(bad,expected),bad[1],expected[1]);glDeleteProgram(fault);
            bad=fixture.draw(p,v,origin,4,1,1,false);check("fault-unbound-pin-array-rejected",!close(bad,expected),bad[1],expected[1]);
            v.pin=0;fault=link(replace(source,"bool fixedBoundary = waterBoundaryPinsV1 > 0.5 && uv3 > 0.5;","bool fixedBoundary = true;"),fragment);bad=fixture.draw(fault,v,origin,4,1);check("fault-pin-all-originals-rejected",!close(bad,oracle(v,origin,4,1)),bad[1],oracle(v,origin,4,1)[1]);glDeleteProgram(fault);
            bool rejected=false;try {fault=link(replace(source,"in float uv3;",""),fragment);glDeleteProgram(fault);}catch(const std::exception&) {rejected=true;}check("fault-missing-pin-declaration-rejected",rejected);
            rejected=false;try {fault=link(replace(source,"uniform float waterBoundaryPinsV1;",""),fragment);glDeleteProgram(fault);}catch(const std::exception&) {rejected=true;}check("fault-missing-guard-declaration-rejected",rejected);
            const auto noContact=replace(source,"if (waterBoundaryPinsV1 > 0.5 && uv1.y > 0.0)","if (false)");
            fault=link(noContact,fragment);const auto unsafeTop=fixture.draw(fault,highTop,origin,78.02f,1);check("fault-no-shore-contact-wave-inverts-highcut-rejected",unsafeTop[1]<highBottom[1],unsafeTop[1],highBottom[1]);glDeleteProgram(fault);
            fault=link(replace(source,"if (waterBoundaryPinsV1 > 0.5 && uv1.y > 0.0)","if (waterBoundaryPinsV1 > 0.5)"),fragment);
            Vertex openTop=highTop;openTop.depthShore[1]=0;const auto openExpected=oracle(openTop,origin,78.02f,1),openActual=fixture.draw(p,openTop,origin,78.02f,1);
            bad=fixture.draw(fault,openTop,origin,78.02f,1);check("open-sea-original-wave-retained",close(openActual,openExpected) && openActual[1]<66.875f,openActual[1],openExpected[1]);check("fault-blanket-wave-disable-rejected",!close(bad,openExpected),bad[1],openExpected[1]);glDeleteProgram(fault);
            fault=link(replace(source,"if (waterBoundaryPinsV1 > 0.5 && uv1.y > 0.0)","if (uv1.y > 0.0)"),fragment);
            bad=fixture.draw(fault,highTop,origin,78.02f,1,1,true,0);check("fault-unguarded-shore-new-semantics-rejected",!close(bad,oracle(highTop,origin,78.02f,1,0)),bad[1],oracle(highTop,origin,78.02f,1,0)[1]);glDeleteProgram(fault);
            v.pin=1;v.lightSources={.25f,0,0};const auto noSources=fixture.draw(p,v,origin,4,1);check("no-source-sentinel-retained",noSources[7]==-1 && noSources[8]==-1);check("depth-shore-drift-light-pass-through",noSources[6]==.25f && noSources[10]==3 && noSources[11]==.25f && noSources[12]==.12f && noSources[13]==.09f);
            const GLenum error=glGetError();check("no-GL-errors",error==GL_NO_ERROR,error,GL_NO_ERROR);std::ofstream(evidence/"error-bounds.txt")<<std::setprecision(12)<<"independent-double-oracle-worst="<<oracleWorst<<" tolerance=.00005\nunpinned-before-after-worst="<<oldWorst<<'\n';glDeleteProgram(p);glDeleteProgram(old);
        }
        std::ofstream(evidence/"result.txt")<<"checks="<<checks<<" failures="<<failures<<" semantic_failures="<<failures-precisionStressFailures<<" float32_bit_stress_failures="<<precisionStressFailures<<" scope=actual-production-VS-FS-link-GPU-transform-feedback\nnative-Ogre-VAO=NOT_RUN whole-native-clipping=NOT_RUN normal-input=NOT_RUN old-override-pin-capability=native-NOT_RUN float32-bit-stress=NOT_GUARANTEED\n";CGLSetCurrentContext(nullptr);CGLDestroyContext(context);context=nullptr;std::cout<<"[WATER_CLIP_VERTEX_GPU] checks="<<checks<<" failures="<<failures<<" semantic_failures="<<failures-precisionStressFailures<<" float32_bit_stress_failures="<<precisionStressFailures<<'\n';return failures?1:0;
    }catch(const std::exception& e) {if(context){CGLSetCurrentContext(nullptr);CGLDestroyContext(context);}if(format)CGLDestroyPixelFormat(format);std::cerr<<"[WATER_CLIP_VERTEX_GPU] ERROR "<<e.what()<<'\n';return 2;}
}
