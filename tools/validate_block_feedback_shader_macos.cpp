// Execute production shaders and textures; no CPU copy of the fracture algorithm.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_block_feedback_shader_macos.cpp \
//   -framework OpenGL -framework CoreGraphics -framework ImageIO -framework CoreFoundation -o /tmp/block-feedback-gpu
// /tmp/block-feedback-gpu <repository-or-package-resources> <new-output-directory>
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
GLuint program(const std::filesystem::path &root, const char *vertex, const char *fragment, bool array, bool unfilteredGeology = false)
{
    GLuint result = glCreateProgram();
    for (const auto &entry : {std::pair<GLenum,const char *>{GL_VERTEX_SHADER,vertex},
                              {GL_FRAGMENT_SHADER,fragment}})
    {
        auto source = read(root / "media/ogre" / entry.second);
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
    value(p,"globalTime",1.23f); value(p,"fogDensity",0);
    glUniform2f(glGetUniformLocation(p,"crackSeed"),17.0f,31.0f);
    glUniform2f(glGetUniformLocation(p,"crackStretch"),1,1);
}
void quad(GLuint p, int tileX, int tileY)
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
    }
    GLint tile = glGetAttribLocation(p,"uv0"), light = glGetAttribLocation(p,"uv2");
    GLint colour = glGetAttribLocation(p,"colour");
    if (tile >= 0) glVertexAttrib2f(tile,(tileX+0.5f)/16.f,(tileY+0.5f)/16.f);
    if (light >= 0) glVertexAttrib1f(light,1);
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
                    int tileY = 0, float daylight = 1.f)
{
    glDisable(GL_BLEND); glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    setup(shader);
    value(shader, "surfaceLightingStrength", enabled);
    value(shader, "environmentLight", daylight);
    glUniform1i(glGetUniformLocation(shader, "directionalShadowMap"), 1);
    const float world[]{1,0,0,0, 0,1,0,0, 0,0,1,0, offset,0,offset,1};
    glUniformMatrix4fv(glGetUniformLocation(shader,"world"),1,GL_FALSE,world);
    quad(shader, tile, tileY);
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
                     float time = 1.23f, float daylight = 1.f)
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
    for (const auto *name : {"uv0","uv1","uv2"})
    {
        const auto a = glGetAttribLocation(shader,name); if (a >= 0) glDisableVertexAttribArray(a);
    }
    glVertexAttrib2f(glGetAttribLocation(shader,"uv0"),(tileX+.5f)/16.f,(tileY+.5f)/16.f);
    glVertexAttrib2f(glGetAttribLocation(shader,"uv1"),.37f,.43f);
    glVertexAttrib1f(glGetAttribLocation(shader,"uv2"),1.f);
    glDrawArrays(GL_TRIANGLES,0,6); glDisableVertexAttribArray(vertex); glDeleteBuffers(1,&vbo);
    Pixels pixels(Edge*Edge*4); glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR,"Geology draw/readback failed"); return pixels;
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
        require(argc==3 || argc==4,"Usage: block-feedback-gpu <root> <new-output-directory> [baseline-root-for-geology]");
        const std::filesystem::path root(argv[1]), output(argv[2]);
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
        int checks=0,failures=0;
        const auto check = [&](const std::string &name,bool ok)
        {
            ++checks; failures+=!ok;
            std::cout << "[BLOCK_FEEDBACK_GPU] " << (ok?"PASS ":"FAIL ") << name << '\n';
        };
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
            const auto originalGround = renderGround(base, 0.f, -16.f);
            const auto quietGround = renderGround(base, 1.f, -16.f);
            check(mode+"-ground-palette-affects-grass", originalGround != quietGround);
            check(mode+"-ground-shadow-off-matches-standard", quietGround == renderGround(shadow, 1.f, -16.f));
            check(mode+"-natural-palette-preserves-machinery", renderGround(base, 0.f, 0.f, 2, 1) == renderGround(base, 1.f, 0.f, 2, 1));
            check(mode+"-ground-palette-world-space-variation", quietGround != renderGround(base, 1.f, 32.f));
            png(output/(mode+"-ground-before.png"),originalGround);
            png(output/(mode+"-ground-after.png"),quietGround);
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
        std::cout << "checks=" << checks << " failures=" << failures << '\n';
        return failures?1:0;
    }
    catch(const std::exception &error) { std::cerr<<error.what()<<'\n'; return 2; }
}
