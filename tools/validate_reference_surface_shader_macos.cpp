// Actual production PBR GLSL + real explicitly authored channel mips in CGL.
// This tool does not certify Ogre material binding, native game or gameplay.
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
namespace fs = std::filesystem;
struct V { double x=0,y=0,z=0; V operator+(V v)const{return{x+v.x,y+v.y,z+v.z};} V operator-(V v)const{return{x-v.x,y-v.y,z-v.z};}
    V operator*(double s)const{return{x*s,y*s,z*s};} V operator*(V v)const{return{x*v.x,y*v.y,z*v.z};} };
using Pixel=std::array<float,4>;
double dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V unit(V a){double l=std::sqrt(dot(a,a));return l>0?a*(1/l):V{};}
V mix(V a,V b,double t){return a*(1-t)+b*t;}
double clamp(double x,double lo=0,double hi=1){return std::clamp(x,lo,hi);}
V yaw(V p,int q){switch(q%4){case 1:return{p.z,p.y,-p.x};case 2:return{-p.x,p.y,-p.z};case 3:return{-p.z,p.y,p.x};default:return p;}}
double srgb(double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);}
V decode(V c){return{srgb(c.x),srgb(c.y),srgb(c.z)};}
int checks=0,failures=0;std::ofstream samples;
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void check(const std::string& name,bool ok,double actual=0,double expected=0){++checks;failures+=!ok;
    samples<<name<<'\t'<<(ok?"PASS":"FAIL")<<'\t'<<std::setprecision(14)<<actual<<'\t'<<expected<<'\n';
    if(!ok)std::cout<<"[REFERENCE_SURFACE_GPU] FAIL "<<name<<" actual="<<actual<<" expected="<<expected<<'\n';}
void close(const std::string& name,Pixel actual,V expected,double tolerance=.0002){
    const std::array<double,3> e{expected.x,expected.y,expected.z};
    for(unsigned c=0;c<3;++c)check(name+"-c"+std::to_string(c),std::isfinite(actual[c])&&std::abs(actual[c]-e[c])<=std::max(tolerance,std::abs(e[c])*.003),actual[c],e[c]);}
std::string read(const fs::path& path){std::ifstream in(path,std::ios::binary);require(in.good(),"Cannot read "+path.string());return{std::istreambuf_iterator<char>(in),{}};}
void write(const fs::path& path,const std::string& text){std::ofstream out(path,std::ios::binary);require(out.good(),"Cannot write "+path.string());out<<text;}
std::string replace(std::string source,const std::string& from,const std::string& to){auto at=source.find(from);require(at!=std::string::npos,"Fault anchor absent: "+from);source.replace(at,from.size(),to);return source;}
std::string function(const std::string& source,const char* signature){auto at=source.find(signature);require(at!=std::string::npos,std::string("Missing production function: ")+signature);
    auto start=source.find('{',at);require(start!=std::string::npos,"Missing function body");unsigned depth=1;auto end=start+1;
    for(;end<source.size()&&depth;++end){if(source[end]=='{')++depth;else if(source[end]=='}')--depth;}require(!depth,"Unclosed production function");return source.substr(at,end-at)+"\n";}
std::string defines(std::string s){auto at=s.find('\n');require(at!=std::string::npos,"Missing GLSL version");s.insert(at+1,"#define TERRAIN_ARRAY 1\n#define TERRAIN_SURFACE 1\n");return s;}

