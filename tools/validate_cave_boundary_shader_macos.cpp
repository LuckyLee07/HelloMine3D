// Diagnostic GPU samples of actual mask and view-range shaders, not ordinary gameplay.
// clang++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations \
//   tools/validate_cave_boundary_shader_macos.cpp \
//   -framework OpenGL -o <output>; <output> <repository root> [new-range-evidence-directory]
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

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

// These samples render the complete production VS/FS with an independently
// calculated world-space footprint. They do not establish Ogre's per-frame
// logical-centre binding or ordinary camera/lifecycle behaviour.
const std::array<float,16> Identity{{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}};
struct RangeCase {
    std::string name;
    std::array<float,16> world = Identity;
    std::array<float,2> range{{8,14}}, centre{{0,0}};
    float strength = 1, mode = 0;
    int pattern = 1;
};
struct RangePixels {
    std::vector<unsigned char> colour;
    std::vector<float> beforeDepth, afterDepth;
    std::array<float,16> actualWorld{},actualWvp{};
    std::array<float,2> actualRange{},actualCentre{};
    std::array<float,3> actualFog{};
    float actualStrength=0,actualMode=0;
    GLint actualMaskUnit=-1;
    bool uniformReadback = false;
};
RangeCase rangeCase(const std::string& name, float x, float z,
                    float strength = 1, float mode = 0,
                    std::array<float,2> centre = {{0,0}}) {
    RangeCase value; value.name=name; value.strength=strength;
    value.mode=mode; value.centre=centre;
    // A real rigid world transform rotates the screen quad onto an X/Z plane.
    // worldViewProj compensates that model transform to keep the raster grid.
    value.world[5]=0; value.world[6]=1; value.world[9]=-1; value.world[10]=0;
    value.world[12]=x; value.world[13]=64; value.world[14]=z;
    return value;
}
float rangeCoverage(double x, double z, const RangeCase& value) {
    if (value.range[1] <= value.range[0]) return 1;
    const double distance=std::max(std::abs(x-value.centre[0]),
                                   std::abs(z-value.centre[1]));
    const double t=std::clamp((distance-value.range[0])/
        (value.range[1]-value.range[0]),0.0,1.0);
    const double smooth=t*t*(3-2*t);
    return static_cast<float>(1-std::clamp(double(value.strength),0.0,1.0)*smooth);
}
double rangeDecode(double colour) {
    return colour <= .04045 ? colour/12.92 : std::pow((colour+.055)/1.055,2.4);
}
RangePixels drawRange(GLuint shader, GLuint buffer, GLuint atlas,
                     const RangeCase& value) {
    std::array<unsigned char,256> tile{};
    for(int y=0;y<16;++y) for(int x=0;x<16;++x)
        tile[y*16+x]=(value.pattern==1 || (value.pattern==2 && (x+y)%2==0))?255:0;
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,atlas);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,16,16,GL_RED,GL_UNSIGNED_BYTE,tile.data());
    const auto vertices=quad(0); glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices.data(),GL_DYNAMIC_DRAW);
    glUseProgram(shader);
    const GLint position=glGetAttribLocation(shader,"vertex"),uv=glGetAttribLocation(shader,"uv0");
    require(position>=0 && uv>=0,"Range production vertex/UV interface inactive");
    glEnableVertexAttribArray(position); glEnableVertexAttribArray(uv);
    glVertexAttribPointer(position,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glVertexAttribPointer(uv,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(12));
    glUniformMatrix4fv(glGetUniformLocation(shader,"worldViewProj"),1,GL_FALSE,Identity.data());
    const GLint world=glGetUniformLocation(shader,"world");
    if(world>=0) glUniformMatrix4fv(world,1,GL_FALSE,value.world.data());
    glUniform1i(glGetUniformLocation(shader,"caveBoundaryMask"),0);
    glUniform1f(glGetUniformLocation(shader,"linearHdrMode"),value.mode);
    const GLint range=glGetUniformLocation(shader,"viewRange"),centre=glGetUniformLocation(shader,"viewRangeCentre");
    const GLint strength=glGetUniformLocation(shader,"viewRangeStrength"),fog=glGetUniformLocation(shader,"fogColour");
    if(range>=0) glUniform2fv(range,1,value.range.data());
    if(centre>=0) glUniform2fv(centre,1,value.centre.data());
    if(strength>=0) glUniform1f(strength,value.strength);
    if(fog>=0) glUniform3f(fog,.6f,.7f,.8f);
    RangePixels result;
    glGetUniformfv(shader,glGetUniformLocation(shader,"worldViewProj"),result.actualWvp.data());
    glGetUniformfv(shader,glGetUniformLocation(shader,"linearHdrMode"),&result.actualMode);
    glGetUniformiv(shader,glGetUniformLocation(shader,"caveBoundaryMask"),&result.actualMaskUnit);
    if(world>=0 && range>=0 && centre>=0 && strength>=0 && fog>=0) {
        glGetUniformfv(shader,world,result.actualWorld.data()); glGetUniformfv(shader,range,result.actualRange.data());
        glGetUniformfv(shader,centre,result.actualCentre.data()); glGetUniformfv(shader,strength,&result.actualStrength);
        glGetUniformfv(shader,fog,result.actualFog.data());
        result.uniformReadback=result.actualWorld==value.world && result.actualWvp==Identity && result.actualRange==value.range &&
            result.actualCentre==value.centre && result.actualStrength==value.strength && result.actualMode==value.mode &&
            result.actualFog==std::array<float,3>{{.6f,.7f,.8f}} && result.actualMaskUnit==0;
    }
    glDisable(GL_BLEND); glDisable(GL_FRAMEBUFFER_SRGB); glDepthMask(GL_TRUE);
    glClearDepth(.7); glClearColor(.2f,.55f,.85f,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    result.colour.resize(Edge*Edge*4); result.beforeDepth.resize(Edge*Edge); result.afterDepth.resize(Edge*Edge);
    glReadPixels(0,0,Edge,Edge,GL_DEPTH_COMPONENT,GL_FLOAT,result.beforeDepth.data());
    glDepthMask(GL_FALSE); glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glDrawArrays(GL_TRIANGLES,0,6);
    glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_UNSIGNED_BYTE,result.colour.data());
    glReadPixels(0,0,Edge,Edge,GL_DEPTH_COMPONENT,GL_FLOAT,result.afterDepth.data());
    require(glGetError()==GL_NO_ERROR,"Range actual draw/uniform/readback GL error");
    return result;
}
bool depthExact(const RangePixels& value) {
    return value.beforeDepth.size()==value.afterDepth.size() &&
        std::memcmp(value.beforeDepth.data(),value.afterDepth.data(),
                    value.beforeDepth.size()*sizeof(float))==0;
}
bool exactPixels(const RangePixels& actual, const RangePixels& old) {
    return actual.colour==old.colour && depthExact(actual) && depthExact(old) &&
        actual.beforeDepth==old.beforeDepth;
}
bool rangePixelsMatch(const RangePixels& actual, const RangeCase& value) {
    if(!actual.uniformReadback || !depthExact(actual)) return false;
    const std::array<double,3> dark{{.035,.043,.054}},fog{{.6,.7,.8}};
    for(int y=0;y<Edge;++y) for(int x=0;x<Edge;++x) {
        const double localX=(2*x+1.0)/Edge-1,localY=(2*y+1.0)/Edge-1;
        const double worldX=value.world[0]*localX+value.world[4]*localY+value.world[12];
        const double worldZ=value.world[2]*localX+value.world[6]*localY+value.world[14];
        const float coverage=rangeCoverage(worldX,worldZ,value);
        const bool mask=value.pattern==1 || (value.pattern==2 && ((x/4+y/4)%2==0));
        const bool drawn=mask && coverage>0;
        for(int c=0;c<3;++c) {
            const double component=drawn ?
                (value.mode>=.5f ? rangeDecode(fog[c])*(1-coverage)+rangeDecode(dark[c])*coverage :
                                 fog[c]*(1-coverage)+dark[c]*coverage) :
                std::array<double,3>{{.2,.55,.85}}[c];
            if(std::abs(int(actual.colour[(y*Edge+x)*4+c])-std::lround(component*255))>1) return false;
        }
        if(actual.colour[(y*Edge+x)*4+3]!=255) return false;
    }
    return true;
}
std::string replaceOnce(std::string source, const std::string& from, const std::string& to) {
    const auto offset=source.find(from); require(offset!=std::string::npos &&
        source.find(from,offset+from.size())==std::string::npos,"Missing/duplicate range fault seam: "+from);
    source.replace(offset,from.size(),to); return source;
}
// Frozen pre-range production FS at 2a448fdab39523f797f4339f154c824054d720ea.
const std::string OldMaskFragment=R"BASELINE(#version 150

