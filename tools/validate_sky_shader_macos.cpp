// Render the production sky fragment shader; these are diagnostic GPU samples.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_sky_shader_macos.cpp \
//   -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation -o /tmp/sky-gpu
// /tmp/sky-gpu <candidate Skybox.frag> <baseline Skybox.frag> <new output directory> [--celestial|--cloud-form|--natural-sky|--atmospheric-clouds]
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
constexpr float CloudBaseHeight = 320.f;
constexpr float CloudBottom = CloudBaseHeight - 12.f;
constexpr float CloudTop = CloudBaseHeight + 12.f;
constexpr float AboveClouds = CloudTop + 30.f;
using Pixels = std::vector<unsigned char>;
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string read(const std::filesystem::path& path) {
    std::ifstream input(path);
    require(input.good(), "Missing shader " + path.string());
    return {std::istreambuf_iterator<char>(input), {}};
}
GLuint program(const std::string& fragment, const std::string& customVertex = {}) {
    const std::string vertex = customVertex.empty() ? R"GLSL(#version 150
out vec3 vDirection;
uniform float viewPitch;
void main() {
    vec2 p = gl_VertexID == 0 ? vec2(-1,-1) :
        (gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3));
    gl_Position = vec4(p,0,1);
    vDirection = vec3(p.x * 1.5, p.y * .75 + viewPitch, -1);
})GLSL" : customVertex;
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
              float viewPitch=.4f, float z=552) {
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
    scalar("cloudBaseHeight",CloudBaseHeight); scalar("cloudThickness",24); scalar("cloudHorizontalScale",92);
    glUniform2f(glGetUniformLocation(p,"cloudVelocity"),1.6f,.55f);
    scalar("cloudMaxDistance",2400); vector("cameraPosition",x,height,z);
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

const std::string CelestialVertex = R"GLSL(#version 150
out vec3 vDirection;
uniform vec3 probeDirection;
void main() {
    vec2 p = gl_VertexID == 0 ? vec2(-1,-1) :
        (gl_VertexID == 1 ? vec2(3,-1) : vec2(-1,3));
    gl_Position = vec4(p,0,1);
    vec3 right = normalize(cross(vec3(0,0,1), probeDirection));
    vDirection = probeDirection + 0.14 *
        (p.x * right + p.y * cross(probeDirection, right));
})GLSL";

Pixels celestialProbe(GLuint p, bool moon=false, float height=AboveClouds,
                      float x=168, bool legacy=false, bool reverse=false,
                      float noonOffset=2.f) {
    glUseProgram(p);
    const auto scalar=[&](const char* key,float value) {
        glUniform1f(glGetUniformLocation(p,key),value);
    };
    const auto vector=[&](const char* key,float x,float y,float z) {
        glUniform3f(glGetUniformLocation(p,key),x,y,z);
    };
    // Equal sky and cloud colours isolate transmission of the real body.
    for(const char* key : {"skyZenithColour","skyHorizonColour",
                          "cloudLightColour","cloudShadowColour","fogSunwardColour"})
        vector(key,.05f,.05f,.05f);
    float sx=0,sy=.70710678f,sz=-.70710678f;
    if(noonOffset!=2.f) {sx=noonOffset;sy=1;sz=0;}
    const float sign=moon?-1.f:1.f;
    vector("sunDirection",sign*sx,sign*sy,sign*sz);
    vector("sunColour",1,.92f,.72f);
    scalar("sunIntensity",moon?0:1);scalar("moonIntensity",moon?1:0);
    scalar("starIntensity",0);scalar("fogDirectionalStrength",0);
    scalar("cloudCoverage",1);scalar("cloudLayerEnabled",legacy?0:1);
    scalar("cloudBaseHeight",CloudBaseHeight);scalar("cloudThickness",24);
    scalar("cloudHorizontalScale",92);scalar("cloudMaxDistance",2400);
    glUniform2f(glGetUniformLocation(p,"cloudVelocity"),1.6f,.55f);
    vector("cameraPosition",x,height,552);scalar("globalTime",5);scalar("legacyTime",0);
    float facing=reverse?-1.f:1.f;
    // A fixed probe through noon catches any discontinuity in the body basis.
    if(noonOffset!=2.f) {sx=0;sy=1;sz=0;}
    vector("probeDirection",facing*sx,facing*sy,facing*sz);
    glDrawArrays(GL_TRIANGLES,0,3);
    Pixels pixels(Edge*Edge*4);
    glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR,"Celestial draw/read failure");
    return pixels;
}