// Double reference uses the microfacet BRDF definition, independent of shader
// parsing/GPU results. Diffuse stays with propagated sky/block light authority.
V brdf(V colour,double rough,double metal,V n,V view,V light){
    const double nl=std::max(0.,dot(n,light)),nv=std::max(.001,dot(n,view));
    if(nl<=0||dot(view+light,view+light)<1.e-8)return{};
    const V h=unit(view+light);const double nh=std::max(0.,dot(n,h)),vh=std::max(0.,dot(view,h));
    const double alpha=rough*rough,alpha2=alpha*alpha;
    const double denom=1+(alpha2-1)*nh*nh;
    const double ndf=alpha2/(3.14159265*denom*denom);
    const double k=std::pow(rough+1,2)/8;
    auto smith=[k](double c){return c/(c*(1-k)+k);};
    const V f0=mix({.04,.04,.04},colour,metal),f=f0+(V{1,1,1}-f0)*std::pow(1-vh,5);
    return f*(ndf*smith(nv)*smith(nl)/(4*nv*std::max(nl,.001)))*nl;
}
V normal(V sample,V face,V dx,V dy,std::array<double,2> a,std::array<double,2> b){
    const double det=a[0]*b[1]-a[1]*b[0];if(std::abs(det)<1.e-10||dot(face,face)<.5)return face;
    // Solve the 2x2 UV Jacobian for the geometric axes, then Gram-Schmidt.
    V basisU=dx*(b[1]/det)+dy*(-a[1]/det),basisV=dx*(-b[0]/det)+dy*(a[0]/det);
    basisU=basisU-face*dot(face,basisU);if(dot(basisU,basisU)<1.e-10)return face;basisU=unit(basisU);
    basisV=basisV-face*dot(face,basisV)-basisU*dot(basisU,basisV);if(dot(basisV,basisV)<1.e-10)return face;
    return unit(basisU*(sample.x*2-1)+unit(basisV)*(sample.y*2-1)+face*(sample.z*2-1));
}
struct Light{V position;double radius=12;V colour{1,.73,.42};double energy=3;};
struct State{V position{},camera{0,0,3},sun{0,0,1},sunColour{1,.92,.72};double terrainLight=1,sky=1,block=0,daylight=1,sunIntensity=1,playerExposure=-1,shadow=1;int lightCount=0;std::array<Light,8> lights{};};
V lighting(V colour,V n,V data,const State& s){
    const double rough=clamp(data.x,.2,1),metal=clamp(data.y);
    const V dv=s.camera-s.position,view=dot(dv,dv)>1.e-8?unit(dv):n;
    const double sky=clamp(s.sky),block=clamp(s.block),maximum=std::max(sky,block);
    const double ao=.24+.76*clamp(s.terrainLight/std::max(maximum,.0001));
    V indirect=decode({.80,.88,1})*(.035+.36*sky*s.daylight)+decode({1,.87,.64})*(.68*block);
    if(s.playerExposure>=0)indirect=indirect*clamp(s.playerExposure,.12,1);
    const V diffuse=colour*(1-metal);V result=diffuse*indirect*ao;
    const V sun=decode({clamp(s.sunColour.x),clamp(s.sunColour.y),clamp(s.sunColour.z)})*(std::max(s.sunIntensity,0.)*1.35);
    result=result+(diffuse*(std::max(0.,dot(n,s.sun))/3.14159265)+brdf(colour,rough,metal,n,view,s.sun))*sun*(sky*s.shadow*ao);
    for(int i=0;i<std::min(8,s.lightCount);++i){const auto& l=s.lights[std::size_t(i)];V d=l.position-s.position;double d2=dot(d,d);
        if(d2<1.e-8||l.radius<=0)continue;double fade=clamp(1-d2/(l.radius*l.radius)),atten=fade*fade/(1+d2*.25);
        result=result+brdf(colour,rough,metal,n,view,unit(d))*l.colour*(l.energy*atten*block*ao);}
    return result+colour*(clamp(data.z)*4);
}

struct Array{
    std::string bytes;std::uint64_t hash=0;std::array<std::size_t,7> offsets{};
    Pixel pixel(unsigned mip,unsigned layer,unsigned x,unsigned y)const{const auto edge=64u>>mip;const auto at=offsets[mip]+((std::size_t(layer)*edge+y)*edge+x)*4;
        Pixel p{};for(unsigned c=0;c<4;++c)p[c]=float(static_cast<unsigned char>(bytes[at+c]))/255;return p;}
};
std::uint64_t fnv(const std::string& bytes,std::size_t start){std::uint64_t h=UINT64_C(14695981039346656037);for(auto i=start;i<bytes.size();++i)h=(h^static_cast<unsigned char>(bytes[i]))*UINT64_C(1099511628211);return h;}
Array array(std::string bytes,unsigned channel){
    require(bytes.size()>=36&&bytes.size()<=24*1024*1024,"Array size bounds");
    auto u32=[&](unsigned at){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(static_cast<unsigned char>(bytes[at+i]))<<(8*i);return v;};
    require(bytes.substr(0,8)=="HMTARRAY"&&u32(8)==1,"Array header identity");
    require(u32(12)==64&&u32(16)==256&&u32(20)==7,"Array dimensions/mip coherence");
    Array a;a.bytes=std::move(bytes);std::size_t size=36;
    for(unsigned m=0;m<7;++m){a.offsets[m]=size;unsigned edge=64u>>m;size+=std::size_t(edge)*edge*256*4;}
    // u32's captured buffer was moved; read sizes explicitly from owned bytes.
    auto owned32=[&](unsigned at){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(static_cast<unsigned char>(a.bytes[at+i]))<<(8*i);return v;};
    require(a.bytes.size()==size&&owned32(24)==size-36,"Array payload length");
    for(unsigned i=0;i<8;++i)a.hash|=std::uint64_t(static_cast<unsigned char>(a.bytes[28+i]))<<(8*i);
    require(a.hash==fnv(a.bytes,36),"Array checksum");
    for(std::size_t i=36;i<a.bytes.size();i+=4){auto byte=[&](unsigned c){return static_cast<unsigned char>(a.bytes[i+c]);};
        if(channel==1){double x=double(byte(0))/127.5-1,y=double(byte(1))/127.5-1,z=double(byte(2))/127.5-1;
            require(x*x+y*y+z*z>=.96&&x*x+y*y+z*z<=1.04&&z>0&&byte(3)==255,"Normal payload validity");}
        if(channel==2)require(byte(0)>=51&&byte(3)==255,"Surface roughness/reserved alpha");}
    return a;
}
void patchHash(std::string& bytes){auto h=fnv(bytes,36);for(unsigned i=0;i<8;++i)bytes[28+i]=static_cast<char>((h>>(8*i))&255);}
void rejectArray(const std::string& name,std::string bytes,unsigned channel){bool rejected=false;try{(void)array(std::move(bytes),channel);}catch(const std::exception&){rejected=true;}check("bad-input-"+name,rejected);}