// The legacy branch keeps authored display colours untouched. HDR scene
// shaders decode colour inputs before lighting/blending; alpha/data stay raw.
uniform float linearHdrMode;
vec3 sceneColour(vec3 authored)
{
    if (linearHdrMode < 0.5) return authored;
    vec3 c = max(authored, vec3(0.0));
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), c));
}


in vec2 boundaryUV;
uniform sampler2D caveBoundaryMask;
out vec4 fragColour;

void main()
{
    if (texture(caveBoundaryMask, boundaryUV).r < 0.5)
        discard;
    // The same unlit underground background used by the terrain fog.
    fragColour = vec4(sceneColour(vec3(0.035, 0.043, 0.054)), 1.0);
}
)BASELINE";
std::filesystem::path rangeEvidence;
void saveRange(const RangeCase& input, const RangePixels& actual) {
    if(rangeEvidence.empty()) return;
    const auto name=rangeEvidence/"raw"/input.name;
    auto bytes=[](const std::filesystem::path& path,const void* data,std::size_t count) {
        std::ofstream out(path,std::ios::binary); require(out.good(),"Cannot save range readback");
        out.write(static_cast<const char*>(data),std::streamsize(count)); require(out.good(),"Truncated range readback");
    };
    bytes(name.string()+".rgba8",actual.colour.data(),actual.colour.size());
    bytes(name.string()+".depth-before.f32",actual.beforeDepth.data(),actual.beforeDepth.size()*sizeof(float));
    bytes(name.string()+".depth-after.f32",actual.afterDepth.data(),actual.afterDepth.size()*sizeof(float));
    std::ofstream facts(name.string()+".facts.txt"); facts.precision(9);
    facts<<"scope=actual-full-production-VS-FS-64x64-RGBA8-depth24\nviewRange="<<input.range[0]<<','<<input.range[1]
        <<"\ncentre="<<input.centre[0]<<','<<input.centre[1]<<"\nstrength="<<input.strength
        <<"\nlinearHdrMode="<<input.mode<<"\npattern="<<input.pattern<<"\nuniform_readback="<<actual.uniformReadback
        <<"\ndepth_exact="<<depthExact(actual)<<"\nworld=";
    for(float v:input.world) facts<<v<<',';
    facts<<"\nworld_actual=";for(float v:actual.actualWorld) facts<<v<<',';
    facts<<"\nwvp_actual=";for(float v:actual.actualWvp) facts<<v<<',';
    facts<<"\nrange_actual="<<actual.actualRange[0]<<','<<actual.actualRange[1]
        <<"\ncentre_actual="<<actual.actualCentre[0]<<','<<actual.actualCentre[1]
        <<"\nstrength_actual="<<actual.actualStrength<<"\nmode_actual="<<actual.actualMode
        <<"\nmask_unit_actual="<<actual.actualMaskUnit<<"\nfog_actual=";
    for(float v:actual.actualFog) facts<<v<<',';
    facts<<"\nordinary_input=NOT_RUN\nOgre_logic_centre_binding=NOT_RUN\n";
    require(facts.good(),"Cannot save range readback facts");
}
void rangeChecks(GLuint candidate, GLuint buffer, GLuint atlas,
                 const std::string& vertex, const std::string& fragment) {
    bool interfaces=true;
    for(const char* name:{"world","worldViewProj","viewRange","viewRangeCentre","viewRangeStrength","fogColour","linearHdrMode","caveBoundaryMask"})
        interfaces=interfaces && glGetUniformLocation(candidate,name)>=0;
    check("range-actual-linked-uniform-interface",interfaces);
    if(!rangeEvidence.empty()) std::ofstream(rangeEvidence/"inputs/pre-range-baseline.frag")<<OldMaskFragment;
    const GLuint old=program(vertex,OldMaskFragment);
    for(float mode:{0.f,1.f}) {
        auto off=rangeCase("strength0-far-mode"+std::to_string(int(mode)),100,-100,0,mode);off.pattern=2;
        const auto current=drawRange(candidate,buffer,atlas,off),baseline=drawRange(old,buffer,atlas,off);
        saveRange(off,current);auto offBaseline=off;offBaseline.name+="-pre-range-baseline";saveRange(offBaseline,baseline);
        check(off.name+"-old-pixels-depth-exact",current.uniformReadback && exactPixels(current,baseline));
        auto near=rangeCase("strength1-near-mode"+std::to_string(int(mode)),2,3,1,mode);
        const auto newNear=drawRange(candidate,buffer,atlas,near),oldNear=drawRange(old,buffer,atlas,near);
        saveRange(near,newNear);auto nearBaseline=near;nearBaseline.name+="-pre-range-baseline";saveRange(nearBaseline,oldNear);
        check(near.name+"-old-pixels-depth-exact",newNear.uniformReadback && exactPixels(newNear,oldNear));
    }
    std::vector<RangeCase> cases{
        rangeCase("far-discard",16,0),rangeCase("negative-far-discard",-16,-16),
        rangeCase("partial-fog-legacy",11,0),rangeCase("partial-fog-linear",11,0,1,1),
        rangeCase("negative-partial",-43,-48,1,0,{{-32,-48}}),
        rangeCase("diagonal-chebyshev",11,11),
        rangeCase("logic-centre-nonzero",139,-85,1,0,{{128,-96}}),
        rangeCase("partial-strength",11,0,.25f),rangeCase("clamped-strength-high",16,0,1.25f),
        rangeCase("clamped-strength-low",100,-100,-.25f)
    };
    auto masked=rangeCase("mask-discard-with-range",11,0);masked.pattern=0;cases.push_back(masked);
    auto mixed=rangeCase("mixed-mask-partial",11,0);mixed.pattern=2;cases.push_back(mixed);
    for(const auto& value:cases) {
        const auto actual=drawRange(candidate,buffer,atlas,value);saveRange(value,actual);
        check(value.name+"-independent-colour-and-depth",rangePixelsMatch(actual,value));
    }
    auto invalid=rangeCase("invalid-range-keeps-old-mask",100,-100);invalid.range={{14,8}};
    const auto unchanged=drawRange(candidate,buffer,atlas,invalid),baseline=drawRange(old,buffer,atlas,invalid);
    saveRange(invalid,unchanged);check(invalid.name+"-pixels-depth-exact",unchanged.uniformReadback && exactPixels(unchanged,baseline));
    const auto wrongCoverage=replaceOnce(fragment,
        "float coverage = 1.0 - smoothstep(viewRange.x, viewRange.y, edgeDistance);",
        "float coverage = 1.0 - 0.5 * smoothstep(viewRange.x, viewRange.y, edgeDistance);");
    const auto noRangeDiscard=replaceOnce(fragment,"if (coverage <= 0.0) discard;","if (coverage < -1.0) discard;");
    const auto swappedWorld=replaceOnce(vertex,"boundaryWorldPosition = (world * vertex).xyz;","boundaryWorldPosition = (world * vertex).zyx;");
    if(!rangeEvidence.empty()) {
        for(const auto& source:{std::pair<std::string,std::string>{"half-range-coverage.frag",wrongCoverage},
             {"no-range-discard.frag",noRangeDiscard},{"swapped-world-position.vert",swappedWorld}}) {
            std::ofstream out(rangeEvidence/"faults"/source.first);out<<source.second;
        }
    }
    const GLuint coverageFault=program(vertex,wrongCoverage),discardFault=program(vertex,noRangeDiscard),worldFault=program(swappedWorld,fragment);
    const auto far=rangeCase("fault-far-domain",16,0),centre=rangeCase("fault-logic-centre-domain",139,-85,1,0,{{128,-96}});
    const auto badCoverage=drawRange(coverageFault,buffer,atlas,far),badDiscard=drawRange(discardFault,buffer,atlas,far),badWorld=drawRange(worldFault,buffer,atlas,centre);
    auto coverageInput=far;coverageInput.name="fault-half-range-coverage";saveRange(coverageInput,badCoverage);
    auto discardInput=far;discardInput.name="fault-no-range-discard";saveRange(discardInput,badDiscard);
    auto worldInput=centre;worldInput.name="fault-swapped-world-position";saveRange(worldInput,badWorld);
    check("reject-half-range-coverage-at-far-pixel-gate",badCoverage.uniformReadback && depthExact(badCoverage) && !rangePixelsMatch(badCoverage,far));
    check("reject-no-range-discard-at-far-pixel-gate",badDiscard.uniformReadback && depthExact(badDiscard) && !rangePixelsMatch(badDiscard,far));
    check("reject-swapped-world-position-at-centred-pixel-gate",badWorld.uniformReadback && depthExact(badWorld) && !rangePixelsMatch(badWorld,centre));
    glDeleteProgram(coverageFault);glDeleteProgram(discardFault);glDeleteProgram(worldFault);glDeleteProgram(old);
}

}
int main(int argc, char **argv) {
    try {
        require(argc==2 || argc==3,"Usage: cave-boundary-gpu <repository root> [new-range-evidence-directory]");
        if(argc==3) {
            rangeEvidence=std::filesystem::absolute(argv[2]);
            require(!std::filesystem::exists(rangeEvidence),"Refusing to overwrite range evidence");
            std::filesystem::create_directories(rangeEvidence/"inputs");
            std::filesystem::create_directories(rangeEvidence/"faults");
            std::filesystem::create_directories(rangeEvidence/"raw");
        }
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
        if(!rangeEvidence.empty()) {
            std::ofstream(rangeEvidence/"inputs/HelloMine3DCaveBoundary.vert")<<vertex;
            std::ofstream(rangeEvidence/"inputs/HelloMine3DCaveBoundary.frag")<<fragment;
        }
        const GLuint candidate=program(vertex,fragment);
        for(int slot : {0,31,32,2047}) for(int pattern : {0,1,2,3})
            check("actual-shader-slot-"+std::to_string(slot)+"-pattern-"+std::to_string(pattern),
                  probe(candidate,buffer,atlas,slot,pattern));
        check("mask-does-not-write-depth",probe(candidate,buffer,atlas,0,1,true));
        std::string noDiscard=fragment;
        const std::string maskCondition="texture(caveBoundaryMask, boundaryUV).r < 0.5";
        const auto condition=noDiscard.find(maskCondition); require(condition!=std::string::npos,"Missing mask discard fault seam");
        noDiscard.replace(condition,maskCondition.size(),"texture(caveBoundaryMask, boundaryUV).r < -1.0");
        const GLuint faulty=program(vertex,noDiscard);
        check("reject-no-discard",!probe(faulty,buffer,atlas,31,2));
        std::string swapped=vertex;
        const auto uv=swapped.find("boundaryUV = uv0;"); require(uv!=std::string::npos,"Missing UV fault seam");
        swapped.replace(uv,std::string("boundaryUV = uv0;").size(),"boundaryUV = uv0.yx;");
        const GLuint faultyUV=program(swapped,fragment);
        check("reject-transposed-uv",!probe(faultyUV,buffer,atlas,0,3));
        glDeleteProgram(faultyUV); glDeleteProgram(faulty);
        rangeChecks(candidate,buffer,atlas,vertex,fragment);
        check("all-range-operations-GL-error-zero",glGetError()==GL_NO_ERROR);
        glDeleteProgram(candidate);
        glDeleteTextures(1,&atlas); glDeleteTextures(1,&colour); glDeleteTextures(1,&depth);
        glDeleteFramebuffers(1,&fbo); glDeleteBuffers(1,&buffer); glDeleteVertexArrays(1,&vao);
        CGLSetCurrentContext(nullptr); CGLDestroyContext(context);
        std::cout<<"[CAVE_BOUNDARY_GPU] checks="<<checks<<" failures="<<failures<<'\n';
        return failures==0?0:1;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
