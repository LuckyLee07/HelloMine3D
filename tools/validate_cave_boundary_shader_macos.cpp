// Diagnostic GPU samples of the actual V09c shaders, not ordinary gameplay.
// clang++ -std=c++17 -Wno-deprecated-declarations tools/validate_cave_boundary_shader_macos.cpp \
//   -framework OpenGL -o <output>; <output> <repository root>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int Edge = 64, AtlasWidth = 512, AtlasHeight = 1024;
int checks = 0, failures = 0;
void require(bool value, const std::string &message) {
    if (!value) throw std::runtime_error(message);
}
void check(const std::string &name, bool value) {
    ++checks; failures += value ? 0 : 1;
    std::cout << "[CAVE_BOUNDARY_GPU] " << (value ? "PASS " : "FAIL ") << name << '\n';
}
std::string read(const std::filesystem::path &path) {
    std::ifstream file(path); require(file.good(), "Missing " + path.string());
    return {std::istreambuf_iterator<char>(file), {}};
}
GLuint program(const std::string &vertex, const std::string &fragment) {
    const GLuint result = glCreateProgram();
    for (const auto &entry : {std::pair<GLenum, const std::string *>{GL_VERTEX_SHADER, &vertex},
                              {GL_FRAGMENT_SHADER, &fragment}}) {
        const GLuint shader = glCreateShader(entry.first);
        const char *source = entry.second->c_str();
        glShaderSource(shader, 1, &source, nullptr); glCompileShader(shader);
        GLint valid = 0; char log[4096]{};
        glGetShaderiv(shader, GL_COMPILE_STATUS, &valid);
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        require(valid, std::string("Compile: ") + log);
        glAttachShader(result, shader); glDeleteShader(shader);
    }
    glLinkProgram(result); GLint valid = 0; char log[4096]{};
    glGetProgramiv(result, GL_LINK_STATUS, &valid);
    glGetProgramInfoLog(result, sizeof(log), nullptr, log);
    require(valid, std::string("Link: ") + log);
    return result;
}
struct Vertex { float x, y, z, u, v; };
std::array<Vertex, 6> quad(int slot) {
    const float u = (slot % 32) * 16.f / AtlasWidth;
    const float v = (slot / 32) * 16.f / AtlasHeight;
    const float du = 16.f / AtlasWidth, dv = 16.f / AtlasHeight;
    return {{{-1,-1,0,u,v}, {1,-1,0,u+du,v}, {1,1,0,u+du,v+dv},
             {-1,-1,0,u,v}, {1,1,0,u+du,v+dv}, {-1,1,0,u,v+dv}}};
}
bool probe(GLuint shader, GLuint buffer, GLuint atlas, int slot, int pattern,
           bool verifyDepth = false) {
    std::array<unsigned char, 256> tile{};
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
        tile[y*16+x] = (pattern == 1 || (pattern == 2 && ((x+y)%2==0)) ||
            (pattern == 3 && ((y==2 && (x==1 || x==11)) || (y==10 && x==4) ||
                              (y==15 && x>11)))) ? 255 : 0;
    glBindTexture(GL_TEXTURE_2D, atlas);
    glTexSubImage2D(GL_TEXTURE_2D,0,(slot%32)*16,(slot/32)*16,16,16,
                    GL_RED,GL_UNSIGNED_BYTE,tile.data());
    const auto vertices = quad(slot);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices.data(),GL_DYNAMIC_DRAW);
    glUseProgram(shader);
    const GLint position = glGetAttribLocation(shader,"vertex");
    const GLint uv = glGetAttribLocation(shader,"uv0");
    require(position >= 0 && uv >= 0, "Production vertex interface inactive");
    glEnableVertexAttribArray(position); glEnableVertexAttribArray(uv);
    glVertexAttribPointer(position,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glVertexAttribPointer(uv,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(12));
    const std::array<float,16> identity{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
    glUniformMatrix4fv(glGetUniformLocation(shader,"worldViewProj"),1,GL_FALSE,identity.data());
    glUniform1i(glGetUniformLocation(shader,"caveBoundaryMask"),0);
    glDepthMask(GL_TRUE); glClearDepth(.7); glClearColor(.2f,.55f,.85f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glDrawArrays(GL_TRIANGLES,0,6);
    std::vector<unsigned char> pixels(Edge*Edge*4);
    glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    require(glGetError()==GL_NO_ERROR, "Draw/read GL error");
    bool correct = true;
    for (int y=0;y<Edge;++y) for (int x=0;x<Edge;++x) {
        const bool dark = tile[(y/4)*16+x/4] != 0;
        const std::array<int,3> expected = dark ? std::array<int,3>{{9,11,14}} :
                                                        std::array<int,3>{{51,140,217}};
        for (int c=0;c<3;++c) correct = correct &&
            std::abs(int(pixels[(y*Edge+x)*4+c])-expected[c]) <= 1;
    }
    if (verifyDepth) {
        std::vector<float> depth(Edge*Edge);
        glReadPixels(0,0,Edge,Edge,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
        for (float value : depth) correct = correct && std::abs(value-.7f)<.00001f;
        // A later ordinary opaque surface must still cover the background.
        const GLuint opaque = program(
            "#version 150\nin vec4 vertex; uniform mat4 worldViewProj;\n"
            "void main(){gl_Position=worldViewProj*vertex;}\n",
            "#version 150\nout vec4 fragColour;\n"
            "void main(){fragColour=vec4(1,0,0,1);}\n");
        glUseProgram(opaque);
        const GLint opaquePosition = glGetAttribLocation(opaque,"vertex");
        glEnableVertexAttribArray(opaquePosition);
        glVertexAttribPointer(opaquePosition,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
        glUniformMatrix4fv(glGetUniformLocation(opaque,"worldViewProj"),1,GL_FALSE,identity.data());
        glEnable(GL_SCISSOR_TEST); glScissor(16,16,32,32); glDepthMask(GL_TRUE);
        glDrawArrays(GL_TRIANGLES,0,6); glDisable(GL_SCISSOR_TEST);
        glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        correct = correct && pixels[0]==255 && pixels[1]==0 && pixels[2]==0;
        glDeleteProgram(opaque);
    }
    return correct;
}
}
int main(int argc, char **argv) {
    try {
        require(argc==2,"Usage: cave-boundary-gpu <repository root>");
        CGLPixelFormatAttribute attributes[] = {kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),
            static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format=nullptr; GLint count=0; CGLContextObj context=nullptr;
        require(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError && format,
                "CGL pixel format unavailable");
        require(CGLCreateContext(format,nullptr,&context)==kCGLNoError,"CGL context unavailable");
        CGLDestroyPixelFormat(format); CGLSetCurrentContext(context);
        GLuint vao=0,buffer=0,fbo=0,colour=0,depth=0,atlas=0;
        glGenVertexArrays(1,&vao); glBindVertexArray(vao); glGenBuffers(1,&buffer);
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenTextures(1,&colour); glBindTexture(GL_TEXTURE_2D,colour);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,Edge,Edge,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour,0);
        glGenTextures(1,&depth); glBindTexture(GL_TEXTURE_2D,depth);
        glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,Edge,Edge,0,GL_DEPTH_COMPONENT,GL_FLOAT,nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth,0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Incomplete FBO");
        glViewport(0,0,Edge,Edge); glGenTextures(1,&atlas); glBindTexture(GL_TEXTURE_2D,atlas);
        std::vector<unsigned char> zero(AtlasWidth*AtlasHeight,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_R8,AtlasWidth,AtlasHeight,0,GL_RED,GL_UNSIGNED_BYTE,zero.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        const std::filesystem::path root=argv[1];
        const auto vertex=read(root/"media/ogre/HelloMine3DCaveBoundary.vert");
        const auto fragment=read(root/"media/ogre/HelloMine3DCaveBoundary.frag");
        const GLuint candidate=program(vertex,fragment);
        for(int slot : {0,31,32,2047}) for(int pattern : {0,1,2,3})
            check("actual-shader-slot-"+std::to_string(slot)+"-pattern-"+std::to_string(pattern),
                  probe(candidate,buffer,atlas,slot,pattern));
        check("mask-does-not-write-depth",probe(candidate,buffer,atlas,0,1,true));
        std::string noDiscard=fragment;
        const auto condition=noDiscard.find("< 0.5"); require(condition!=std::string::npos,"Missing fault seam");
        noDiscard.replace(condition,5,"< -1.0");
        const GLuint faulty=program(vertex,noDiscard);
        check("reject-no-discard",!probe(faulty,buffer,atlas,31,2));
        std::string swapped=vertex;
        const auto uv=swapped.find("boundaryUV = uv0;"); require(uv!=std::string::npos,"Missing UV fault seam");
        swapped.replace(uv,std::string("boundaryUV = uv0;").size(),"boundaryUV = uv0.yx;");
        const GLuint faultyUV=program(swapped,fragment);
        check("reject-transposed-uv",!probe(faultyUV,buffer,atlas,0,3));
        glDeleteProgram(faultyUV); glDeleteProgram(faulty); glDeleteProgram(candidate);
        glDeleteTextures(1,&atlas); glDeleteTextures(1,&colour); glDeleteTextures(1,&depth);
        glDeleteFramebuffers(1,&fbo); glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao);
        CGLSetCurrentContext(nullptr); CGLDestroyContext(context);
        std::cout<<"[CAVE_BOUNDARY_GPU] checks="<<checks<<" failures="<<failures<<'\n';
        return failures==0?0:1;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
