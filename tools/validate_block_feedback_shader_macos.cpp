// Execute production shaders and textures; no CPU copy of the fracture algorithm.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_block_feedback_shader_macos.cpp \
//   -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation -o /tmp/block-feedback-gpu
// /tmp/block-feedback-gpu <repository-or-package-resources> <new-output-directory>
// /tmp/block-feedback-gpu <root> <new-output-directory> --species-leaves <frozen-fragment-directory>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "../src/HelloMine3D/World/Block/TerrainTextureArray.h"

namespace {
constexpr int Edge = 256;
using Pixels = std::vector<unsigned char>;
void require(bool condition, const std::string &message)
{
    if (!condition) throw std::runtime_error(message);
}
std::string read(const std::filesystem::path &path)
{
    std::ifstream stream(path);
    require(stream.good(), "Missing shader " + path.string());
    return {std::istreambuf_iterator<char>(stream), {}};
}
GLuint program(const std::filesystem::path &root, const char *vertex, const char *fragment, bool array, bool unfilteredGeology = false,
               const std::filesystem::path &fragmentDirectory = {})
{
    GLuint result = glCreateProgram();
    for (const auto &entry : {std::pair<GLenum,const char *>{GL_VERTEX_SHADER,vertex},
                              {GL_FRAGMENT_SHADER,fragment}})
    {
        auto source = read(entry.first == GL_FRAGMENT_SHADER && !fragmentDirectory.empty()
            ? fragmentDirectory / entry.second : root / "media/ogre" / entry.second);
        if (array && entry.first == GL_FRAGMENT_SHADER)
            source.insert(source.find('\n') + 1, "#define TERRAIN_ARRAY 1\n");
        if (unfilteredGeology && entry.first == GL_FRAGMENT_SHADER)
        {
            // Fault injection: retain the real production colour algorithm but
            // remove only its footprint attenuation.
            for (const auto *line : {
                "float resolved = 1.0 - smoothstep(0.20, 0.65, footprint * 0.31);",
                "float resolved = 1.0 - smoothstep(0.10, 0.32, footprint * 0.84);"})
            {
                const auto at = source.find(line);
                require(at != std::string::npos, "Geology negative target missing");
                source.replace(at, std::string(line).size(), "float resolved = 1.0;");
            }
        }
        auto shader = glCreateShader(entry.first);
        const char *text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        char log[8192]{};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        require(ok, std::string(entry.second) + ": " + log);
        glAttachShader(result, shader);
        glDeleteShader(shader);
    }
    glLinkProgram(result);
    GLint ok = 0;
    char log[8192]{};
    glGetProgramiv(result, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(result, sizeof(log), nullptr, log);
    require(ok, std::string(fragment) + " link: " + log);
    return result;
}
void value(GLuint p, const char *name, float v) { glUniform1f(glGetUniformLocation(p,name),v); }
void setup(GLuint p)
{
    glUseProgram(p);
    constexpr float identity[]{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    for (const char *name : {"worldViewProj","worldView","world"})
        glUniformMatrix4fv(glGetUniformLocation(p,name),1,GL_FALSE,identity);
    for (const char *name : {"terrainAtlas","terrainArray"})
        glUniform1i(glGetUniformLocation(p,name),0);
    value(p,"atlasPixels",256); value(p,"tilePixels",16); value(p,"tilesPerRow",16);
    value(p,"alphaCutoff",0.4999f); value(p,"highlightStrength",0.27f);
    value(p,"colourSaturation",1); value(p,"toneGamma",1); value(p,"environmentLight",1);
    value(p,"playerExposure",-1);
    value(p,"directionalShadowEnabled",0); value(p,"directionalShadowStrength",0);
    value(p,"globalTime",1.23f); value(p,"fogDensity",0);
    glUniform2f(glGetUniformLocation(p,"crackSeed"),17.0f,31.0f);
    glUniform2f(glGetUniformLocation(p,"crackStretch"),1,1);
}
void quad(GLuint p, int tileX, int tileY, float vertexLight = 1.f, float skySource = -1.f, float blockSource = -1.f, float rootTag = 0.f)
{
    // All geometry reaches the production vertex shader, including plant wind.
    struct Vertex { float x,y,z,u,v; };
    constexpr Vertex vertices[]{
        {-1,-1,0,0,1},{1,-1,0,1,1},{1,1,0,1,0},
        {1,1,0,1,0},{-1,1,0,0,0},{-1,-1,0,0,1}};
    GLuint vbo;
    glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);
    GLint position = glGetAttribLocation(p,"vertex"), repeat = glGetAttribLocation(p,"uv1");
    glEnableVertexAttribArray(position);
    glVertexAttribPointer(position,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    if (repeat >= 0)
    {
        glEnableVertexAttribArray(repeat);
        glVertexAttribPointer(repeat,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void *>(12));
        if (rootTag > 0.f) {
            // Natural-tree coverage tests use an opaque constant texel and
            // root UV V=1, so plant wind cannot alter the comparison geometry.
            glDisableVertexAttribArray(repeat); glVertexAttrib2f(repeat,.37f,1.f);
        }
    }
    GLint tile = glGetAttribLocation(p,"uv0"), light = glGetAttribLocation(p,"uv2");
    GLint colour = glGetAttribLocation(p,"colour");
    GLint root = glGetAttribLocation(p,"uv3");
    if (root >= 0) { glDisableVertexAttribArray(root); glVertexAttrib1f(root,rootTag); }
    if (tile >= 0) glVertexAttrib2f(tile,(tileX+0.5f)/16.f,(tileY+0.5f)/16.f);
    if (light >= 0) glVertexAttrib3f(light,vertexLight,skySource,blockSource+1.f);
    if (colour >= 0) glVertexAttrib4f(colour,0.9f,0.9f,0.9f,0.65f);
    glDrawArrays(GL_TRIANGLES,0,6);
    glDisableVertexAttribArray(position);
    if (repeat >= 0) glDisableVertexAttribArray(repeat);
    glDeleteBuffers(1,&vbo);
}
Pixels render(GLuint base, GLuint overlay, int stage, int tileX, bool occluded = false)
{
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
    glClearColor(0.12f,0.16f,0.22f,1); glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setup(base); quad(base,tileX,0);
    if (overlay)
    {
        if (occluded) { glClearDepth(0.1); glClear(GL_DEPTH_BUFFER_BIT); }
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        setup(overlay); value(overlay,"crackStage",static_cast<float>(stage));
        quad(overlay,tileX,0);
    }
    Pixels pixels(Edge*Edge*4);
    glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR,"OpenGL draw/readback failure");
    return pixels;
}
Pixels renderGround(GLuint shader, float enabled, float offset, int tile = 0,
                    int tileY = 0, float daylight = 1.f,
                    float playerExposure = -1.f, float vertexLight = 1.f,
                    float shadowStrength = 0.f, float fog = 0.f,
                    float alphaCutoff = .4999f, float skySource = -1.f, float blockSource = -1.f, bool rangeFade = false, float rangeCentre = 0.f, float rangeStrength = 1.f, float rootTag = 0.f, bool minimumRange = false, int centreAxis = 0)
{
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
    glClearColor(0,0,0,0); glClearDepth(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setup(shader);
    glUniform2f(glGetUniformLocation(shader,"viewRange"),rangeFade?(minimumRange?7.f:118.f):0.f,rangeFade?(minimumRange?14.f:126.f):0.f);
    glUniform2f(glGetUniformLocation(shader,"viewRangeCentre"),centreAxis==2?0.f:rangeCentre,centreAxis==1?0.f:rangeCentre);
    value(shader,"viewRangeStrength",rangeStrength);
    value(shader, "playerExposure", playerExposure);
    value(shader, "alphaCutoff", alphaCutoff);
    value(shader, "fogDensity", fog);
    value(shader, "directionalShadowEnabled", shadowStrength > 0 ? 1.f : 0.f);
    value(shader, "directionalShadowStrength", shadowStrength);
    value(shader, "directionalShadowBias", .003f);
    value(shader, "directionalShadowFadeStart", 72.f);
    value(shader, "directionalShadowFadeEnd", 96.f);
    const float shadowProjection[]{0,0,0,0, 0,0,0,0, 0,0,0,0, .5f,.5f,0,1};
    glUniformMatrix4fv(glGetUniformLocation(shader,"shadowWorldViewProj"),1,GL_FALSE,shadowProjection);
    glUniformMatrix4fv(glGetUniformLocation(shader,"directionalShadowViewProj"),1,GL_FALSE,shadowProjection);
    value(shader, "surfaceLightingStrength", enabled);
    value(shader, "environmentLight", daylight);
    glUniform1i(glGetUniformLocation(shader, "directionalShadowMap"), 1);
    const float world[]{1,0,0,0, 0,1,0,0, 0,0,1,0, offset,0,offset,1};
    glUniformMatrix4fv(glGetUniformLocation(shader,"world"),1,GL_FALSE,world);
    quad(shader, tile, tileY, vertexLight, skySource, blockSource, rootTag);
    Pixels pixels(Edge * Edge * 4);
    glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError() == GL_NO_ERROR, "Ground shader draw failed");
    return pixels;
}
// Constant UV removes image texel variation from the geology measurement;
// positions still run through both complete production vertex/fragment stages.
Pixels renderGeology(GLuint shader, int tileX, int tileY, bool top,
                     float span = 8.f, float originX = -24.f,
                     float originY = 72.f, float originZ = 16.f,
                     float enabled = 1.f, float localOrigin = 0.f,
                     float time = 1.23f, float daylight = 1.f,
                     float warmth = .5f, float forest = 0.f,
                     float sampleU = .37f, float sampleV = .43f)
{
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); setup(shader);
    value(shader,"surfaceLightingStrength",enabled);
    value(shader,"globalTime",time); value(shader,"environmentLight",daylight);
    glUniform1i(glGetUniformLocation(shader,"directionalShadowMap"),1);
    const float shift = localOrigin * span;
    const std::array<float,16> world = top
        ? std::array<float,16>{span,0,0,0, 0,0,-span,0, 0,span,0,0,
                              originX-shift,originY-shift,originZ+shift,1}
        : std::array<float,16>{span,0,0,0, 0,span,0,0, 0,0,span,0,
                              originX-shift,originY-shift,originZ-shift,1};
    const float clip[]{1,0,0,0, 0,1,0,0, 0,0,1,0,
                       -localOrigin,-localOrigin,-localOrigin,1};
    glUniformMatrix4fv(glGetUniformLocation(shader,"world"),1,GL_FALSE,world.data());
    glUniformMatrix4fv(glGetUniformLocation(shader,"worldViewProj"),1,GL_FALSE,clip);
    std::array<float,18> points{-1,-1,0, 1,-1,0, 1,1,0, 1,1,0, -1,1,0, -1,-1,0};
    for (auto &v : points) v += localOrigin;
    GLuint vbo; glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(points),points.data(),GL_STATIC_DRAW);
    const auto vertex = glGetAttribLocation(shader,"vertex");
    glEnableVertexAttribArray(vertex); glVertexAttribPointer(vertex,3,GL_FLOAT,GL_FALSE,0,nullptr);
    for (const auto *name : {"uv0","uv1","uv2","uv3"})
    {
        const auto a = glGetAttribLocation(shader,name); if (a >= 0) glDisableVertexAttribArray(a);
    }
    glVertexAttrib2f(glGetAttribLocation(shader,"uv0"),
        (tileX+.25f+.5f*warmth)/16.f,(tileY+.5f+.25f*forest)/16.f);
    glVertexAttrib2f(glGetAttribLocation(shader,"uv1"),sampleU,sampleV);
    glVertexAttrib1f(glGetAttribLocation(shader,"uv2"),1.f);
    const auto rootAttribute = glGetAttribLocation(shader,"uv3");
    if (rootAttribute >= 0) glVertexAttrib1f(rootAttribute,0.f);
    glDrawArrays(GL_TRIANGLES,0,6); glDisableVertexAttribArray(vertex); glDeleteBuffers(1,&vbo);
    Pixels pixels(Edge*Edge*4); glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR,"Geology draw/readback failed"); return pixels;
}
bool scaledExposure(const Pixels &bright, const Pixels &dim, float ratio)
{
    for (std::size_t i=0;i<bright.size();++i)
        if (i%4==3 ? bright[i]!=dim[i] : std::abs(bright[i]*ratio-dim[i])>1.f)
            return false;
    return true;
}
float colourDifference(const Pixels &a, const Pixels &b)
{
    double sum=0;
    for (std::size_t p=0;p<a.size();p+=4)
        for (int c=0;c<3;++c) sum+=std::abs(int(a[p+c])-int(b[p+c]));
    return static_cast<float>(sum/(Edge*Edge*3));
}
float pixelGradient(const Pixels &pixels)
{
    double sum=0;
    for (int y=1;y<Edge;++y) for (int x=1;x<Edge;++x) for (int c=0;c<3;++c)
    {
        const int p=(y*Edge+x)*4+c;
        sum+=std::abs(int(pixels[p])-int(pixels[p-4]));
        sum+=std::abs(int(pixels[p])-int(pixels[p-Edge*4]));
    }
    return static_cast<float>(sum/((Edge-1)*(Edge-1)*6));
}
void png(const std::filesystem::path &path, const Pixels &pixels)
{
    Pixels flipped(pixels.size());
    for (int y=0;y<Edge;++y)
        std::copy_n(pixels.data()+y*Edge*4,Edge*4,flipped.data()+(Edge-1-y)*Edge*4);
    auto colour = CGColorSpaceCreateDeviceRGB();
    auto provider = CGDataProviderCreateWithData(nullptr,flipped.data(),flipped.size(),nullptr);
    auto image = CGImageCreate(Edge,Edge,8,32,Edge*4,colour,
        kCGBitmapByteOrder32Big | kCGImageAlphaLast,provider,nullptr,false,kCGRenderingIntentDefault);
    auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
        reinterpret_cast<const UInt8 *>(path.c_str()),path.string().size(),false);
    auto destination = CGImageDestinationCreateWithURL(url,CFSTR("public.png"),1,nullptr);
    require(destination && image,"Cannot create PNG output");
    CGImageDestinationAddImage(destination,image,nullptr);
    require(CGImageDestinationFinalize(destination),"Cannot encode PNG");
    CFRelease(destination); CFRelease(url); CGImageRelease(image);
    CGDataProviderRelease(provider); CGColorSpaceRelease(colour);
}
GLuint texture(const std::filesystem::path &root, bool array)
{
    GLuint result; glGenTextures(1,&result);
    const GLenum target = array ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
    glBindTexture(target,result);
    glTexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    if (array)
    {
        auto data = TerrainTextureArray::load((root/"media/textures/WarmWilderness64.hmt").string());
        glTexImage3D(target,0,GL_RGBA8,data.edge,data.edge,data.layers,0,GL_RGBA,GL_UNSIGNED_BYTE,data.rgba.data());
    }
    else
    {
        auto path = (root/"media/textures/DefaultPack.png").string();
        auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
            reinterpret_cast<const UInt8 *>(path.c_str()),path.size(),false);
        auto source = CGImageSourceCreateWithURL(url,nullptr);
        require(source,"Cannot load atlas");
        auto image = CGImageSourceCreateImageAtIndex(source,0,nullptr);
        const auto width = CGImageGetWidth(image), height = CGImageGetHeight(image);
        Pixels data(width*height*4);
        auto space = CGColorSpaceCreateDeviceRGB();
        auto context = CGBitmapContextCreate(data.data(),width,height,8,width*4,space,
            kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast);
        CGContextDrawImage(context,CGRectMake(0,0,width,height),image);
        glTexImage2D(target,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,data.data());
        CGContextRelease(context); CGColorSpaceRelease(space); CGImageRelease(image);
        CFRelease(source); CFRelease(url);
    }
    return result;
}
}

