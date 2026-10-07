// Actual geometry coverage, four-sample linear HDR resolve and the full
// production tone/encode shader. This never invokes the retained spatial-AA
// main or changes its 719 checks / 12 quality failures.
#define main retained_spatial_aa_main
#include "validate_spatial_aa_macos.cpp"
#undef main
#include <cstdint>
namespace {
int edgeChecks=0,edgeFailures=0,edgeQualityFailures=0;
std::ofstream edgeResults,edgeQuality;
void edgeCheck(const std::string& name,bool ok,double actual=0,double expected=0,bool qualityCheck=false) {
    ++edgeChecks;edgeFailures+=!ok;edgeQualityFailures+=!ok && qualityCheck;
    edgeResults<<name<<'\t'<<(ok?"PASS":"FAIL")<<'\t'<<std::setprecision(12)<<actual<<'\t'<<expected<<'\n';
    if(!ok) std::cout<<"[MSAA_EDGE_GPU] FAIL "<<name<<" actual="<<actual<<" expected="<<expected<<'\n';
}
struct Point {double x,y;};
std::vector<Point> clip(std::vector<Point> input,double nx,double ny,double limit) {
    std::vector<Point> output;
    if(input.empty()) return output;
    Point previous=input.back();double prior=nx*previous.x+ny*previous.y-limit;
    for(const auto current:input) {
        const double distance=nx*current.x+ny*current.y-limit;
        if((prior<=0)!=(distance<=0)) {
            const double t=prior/(prior-distance);
            output.push_back({previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t});
        }
        if(distance<=0) output.push_back(current);
        previous=current;prior=distance;
    }
    return output;
}
std::vector<Point> polygon(const GeometryCase& c) {
    std::vector<Point> p{{0,0},{double(c.width),0},{double(c.width),double(c.height)},{0,double(c.height)}};
    if(c.lineWidth>0) {
        p=clip(std::move(p),c.nx,c.ny,c.offset+c.lineWidth*.5);
        p=clip(std::move(p),-c.nx,-c.ny,-c.offset+c.lineWidth*.5);
    } else p=clip(std::move(p),c.nx,c.ny,c.offset);
    return p;
}
const std::string geometryVertex=R"GLSL(#version 150
in vec2 position;
uniform vec2 viewportSize;
void main(){gl_Position=vec4(position/viewportSize*2.0-1.0,0.0,1.0);}
)GLSL";
const std::string geometryFragment=R"GLSL(#version 150
uniform vec4 ink;
out vec4 fragmentColour;
void main(){fragmentColour=ink;}
)GLSL";
constexpr float ConstantAlpha=.75f;
void drawGeometry(const Target& target,const GeometryCase& c,GLuint geometry,bool multisampleEnabled=true) {
    target.bind();
    if(multisampleEnabled) glEnable(GL_MULTISAMPLE);else glDisable(GL_MULTISAMPLE);
    glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
    const GLfloat bg[]{c.background[0],c.background[1],c.background[2],ConstantAlpha},depth=1;
    glClearBufferfv(GL_COLOR,0,bg);glClearBufferfv(GL_DEPTH,0,&depth);
    const auto p=polygon(c);std::vector<GLfloat> vertices;
    for(const auto& q:p){vertices.push_back(float(q.x));vertices.push_back(float(q.y));}
    if(vertices.empty()) return;
    glUseProgram(geometry);glUniform2f(glGetUniformLocation(geometry,"viewportSize"),float(c.width),float(c.height));
    glUniform4f(glGetUniformLocation(geometry,"ink"),c.foreground[0],c.foreground[1],c.foreground[2],ConstantAlpha);
    GLuint vbo=0;glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(GLfloat),vertices.data(),GL_STREAM_DRAW);
    const GLuint attribute=static_cast<GLuint>(glGetAttribLocation(geometry,"position"));
    glEnableVertexAttribArray(attribute);glVertexAttribPointer(attribute,2,GL_FLOAT,GL_FALSE,0,nullptr);
    glDrawArrays(GL_TRIANGLE_FAN,0,static_cast<GLsizei>(vertices.size()/2));
    glDisableVertexAttribArray(attribute);glBindBuffer(GL_ARRAY_BUFFER,0);glDeleteBuffers(1,&vbo);
}
struct TextureTarget {
    GLuint fbo=0,texture=0;int width,height;
    TextureTarget(int w,int h,GLenum format=GL_RGBA16F):width(w),height(h) {
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,GL_RGBA,GL_FLOAT,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        need(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete resolved texture FBO");
        GLint actual=0,object=0,type=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&actual);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&object);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,&type);
        need(actual==static_cast<GLint>(format)&&type==GL_TEXTURE&&object==static_cast<GLint>(texture),"Wrong resolved texture storage");
    }
    void blit(const Target& source) const {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,source.fbo);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo);
        glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    }
    std::vector<Pixel> pixels() const {
        glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);glReadBuffer(GL_COLOR_ATTACHMENT0);
        std::vector<Pixel> p(width*height);glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,p.data());return p;
    }
    ~TextureTarget(){glDeleteTextures(1,&texture);glDeleteFramebuffers(1,&fbo);}
};
std::vector<Pixel> productionResolve(const TextureTarget& source,const Target& displayTarget,GLuint resolveProgram) {
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_FRAMEBUFFER_SRGB);
    displayTarget.bind();glUseProgram(resolveProgram);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source.texture);
    glUniform1i(glGetUniformLocation(resolveProgram,"sceneTexture"),0);glUniform1f(glGetUniformLocation(resolveProgram,"exposure"),1);
    glUniform1f(glGetUniformLocation(resolveProgram,"spatialAaStrength"),0);
    glUniform4f(glGetUniformLocation(resolveProgram,"inverseTextureSize"),1.f/source.width,1.f/source.height,1,1);
    glDrawArrays(GL_TRIANGLES,0,3);return displayTarget.pixels();
}
void actualStorage(const Target& target,const std::string& prefix) {
    target.bind();
    const auto probe=[&](GLuint object,GLenum format,const std::string& suffix) {
        glBindRenderbuffer(GL_RENDERBUFFER,object);GLint f=0,s=0,w=0,h=0;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_INTERNAL_FORMAT,&f);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_SAMPLES,&s);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&w);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&h);
        edgeCheck(prefix+suffix,f==static_cast<GLint>(format)&&s==target.samples&&w==target.width&&h==target.height,s,target.samples);
    };
    probe(target.colour,GL_RGBA16F,"/actual-colour-RGBA16F-samples");probe(target.depth,GL_DEPTH24_STENCIL8,"/actual-depth-stencil-samples");
    GLint depth=0,stencil=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&depth);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_STENCIL_ATTACHMENT,GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,&stencil);
    edgeCheck(prefix+"/depth-stencil-packed-same-object",depth==stencil&&depth==static_cast<GLint>(target.depth));
}
double alphaError(const std::vector<Pixel>& p) {
    double e=0;for(const auto& q:p)e=std::max(e,double(std::abs(q[3]-ConstantAlpha)));return e;
}
double partialCoverage(const GeometryCase& c,const std::vector<Pixel>& pixels) {
    int channel=0;for(int k=1;k<3;++k)if(std::abs(c.foreground[k]-c.background[k])>std::abs(c.foreground[channel]-c.background[channel]))channel=k;
    const double contrast=c.foreground[channel]-c.background[channel];
    int count=0;for(const auto& q:pixels) {
        const double f=(q[channel]-c.background[channel])/contrast;count+=f>.02&&f<.98;
    }
    return count;
}
bool quarterCoverage(const GeometryCase& c,const std::vector<Pixel>& pixels,int samples) {
    int channel=0;for(int k=1;k<3;++k)if(std::abs(c.foreground[k]-c.background[k])>std::abs(c.foreground[channel]-c.background[channel]))channel=k;
    const double contrast=c.foreground[channel]-c.background[channel];
    for(const auto& q:pixels) {
        const double f=(q[channel]-c.background[channel])/contrast;
        // Bound solely by half-float storage quantisation, not a quality gate.
        if(std::abs(f*samples-std::round(f*samples))*std::abs(contrast)>.003)return false;
    }
    return true;
}
}
int main(int argc,char** argv) {
    try {
        need(argc==3,"Usage: msaa-edges ROOT NEW_OUTPUT");const fs::path root=fs::absolute(argv[1]),output=fs::absolute(argv[2]);
        need(!fs::exists(output),"Refusing existing evidence");fs::create_directories(output);
        edgeResults.open(output/"checks.tsv");edgeResults<<"check\tstatus\tactual\texpected\n";
        edgeQuality.open(output/"quality.tsv");edgeQuality<<"case\twidth\theight\tline_width\tmae_single\tmae_msaa4\tdelta\tclassification\tluma_single\tluma_msaa4\tarea_oracle_coverage\tcpu_centre_coverage\tactual_mixed_pixels\n";
        const auto fragment=read(root/"media/ogre/HelloMine3DHdrResolve.frag");std::ofstream(output/"production-resolve.frag")<<fragment;
        std::ofstream(output/"geometry.vert")<<geometryVertex;std::ofstream(output/"geometry.frag")<<geometryFragment;
        Context context;GLuint vao=0;glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glClampColor(GL_CLAMP_READ_COLOR,GL_FALSE);glPixelStorei(GL_PACK_ALIGNMENT,1);
        GLint maximum=0;glGetIntegerv(GL_MAX_SAMPLES,&maximum);need(maximum>=4,"Native four-sample storage unsupported");
        const GLuint resolveProgram=program(vertex,fragment),geometry=program(geometryVertex,geometryFragment);
        std::ofstream facts(output/"platform.txt");facts<<"renderer="<<glGetString(GL_RENDERER)<<"\nversion="<<glGetString(GL_VERSION)<<"\nmax_samples="<<maximum<<"\nscene=RGBA16F+DEPTH24_STENCIL8\nresolve=real GL blit to RGBA16F texture\nproduction_spatial_strength=0\ngeometry=CPU halfplane/strip clipping -> actual GL triangle fan\noracle=independent64x64 linear-radiance-area then production-independent-double tone+encode\n";
        std::array<std::array<GLfloat,2>,4> samplePositions{};
        {
            Target multisample(13,7,4,GL_RGBA16F,true),single(13,7,0,GL_RGBA16F,true);actualStorage(multisample,"storage/msaa4");actualStorage(single,"storage/single");
            multisample.bind();
            for(GLuint i=0;i<4;++i){glGetMultisamplefv(GL_SAMPLE_POSITION,i,samplePositions[i].data());facts<<"sample"<<i<<'='<<samplePositions[i][0]<<','<<samplePositions[i][1]<<'\n';}
            TextureTarget resolved(13,7);Target displayed(13,7,0,GL_RGBA32F);
            GeometryCase constant;constant.name="constant";constant.width=13;constant.height=7;constant.nx=0;constant.ny=1;constant.offset=100;
            constant.background=constant.foreground={.18f,1.f,8.f,ConstantAlpha};drawGeometry(multisample,constant,geometry);resolved.blit(multisample);
            const auto raw=resolved.pixels(),encoded=productionResolve(resolved,displayed,resolveProgram);double rawError=0,displayError=0;
            for(std::size_t i=0;i<raw.size();++i)for(int k=0;k<3;++k){rawError=std::max(rawError,std::abs(double(raw[i][k])-constant.foreground[k]));displayError=std::max(displayError,std::abs(double(encoded[i][k])-display(constant.foreground[k])));}
            edgeCheck("uniform/linear-HDR-greater-than-one-preserved",rawError<.0003,rawError,.0003);
            edgeCheck("uniform/full-production-tone-encode-reference",displayError<.0003,displayError,.0003);
            edgeCheck("uniform/constant-alpha-preserved",alphaError(raw)<.00001&&alphaError(encoded)<.00001);
            TextureTarget wrongStorage(13,7,GL_RGBA8);
            const GLfloat hdrSignal[]{.18f,1.f,8.f,ConstantAlpha};glClearBufferfv(GL_COLOR,0,hdrSignal);
            GLint wrongFormat=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_INTERNAL_FORMAT,&wrongFormat);
            edgeCheck("negative/eight-bit-storage-rejected-by-format",wrongFormat==GL_RGBA8 && wrongFormat!=GL_RGBA16F);
            edgeCheck("negative/eight-bit-storage-clips-HDR",wrongStorage.pixels()[0][2]<=1.00001);
            const auto storageError=glGetError();edgeCheck("storage/GL-errors",storageError==GL_NO_ERROR,storageError,0);
        }
        int regressions=0,improvements=0,unchanged=0,mixedTotal=0;double originalSingle=0,originalMsaa=0,totalSingle=0,totalMsaa=0;
        const auto geometryCases=cases();edgeCheck("oracle/exact-retained-97-cases",geometryCases.size()==97,geometryCases.size(),97);
        for(const auto& c:geometryCases) {
            Target msaa(c.width,c.height,4,GL_RGBA16F,true),single(c.width,c.height,0,GL_RGBA16F,true),displayed(c.width,c.height,0,GL_RGBA32F);
            TextureTarget resolved(c.width,c.height);
            drawGeometry(single,c,geometry);resolved.blit(single);const auto rawSingle=resolved.pixels(),singleOutput=productionResolve(resolved,displayed,resolveProgram);
            drawGeometry(msaa,c,geometry);resolved.blit(msaa);const auto rawMsaa=resolved.pixels(),msaaOutput=productionResolve(resolved,displayed,resolveProgram);
            std::vector<Pixel> centre(c.width*c.height);
            for(int y=0;y<c.height;++y)for(int x=0;x<c.width;++x)centre[y*c.width+x]=c.inside(x+.5,y+.5)?c.foreground:c.background;
            const auto baseline=areaError(c,singleOutput,centre),aa=areaError(c,msaaOutput,centre);const double delta=aa.rgb-baseline.rgb;
            const char* classification=delta>1e-6?"REGRESSION":delta<-1e-6?"IMPROVEMENT":"UNCHANGED";
            regressions+=delta>1e-6;improvements+=delta<-1e-6;unchanged+=std::abs(delta)<=1e-6;
            totalSingle+=baseline.rgb;totalMsaa+=aa.rgb;if(c.originalDiagonal){originalSingle+=baseline.rgb;originalMsaa+=aa.rgb;}
            const int mixed=static_cast<int>(partialCoverage(c,rawMsaa));mixedTotal+=mixed;
            edgeQuality<<c.name<<'\t'<<c.width<<'\t'<<c.height<<'\t'<<c.lineWidth<<'\t'<<std::setprecision(12)<<baseline.rgb<<'\t'<<aa.rgb<<'\t'<<delta<<'\t'<<classification<<'\t'<<baseline.luma<<'\t'<<aa.luma<<'\t'<<aa.referenceCoverage<<'\t'<<baseline.sourceCoverage<<'\t'<<mixed<<'\n';
            edgeCheck(c.name+"/area-MAE-no-regression",aa.rgb<=baseline.rgb+1e-6,aa.rgb,baseline.rgb,true);
            edgeCheck(c.name+"/alpha-single-and-MSAA",alphaError(rawSingle)<.00001&&alphaError(rawMsaa)<.00001&&alphaError(singleOutput)<.00001&&alphaError(msaaOutput)<.00001);
            edgeCheck(c.name+"/native-four-sample-coverage-quanta",quarterCoverage(c,rawMsaa,4));
            edgeCheck(c.name+"/native-single-sample-coverage-quanta",quarterCoverage(c,rawSingle,1));
            bool finite=true;for(const auto& q:msaaOutput)for(int k=0;k<3;++k)finite&=std::isfinite(q[k])&&q[k]>=0&&q[k]<=1.00001f;
            edgeCheck(c.name+"/finite-display-range",finite);
            const auto error=glGetError();edgeCheck(c.name+"/GL-errors",error==GL_NO_ERROR,error,0);
        }
        edgeCheck("aggregate/original-15-area-improves",originalMsaa<originalSingle,originalMsaa,originalSingle,true);
        edgeCheck("aggregate/all-97-area-improves",totalMsaa<totalSingle,totalMsaa,totalSingle,true);
        edgeCheck("geometry/actual-partial-sample-coverage",mixedTotal>100,mixedTotal,100);
        {
            auto c=edge("negative-coverage",48,48,.7,.213);c.background={.18f,.18f,.18f,ConstantAlpha};c.foreground={8,8,8,ConstantAlpha};
            Target msaa(48,48,4,GL_RGBA16F,true),single(48,48,0,GL_RGBA16F,true),displayed(48,48,0,GL_RGBA32F);TextureTarget resolved(48,48);
            drawGeometry(msaa,c,geometry);resolved.blit(msaa);const auto real=resolved.pixels(),actualDisplay=productionResolve(resolved,displayed,resolveProgram);
            double patternError=0;
            for(int y=0;y<c.height;++y)for(int x=0;x<c.width;++x) {
                int covered=0;for(const auto& sample:samplePositions)covered+=c.inside(x+sample[0],y+sample[1]);
                const double expected=.18+(8-.18)*covered/4.;
                patternError=std::max(patternError,std::abs(double(real[y*c.width+x][0])-expected));
            }
            edgeCheck("geometry/query-sample-pattern-independent-coverage",patternError<.003,patternError,.003);
            drawGeometry(msaa,c,geometry,false);resolved.blit(msaa);const auto disabled=resolved.pixels();
            drawGeometry(single,c,geometry);resolved.blit(single);const auto nativeSingle=resolved.pixels();
            edgeCheck("negative/multisampling-disabled-reproduces-single",maximumDifference(disabled,nativeSingle,0,4)<.0003);
            edgeCheck("negative/multisampling-disabled-loses-real-coverage",maximumDifference(real,disabled,0,3)>1,maximumDifference(real,disabled,0,3),1);
            double wrongDisplayDifference=0,linearOracleError=0;int mixed=0;
            for(std::size_t i=0;i<real.size();++i) {
                const double coverage=std::round((real[i][0]-.18)/(8-.18)*4)/4;
                if(coverage<=0||coverage>=1)continue;++mixed;
                const double linear=display(.18+(8-.18)*coverage),wrong=display(.18)*(1-coverage)+display(8)*coverage;
                wrongDisplayDifference=std::max(wrongDisplayDifference,std::abs(double(actualDisplay[i][0])-wrong));
                linearOracleError=std::max(linearOracleError,std::abs(double(actualDisplay[i][0])-linear));
            }
            edgeCheck("linear-mix-before-tone/actual-mixed-geometry",mixed>0,mixed,0);
            edgeCheck("linear-mix-before-tone/independent-reference",linearOracleError<.0003,linearOracleError,.0003);
            edgeCheck("negative/display-space-sample-average-rejected",wrongDisplayDifference>.03,wrongDisplayDifference,.03);
        }
        const auto finalError=glGetError();edgeCheck("GL-errors/final",finalError==GL_NO_ERROR,finalError,0);glDeleteProgram(resolveProgram);glDeleteProgram(geometry);glDeleteVertexArrays(1,&vao);
        std::ofstream(output/"result.txt")<<std::setprecision(12)<<"status="<<(edgeFailures?"FAIL":"PASS")<<"\nchecks="<<edgeChecks<<"\nfailures="<<edgeFailures<<"\nfunctional_failures="<<edgeFailures-edgeQualityFailures<<"\nquality_failures="<<edgeQualityFailures<<"\nregression_cases="<<regressions<<"\nimprovement_cases="<<improvements<<"\nunchanged_cases="<<unchanged<<"\noriginal_msaa4="<<originalMsaa<<"\noriginal_single="<<originalSingle<<"\nexpanded_msaa4="<<totalMsaa<<"\nexpanded_single="<<totalSingle<<"\nnormal_input=NOT_RUN\nnative_Ogre_binding=NOT_RUN\ndynamic_scene_quality=NOT_RUN\nretained_spatial_AA_719_checks_12_failures=UNCHANGED\n";
        std::cout<<"[MSAA_EDGE_GPU] status="<<(edgeFailures?"FAIL":"PASS")<<" checks="<<edgeChecks<<" functional_failures="<<edgeFailures-edgeQualityFailures<<" quality_failures="<<edgeQualityFailures<<" regressions="<<regressions<<" improvements="<<improvements<<" unchanged="<<unchanged<<" original_msaa="<<originalMsaa<<" original_single="<<originalSingle<<" expanded_msaa="<<totalMsaa<<" expanded_single="<<totalSingle<<'\n';
        return edgeFailures?1:0;
    } catch(const std::exception& error){std::cerr<<"[MSAA_EDGE_GPU] FATAL "<<error.what()<<'\n';return 2;}
}