GLuint compile(GLenum type,const std::string& source){GLuint shader=glCreateShader(type);const char* text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){char log[16384]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);throw std::runtime_error(log);}return shader;}
GLuint link(const std::string& vertex,const std::string& fragment){GLuint v=compile(GL_VERTEX_SHADER,vertex),f=compile(GL_FRAGMENT_SHADER,fragment),p=glCreateProgram();
    glAttachShader(p,v);glAttachShader(p,f);glBindFragDataLocation(p,0,"fragmentColour");glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);
    GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[16384]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);glDeleteProgram(p);throw std::runtime_error(log);}return p;}
void scalar(GLuint p,const char* name,double x){glUniform1f(glGetUniformLocation(p,name),float(x));}
void integer(GLuint p,const char* name,int x){glUniform1i(glGetUniformLocation(p,name),x);}
void vec(GLuint p,const char* name,V v){glUniform3f(glGetUniformLocation(p,name),float(v.x),float(v.y),float(v.z));}
void uv(GLuint p,const char* name,std::array<double,2> v){glUniform2f(glGetUniformLocation(p,name),float(v[0]),float(v[1]));}
const float Identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
std::string extracted(const std::string& source){return std::string(R"GLSL(#version 150
in vec3 terrainWorldPosition;in float terrainLight;in vec2 terrainLightSources;out vec4 fragmentColour;
uniform float linearHdrMode,environmentLight,playerExposure,sunIntensity;
uniform vec3 cameraPosition,sunDirection,sunColour;
uniform vec4 localLightPositionRadius[8],localLightColourEnergy[8];uniform int localLightCount;
uniform sampler2DArray terrainArray,terrainNormalArray,terrainSurfaceArray;
uniform int fixtureMode,fixtureChannel;uniform float fixtureMip,fixtureLayer,fixtureRough,fixtureMetal,fixtureShadow;
uniform vec3 fixtureAlbedo,fixtureData,fixtureNormal,fixtureView,fixtureLight,fixtureFace,fixtureDx,fixtureDy;
uniform vec2 fixtureUvDx,fixtureUvDy,fixtureUv;
)GLSL")+function(source,"vec3 sceneColour(")+function(source,"vec3 mappedSurfaceNormal(")+function(source,"vec3 surfaceSpecular(")+function(source,"vec3 referenceSurfaceLighting(")+R"GLSL(
void main(){vec3 result=vec3(0.0);float alpha=1.0;
if(fixtureMode==0)result=surfaceSpecular(fixtureAlbedo,fixtureRough,fixtureMetal,fixtureNormal,fixtureView,fixtureLight);
else if(fixtureMode==1)result=mappedSurfaceNormal(fixtureData,fixtureFace,fixtureDx,fixtureDy,fixtureUvDx,fixtureUvDy);
else if(fixtureMode==2)result=referenceSurfaceLighting(fixtureAlbedo,fixtureNormal,fixtureData,fixtureShadow);
else {vec4 texel;
if(fixtureChannel==0)texel=textureLod(terrainArray,vec3(fixtureUv,fixtureLayer),fixtureMip);
else if(fixtureChannel==1)texel=textureLod(terrainNormalArray,vec3(fixtureUv,fixtureLayer),fixtureMip);
else texel=textureLod(terrainSurfaceArray,vec3(fixtureUv,fixtureLayer),fixtureMip);
result=texel.rgb;alpha=texel.a;}
fragmentColour=vec4(result,alpha);})GLSL";}
const std::string FixtureVertex=R"GLSL(#version 150
out vec3 terrainWorldPosition;out float terrainLight;out vec2 terrainLightSources;
uniform vec3 fixturePosition;uniform float fixtureLightLevel;uniform vec2 fixtureSources;
void main(){vec2 p=gl_VertexID==0?vec2(-1,-1):gl_VertexID==1?vec2(3,-1):vec2(-1,3);
gl_Position=vec4(p,0,1);terrainWorldPosition=fixturePosition;terrainLight=fixtureLightLevel;terrainLightSources=fixtureSources;})GLSL";
struct GPU {
    GLuint fbo=0,output=0,shadowMap=0,vao=0,vbo=0;std::array<GLuint,3> textures{};
    GPU(const std::array<Array,3>& arrays){
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&output);glBindTexture(GL_TEXTURE_2D,output);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,32,32,0,GL_RGBA,GL_FLOAT,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,output,0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"RGBA32F test target incomplete");
        glGenTextures(1,&shadowMap);glBindTexture(GL_TEXTURE_2D,shadowMap);const float blockedDepth=.3f;
        glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&blockedDepth);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glGenTextures(3,textures.data());for(unsigned c=0;c<3;++c){glBindTexture(GL_TEXTURE_2D_ARRAY,textures[c]);
            for(unsigned m=0;m<7;++m){unsigned edge=64u>>m;glTexImage3D(GL_TEXTURE_2D_ARRAY,GLint(m),c==0?GL_SRGB8_ALPHA8:GL_RGBA8,GLsizei(edge),GLsizei(edge),256,0,GL_RGBA,GL_UNSIGNED_BYTE,arrays[c].bytes.data()+arrays[c].offsets[m]);}
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAX_LEVEL,6);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_REPEAT);}
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        const float mesh[]={-1,-1,0,.49f,.49f, 3,-1,0,.53f,.49f, -1,3,0,.49f,.53f};glBufferData(GL_ARRAY_BUFFER,sizeof(mesh),mesh,GL_STATIC_DRAW);
    }
    ~GPU(){glDeleteBuffers(1,&vbo);glDeleteVertexArrays(1,&vao);glDeleteTextures(3,textures.data());glDeleteTextures(1,&output);glDeleteTextures(1,&shadowMap);glDeleteFramebuffers(1,&fbo);}
    void begin(GLuint p){glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,32,32);glDisable(GL_FRAMEBUFFER_SRGB);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glUseProgram(p);glBindVertexArray(vao);for(unsigned c=0;c<3;++c){glActiveTexture(GL_TEXTURE0+c);glBindTexture(GL_TEXTURE_2D_ARRAY,textures[c]);}
        glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,shadowMap);integer(p,"directionalShadowMap",3);
        integer(p,"terrainArray",0);integer(p,"terrainNormalArray",1);integer(p,"terrainSurfaceArray",2);scalar(p,"linearHdrMode",1);}
    Pixel finish(){glDrawArrays(GL_TRIANGLES,0,3);Pixel p{};glReadPixels(16,16,1,1,GL_RGBA,GL_FLOAT,p.data());return p;}
    void state(GLuint p,const State& s){vec(p,"fixturePosition",s.position);scalar(p,"fixtureLightLevel",s.terrainLight);uv(p,"fixtureSources",{s.sky,s.block});
        vec(p,"cameraPosition",s.camera);vec(p,"sunDirection",s.sun);vec(p,"sunColour",s.sunColour);scalar(p,"sunIntensity",s.sunIntensity);scalar(p,"environmentLight",s.daylight);scalar(p,"playerExposure",s.playerExposure);scalar(p,"fixtureShadow",s.shadow);
        integer(p,"localLightCount",s.lightCount);std::array<float,32> positions{},colours{};
        for(unsigned i=0;i<8;++i){const auto& l=s.lights[i];positions[i*4]=float(l.position.x);positions[i*4+1]=float(l.position.y);positions[i*4+2]=float(l.position.z);positions[i*4+3]=float(l.radius);
            colours[i*4]=float(l.colour.x);colours[i*4+1]=float(l.colour.y);colours[i*4+2]=float(l.colour.z);colours[i*4+3]=float(l.energy);}
        glUniform4fv(glGetUniformLocation(p,"localLightPositionRadius"),8,positions.data());glUniform4fv(glGetUniformLocation(p,"localLightColourEnergy"),8,colours.data());}
    Pixel specular(GLuint p,V colour,double rough,double metal,V n,V view,V light){begin(p);integer(p,"fixtureMode",0);vec(p,"fixtureAlbedo",colour);scalar(p,"fixtureRough",rough);scalar(p,"fixtureMetal",metal);vec(p,"fixtureNormal",n);vec(p,"fixtureView",view);vec(p,"fixtureLight",light);return finish();}
    Pixel mapped(GLuint p,V data,V face,V dx,V dy,std::array<double,2> a,std::array<double,2> b){begin(p);integer(p,"fixtureMode",1);vec(p,"fixtureData",data);vec(p,"fixtureFace",face);vec(p,"fixtureDx",dx);vec(p,"fixtureDy",dy);uv(p,"fixtureUvDx",a);uv(p,"fixtureUvDy",b);return finish();}
    Pixel lit(GLuint p,V colour,V n,V data,const State& s){begin(p);state(p,s);integer(p,"fixtureMode",2);vec(p,"fixtureAlbedo",colour);vec(p,"fixtureNormal",n);vec(p,"fixtureData",data);return finish();}
    Pixel texel(GLuint p,unsigned channel,unsigned layer,unsigned mip,unsigned x,unsigned y){begin(p);integer(p,"fixtureMode",3);integer(p,"fixtureChannel",int(channel));scalar(p,"fixtureLayer",layer);scalar(p,"fixtureMip",mip);unsigned edge=64u>>mip;uv(p,"fixtureUv",{(x+.5)/edge,(y+.5)/edge});return finish();}
    Pixel whole(GLuint p,unsigned layer,unsigned mip,int q,V origin,State s,bool shadowed=false){begin(p);
        for(unsigned c=0;c<3;++c){glActiveTexture(GL_TEXTURE0+c);glBindTexture(GL_TEXTURE_2D_ARRAY,textures[c]);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_BASE_LEVEL,GLint(mip));glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAX_LEVEL,GLint(mip));}
        for(GLuint a=0;a<8;++a)glDisableVertexAttribArray(a);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);auto position=glGetAttribLocation(p,"vertex"),repeat=glGetAttribLocation(p,"uv1");
        require(position>=0&&repeat>=0,"Whole production vertex interface missing");glEnableVertexAttribArray(GLuint(position));glVertexAttribPointer(GLuint(position),3,GL_FLOAT,GL_FALSE,5*sizeof(float),nullptr);
        glEnableVertexAttribArray(GLuint(repeat));glVertexAttribPointer(GLuint(repeat),2,GL_FLOAT,GL_FALSE,5*sizeof(float),reinterpret_cast<const void*>(3*sizeof(float)));
        auto tile=glGetAttribLocation(p,"uv0"),light=glGetAttribLocation(p,"uv2"),root=glGetAttribLocation(p,"uv3");
        if(tile>=0)glVertexAttrib2f(GLuint(tile),float(layer%16+.25)/16,float(layer/16+.5)/16);
        if(light>=0)glVertexAttrib3f(GLuint(light),float(s.terrainLight),float(s.sky),float(s.block+1));if(root>=0)glVertexAttrib1f(GLuint(root),0);
        float world[16];std::copy(std::begin(Identity),std::end(Identity),world);V x=yaw({1,0,0},q),z=yaw({0,0,1},q);
        world[0]=float(x.x);world[2]=float(x.z);world[8]=float(z.x);world[10]=float(z.z);world[12]=float(origin.x);world[13]=float(origin.y);world[14]=float(origin.z);
        glUniformMatrix4fv(glGetUniformLocation(p,"world"),1,GL_FALSE,world);glUniformMatrix4fv(glGetUniformLocation(p,"worldViewProj"),1,GL_FALSE,Identity);glUniformMatrix4fv(glGetUniformLocation(p,"worldView"),1,GL_FALSE,Identity);
        glUniformMatrix4fv(glGetUniformLocation(p,"shadowWorldViewProj"),1,GL_FALSE,Identity);glUniformMatrix4fv(glGetUniformLocation(p,"directionalShadowViewProj"),1,GL_FALSE,Identity);
        state(p,s);integer(p,"directionalShadowMap",3);scalar(p,"directionalShadowEnabled",shadowed?1:0);scalar(p,"directionalShadowStrength",shadowed?.34:0);
        scalar(p,"directionalShadowBias",.003);scalar(p,"directionalShadowFadeStart",72);scalar(p,"directionalShadowFadeEnd",96);
        scalar(p,"tilesPerRow",16);scalar(p,"colourSaturation",1);scalar(p,"toneGamma",1);scalar(p,"surfaceLightingStrength",0);scalar(p,"fogDensity",0);scalar(p,"alphaCutoff",.01);
        Pixel result=finish();
        for(GLuint a=0;a<8;++a)glDisableVertexAttribArray(a);
        for(unsigned c=0;c<3;++c){glActiveTexture(GL_TEXTURE0+c);glBindTexture(GL_TEXTURE_2D_ARRAY,textures[c]);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAX_LEVEL,6);}
        return result;
    }
};
V rgb(Pixel p){return{p[0],p[1],p[2]};}
double error(Pixel p,V v){return std::max({std::abs(p[0]-v.x),std::abs(p[1]-v.y),std::abs(p[2]-v.z)});}
void suite(GPU& gpu,GLuint p,const std::string& name,const std::array<Array,3>& arrays){
    unsigned sequence=0;
    for(double rough:{.2,.35,.6,.9,1.})for(double metal:{0.,.5,1.})for(V view:std::array<V,3>{{{0,0,1},unit({1,0,1}),unit({2,1,.2})}})for(V light:std::array<V,3>{{{0,0,1},unit({-1,1,2}),unit({1,0,-.5})}}){
        V colour{.24,.47,.71},n{0,0,1};close(name+"-brdf-"+std::to_string(sequence++),gpu.specular(p,colour,rough,metal,n,view,light),brdf(colour,rough,metal,n,view,light));}
    close(name+"-opposed-light-view",gpu.specular(p,{.2,.5,.8},.4,.2,{0,0,1},{0,0,-1},{0,0,1}),{});
    for(int q=0;q<4;++q)for(bool mirror:{false,true})for(V data:std::array<V,3>{{{.5,.5,1},{.7,.4,.95},{.3,.65,.93}}}){
        V face=yaw({0,0,1},q),dx=yaw({1,0,0},q),dy{0,1,0};std::array<double,2>a{mirror?-1.:1.,0},b{0,1};
        close(name+"-normal-yaw"+std::to_string(q)+"-mirror"+std::to_string(mirror)+"-"+std::to_string(sequence++),gpu.mapped(p,data,face,dx,dy,a,b),normal(data,face,dx,dy,a,b));}
    close(name+"-degenerate-uv",gpu.mapped(p,{.7,.3,.9},{0,0,1},{1,0,0},{0,1,0},{0,0},{0,0}),{0,0,1});
    close(name+"-degenerate-world-u",gpu.mapped(p,{.7,.3,.9},{0,0,1},{0,0,0},{0,1,0},{1,0},{0,1}),{0,0,1});
    close(name+"-degenerate-face",gpu.mapped(p,{.7,.3,.9},{0,0,0},{1,0,0},{0,1,0},{1,0},{0,1}),{});
    State s;for(unsigned i=0;i<8;++i)s.lights[i].position={double(i)*.3-.8,.2,2+double(i)*.2};
    for(double rough:{0.,.2,.5,1.,2.})for(double metal:{-.5,0.,1.,2.})for(double emission:{-.3,0.,.5,1.,2.}){
        V c{.23,.5,.8},n{0,0,1},data{rough,metal,emission};close(name+"-rme-clamp-"+std::to_string(sequence++),gpu.lit(p,c,n,data,s),lighting(c,n,data,s));}
    for(int count:{-1,0,1,7,8,9,255})for(double block:{0.,.7,1.})for(double shadow:{0.,.4,1.}){
        s.lightCount=count;s.block=block;s.shadow=shadow;V c{.4,.3,.2},n{0,0,1},data{.25,.5,.1};close(name+"-lights-cap-propagation-shadow-"+std::to_string(sequence++),gpu.lit(p,c,n,data,s),lighting(c,n,data,s));}
    s.lightCount=8;s.block=1;s.shadow=1;s.lights[0].position=s.position;s.lights[1].radius=0;s.lights[2].radius=-1;s.lights[3].position={0,0,40};
    close(name+"-invalid-light-distance-radius",gpu.lit(p,{.2,.3,.4},{0,0,1},{.4,.2,0},s),lighting({.2,.3,.4},{0,0,1},{.4,.2,0},s));
    for(unsigned layer:{3u,21u,145u,146u,147u,148u,149u,150u})for(unsigned mip=0;mip<7;++mip)for(unsigned channel=0;channel<3;++channel){
        unsigned edge=64u>>mip,x=edge/2,y=edge/2;auto expected=arrays[channel].pixel(mip,layer,x,y);V value=channel==0?decode(rgb(expected)):rgb(expected);
        auto result=gpu.texel(p,channel,layer,mip,x,y);close(name+"-channel"+std::to_string(channel)+"-layer"+std::to_string(layer)+"-mip"+std::to_string(mip),result,value,.002);
        check(name+"-raw-alpha-layer"+std::to_string(layer)+"-mip"+std::to_string(mip)+"-c"+std::to_string(channel),std::abs(result[3]-expected[3])<.00001,result[3],expected[3]);}
}
}
int main(int argc,char** argv){CGLContextObj context=nullptr;CGLPixelFormatObj format=nullptr;
    try{
        require(argc==3,"Usage: validator <repo-or-resources-root> <new-evidence-dir>");fs::path root(argv[1]),evidence(argv[2]);require(!fs::exists(evidence),"Evidence directory exists");fs::create_directories(evidence);samples.open(evidence/"samples.tsv");
        const std::array<const char*,3> names{"ReferenceColour64.hmt","ReferenceNormal64.hmt","ReferenceSurface64.hmt"};std::array<Array,3> arrays;
        std::ofstream identities(evidence/"inputs.txt");for(unsigned c=0;c<3;++c){arrays[c]=array(read(root/"media/textures"/names[c]),c);identities<<names[c]<<" bytes="<<arrays[c].bytes.size()<<" fnv64="<<std::hex<<arrays[c].hash<<std::dec<<'\n';}
        std::string broken=arrays[0].bytes;broken[20]=6;rejectArray("missing-mip",broken,0);broken=arrays[0].bytes;broken.pop_back();rejectArray("truncated",broken,0);
        broken=arrays[0].bytes;broken[40]^=1;rejectArray("checksum",broken,0);broken=arrays[1].bytes;broken[36]=broken[37]=broken[38]=char(128);patchHash(broken);rejectArray("zero-normal",broken,1);
        broken=arrays[1].bytes;broken[38]=0;patchHash(broken);rejectArray("negative-normal-z",broken,1);broken=arrays[2].bytes;broken[36]=0;patchHash(broken);rejectArray("roughness-below-point-two",broken,2);
        broken=arrays[2].bytes;broken[39]=0;patchHash(broken);rejectArray("reserved-alpha",broken,2);
        std::array<std::string,2> fragments{read(root/"media/ogre/HelloMine3DTerrain.frag"),read(root/"media/ogre/HelloMine3DTerrainShadow.frag")};
        std::array<std::string,2> vertices{read(root/"media/ogre/HelloMine3DTerrain.vert"),read(root/"media/ogre/HelloMine3DTerrainShadow.vert")};
        for(unsigned i=0;i<2;++i){write(evidence/(i?"TerrainShadow.frag":"Terrain.frag"),fragments[i]);write(evidence/(i?"TerrainShadow.vert":"Terrain.vert"),vertices[i]);}
        CGLPixelFormatAttribute attrs[]={kCGLPFAAccelerated,kCGLPFAOpenGLProfile,static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),static_cast<CGLPixelFormatAttribute>(0)};
        GLint count=0;require(CGLChoosePixelFormat(attrs,&format,&count)==kCGLNoError&&format,"CGL format unavailable");require(CGLCreateContext(format,nullptr,&context)==kCGLNoError&&context,"CGL context unavailable");
        CGLDestroyPixelFormat(format);format=nullptr;require(CGLSetCurrentContext(context)==kCGLNoError,"CGL current failed");
        write(evidence/"driver.txt",std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR)))+"\n"+
            reinterpret_cast<const char*>(glGetString(GL_RENDERER))+"\n"+
            reinterpret_cast<const char*>(glGetString(GL_VERSION))+"\n");
        {
            GPU gpu(arrays);std::array<GLuint,2> extractedPrograms{},wholePrograms{};
            for(unsigned variant=0;variant<2;++variant){const std::string name=variant?"ShadowSurface":"TerrainSurface";
                wholePrograms[variant]=link(vertices[variant],defines(fragments[variant]));check(name+"-whole-production-compile-link",wholePrograms[variant]!=0);
                extractedPrograms[variant]=link(FixtureVertex,extracted(fragments[variant]));suite(gpu,extractedPrograms[variant],name,arrays);
                for(int q=0;q<4;++q)for(unsigned mip:{0u,3u,6u})for(V origin:std::array<V,2>{{{}, {1000000,1000000,-1000000}}}){
                    const unsigned layer=145;State s;s.position=origin+yaw({.03125,.03125,0},q);s.camera=s.position+yaw({0,0,3},q);s.sun=yaw({0,0,1},q);
                    const unsigned edge=64u>>mip,x=edge/2,y=edge/2;V albedo=decode(rgb(arrays[0].pixel(mip,layer,x,y))),data=rgb(arrays[2].pixel(mip,layer,x,y));
                    V n=normal(rgb(arrays[1].pixel(mip,layer,x,y)),yaw({0,0,1},q),yaw({1,0,0},q),{0,1,0},{1,0},{0,1});
                    close(name+"-whole-real-mips-yaw"+std::to_string(q)+"-mip"+std::to_string(mip)+"-origin"+std::to_string(int(origin.x)),gpu.whole(wholePrograms[variant],layer,mip,q,origin,s),lighting(albedo,n,data,s),.004);
                }
            }
            for(unsigned layer:{148u,149u}){
                State materialState;materialState.position={.03125,.03125,0};materialState.camera=materialState.position+V{0,0,3};
                const V albedo=decode(rgb(arrays[0].pixel(6,layer,0,0))),rme=rgb(arrays[2].pixel(6,layer,0,0));
                auto actual=gpu.whole(wholePrograms[0],layer,6,0,{},materialState);
                close("whole-real-metal-or-emission-layer"+std::to_string(layer),actual,lighting(albedo,{0,0,1},rme,materialState),.004);
                if(layer==149)check("whole-emission-radiance-over-one",std::max({actual[0],actual[1],actual[2]})>1, std::max({actual[0],actual[1],actual[2]}),1);
            }
            State shadowState;shadowState.position={.03125,.03125,0};shadowState.camera=shadowState.position+V{0,0,3};
            const V shadowAlbedo=decode(rgb(arrays[0].pixel(6,145,0,0))),shadowData=rgb(arrays[2].pixel(6,145,0,0));
            shadowState.shadow=0;
            close("ShadowSurface-whole-real-shadow-blocks-sun",gpu.whole(wholePrograms[1],145,6,0,{},shadowState,true),lighting(shadowAlbedo,{0,0,1},shadowData,shadowState),.004);
            shadowState.sky=0;shadowState.block=1;
            close("ShadowSurface-whole-shadow-keeps-propagated-block-light",gpu.whole(wholePrograms[1],145,6,0,{},shadowState,true),lighting(shadowAlbedo,{0,0,1},shadowData,shadowState),.004);
            for(unsigned c=0;c<3;++c){glActiveTexture(GL_TEXTURE0+c);glBindTexture(GL_TEXTURE_2D_ARRAY,gpu.textures[c]);
                GLint internal=0;glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,0,GL_TEXTURE_INTERNAL_FORMAT,&internal);
                check("actual-internal-channel"+std::to_string(c),internal==(c==0?GL_SRGB8_ALPHA8:GL_RGBA8),internal,c==0?GL_SRGB8_ALPHA8:GL_RGBA8);
                for(unsigned m=0;m<7;++m){GLint w=0,h=0,d=0;glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,GLint(m),GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,GLint(m),GL_TEXTURE_HEIGHT,&h);glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY,GLint(m),GL_TEXTURE_DEPTH,&d);
                    check("actual-storage-channel"+std::to_string(c)+"-mip"+std::to_string(m),w==int(64u>>m)&&h==w&&d==256);}}
            GLuint fault=link(FixtureVertex,extracted(replace(fragments[0],"vec3 n=normalData*2.0-1.0;","vec3 n=normalData*2.0-1.0; n.x=-n.x;")));
            V expectedNormal=normal({.7,.4,.95},{0,0,1},{1,0,0},{0,1,0},{1,0},{0,1});check("fault-normal-direction-rejected",error(gpu.mapped(fault,{.7,.4,.95},{0,0,1},{1,0,0},{0,1,0},{1,0},{0,1}),expectedNormal)>.5);glDeleteProgram(fault);
            fault=link(FixtureVertex,extracted(replace(fragments[0],"roughness=clamp(data.r,.2,1.0)","roughness=.2")));State s;V c{.3,.4,.5},data{.9,0,0};
            check("fault-roughness-channel-rejected",error(gpu.lit(fault,c,{0,0,1},data,s),lighting(c,{0,0,1},data,s))>.3);glDeleteProgram(fault);
            fault=link(FixtureVertex,extracted(replace(fragments[0],"i<8","i<7")));s.lightCount=8;s.block=1;s.sky=0;s.sunIntensity=0;for(unsigned i=0;i<8;++i)s.lights[i].position={0,0,2};
            check("fault-light-cap-rejected",error(gpu.lit(fault,c,{0,0,1},{.2,0,0},s),lighting(c,{0,0,1},{.2,0,0},s))>.3);glDeleteProgram(fault);
            fault=link(vertices[0],defines(replace(fragments[0],"tileIndex.y==9.0?rawLinearAlbedo:balancedColour","tileIndex.y==9.0?sceneColour(rawLinearAlbedo):balancedColour")));
            s=State{};s.position={.03125,.03125,0};s.camera=s.position+V{0,0,3};s.sunIntensity=0;
            const auto linear=decode(rgb(arrays[0].pixel(6,145,0,0))),rme=rgb(arrays[2].pixel(6,145,0,0));
            const auto target=lighting(linear,{0,0,1},rme,s);const auto bad=gpu.whole(fault,145,6,0,{},s);
            check("fault-hardware-srgb-double-decode-rejected",error(bad,target)>.025,error(bad,target),.025);glDeleteProgram(fault);
            // Calibrate the actual texture-domain boundary as well as a bad
            // shader: gamma bytes uploaded to RGBA8 must fail the same oracle.
            glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D_ARRAY,gpu.textures[0]);
            for(unsigned m=0;m<7;++m){const unsigned edge=64u>>m;glTexImage3D(GL_TEXTURE_2D_ARRAY,GLint(m),GL_RGBA8,GLsizei(edge),GLsizei(edge),256,0,GL_RGBA,GL_UNSIGNED_BYTE,arrays[0].bytes.data()+arrays[0].offsets[m]);}
            const auto missingDecode=gpu.whole(wholePrograms[0],145,6,0,{},s);
            check("fault-missing-hardware-srgb-storage-rejected",error(missingDecode,target)>.025,error(missingDecode,target),.025);
            for(auto p:extractedPrograms)glDeleteProgram(p);for(auto p:wholePrograms)glDeleteProgram(p);
            check("no-gl-errors",glGetError()==GL_NO_ERROR);
        }
        std::cout<<"[REFERENCE_SURFACE_GPU] checks="<<checks<<" failures="<<failures<<" scope=production-GLSL-and-real-channel-mips-CGL\n";
        write(evidence/"result.txt","checks="+std::to_string(checks)+" failures="+std::to_string(failures)+"\nscope=production-GLSL-and-real-channel-mips-CGL\nnative-Ogre-material-binding=NOT_RUN normal-gameplay=NOT_RUN local-light-World-producer=NOT_RUN\n");
        CGLSetCurrentContext(nullptr);CGLDestroyContext(context);context=nullptr;return failures?1:0;
    }catch(const std::exception& e){if(context){CGLSetCurrentContext(nullptr);CGLDestroyContext(context);}if(format)CGLDestroyPixelFormat(format);std::cerr<<"[REFERENCE_SURFACE_GPU] ERROR "<<e.what()<<'\n';return 2;}
}