int main(int argc,char **argv)
{
    try
    {
        const bool speciesLeaves = argc==5 && std::string(argv[3])=="--species-leaves";
        const bool irregularGeology = argc==5 && std::string(argv[4])=="--irregular-geology";
        require(argc==3 || (argc==4 && std::string(argv[3])!="--species-leaves") || speciesLeaves || irregularGeology,"Usage: block-feedback-gpu <root> <new-output-directory> [baseline-root-for-geology] [--irregular-geology] OR <root> <new-output-directory> --species-leaves <frozen-fragment-directory>");
        const std::filesystem::path root(argv[1]), output(argv[2]);
        if (speciesLeaves)
            for (const auto *name : {"HelloMine3DTerrain.frag", "HelloMine3DTerrainShadow.frag"})
                require(std::filesystem::is_regular_file(std::filesystem::path(argv[4])/name), "Missing frozen leaf shader");
        require(!std::filesystem::exists(output),"Output must be new");
        std::filesystem::create_directories(output);
        const CGLPixelFormatAttribute attributes[]{kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),
            kCGLPFAAccelerated,static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format; GLint count; CGLContextObj context;
        require(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError && format,"No OpenGL context");
        require(CGLCreateContext(format,nullptr,&context)==kCGLNoError,"Cannot create context");
        CGLDestroyPixelFormat(format); CGLSetCurrentContext(context);
        std::cout << "renderer=" << glGetString(GL_RENDERER) << '\n';
        GLuint vao,fbo,colour,depth;
        glGenVertexArrays(1,&vao); glBindVertexArray(vao);
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenRenderbuffers(1,&colour); glBindRenderbuffer(GL_RENDERBUFFER,colour);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA8,Edge,Edge);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colour);
        glGenRenderbuffers(1,&depth); glBindRenderbuffer(GL_RENDERBUFFER,depth);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,Edge,Edge);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete framebuffer");
        glViewport(0,0,Edge,Edge); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
        GLuint shadowMap; glGenTextures(1,&shadowMap); glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D,shadowMap);
        const float occluder=.25f;
        glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,1,1,0,GL_RED,GL_FLOAT,&occluder);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glActiveTexture(GL_TEXTURE0);
        int checks=0,failures=0;
        const auto check = [&](const std::string &name,bool ok)
        {
            ++checks; failures+=!ok;
            std::cout << "[BLOCK_FEEDBACK_GPU] " << (ok?"PASS ":"FAIL ") << name << '\n';
        };
        auto caster=program(root,"HelloMine3DDirectionalShadowCaster.vert","HelloMine3DDirectionalShadowCaster.frag",false);
        const auto cast = [&](float origin,float tag,bool fade=true,float strength=1.f) {
            return renderGround(caster,0,origin,0,0,1,-1,1,0,0,.4999f,-1,-1,fade,0,strength,tag);
        };
        const auto casterTag=[](int x,int z){return float(1+(x+6)*32+z+6);};
        check("tree-caster-non-tree-original",cast(130,0)==cast(130,0,false));
        check("tree-caster-inward-crown-original",cast(118,casterTag(-6,-6))==cast(118,casterTag(-6,-6),false));
        const auto retiredCaster=cast(114,casterTag(6,6));
        check("tree-caster-retired-root-stops-shadow",std::all_of(retiredCaster.begin(),retiredCaster.end(),[](auto v){return v==0;}));
        check("tree-caster-underground-original",cast(114,casterTag(6,6),true,0)==cast(114,casterTag(6,6),false));
        const auto minimumCast = [&](float origin,int root,bool fade=true) {
            return renderGround(caster,0,origin,0,0,1,-1,1,0,0,.4999f,-1,-1,fade,0,1,casterTag(root,root),true);
        };
        check("tree-caster-rd1-interaction-crown-preserved",minimumCast(6,6)==minimumCast(6,6,false));
        check("tree-caster-rd1-thirteen-metre-root-guard",minimumCast(7,6)==minimumCast(7,6,false));
        const auto minimumRetiredCaster=minimumCast(8,6);
        check("tree-caster-rd1-root-retires-before-demand-edge",std::all_of(minimumRetiredCaster.begin(),minimumRetiredCaster.end(),[](auto v){return v==0;}));
        glDeleteProgram(caster);
        for (bool array : {false,true})
        {
            const std::string mode = array?"array":"atlas";
            auto tex = texture(root,array);
            auto base = program(root,"HelloMine3DTerrain.vert","HelloMine3DTerrain.frag",array);
            auto shadow = program(root,"HelloMine3DTerrainShadow.vert","HelloMine3DTerrainShadow.frag",array);
            auto surface = program(root,"HelloMine3DTerrain.vert","HelloMine3DBlockFeedback.frag",array);
            auto floraBase = program(root,"HelloMine3DFlora.vert","HelloMine3DTerrain.frag",array);
            auto flora = program(root,"HelloMine3DFlora.vert","HelloMine3DBlockFeedback.frag",array);
            auto particle = program(root,"HelloMine3DBlockParticle.vert","HelloMine3DBlockParticle.frag",array);
            check(mode+"-production-programs-link",true);
            auto floraShadow = program(root,"HelloMine3DFloraShadow.vert","HelloMine3DTerrainShadow.frag",array);
            // Real production vertex -> fragment sources; no CPU lighting replica.
            for (auto shader : {base, shadow, floraBase, floraShadow}) {
                const auto range = [&](float distance,bool fade) {
                    return renderGround(shader,1,distance,0,0,1,-1,1,0,0,.4999f,-1,-1,fade);
                };
                check(mode+"-view-range-near-unchanged",range(80,true)==range(80,false));
                check(mode+"-view-range-diagonal-preserved",range(110,true)==range(110,false));
                check(mode+"-view-range-follows-logical-centre",
                    renderGround(shader,1,130,0,0,1,-1,1,0,0,.4999f,-1,-1,true,130)==range(130,false));
                check(mode+"-view-range-underground-preserved",
                    renderGround(shader,1,130,0,0,1,-1,1,0,0,.4999f,-1,-1,true,0,0)==range(130,false));
                const auto retired=range(130,true);
                check(mode+"-view-range-far-retired",std::all_of(retired.begin(),retired.end(),[](auto v){return v==0;}));
                const auto middle=range(122,true);
                const auto unfaded=range(122,false);
                bool covered=true;for(std::size_t i=3;i<middle.size();i+=4)covered &= middle[i]==unfaded[i];
                check(mode+"-view-range-no-stipple",covered);
                check(mode+"-view-range-transition-stable",middle==range(122,true));
                std::cout << mode << " normalized_range_move_delta=" << colourDifference(middle,range(122.f + 8.f / 3200.f,true))
                          << " unfaded_move_delta=" << colourDifference(unfaded,range(122.f + 8.f / 3200.f,false)) << '\n';
                // Keep the original relative movement and 0.1 colour gate:
                // 1 cm in a 32 m band is 2.5 mm in this 8 m band.
                check(mode+"-view-range-transition-continuous",colourDifference(middle,range(122.f + 8.f / 3200.f,true))<.1);
                check(mode+"-view-range-contrast-reduced",colourDifference(middle,range(122,false))>5);
                // Move only the range centre; keep texture, wind and fog fixed.
                const auto stationary=range(0,false);
                double previousContrast=-1;bool rangeMonotonic=true;
                for(float distance:{116.f,118.f,120.f,122.f,124.f}) {
                    const auto movedCentre=renderGround(shader,1,0,0,0,1,-1,1,0,0,.4999f,-1,-1,true,-distance);
                    const double contrast=colourDifference(stationary,movedCentre);
                    rangeMonotonic &= contrast>=previousContrast;previousContrast=contrast;
                }
                check(mode+"-view-range-coverage-monotonic",rangeMonotonic);
                const auto rootTag = [](int x,int z) { return float(1+(x+6)*32+z+6); };
                const auto tree = [&](float origin,int root,bool fade=true,float strength=1.f,float centre=0.f) {
                    return renderGround(shader,0,origin,0,0,1,-1,1,0,0,.4999f,-1,-1,fade,centre,strength,rootTag(root,root));
                };
                // Six-metre crown tips retain their root's coverage, even when
                // their own fragments lie in a different part of the range band.
                check(mode+"-tree-root-retains-complete-inward-crown",tree(118,-6)==tree(118,-6,false));
                const auto retiredTree=tree(114,6);
                check(mode+"-tree-root-retires-complete-outward-crown",std::all_of(retiredTree.begin(),retiredTree.end(),[](auto v){return v==0;}));
                check(mode+"-tree-root-shares-coverage-across-section-origins",tree(110,6)==tree(122,-6));
                check(mode+"-tree-root-follows-logical-centre",tree(114,6,true,1,120)==tree(114,6,false));
                check(mode+"-tree-root-underground-retains-original",tree(114,6,true,0)==tree(114,6,false));
                const auto middleTree=tree(110,6);
                const auto brightTree=tree(110,6,false);
                bool completeTree=true;for(std::size_t i=3;i<middleTree.size();i+=4)completeTree &= middleTree[i]==brightTree[i];
                check(mode+"-tree-root-no-crown-stipple",completeTree && colourDifference(middleTree,brightTree)>5);
                const auto minimumTree = [&](float origin,int root,bool fade=true) {
                    return renderGround(shader,0,origin,0,0,1,-1,1,0,0,.4999f,-1,-1,fade,0,1,rootTag(root,root),true);
                };
                check(mode+"-tree-rd1-interaction-crown-preserved",minimumTree(6,6)==minimumTree(6,6,false));
                check(mode+"-tree-rd1-thirteen-metre-root-guard",minimumTree(7,6)==minimumTree(7,6,false));
                const auto minimumRetired=minimumTree(8,6);
                check(mode+"-tree-rd1-root-retires-before-demand-edge",std::all_of(minimumRetired.begin(),minimumRetired.end(),[](auto v){return v==0;}));
                const auto guardRange = [&](float distance,int axis=1,float sign=-1.f) {
                    return renderGround(shader,0,0,0,0,1,-1,1,0,0,.4999f,-1,-1,true,sign*distance,1,rootTag(0,0),true,axis);
                };
                bool guardMonotonic=true,guardDirections=true;double previousGuardContrast=-1;
                const auto guardBright=tree(0,0,false);
                for(float distance:{13.f,13.25f,13.5f,13.75f,14.f}) {
                    const auto pixels=guardRange(distance);
                    const auto contrast=colourDifference(guardBright,pixels);
                    guardMonotonic &= contrast>=previousGuardContrast;previousGuardContrast=contrast;
                    guardDirections &= pixels==guardRange(distance,1,1.f) && pixels==guardRange(distance,2,-1.f) && pixels==guardRange(distance,2,1.f);
                }
                check(mode+"-tree-rd1-continuous-monotonic-guard-band",guardMonotonic && guardRange(13)==guardBright && colourDifference(guardBright,guardRange(14))>5);
                check(mode+"-tree-rd1-four-signed-directions-identical",guardDirections);
                check(mode+"-tree-rd1-normalized-movement-continuous",colourDifference(guardRange(13.5f),guardRange(13.5f+1.f/3200.f))<.1);

                const auto source = [&](float sky, float local, float day, float shadowAmount = 0.f, float fog = 0.f) {
                    const float light = .15f + .85f * std::max(sky,local);
                    return renderGround(shader,1,0,3,0,day,-1,light,shadowAmount,fog,.4999f,sky,local);
                };
                const std::string label=mode+"-source-"+std::to_string(shader);
                const auto torch=source(0,.8f,1), dark=source(0,0,1);
                check(label+"-torch-stable-through-day-night-and-sun-shadow",
                    torch == source(0,.8f,0,1) && source(0,.8f,1,1,.2f)==source(0,.8f,0,0,.2f));
                check(label+"-unlit-floor-stable-and-dimmer-than-torch",
                    dark == source(0,0,0,1) && colourDifference(dark,torch)>20);
                check(label+"-skylight-still-follows-day",colourDifference(source(1,0,1),source(1,0,0))>20);
                if (shader==shadow || shader==floraShadow)
                    check(label+"-sun-shadow-affects-sky",colourDifference(source(1,0,1),source(1,0,1,1))>10);
                bool monotonic=true; float previous=-1;
                for (int level=0;level<=15;++level) {
                    const auto pixels=source(0,level/15.f,0,1);
                    const float difference=colourDifference(dark,pixels);
                    monotonic &= difference>=previous; previous=difference;
                }
                check(label+"-local-falloff-monotonic",monotonic);
                check(label+"-legacy-outdoor-exposure-preserved",
                    source(1,0,1)==renderGround(shader,1,0,3,0,1));
            }
            glDeleteProgram(floraShadow);
            // The same complete shaders drive the isolated held-item passes.
            // Disable colour accents to measure exposure independently, then
            // check the production palette, AO, fog and alpha paths separately.
            const auto held = [&](GLuint shader,float exposure,float day=1.f,
                                  float light=1.f,float shadowStrength=0.f,
                                  int tile=3,float fog=0.f,float surface=0.f) {
                return renderGround(shader,surface,0,tile,0,day,exposure,light,shadowStrength,fog);
            };
            for (auto shader : {base,shadow}) {
                const std::string path=mode+(shader==base?"-held-normal":"-held-shadow");
                check(path+"-exposure-uniform-active",glGetUniformLocation(shader,"playerExposure")>=0);
                const auto bright=held(shader,1),dark=held(shader,.08f);
                check(path+"-local-exposure-darkens",colourDifference(bright,dark)>30.f &&
                      scaledExposure(bright,dark,.08f));
                check(path+"-zero-exposure",scaledExposure(bright,held(shader,0),0));
                check(path+"-daylight-not-applied-twice",dark==held(shader,.08f,0));
                check(path+"-negative-fallback-keeps-daylight",held(shader,-1,0)==held(shader,.34f));
                check(path+"-vertex-light-shaping-retained",
                      scaledExposure(held(shader,.5f),held(shader,.5f,1,0),.24f));
                check(path+"-material-palette-retained",
                      scaledExposure(held(shader,1,1,1,0,0,0,1),held(shader,.08f,1,1,0,0,0,1),.08f));
                check(path+"-opaque-fog-hides-exposure",
                      held(shader,1,1,1,0,3,20)==held(shader,.08f,1,1,0,3,20));
                for (int tile : {10,14}) for (float cutoff : {.4999f,.01f}) {
                    const auto legacy=renderGround(shader,0,0,tile,0,1,-1,1,0,0,cutoff);
                    const auto local=renderGround(shader,0,0,tile,0,1,.08f,1,0,0,cutoff);
                    bool sameAlpha=true; int empty=0,covered=0;
                    for (std::size_t i=3;i<local.size();i+=4) {
                        sameAlpha &= legacy[i]==local[i];
                        empty += local[i]==0; covered += local[i]>0;
                    }
                    check(path+"-alpha-and-discard-preserved-"+std::to_string(tile)+
                          (cutoff>.1f?"-cutout":"-transparent"),
                          sameAlpha && covered>100 && (tile!=10 || empty>1000));
                }
            }
            check(mode+"-held-normal-shadow-off-agrees",held(base,.08f)==held(shadow,.08f));
            check(mode+"-held-directional-shadow-retained",
                  scaledExposure(held(shadow,.5f),held(shadow,.5f,1,1,.6f),.4f));
            png(output/(mode+"-held-bright.png"),held(base,1));
            png(output/(mode+"-held-dark.png"),held(base,.08f));
            const auto originalGround = renderGround(base, 0.f, -16.f);
            const auto quietGround = renderGround(base, 1.f, -16.f);
            check(mode+"-ground-palette-affects-grass", originalGround != quietGround);
            check(mode+"-ground-shadow-off-matches-standard", quietGround == renderGround(shadow, 1.f, -16.f));
            check(mode+"-natural-palette-preserves-machinery", renderGround(base, 0.f, 0.f, 2, 1) == renderGround(base, 1.f, 0.f, 2, 1));
            check(mode+"-ground-palette-world-space-variation", quietGround != renderGround(base, 1.f, 32.f));
            png(output/(mode+"-ground-before.png"),originalGround);
            png(output/(mode+"-ground-after.png"),quietGround);
            if (speciesLeaves)
            {
                // Compile the frozen real fragments with the same vertices and
                // real textures. All colour measurements come from GPU output.
                const std::filesystem::path frozen(argv[4]);
                const auto old=program(root,"HelloMine3DTerrain.vert","HelloMine3DTerrain.frag",array,false,frozen);
                const auto oldShadow=program(root,"HelloMine3DTerrainShadow.vert","HelloMine3DTerrainShadow.frag",array,false,frozen);
                for (int column : {2,5})
                {
                    const std::string label=mode+(column==2?"-spruce-leaves":"-birch-leaves");
                    const auto before=renderGround(old,1,-16,column,8);
                    const auto after=renderGround(base,1,-16,column,8);
                    bool sameAlpha=true;
                    for(std::size_t i=3;i<after.size();i+=4) sameAlpha &= before[i]==after[i];
                    check(label+"-colour-changes-with-alpha-preserved",sameAlpha && colourDifference(before,after)>1.f);
                    check(label+"-off-preserves-frozen-colour",renderGround(base,0,-16,column,8)==renderGround(old,0,-16,column,8) &&
                          renderGround(shadow,0,-16,column,8)==renderGround(oldShadow,0,-16,column,8));
                    check(label+"-normal-shadow-agree",after==renderGround(shadow,1,-16,column,8) &&
                          before==renderGround(oldShadow,1,-16,column,8));
                    const auto oldZero=renderGround(old,1,0,column,8),oldPositive=renderGround(old,1,32,column,8);
                    const auto newZero=renderGround(base,1,0,column,8),newPositive=renderGround(base,1,32,column,8);
                    const std::array<float,3> newDeltas{colourDifference(after,newZero),colourDifference(after,newPositive),colourDifference(newZero,newPositive)};
                    const std::array<float,3> oldDeltas{colourDifference(before,oldZero),colourDifference(before,oldPositive),colourDifference(oldZero,oldPositive)};
                    check(label+"-broad-world-field-added",*std::max_element(newDeltas.begin(),newDeltas.end())>.5f &&
                          before==oldZero && before==oldPositive && oldZero==oldPositive);
                    std::cout << label << " world_pairs=-16:0,-16:32,0:32 new_delta=" << newDeltas[0] << ',' << newDeltas[1] << ',' << newDeltas[2]
                              << " old_delta=" << oldDeltas[0] << ',' << oldDeltas[1] << ',' << oldDeltas[2] << '\n';
                    // Locate actual covered high/low texels through the disabled
                    // production shader, rather than inventing input colours.
                    std::array<float,2> low{},high{};
                    double lowLuma=1e9,highLuma=-1;int opaque=0;
                    const auto sample = [&](GLuint shader,float enabled,const std::array<float,2> &uv,float origin) {
                        return renderGeology(shader,column,8,true,.125f,origin,72,origin,enabled,0,1.23f,1,.5f,0,uv[0],uv[1]);
                    };
                    for(int y=0;y<4;++y) for(int x=0;x<4;++x)
                    {
                        const std::array<float,2> uv{(x+.5f)/4.f,(y+.5f)/4.f};
                        const auto pixels=sample(old,0,uv,-16);
                        // Array foliage uses alpha 128, which passes the real
                        // .4999 cutoff. Require complete constant coverage,
                        // retaining the source alpha rather than inventing 255.
                        bool covered=pixels[3]>=128;
                        for(std::size_t i=3;i<pixels.size();i+=4) covered &= pixels[i]==pixels[3];
                        if(!covered) continue;
                        ++opaque;
                        const double luma=.2126*pixels[0]+.7152*pixels[1]+.0722*pixels[2];
                        if(luma<lowLuma) { lowLuma=luma;low=uv; }
                        if(luma>highLuma) { highLuma=luma;high=uv; }
                    }
                    check(label+"-real-high-low-texels-readable",opaque>=2 && highLuma-lowLuma>5);
                    std::cout << label << " covered_texels=" << opaque << " high_low_luma=" << highLuma-lowLuma << '\n';
                    for(float origin : {-16.f,0.f,32.f})
                    {
                        const auto oldLow=sample(old,1,low,origin),oldHigh=sample(old,1,high,origin);
                        const auto newLow=sample(base,1,low,origin),newHigh=sample(base,1,high,origin);
                        const float oldContrast=colourDifference(oldLow,oldHigh),newContrast=colourDifference(newLow,newHigh);
                        const std::string position=label+"-"+std::to_string(int(origin));
                        check(position+"-fine-contrast-reduced",oldContrast>5 && newContrast>1 && newContrast<oldContrast-.25f);
                        check(position+"-frozen-negative-has-no-compression",oldLow==sample(old,0,low,origin) && oldHigh==sample(old,0,high,origin));
                        check(position+"-high-low-shadow-agree",newLow==sample(shadow,1,low,origin) && newHigh==sample(shadow,1,high,origin));
                        std::cout << position << " low_uv=" << low[0] << ',' << low[1] << " high_uv=" << high[0] << ',' << high[1]
                                  << " old_contrast=" << oldContrast << " new_contrast=" << newContrast << '\n';
                    }
                    png(output/(label+"-before.png"),before);png(output/(label+"-after.png"),after);
                }
                // Oak and neighboring row-eight materials retain every RGBA
                // byte, so recognizing two leaves cannot reclassify the row.
                for(const auto tile : {std::pair<int,int>{6,0},{6,5},{0,8},{1,8},{3,8},{4,8},{6,8},{7,8},{2,1},{3,0}})
                    check(mode+"-species-leaves-preserve-"+std::to_string(tile.first)+"-"+std::to_string(tile.second),
                        renderGround(base,1,-16,tile.first,tile.second)==renderGround(old,1,-16,tile.first,tile.second) &&
                        renderGround(shadow,1,-16,tile.first,tile.second)==renderGround(oldShadow,1,-16,tile.first,tile.second));
                glDeleteProgram(old);glDeleteProgram(oldShadow);
            }
            for (int tile : {1, 2, 3, 4, 5, 6, 10, 11})
            {
                const auto before = renderGround(base, 0.f, -16.f, tile);
                const auto after = renderGround(base, 1.f, -16.f, tile);
                bool sameSilhouette = true;
                for (std::size_t p = 3; p < after.size(); p += 4)
                    sameSilhouette &= before[p] == after[p];
                const std::string name = mode + "-natural-" + std::to_string(tile);
                check(name + "-palette-and-alpha", before != after && sameSilhouette);
                check(name + "-shadow-off-agrees", after == renderGround(shadow, 1.f, -16.f, tile));
                png(output/(name + "-before.png"), before);
                png(output/(name + "-after.png"), after);
            }
            for (int row = 3; row <= 7; ++row)
                for (int column : {0, 3, 6, 12})
                    check(mode + "-ecology-" + std::to_string(row) + "-" + std::to_string(column),
                        renderGround(base, 1.f, -16.f, column, row) ==
                        renderGround(shadow, 1.f, -16.f, column, row));
            const auto climateImage = [](GLuint shader, int col, int row,
                    float warm, float forest, float enabled = 1.f,
                    float daylight = 1.f, float u = .37f, float v = .43f) {
                return renderGeology(shader, col, row, true, 8.f, -24.f, 72.f, 16.f,
                                     enabled, 0.f, 1.23f, daylight, warm, forest, u, v);
            };
            for (int column : {0, 3, 6, 12}) {
                const auto shared = climateImage(base, column, 3, .35f, .4f);
                check(mode+"-ecotone-row-boundary-"+std::to_string(column),
                    shared == climateImage(base, column, 6, .35f, .4f));
                check(mode+"-ecotone-shadow-agrees-"+std::to_string(column),
                    shared == climateImage(shadow, column, 6, .35f, .4f));
                const auto dry = climateImage(base, column, 3, 1.f, 0.f);
                const auto forest = climateImage(base, column, 6, 0.f, 1.f);
                bool sameAlpha = true;
                for (std::size_t i = 3; i < dry.size(); i += 4) sameAlpha &= dry[i] == forest[i];
                check(mode+"-ecotone-keeps-alpha-"+std::to_string(column), sameAlpha);
                check(mode+"-ecotone-compatibility-keeps-original-row-"+std::to_string(column),
                    climateImage(base, column, 3, 0.f, 1.f, 0.f) ==
                    climateImage(base, column, 3, 1.f, 0.f, 0.f));
            }
            check(mode+"-ecotone-regions-retain-colour-character",
                colourDifference(climateImage(base,0,3,1,0), climateImage(base,0,6,0,1)) > 2.f &&
                colourDifference(climateImage(base,0,4,0,0), climateImage(base,0,7,0,-1)) > 1.f);
            check(mode+"-ecotone-grass-side-dirt-is-neutral",
                climateImage(base,3,3,1,0,1,1,.37f,.8f) == climateImage(base,3,6,0,1,1,1,.37f,.8f));
            png(output/(mode+"-ecotone-night-base.png"),climateImage(base,0,3,.4f,.3f,1,.18f));
            png(output/(mode+"-ecotone-night-shadow.png"),climateImage(shadow,0,6,.4f,.3f,1,.18f));
            check(mode+"-ecotone-night-shadow-agrees",
                climateImage(base,0,3,.4f,.3f,1,.18f) == climateImage(shadow,0,6,.4f,.3f,1,.18f));
            const auto coreBefore = renderGround(base, 0.f, 0.f, 15, 0, .18f);
            const auto coreAfter = renderGround(base, 1.f, 0.f, 15, 0, .18f);
            int brighterCore = 0, retainedFrame = 0;
            bool coreAlpha = true;
            for (std::size_t p=0;p<coreAfter.size();p+=4) {
                brighterCore += coreAfter[p+2] > coreBefore[p+2] + 10;
                retainedFrame += std::equal(coreBefore.begin()+p, coreBefore.begin()+p+3, coreAfter.begin()+p);
                coreAlpha &= coreBefore[p+3] == coreAfter[p+3];
            }
            check(mode+"-waystone-inset-readable-at-night", brighterCore > 100 && retainedFrame > 100 && coreAlpha);
            check(mode+"-waystone-shadow-off-agrees", coreAfter == renderGround(shadow, 1.f, 0.f, 15, 0, .18f));
            png(output/(mode+"-waystone-night.png"), coreAfter);
            if (irregularGeology)
            {
                // The former test intentionally required strong periodic bands.
                // This mode measures the user-requested quieter, irregular rock
                // against that frozen shader while keeping all shared checks.
                auto old=program(std::filesystem::path(argv[3]),"HelloMine3DTerrain.vert","HelloMine3DTerrain.frag",array);
                const auto near=renderGeology(base,3,0,false,32);
                const auto before=renderGeology(old,3,0,false,32);
                const float oldGradient=pixelGradient(before),newGradient=pixelGradient(near);
                check(mode+"-rock-reduces-repeated-band-contrast",newGradient<oldGradient*.45f && colourDifference(near,before)>1.f);
                check(mode+"-rock-retains-broad-spatial-variation",newGradient>.001f);
                check(mode+"-old-periodic-rock-negative-detected",oldGradient>.10f && oldGradient>newGradient*2.f);
                check(mode+"-irregular-rock-shadow-agrees",near==renderGeology(shadow,3,0,false,32));
                check(mode+"-irregular-rock-time-independent",near==renderGeology(base,3,0,false,32,-24,72,16,1,0,73));
                check(mode+"-irregular-rock-origin-independent",near==renderGeology(base,3,0,false,32,-24,72,16,1,16));
                check(mode+"-irregular-rock-disabled-unchanged",renderGeology(base,3,0,false,32,-24,72,16,0)==renderGeology(old,3,0,false,32,-24,72,16,0));
                check(mode+"-irregular-rock-night-agrees",renderGeology(base,3,0,false,32,-24,72,16,1,0,1.23f,.18f)==renderGeology(shadow,3,0,false,32,-24,72,16,1,0,1.23f,.18f));
                bool boundary=true;
                for(float seam:{-16.f,0.f,16.f})boundary &= colourDifference(renderGeology(base,3,0,false,.25f,seam-.0001f),renderGeology(base,3,0,false,.25f,seam+.0001f))<.1f;
                check(mode+"-irregular-rock-signed-seams-continuous",boundary);
                const auto far=renderGeology(base,3,0,false,384);
                check(mode+"-irregular-rock-far-gradient-bounded",pixelGradient(far)<.12f);
                for(const auto tile:{std::pair<int,int>{7,0},{13,0},{14,0},{7,1},{2,1},{0,4},{4,0},{6,8},{7,8},{8,8},{9,8},{10,8},{11,8}})
                    check(mode+"-irregular-rock-preserves-"+std::to_string(tile.first)+"-"+std::to_string(tile.second),
                        renderGeology(base,tile.first,tile.second,false)==renderGeology(old,tile.first,tile.second,false));
                std::cout << mode << " rock_old_gradient=" << oldGradient << " rock_new_gradient=" << newGradient << " far_gradient=" << pixelGradient(far) << '\n';
                png(output/(mode+"-rock-periodic-before.png"),before);png(output/(mode+"-rock-irregular-after.png"),near);
                glDeleteProgram(old);
            }
            if (argc == 4)
            {
                const std::filesystem::path baseline(argv[3]);
                auto old = program(baseline,"HelloMine3DTerrain.vert","HelloMine3DTerrain.frag",array);
                auto unfiltered = program(root,"HelloMine3DTerrain.vert","HelloMine3DTerrain.frag",array,true);
                for (const int tile : {3,7})
                {
                    const bool top = tile==7;
                    const auto near = renderGeology(base,tile,0,top);
                    const auto before = renderGeology(old,tile,0,top);
                    const std::string label=mode+"-geology-"+std::to_string(tile);
                    check(label+"-visible-world-pattern",colourDifference(near,before)>2.f && pixelGradient(near)>.10f);
                    check(label+"-shadow-agrees",near==renderGeology(shadow,tile,0,top));
                    check(label+"-section-origin-independent",near==renderGeology(base,tile,0,top,8,-24,72,16,1,16));
                    check(label+"-time-independent",near==renderGeology(base,tile,0,top,8,-24,72,16,1,0,71.23f));
                    check(label+"-disabled-unchanged",renderGeology(base,tile,0,top,8,-24,72,16,0)==renderGeology(old,tile,0,top,8,-24,72,16,0));
                    check(label+"-night-shadow-agrees",renderGeology(base,tile,0,top,8,-24,72,16,1,0,1.23f,.18f)==renderGeology(shadow,tile,0,top,8,-24,72,16,1,0,1.23f,.18f));
                    bool boundary=true;
                    for (const float seam : {-16.f,0.f,16.f})
                        boundary &= colourDifference(renderGeology(base,tile,0,top,.25f,seam-.0001f),renderGeology(base,tile,0,top,.25f,seam+.0001f))<.1f;
                    check(label+"-signed-section-boundaries-continuous",boundary);
                    const float farSpan=top?64.f:384.f;
                    const auto far = renderGeology(base,tile,0,top,farSpan);
                    const auto alias = renderGeology(unfiltered,tile,0,top,farSpan);
                    const float smooth = pixelGradient(far), noisy = pixelGradient(alias);
                    check(label+"-footprint-fade-rejects-unfiltered",smooth<noisy*.70f && noisy>.20f);
                    check(label+"-old-shader-negative-detected",pixelGradient(before)<=.10f);
                    bool alpha=true;for (std::size_t p=3;p<near.size();p+=4)alpha &= near[p]==before[p];
                    check(label+"-alpha-unchanged",alpha);
                    std::cout << label << " mean_change=" << colourDifference(near,before)
                              << " near_gradient=" << pixelGradient(near) << " far_gradient=" << smooth
                              << " unfiltered_gradient=" << noisy << '\n';
                    png(output/(label+"-before.png"),before);png(output/(label+"-after.png"),near);
                    png(output/(label+"-far.png"),far);png(output/(label+"-unfiltered-negative.png"),alias);
                }
                for (const auto tile : {std::pair<int,int>{13,0},{14,0},{7,1},{2,1},{0,4},{4,0}})
                    check(mode+"-geology-preserves-"+std::to_string(tile.first)+"-"+std::to_string(tile.second),
                        renderGeology(base,tile.first,tile.second,false)==renderGeology(old,tile.first,tile.second,false));
                glDeleteProgram(old); glDeleteProgram(unfiltered);
            }
            glUseProgram(base);
            value(base,"surfaceLightingStrength",0);
            glDeleteProgram(shadow);
            auto plain = render(base,0,-1,3), highlight = render(base,surface,-1,3);
            check(mode+"-solid-surface-highlights",plain!=highlight);
            png(output/(mode+"-highlight.png"),highlight);
            int previous=-1,first=0,last=0; bool monotonic=true;
            for (int stage=0;stage<10;++stage)
            {
                auto cracked=render(base,surface,stage,3);
                int dark=0;
                for (std::size_t p=0;p<cracked.size();p+=4)
                    dark+=cracked[p]+cracked[p+1]+cracked[p+2]+40 < highlight[p]+highlight[p+1]+highlight[p+2];
                monotonic &= dark>=previous; previous=dark;
                if(stage==0) first=dark; last=dark;
                std::cout << mode << " stage=" << stage << " dark_pixels=" << dark << '\n';
                png(output/(mode+"-crack-"+std::to_string(stage)+".png"),cracked);
            }
            check(mode+"-ten-stages-grow-monotonically",monotonic && first>0 && last>first*3);
            check(mode+"-cancel-restores-highlight",render(base,surface,-1,3)==highlight);
            check(mode+"-nearer-surface-occludes-highlight-and-cracks",render(base,surface,9,3,true)==plain);
            auto flower = render(floraBase,0,-1,10), selectedFlower = render(floraBase,flora,9,10);
            int empty=0,leaks=0,lit=0;
            for(std::size_t p=0;p<flower.size();p+=4)
            {
                bool isEmpty=flower[p]==31 && flower[p+1]==41 && flower[p+2]==56;
                bool changed=!std::equal(flower.begin()+p,flower.begin()+p+3,selectedFlower.begin()+p);
                empty+=isEmpty; leaks+=isEmpty && changed; lit+=!isEmpty && changed;
            }
            check(mode+"-moving-flower-alpha-silhouette",empty>1000 && leaks==0 && lit>100);
            png(output/(mode+"-flower.png"),selectedFlower);
            png(output/(mode+"-particle.png"),render(base,particle,-1,3));
            for(auto p : {base,surface,floraBase,flora,particle}) glDeleteProgram(p);
            glDeleteTextures(1,&tex);
        }
        glDeleteTextures(1,&shadowMap);
        std::cout << "checks=" << checks << " failures=" << failures << '\n';
        return failures?1:0;
    }
    catch(const std::exception &error) { std::cerr<<error.what()<<'\n'; return 2; }
}