double centreLight(const Pixels& image) {
    double value=0;
    for(int y=120;y<136;++y) for(int x=120;x<136;++x)
        for(int c=0;c<3;++c) value+=image[(y*Edge+x)*4+c];
    return value/(16*16*3);
}
double minimumTransmission(GLuint program,bool moon) {
    const double clear=centreLight(celestialProbe(program,moon))-12.75;
    double minimum=1;
    // All predetermined columns contribute; no selection of a new fixture
    // after inspecting the candidate or weakening the rejection threshold.
    for(int x=0;x<2048;x+=128) {
        const double covered=centreLight(celestialProbe(program,moon,108,float(x)))-12.75;
        minimum=std::min(minimum,covered/clear);
    }
    return minimum;
}

bool roundBody(const Pixels& pixels,bool moon) {
    // Sample the rendered surface, not a reimplementation of the mask.
    // Cardinal edges remain lit; diagonal corners at the same box extent
    // must show sky. Check all quadrants so crater patterns cannot hide a card.
    const float radius=(moon?.046f:.052f)/.14f*Edge*.5f;
    const auto red=[&](float x,float y) {
        const int px=int(Edge*.5f+x*radius),py=int(Edge*.5f+y*radius);
        return int(pixels[(py*Edge+px)*4]);
    };
    for(float sign:{-1.f,1.f}) {
        if(red(sign*.88f,0)<100||red(0,sign*.88f)<100) return false;
        for(float other:{-1.f,1.f})
            if(red(sign*.84f,other*.84f)>80) return false;
    }
    return true;
}
}
int main(int argc,char** argv) {
    try {
        const bool atmosphericClouds=argc==5&&std::string(argv[4])=="--atmospheric-clouds";
        const bool naturalSky=argc==5&&(std::string(argv[4])=="--natural-sky"||atmosphericClouds);
        const bool cloudForm=argc==5&&(std::string(argv[4])=="--cloud-form"||naturalSky);
        const bool celestial=argc==5&&(std::string(argv[4])=="--celestial"||cloudForm);
        require(argc==4||celestial,"Usage: sky-gpu <candidate> <baseline> <new output> [--celestial|--cloud-form|--natural-sky|--atmospheric-clouds]");
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
        if(celestial&&!cloudForm) check("cloud-only-composition-preserved",day==old);
        else check("bounded-cloud-composition-changes",difference(day,old)>.1);
        check("world-space-parallax",difference(day,render(current,5,true,false,108,208))>.1);
        check("cloud-layer-height-parallax",difference(day,render(current,5,true,false,CloudBaseHeight))>.1);
        check("day-and-night-readable",difference(day,render(current,5,true,true))>20);
        check("sun-moon-handoff-continuity",
              difference(render(current,5,true,false,108,168,1,true,.4999f),
                         render(current,5,true,false,108,168,1,true,.5001f))<.002);
        check("cloud-bottom-crossing-continuity",
              difference(render(current,5,true,false,CloudBottom-.001f),
                         render(current,5,true,false,CloudBottom+.001f))<.1);
        check("cloud-top-crossing-continuity",
              difference(render(current,5,true,false,CloudTop-.001f,168,0,false,-1,-.4f),
                         render(current,5,true,false,CloudTop+.001f,168,0,false,-1,-.4f))<.1);
        png(output/"cloud-inside.png",render(current,5,true,false,CloudBaseHeight));
        png(output/"cloud-above.png",render(current,5,true,false,AboveClouds,168,0,false,-1,-.4f));
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
                  equalRows(render(current,5,true,night,AboveClouds),
                            render(baseline,5,true,night,AboveClouds),128,Edge));
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
        if(celestial) {
            const auto source=read(argv[1]);
            const GLuint body=program(source,CelestialVertex),oldBody=program(read(argv[2]),CelestialVertex);
            for(bool moon:{false,true}) {
                const auto clear=celestialProbe(body,moon),before=celestialProbe(oldBody,moon);
                png(output/(moon?"moon-clear.png":"sun-clear.png"),clear);
                png(output/(moon?"moon-before.png":"sun-before.png"),before);
                check(moon?"moon-redesigned":"sun-redesigned",difference(clear,before)>1);
                const auto transmission=minimumTransmission(body,moon);
                std::cout<<"[SKY_GPU] "<<(moon?"moon":"sun")<<" transmission="<<transmission<<'\n';
                check(moon?"moon-cloud-occlusion":"sun-cloud-occlusion",transmission<.45);
                check(moon?"old-moon-occlusion-defect-detected":"old-sun-occlusion-defect-detected",
                      minimumTransmission(oldBody,moon)>.95);
                check(moon?"legacy-moon-exact":"legacy-sun-exact",
                      celestialProbe(body,moon,108,168,true)==celestialProbe(oldBody,moon,108,168,true));
                check(moon?"moon-no-backface":"sun-no-backface",
                      centreLight(celestialProbe(body,moon,AboveClouds,168,false,true))<14);
                check(moon?"moon-zenith-continuity":"sun-zenith-continuity",
                      difference(celestialProbe(body,moon,AboveClouds,168,false,false,-.00001f),
                                 celestialProbe(body,moon,AboveClouds,168,false,false,.00001f))<.1);
            }
            const auto sun=celestialProbe(body),moon=celestialProbe(body,true);
            if(naturalSky) {
                check("sun-round-silhouette",roundBody(sun,false));
                check("moon-round-silhouette",roundBody(moon,true));
                auto square=source;
                const std::string boundary="float boundary = length(uv);";
                const auto at=square.find(boundary);
                require(at!=std::string::npos,"Missing round celestial boundary");
                square.replace(at,boundary.size(),
                    "float boundary = max(abs(uv.x),abs(uv.y));");
                const auto squareBody=program(square,CelestialVertex);
                check("square-sun-negative-detected",!roundBody(celestialProbe(squareBody),false));
                check("square-moon-negative-detected",!roundBody(celestialProbe(squareBody,true),true));
                glDeleteProgram(squareBody);
            }
            // The sun core must retain warm colour instead of saturating white.
            const int centre=(128*Edge+128)*4;
            check("sun-warm-readable-core",sun[centre]-sun[centre+2]>30);
            // Two equal-radius locations: block crater versus plain moon surface.
            const int left=(139*Edge+115)*4,right=(139*Edge+140)*4;
            check("moon-pixel-crater-contrast",int(moon[right])-int(moon[left])>15);
            std::string unoccluded=source;
            const std::string blend="colour = mix(colour, cloudColour, cloudMask);";
            const auto start=unoccluded.find("colour = composePixelCelestials(colour, direction);");
            require(start!=std::string::npos,"Missing production celestial composition");
            const auto location=unoccluded.find(blend,start);
            require(location!=std::string::npos,"Missing production cloud composition");
            unoccluded.replace(location,blend.size(),"colour = colour;");
            const GLuint broken=program(unoccluded,CelestialVertex);
            check("removed-cloud-blend-negative-detected",minimumTransmission(broken,false)>.95);
            glDeleteProgram(broken);glDeleteProgram(body);glDeleteProgram(oldBody);
        }
        if(cloudForm) {
            const auto maskShader=[](std::string source) {
                const auto main=source.find("void main()");
                require(main!=std::string::npos,"Missing sky entry point");
                source.replace(main,11,"void unusedMain()");
                return source+R"GLSL(
void main() {
    float mask; vec3 colour;
    sampleBoundedCloudLayer(normalize(vDirection),mask,colour);
    fragColor=vec4(vec3(mask),1);
})GLSL";
            };
            const auto cloud=program(maskShader(read(argv[1])));
            const auto oldCloud=program(maskShader(read(argv[2])));
            const auto currentMask=render(cloud,5),oldMask=render(oldCloud,5);
            const auto fractions=[](const Pixels& p) {
                double filled=0,transition=0;
                for(std::size_t i=0;i<p.size();i+=4) {
                    filled+=p[i]>8;transition+=p[i]>24&&p[i]<160;
                }
                return std::pair<double,double>{filled/(Edge*Edge),transition/(Edge*Edge)};
            };
            const auto shape=fractions(currentMask),before=fractions(oldMask);
            std::cout<<"[SKY_GPU] cloud-filled="<<shape.first
                     <<" transition="<<shape.second<<" old-transition="<<before.second<<'\n';
            check("cloud-groups-leave-clear-sky",shape.first>.02&&shape.first<.60);
            if(atmosphericClouds) {
                // Review the same production shader at ordinary ground,
                // alpine and near-summit heights before opening a client.
                // These are sky-only probes, never normal-play evidence.
                png(output/"cloud-lowland-sky.png",
                    render(current,5,true,false,70));
                png(output/"cloud-alpine-sky.png",
                    render(current,5,true,false,129));
                png(output/"cloud-summit-sky.png",
                    render(current,5,true,false,240));
                // B1e explicitly uses translucent margins. Keep the historical
                // hard-edge check in its original mode; it is not an art score.
                const auto layeredCoverage=[](const Pixels& p) {
                    int clear=0,thin=0,dense=0;
                    for(std::size_t i=0;i<p.size();i+=4) {
                        clear+=p[i]<=8;
                        thin+=p[i]>24&&p[i]<160;
                        dense+=p[i]>=192;
                    }
                    const double pixels=Edge*Edge;
                    return clear/pixels>.25&&thin/pixels>.01&&dense/pixels>.01;
                };
                check("cloud-clear-thin-dense-regions",layeredCoverage(currentMask));
                auto hardSource=maskShader(read(argv[1]));
                const std::string outputMask="fragColor=vec4(vec3(mask),1);";
                auto at=hardSource.find(outputMask);
                require(at!=std::string::npos,"Missing cloud probe output");
                hardSource.replace(at,outputMask.size(),
                                   "fragColor=vec4(vec3(step(0.5,mask)),1);");
                const auto hard=program(hardSource);
                check("hard-cutout-cloud-negative-detected",!layeredCoverage(render(hard,5)));
                glDeleteProgram(hard);
                auto hazeSource=maskShader(read(argv[1]));
                at=hazeSource.find(outputMask);
                hazeSource.replace(at,outputMask.size(),"fragColor=vec4(vec3(0.35),1);");
                const auto haze=program(hazeSource);
                check("uniform-haze-negative-detected",!layeredCoverage(render(haze,5)));
                glDeleteProgram(haze);
            } else {
                check("cloud-boundaries-defined",shape.second<before.second*.80);
            }
            check("world-wind-advection-invariant",
                  difference(currentMask,render(cloud,10,true,false,108,160,0,false,-1,.4f,549.25f))<.01);
            check("negative-coordinate-wind-advection-invariant",
                  difference(render(cloud,5,true,false,108,-168,0,false,-1,.4f,-552),
                             render(cloud,10,true,false,108,-176,0,false,-1,.4f,-554.75f))<.01);
            auto stationarySource=maskShader(read(argv[1]));
            const std::string motion="vec2 motion = cloudVelocity * globalTime;";
            const auto motionAt=stationarySource.find(motion);
            require(motionAt!=std::string::npos,"Missing world cloud advection");
            stationarySource.replace(motionAt,motion.size(),"vec2 motion = vec2(0.0);");
            const auto stationary=program(stationarySource);
            check("stationary-cloud-negative-detected",
                  difference(render(stationary,5),
                             render(stationary,10,true,false,108,160,0,false,-1,.4f,549.25f))>.10);
            png(output/"cloud-mask-before.png",oldMask);
            png(output/"cloud-mask-after.png",currentMask);
            glDeleteProgram(cloud);glDeleteProgram(oldCloud);glDeleteProgram(stationary);
        }
        glDeleteProgram(current);glDeleteProgram(baseline);glDeleteRenderbuffers(1,&colour);
        glDeleteFramebuffers(1,&fbo);glDeleteVertexArrays(1,&vao);
        CGLSetCurrentContext(nullptr);CGLDestroyContext(context);
        std::cout<<"[SKY_GPU] checks="<<checks<<" failures=0\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"[SKY_GPU] FAIL "<<error.what()<<'\n';return 1;}
}
