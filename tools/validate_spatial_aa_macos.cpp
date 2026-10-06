// Full production HDR resolve, independent geometric area coverage and real
// native MSAA storage. Functional guards and static quality regressions are
// reported separately; neither is a moving-scene or Ogre integration claim.
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
namespace fs=std::filesystem;
using Pixel=std::array<float,4>;
int checks=0,failures=0,qualityFailures=0;
std::ofstream results,quality;
void need(bool ok,const std::string& text) { if(!ok) throw std::runtime_error(text); }
void check(const std::string& name,bool ok,double actual=0,double expected=0,bool qualityCheck=false) {
    ++checks;failures+=!ok;qualityFailures+=!ok && qualityCheck;
    results<<name<<'\t'<<(ok?"PASS":"FAIL")<<'\t'<<std::setprecision(12)<<actual<<'\t'<<expected<<'\n';
    if(!ok) std::cout<<"[SPATIAL_AA_GPU] FAIL "<<name<<" actual="<<actual<<" expected="<<expected<<'\n';
}
std::string read(const fs::path& p) { std::ifstream f(p);need(f.good(),"Cannot read "+p.string());return {std::istreambuf_iterator<char>(f),{}}; }
GLuint shader(GLenum kind,const std::string& text) {
    GLuint s=glCreateShader(kind);const char* p=text.c_str();glShaderSource(s,1,&p,nullptr);glCompileShader(s);
    GLint ok=0;char log[16384]{};glGetShaderiv(s,GL_COMPILE_STATUS,&ok);glGetShaderInfoLog(s,sizeof(log),nullptr,log);
    need(ok==GL_TRUE,"Compile: "+std::string(log));return s;
}
GLuint program(const std::string& vertex,const std::string& fragment) {
    GLuint p=glCreateProgram(),v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,fragment);
    glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;char log[16384]{};glGetProgramiv(p,GL_LINK_STATUS,&ok);glGetProgramInfoLog(p,sizeof(log),nullptr,log);
    need(ok==GL_TRUE,"Link: "+std::string(log));return p;
}
struct Context {
    CGLContextObj object=nullptr;
    Context() {
        CGLPixelFormatAttribute a[]{kCGLPFAOpenGLProfile,static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),kCGLPFAAccelerated,static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj p=nullptr;GLint n=0;need(CGLChoosePixelFormat(a,&p,&n)==kCGLNoError&&p,"No accelerated format");
        auto status=CGLCreateContext(p,nullptr,&object);CGLDestroyPixelFormat(p);
        need(status==kCGLNoError&&object,"No context");need(CGLSetCurrentContext(object)==kCGLNoError,"No current context");
    }
    ~Context(){CGLSetCurrentContext(nullptr);if(object) CGLDestroyContext(object);}
};
const std::string vertex=R"GLSL(#version 150
out vec2 postUv;
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0,1);postUv=p;}
)GLSL";
struct Target {
    GLuint fbo=0,colour=0,depth=0;int width,height,samples;
    Target(int w,int h,int s,GLenum format,bool withDepth=false):width(w),height(h),samples(s) {
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenRenderbuffers(1,&colour);glBindRenderbuffer(GL_RENDERBUFFER,colour);
        if(s) glRenderbufferStorageMultisample(GL_RENDERBUFFER,s,format,w,h);else glRenderbufferStorage(GL_RENDERBUFFER,format,w,h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colour);
        if(withDepth) {
            glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
            if(s) glRenderbufferStorageMultisample(GL_RENDERBUFFER,s,GL_DEPTH24_STENCIL8,w,h);else glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,w,h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depth);
        }
        need(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete framebuffer");
        const auto actualStorage=[&](GLuint renderbuffer,GLenum expected) {
            glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);GLint actual=0,count=0;
            glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_INTERNAL_FORMAT,&actual);
            glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_SAMPLES,&count);
            need(actual==static_cast<GLint>(expected)&&count==s,"Wrong actual storage format/samples");
        };
        actualStorage(colour,format);if(depth) actualStorage(depth,GL_DEPTH24_STENCIL8);
    }
    void bind() const {glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,width,height);}
    std::vector<Pixel> pixels() const {bind();std::vector<Pixel> p(width*height);glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,p.data());return p;}
    ~Target(){glDeleteRenderbuffers(1,&colour);if(depth) glDeleteRenderbuffers(1,&depth);glDeleteFramebuffers(1,&fbo);}
};
double display(double x) {
    x=std::max(0.,x);const double y=std::clamp(x*(2.51*x+.03)/(x*(2.43*x+.59)+.14),0.,1.);
    return y<=.0031308?12.92*y:1.055*std::pow(y,1./2.4)-.055;
}
std::vector<Pixel> resolve(const Target& target,GLuint p,const std::vector<Pixel>& data,bool aa,std::array<float,2> inverse={-100,-100}) {
    if(inverse[0]==-100) inverse={1.f/target.width,1.f/target.height};
    GLuint tex=0;glGenTextures(1,&tex);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,target.width,target.height,0,GL_RGBA,GL_FLOAT,data.data());
    target.bind();glUseProgram(p);glUniform1i(glGetUniformLocation(p,"sceneTexture"),0);
    glUniform1f(glGetUniformLocation(p,"exposure"),1);glUniform1f(glGetUniformLocation(p,"spatialAaStrength"),aa?1:0);
    glUniform4f(glGetUniformLocation(p,"inverseTextureSize"),inverse[0],inverse[1],0,0);
    glDrawArrays(GL_TRIANGLES,0,3);auto output=target.pixels();glDeleteTextures(1,&tex);return output;
}
double maximumDifference(const std::vector<Pixel>& a,const std::vector<Pixel>& b,int first,int last) {
    double difference=0;
    for(std::size_t i=0;i<a.size();++i) for(int c=first;c<last;++c) difference=std::max(difference,double(std::abs(a[i][c]-b[i][c])));
    return difference;
}
struct GeometryCase {
    std::string name;int width=48,height=48;double nx=0,ny=1,offset=24,lineWidth=0;
    Pixel background{.05f,.05f,.05f,1},foreground{.8f,.8f,.8f,1};
    bool originalDiagonal=false,lowContrast=false;
    bool inside(double x,double y) const {
        const double distance=nx*x+ny*y-offset;
        return lineWidth>0?std::abs(distance)<lineWidth*.5:distance<0;
    }
    double coverage(int x,int y,int samples) const {
        int covered=0;
        for(int v=0;v<samples;++v) for(int u=0;u<samples;++u) covered+=inside(x+(u+.5)/samples,y+(v+.5)/samples);
        return double(covered)/(samples*samples);
    }
};
GeometryCase edge(std::string name,int w,int h,double slope,double shift) {
    GeometryCase c;c.name=std::move(name);c.width=w;c.height=h;
    const double length=std::sqrt(1+slope*slope);c.nx=-slope/length;c.ny=1/length;
    c.offset=(h*.5-slope*w*.5+shift)/length;return c;
}
struct AreaError {double rgb=0,luma=0,referenceCoverage=0,sourceCoverage=0;};
AreaError areaError(const GeometryCase& c,const std::vector<Pixel>& output,const std::vector<Pixel>& source) {
    AreaError e;constexpr std::array<double,3> weights{.299,.587,.114};
    for(int y=2;y<c.height-2;++y) for(int x=2;x<c.width-2;++x) {
        // Analytic geometry sampled at 64x64 subpixels; no production filter
        // weights, luma gradients or AA neighbourhood are used by the oracle.
        const double f=c.coverage(x,y,64);double expectedLuma=0,actualLuma=0;
        for(int k=0;k<3;++k) {
            const double expected=display(c.background[k]+(c.foreground[k]-c.background[k])*f);
            const double actual=output[y*c.width+x][k];e.rgb+=std::abs(actual-expected)/3;
            expectedLuma+=expected*weights[k];actualLuma+=actual*weights[k];
        }
        e.luma+=std::abs(expectedLuma-actualLuma);e.referenceCoverage+=f;
        e.sourceCoverage+=source[y*c.width+x][0]==c.foreground[0];
    }
    const int count=(c.width-4)*(c.height-4);e.rgb/=count;e.luma/=count;return e;
}
std::vector<GeometryCase> cases() {
    std::vector<GeometryCase> list;
    for(double slope:{.35,.7,1.,1.5,2.7}) for(double shift:{.2,.5,.8}) {
        auto c=edge("diagonal/"+std::to_string(slope)+"/"+std::to_string(shift),48,48,slope,shift);c.originalDiagonal=true;list.push_back(c);
    }
    for(double slope:{.35,.7,1.,1.5,2.7}) for(double width:{1.,.5,.25}) for(double phase:{.2,.5,.8}) {
        auto c=edge("line/"+std::to_string(slope)+"/width-"+std::to_string(width)+"/phase-"+std::to_string(phase),48,48,slope,phase);
        c.lineWidth=width;list.push_back(c);
    }
    for(double shift:{.2,.5,.8}) {
        list.push_back(edge("horizontal/phase-"+std::to_string(shift),48,48,0,shift));
        GeometryCase c;c.name="vertical/phase-"+std::to_string(shift);c.nx=1;c.ny=0;c.offset=24+shift;list.push_back(c);
    }
    for(double width:{1.,.5,.25}) for(double shift:{.2,.5,.8}) {
        auto horizontal=edge("horizontal-line/width-"+std::to_string(width)+"/phase-"+std::to_string(shift),48,48,0,shift);
        horizontal.lineWidth=width;list.push_back(horizontal);
        GeometryCase vertical;vertical.name="vertical-line/width-"+std::to_string(width)+"/phase-"+std::to_string(shift);
        vertical.nx=1;vertical.ny=0;vertical.offset=24+shift;vertical.lineWidth=width;list.push_back(vertical);
    }
    for(double slope:{.35,.7,1.5}) {
        auto coloured=edge("colour-luma/"+std::to_string(slope),48,48,slope,.2);
        coloured.background={.02f,.04f,.6f,1};coloured.foreground={.8f,.35f,.04f,1};list.push_back(coloured);
        auto low=edge("low-contrast/"+std::to_string(slope),48,48,slope,.2);
        low.background={.18f,.3f,.5f,1};low.foreground={.19f,.31f,.51f,1};low.lowContrast=true;list.push_back(low);
        auto iso=edge("equal-display-luma-colour/"+std::to_string(slope),48,48,slope,.5);
        iso.background={.02f,.2f,.8f,1};iso.foreground={.8f,0,.02f,1};iso.lowContrast=true;
        const double target=.299*display(iso.background[0])+.587*display(iso.background[1])+.114*display(iso.background[2]);
        double a=0,b=1;
        for(int i=0;i<60;++i) {
            const double green=(a+b)*.5;
            if(.299*display(iso.foreground[0])+.587*display(green)+.114*display(iso.foreground[2])<target) a=green;else b=green;
        }
        iso.foreground[1]=float((a+b)*.5);list.push_back(iso);
    }
    for(const auto dimension:{std::array<int,2>{79,31},std::array<int,2>{31,79}}) {
        const auto prefix="non-square/"+std::to_string(dimension[0])+"x"+std::to_string(dimension[1]);
        list.push_back(edge(prefix+"/diagonal",dimension[0],dimension[1],.7,.2));
        auto line=edge(prefix+"/half-pixel-line",dimension[0],dimension[1],1.5,.5);line.lineWidth=.5;list.push_back(line);
    }
    return list;
}
}
int main(int argc,char** argv) {
    try {
        need(argc==3,"Usage: aa-gpu ROOT NEW_OUTPUT");const fs::path root=fs::absolute(argv[1]),output=fs::absolute(argv[2]);
        need(!fs::exists(output),"Refusing existing evidence");fs::create_directories(output);
        results.open(output/"samples.tsv");results<<"check\tstatus\tactual\texpected\n";
        quality.open(output/"quality.tsv");quality<<"case\twidth\theight\tline_width\tmae_off\tmae_on\tdelta\tclassification\tluma_mae_off\tluma_mae_on\treference_coverage\tsource_coverage\n";
        const std::string fragment=read(root/"media/ogre/HelloMine3DHdrResolve.frag");std::ofstream(output/"production-resolve.frag")<<fragment;
        Context context;GLuint vao=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glClampColor(GL_CLAMP_READ_COLOR,GL_FALSE);glPixelStorei(GL_PACK_ALIGNMENT,1);
        const GLuint p=program(vertex,fragment);
        for(const auto dimensions:{std::array<int,2>{1,1},std::array<int,2>{17,17},std::array<int,2>{48,48},std::array<int,2>{79,31}}) {
            Target target(dimensions[0],dimensions[1],0,GL_RGBA32F);
            const std::string label="constant/"+std::to_string(target.width)+"x"+std::to_string(target.height);
            const std::vector<Pixel> data(target.width*target.height,{.18f,.5f,2.f,.35f});
            const auto off=resolve(target,p,data,false),on=resolve(target,p,data,true);
            check(label+"/uniform-unchanged",maximumDifference(on,off,0,4)<.00003);
            double expectedError=0;
            for(const auto& pixel:on) for(int c=0;c<3;++c) expectedError=std::max(expectedError,std::abs(double(pixel[c])-display(data[0][c])));
            check(label+"/independent-HDR",expectedError<.0003,expectedError,.0003);
            const float ix=1.f/target.width,iy=1.f/target.height;
            const std::array<std::array<float,2>,5> inverse{{{0,iy},{-ix,iy},{ix,0},{ix,-iy},{-ix,-iy}}};
            const std::array<const char*,5> names{"x-zero","x-negative","y-zero","y-negative","both-negative"};
            for(std::size_t i=0;i<inverse.size();++i) {
                const auto invalid=resolve(target,p,data,true,inverse[i]);
                check(label+"/invalid-inverse-"+names[i],maximumDifference(invalid,off,0,4)<.00003);
            }
        }
        double totalOff=0,totalOn=0,originalOff=0,originalOn=0;int differences=0,regressions=0,improvements=0,unchanged=0;
        for(const auto& c:cases()) {
            Target target(c.width,c.height,0,GL_RGBA32F);std::vector<Pixel> data(c.width*c.height);
            for(int y=0;y<c.height;++y) for(int x=0;x<c.width;++x) {
                Pixel pixel=c.inside(x+.5,y+.5)?c.foreground:c.background;pixel[3]=.25f+(y%4)*.125f;data[y*c.width+x]=pixel;
            }
            const auto off=resolve(target,p,data,false),on=resolve(target,p,data,true);
            const auto eo=areaError(c,off,data),ea=areaError(c,on,data);const double delta=ea.rgb-eo.rgb;
            const char* classification=delta>1e-6?"REGRESSION":delta<-1e-6?"IMPROVEMENT":"UNCHANGED";
            regressions+=delta>1e-6;improvements+=delta<-1e-6;unchanged+=std::abs(delta)<=1e-6;
            totalOff+=eo.rgb;totalOn+=ea.rgb;if(c.originalDiagonal){originalOff+=eo.rgb;originalOn+=ea.rgb;}
            quality<<c.name<<'\t'<<c.width<<'\t'<<c.height<<'\t'<<c.lineWidth<<'\t'<<std::setprecision(12)<<eo.rgb<<'\t'<<ea.rgb<<'\t'<<delta<<'\t'<<classification<<'\t'<<eo.luma<<'\t'<<ea.luma<<'\t'<<eo.referenceCoverage<<'\t'<<eo.sourceCoverage<<'\n';
            check(c.name+"/area-MAE-no-regression",ea.rgb<=eo.rgb+1e-6,ea.rgb,eo.rgb,true);
            check(c.name+"/alpha",maximumDifference(on,off,3,4)<.00002);
            bool bounded=true;
            for(std::size_t i=0;i<on.size();++i) for(int k=0;k<3;++k) {
                const double low=display(std::min(c.background[k],c.foreground[k])),high=display(std::max(c.background[k],c.foreground[k]));
                bounded &= std::isfinite(on[i][k]) && on[i][k]>=low-.001 && on[i][k]<=high+.001;
            }
            check(c.name+"/finite-range",bounded);
            if(c.originalDiagonal) for(std::size_t i=0;i<on.size();++i) differences+=std::abs(on[i][0]-off[i][0])>.001;
            if(c.lowContrast) check(c.name+"/low-contrast-unchanged",maximumDifference(on,off,0,4)<.00003);
            const float ix=1.f/c.width,iy=1.f/c.height;
            const std::array<std::array<float,2>,4> inverse{{{0,iy},{-ix,iy},{ix,0},{ix,-iy}}};
            const std::array<const char*,4> names{"x-zero","x-negative","y-zero","y-negative"};
            for(std::size_t i=0;i<inverse.size();++i) {
                const auto invalid=resolve(target,p,data,true,inverse[i]);
                check(c.name+"/invalid-inverse-"+names[i],maximumDifference(invalid,off,0,4)<.00002);
            }
        }
        // The r1 diagonal aggregate gate is retained exactly. Expanded static
        // coverage and every case are additional gates, not replacements.
        check("aggregate/original-independent-area-coverage-improves",originalOn<originalOff,originalOn,originalOff,true);
        check("aggregate/expanded-independent-area-coverage-improves",totalOn<totalOff,totalOn,totalOff,true);
        check("negative-disabled-AA-differs-from-filtered-edges",differences>100,differences,100);
        GLint maximum=0;glGetIntegerv(GL_MAX_SAMPLES,&maximum);
        std::ofstream facts(output/"platform.txt");facts<<"renderer="<<glGetString(GL_RENDERER)<<"\nversion="<<glGetString(GL_VERSION)<<"\nmax_samples="<<maximum<<"\nproduction_max_scene_samples=9\nproduction_extra_targets=0\nproduction_history_bytes=0\n";
        if(maximum>=4) {
            constexpr int n=48;Target multisample(n,n,4,GL_RGBA16F,true),single(n,n,0,GL_RGBA16F);multisample.bind();glEnable(GL_MULTISAMPLE);
            const GLfloat background[]{.05f,.05f,.05f,.75f};glClearBufferfv(GL_COLOR,0,background);
            const GLuint geom=program(R"GLSL(#version 150
void main(){vec2 p=gl_VertexID==0?vec2(-1,-1):gl_VertexID==1?vec2(1,-1):vec2(-1,1);gl_Position=vec4(p,0,1);}
)GLSL",R"GLSL(#version 150
out vec4 fragmentColour;void main(){fragmentColour=vec4(.8,.8,.8,.75);}
)GLSL");
            glUseProgram(geom);glDrawArrays(GL_TRIANGLES,0,3);glBindFramebuffer(GL_READ_FRAMEBUFFER,multisample.fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,single.fbo);
            glBlitFramebuffer(0,0,n,n,0,0,n,n,GL_COLOR_BUFFER_BIT,GL_NEAREST);const auto pixels=single.pixels();int mixed=0;double alphaError=0;
            for(const auto& q:pixels){mixed+=q[0]>.051&&q[0]<.799;alphaError=std::max(alphaError,double(std::abs(q[3]-.75f)));}
            check("native-MSAA/alpha",alphaError<.001,alphaError,.001);
            check("native-MSAA/RGBA16F-four-samples-real-coverage",mixed>0,mixed,0);
            facts<<"msaa4_RGBA16F_depth24stencil8=SUPPORTED\nmsaa4_colour_bytes_at_2560x1440="<<2560ull*1440*8*4<<"\nmsaa4_depth_bytes_at_2560x1440="<<2560ull*1440*4*4<<"\nresolved_colour_bytes_at_2560x1440="<<2560ull*1440*8<<"\n";glDeleteProgram(geom);
        } else facts<<"msaa4_RGBA16F_depth24stencil8=UNSUPPORTED_MAX_SAMPLES\n";
        check("GL-errors",glGetError()==GL_NO_ERROR);glDeleteProgram(p);glDeleteVertexArrays(1,&vao);
        std::ofstream(output/"result.txt")<<"status="<<(failures?"FAIL":"PASS")<<"\nchecks="<<checks<<"\nfailures="<<failures<<"\nfunctional_failures="<<failures-qualityFailures<<"\nquality_failures="<<qualityFailures<<"\nquality_regression_cases="<<regressions<<"\nquality_improvement_cases="<<improvements<<"\nquality_unchanged_cases="<<unchanged<<"\noriginal_aggregate_on="<<originalOn<<"\noriginal_aggregate_off="<<originalOff<<"\nexpanded_aggregate_on="<<totalOn<<"\nexpanded_aggregate_off="<<totalOff<<"\nordinary_input=NOT_RUN\nnative_Ogre_binding=NOT_RUN\ndynamic_scene_quality=NOT_RUN\n";
        std::cout<<"[SPATIAL_AA_GPU] status="<<(failures?"FAIL":"PASS")<<" checks="<<checks<<" failures="<<failures<<" functional_failures="<<failures-qualityFailures<<" regressions="<<regressions<<" improvements="<<improvements<<" unchanged="<<unchanged<<" original_on="<<originalOn<<" original_off="<<originalOff<<" expanded_on="<<totalOn<<" expanded_off="<<totalOff<<'\n';
        return failures?1:0;
    } catch(const std::exception& e){std::cerr<<"[SPATIAL_AA_GPU] FATAL "<<e.what()<<'\n';return 2;}
}
