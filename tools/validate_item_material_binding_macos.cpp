// Replay exported production item RenderOperation bytes through production GLSL.
// clang++ -std=c++17 -Wall -Wextra -Werror -Wno-deprecated-declarations \
//   tools/validate_item_material_binding_macos.cpp -framework OpenGL \
//   -framework CoreGraphics -framework ImageIO -framework CoreFoundation -o <tool>
// <tool> <repository-resources-root> <raw-operation-directory> <new-output-directory>
// This diagnostic does not create Ogre materials or exercise client TUS binding.
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../src/HelloMine3D/World/Block/TerrainTextureArray.h"

namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
constexpr int Edge = 256;
constexpr double HalfClip = .8;
void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}
Bytes readBytes(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(bool(input), "Cannot read " + path.string());
    const auto size = input.tellg();
    require(size >= 0 && size <= 32 * 1024 * 1024, "Input outside 32 MiB bound: " + path.string());
    Bytes result(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!result.empty()) input.read(reinterpret_cast<char*>(result.data()), result.size());
    require(bool(input), "Truncated input " + path.string());
    return result;
}
std::string sha256(const Bytes& bytes)
{
    unsigned char digest[CC_SHA256_DIGEST_LENGTH]{};
    CC_SHA256(bytes.data(), static_cast<CC_LONG>(bytes.size()), digest);
    std::ostringstream out;
    for (const auto byte : digest) out << std::hex << std::setfill('0') << std::setw(2) << int(byte);
    return out.str();
}
std::map<std::string, std::string> inputs;
Bytes inputBytes(const fs::path& path)
{
    const auto absolute = fs::canonical(path);
    auto data = readBytes(absolute);
    const auto hash = sha256(data);
    const auto found = inputs.find(absolute.string());
    require(found == inputs.end() || found->second == hash, "Input changed while loading: " + absolute.string());
    inputs[absolute.string()] = hash;
    return data;
}
std::string inputText(const fs::path& path)
{
    auto bytes = inputBytes(path);
    return std::string(bytes.begin(), bytes.end());
}
std::string quote(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}

// Strict bounded JSON reader for the explicit raw-operation-v1 interchange.
// No dependency on Ogre and no transformation of exported binary buffers.
struct Json {
    enum Kind { Null, Boolean, Number, String, Array, Object } kind = Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Json> array;
    std::map<std::string, Json> object;
    const Json& at(const std::string& key) const {
        require(kind == Object && object.count(key), "Missing JSON key " + key);
        return object.at(key);
    }
    std::string text() const { require(kind == String, "Expected JSON string"); return string; }
    std::size_t integer() const {
        require(kind == Number && number >= 0 && number <= 16777216 && std::floor(number) == number,
                "Expected bounded nonnegative JSON integer");
        return static_cast<std::size_t>(number);
    }
    double real() const { require(kind == Number && std::isfinite(number), "Expected finite JSON number"); return number; }
    bool flag() const { require(kind == Boolean, "Expected JSON boolean"); return boolean; }
    const std::vector<Json>& list() const { require(kind == Array, "Expected JSON array"); return array; }
};
class JsonReader {
    const std::string& source;
    std::size_t at = 0;
    void space() { while (at < source.size() && std::isspace(static_cast<unsigned char>(source[at]))) ++at; }
    bool take(char c) { space(); if (at < source.size() && source[at] == c) { ++at; return true; } return false; }
    std::string string() {
        require(take('"'), "Expected JSON string opening quote");
        std::string value;
        while (at < source.size()) {
            const unsigned char c = source[at++];
            if (c == '"') return value;
            require(c >= 32, "Unescaped JSON control character");
            if (c != '\\') { value += c; continue; }
            require(at < source.size(), "Truncated JSON escape");
            const char escaped = source[at++];
            if (escaped == '"' || escaped == '\\' || escaped == '/') value += escaped;
            else if (escaped == 'b') value += '\b';
            else if (escaped == 'f') value += '\f';
            else if (escaped == 'n') value += '\n';
            else if (escaped == 'r') value += '\r';
            else if (escaped == 't') value += '\t';
            else if (escaped == 'u') {
                require(at + 4 <= source.size(), "Truncated JSON unicode escape");
                unsigned code = 0;
                for (int i = 0; i < 4; ++i) {
                    const char digit = source[at++];
                    const int v = digit >= '0' && digit <= '9' ? digit-'0' :
                        digit >= 'a' && digit <= 'f' ? digit-'a'+10 :
                        digit >= 'A' && digit <= 'F' ? digit-'A'+10 : -1;
                    require(v >= 0, "Invalid JSON unicode escape"); code = code * 16 + unsigned(v);
                }
                require(code < 128, "Raw-operation protocol requires ASCII escaped paths");
                value += static_cast<char>(code);
            } else require(false, "Unknown JSON escape");
        }
        throw std::runtime_error("Unterminated JSON string");
    }
    Json value(unsigned depth) {
        require(depth < 16, "JSON nesting exceeds protocol bound"); space();
        require(at < source.size(), "Truncated JSON value"); Json result;
        if (source[at] == '"') { result.kind = Json::String; result.string = string(); return result; }
        if (take('{')) {
            result.kind = Json::Object;
            if (take('}')) return result;
            do {
                const auto key = string(); require(take(':'), "Missing JSON colon");
                require(result.object.emplace(key, value(depth+1)).second, "Duplicate JSON key " + key);
            } while (take(','));
            require(take('}'), "Missing JSON object terminator"); return result;
        }
        if (take('[')) {
            result.kind = Json::Array;
            if (take(']')) return result;
            do { require(result.array.size() < 256, "JSON array exceeds protocol bound"); result.array.push_back(value(depth+1)); } while (take(','));
            require(take(']'), "Missing JSON array terminator"); return result;
        }
        for (const auto& token : {std::pair<const char*, Json::Kind>{"true", Json::Boolean}, {"false", Json::Boolean}, {"null", Json::Null}}) {
            const std::string literal(token.first);
            if (source.compare(at, literal.size(), literal) == 0) {
                at += literal.size(); result.kind = token.second; result.boolean = literal == "true"; return result;
            }
        }
        const auto begin = at;
        if (source[at] == '-') ++at;
        require(at < source.size() && std::isdigit(static_cast<unsigned char>(source[at])), "Invalid JSON number");
        if (source[at] == '0') ++at;
        else while (at < source.size() && std::isdigit(static_cast<unsigned char>(source[at]))) ++at;
        if (at < source.size() && source[at] == '.') {
            ++at; const auto fraction = at;
            while (at < source.size() && std::isdigit(static_cast<unsigned char>(source[at]))) ++at;
            require(at > fraction, "Empty JSON fraction");
        }
        if (at < source.size() && (source[at] == 'e' || source[at] == 'E')) {
            ++at; if (at < source.size() && (source[at] == '-' || source[at] == '+')) ++at;
            const auto exponent = at;
            while (at < source.size() && std::isdigit(static_cast<unsigned char>(source[at]))) ++at;
            require(at > exponent, "Empty JSON exponent");
        }
        result.kind = Json::Number; result.number = std::stod(source.substr(begin, at-begin));
        require(std::isfinite(result.number), "Nonfinite JSON number"); return result;
    }
public:
    explicit JsonReader(const std::string& text) : source(text) { require(text.size() < 128 * 1024, "Descriptor too large"); }
    Json parse() { auto result = value(0); space(); require(at == source.size(), "Trailing JSON bytes"); return result; }
};

using Vec = std::array<double, 3>;
using Mat = std::array<double, 16>; // Row-major arithmetic; transpose only at GL upload.
Vec subtract(const Vec& a, const Vec& b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
double dot(const Vec& a, const Vec& b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
Vec unit(const Vec& a) { const auto length = std::sqrt(dot(a,a)); require(length > 1e-7, "Degenerate exported face"); return {a[0]/length,a[1]/length,a[2]/length}; }
Vec transform(const Mat& m, const Vec& p) {
    const double w = m[12]*p[0]+m[13]*p[1]+m[14]*p[2]+m[15];
    require(std::abs(w-1) < 1e-5, "Expected affine production world transform");
    return {m[0]*p[0]+m[1]*p[1]+m[2]*p[2]+m[3],
            m[4]*p[0]+m[5]*p[1]+m[6]*p[2]+m[7],
            m[8]*p[0]+m[9]*p[1]+m[10]*p[2]+m[11]};
}
Mat multiply(const Mat& a, const Mat& b) {
    Mat out{};
    for (unsigned r=0;r<4;++r) for (unsigned c=0;c<4;++c)
        for (unsigned k=0;k<4;++k) out[r*4+c] += a[r*4+k]*b[k*4+c];
    return out;
}
void matrix(GLuint program, const char* name, const Mat& value) {
    std::array<float,16> columns{};
    for (unsigned r=0;r<4;++r) for (unsigned c=0;c<4;++c) columns[c*4+r] = static_cast<float>(value[r*4+c]);
    glUniformMatrix4fv(glGetUniformLocation(program,name),1,GL_FALSE,columns.data());
}
struct Element { unsigned source, offset, semantic, index, components; };
struct Buffer { unsigned source, stride; std::size_t vertices; Bytes bytes; GLuint gl = 0; };
struct Operation {
    std::string name, scope, material, atlas, array;
    std::size_t vertexStart, vertexCount, indexStart, indexCount;
    unsigned atlasPixels, tilePixels, tiles, profile;
    bool usesArray;
    GLenum indexType;
    unsigned indexBytes;
    std::vector<Buffer> buffers;
    std::vector<Element> elements;
    Bytes indices;
    Mat world{};
    GLuint ibo = 0;
    const Buffer& buffer(unsigned source) const {
        const auto found = std::find_if(buffers.begin(),buffers.end(),[source](const auto& b){return b.source == source;});
        require(found != buffers.end(), "Missing source binding"); return *found;
    }
    const Element& element(unsigned semantic, unsigned index) const {
        const auto found = std::find_if(elements.begin(),elements.end(),[=](const auto& e){return e.semantic == semantic && e.index == index;});
        require(found != elements.end(), "Missing declared production attribute"); return *found;
    }
    double attribute(std::size_t vertex, unsigned semantic, unsigned index, unsigned component) const {
        const auto& e = element(semantic,index); const auto& b = buffer(e.source);
        require(vertex < vertexCount && component < e.components, "Attribute read outside operation");
        const auto offset = (vertexStart+vertex)*b.stride+e.offset+component*sizeof(float);
        require(offset+sizeof(float) <= b.bytes.size(), "Attribute read outside original VBO");
        float value; std::memcpy(&value,b.bytes.data()+offset,sizeof(value));
        require(std::isfinite(value), "Nonfinite production attribute"); return value;
    }
    Vec position(std::size_t vertex) const { return {attribute(vertex,1,0,0),attribute(vertex,1,0,1),attribute(vertex,1,0,2)}; }
    unsigned index(std::size_t index) const {
        const auto offset = (indexStart+index)*indexBytes;
        require(offset+indexBytes <= indices.size(), "Index outside original IBO");
        std::uint32_t value = 0; std::memcpy(&value,indices.data()+offset,indexBytes); return value;
    }
};
fs::path sidecar(const fs::path& directory, const std::string& file) {
    require(!file.empty() && fs::path(file).filename() == fs::path(file), "Binary sidecar must be a filename");
    return directory/file;
}
Operation loadOperation(const fs::path& path) {
    const auto source = inputText(path); const auto json = JsonReader(source).parse(); Operation op;
    require(json.at("schema").text() == "hellomine3d-item-binding-raw-operation-v1", "Unexpected raw-operation schema");
    require(json.at("operation").text() == "triangle_list", "Only actual indexed triangle lists supported");
    op.name=json.at("name").text(); op.scope=json.at("scope").text(); op.material=json.at("material_name").text();
    op.atlas=json.at("atlas_source").text(); op.array=json.at("array_source").text();
    op.vertexStart=json.at("vertex_start").integer(); op.vertexCount=json.at("vertex_count").integer();
    op.indexStart=json.at("index_start").integer(); op.indexCount=json.at("index_count").integer();
    op.atlasPixels=unsigned(json.at("atlas_pixels").integer()); op.tilePixels=unsigned(json.at("tile_pixels").integer());
    op.tiles=unsigned(json.at("tiles_per_row").integer()); op.profile=unsigned(json.at("profile_version").integer());
    op.usesArray=json.at("uses_array").flag();
    require(op.vertexCount == 24 && op.indexCount == 36, "Five-cube route requires actual 24-vertex/36-index operations");
    require(op.tiles >= 1 && op.tiles <= 256 && op.tilePixels >= 2 && op.atlasPixels == op.tiles*op.tilePixels && op.atlasPixels <= 8192,
            "Inconsistent frozen profile dimensions");
    const auto type=json.at("index_type").text(); require(type == "u16" || type == "u32", "Unsupported original index type");
    op.indexType=type == "u16" ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT; op.indexBytes=type == "u16" ? 2 : 4;
    for (const auto& b : json.at("vertex_buffers").list()) {
        Buffer buffer{unsigned(b.at("source").integer()),unsigned(b.at("stride").integer()),b.at("vertices").integer(),{},0};
        require(buffer.stride > 0 && buffer.stride <= 1024 && buffer.vertices >= op.vertexStart+op.vertexCount, "Invalid original VBO extent");
        require(std::none_of(op.buffers.begin(),op.buffers.end(),[&](const auto& old){return old.source == buffer.source;}), "Duplicate binding source");
        buffer.bytes=inputBytes(sidecar(path.parent_path(),b.at("file").text()));
        require(buffer.bytes.size() == b.at("bytes").integer() && buffer.bytes.size() == buffer.vertices*buffer.stride,
                "Original VBO length does not match descriptor");
        op.buffers.push_back(std::move(buffer));
    }
    require(!op.buffers.empty() && op.buffers.size() <= 8, "Unexpected VBO binding count");
    for (const auto& e : json.at("elements").list()) {
        Element element{unsigned(e.at("source").integer()),unsigned(e.at("offset").integer()),unsigned(e.at("semantic").integer()),
                        unsigned(e.at("semantic_index").integer()),unsigned(e.at("components").integer())};
        require(element.components >= 1 && element.components <= 4 && e.at("type").integer() == element.components-1 &&
                e.at("bytes").integer() == element.components*sizeof(float), "Only declared Ogre FLOAT1..FLOAT4 attributes supported");
        require(element.offset+element.components*sizeof(float) <= op.buffer(element.source).stride, "Declaration exceeds actual stride");
        require(std::none_of(op.elements.begin(),op.elements.end(),[&](const auto& old){return old.semantic == element.semantic && old.index == element.index;}), "Duplicate semantic declaration");
        op.elements.push_back(element);
    }
    require(op.element(1,0).components == 3 && op.element(7,0).components == 2 && op.element(7,1).components == 2 &&
            op.element(7,2).components == 1 && op.element(7,3).components == 1, "Unexpected production item declaration");
    const auto& ibo = json.at("index_buffer"); op.indices=inputBytes(sidecar(path.parent_path(),ibo.at("file").text()));
    require(op.indices.size() == ibo.at("bytes").integer() && (op.indexStart+op.indexCount)*op.indexBytes <= op.indices.size(), "Original IBO extent mismatch");
    const auto& world=json.at("world_transform_row_major").list(); require(world.size() == 16, "World matrix must have 16 original values");
    for (unsigned i=0;i<16;++i) op.world[i]=world[i].real();
    // Inspect original topology, but never replace its indices or vertices.
    constexpr unsigned corners[]{0,1,2,2,3,0};
    for (unsigned face=0;face<6;++face) for (unsigned i=0;i<6;++i)
        require(op.index(face*6+i) == face*4+corners[i], "Actual operation is not six consecutive quad faces");
    return op;
}

struct GlError { std::string stage; GLenum code; };
std::vector<GlError> glErrors;
void recordErrors(const std::string& stage) {
    for (unsigned i=0;i<64;++i) {
        const GLenum error = glGetError(); if (error == GL_NO_ERROR) return;
        glErrors.push_back({stage,error});
    }
    throw std::runtime_error("OpenGL error queue exceeded bound at " + stage);
}
GLuint createProgram(const std::string& vertexSource, const std::string& fragmentSource, bool array) {
    const auto program=glCreateProgram();
    for (const auto& entry : {std::pair<GLenum,std::string>{GL_VERTEX_SHADER,vertexSource},{GL_FRAGMENT_SHADER,fragmentSource}}) {
        auto source=entry.second;
        if (array && entry.first == GL_FRAGMENT_SHADER) {
            require(source.rfind("#version 150\n",0) == 0, "Unexpected production shader version");
            source.insert(source.find('\n')+1,"#define TERRAIN_ARRAY 1\n");
        }
        const auto shader=glCreateShader(entry.first); const char* text=source.c_str();
        glShaderSource(shader,1,&text,nullptr); glCompileShader(shader);
        GLint success=0; glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
        char log[16384]{}; glGetShaderInfoLog(shader,sizeof(log),nullptr,log);
        require(success == GL_TRUE,"Production shader compile failed: " + std::string(log));
        glAttachShader(program,shader); glDeleteShader(shader);
    }
    glLinkProgram(program); GLint success=0; glGetProgramiv(program,GL_LINK_STATUS,&success);
    char log[16384]{}; glGetProgramInfoLog(program,sizeof(log),nullptr,log);
    require(success == GL_TRUE,"Production shader link failed: " + std::string(log));
    recordErrors(array ? "array-program" : "atlas-program"); return program;
}
struct Texture { GLuint gl=0; unsigned edge=0, layers=1; Bytes pixels; };
Texture loadAtlas(const std::string& path, unsigned expectedEdge) {
    inputBytes(path); Texture texture;
    const auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(path.data()),path.size(),false);
    require(url,"Cannot create atlas URL"); const auto source=CGImageSourceCreateWithURL(url,nullptr); CFRelease(url);
    require(source,"ImageIO cannot decode effective atlas"); const auto image=CGImageSourceCreateImageAtIndex(source,0,nullptr);
    require(image,"ImageIO atlas has no image"); texture.edge=unsigned(CGImageGetWidth(image));
    require(texture.edge == expectedEdge && CGImageGetHeight(image) == expectedEdge,"Decoded atlas differs from frozen dimensions");
    texture.pixels.resize(std::size_t(texture.edge)*texture.edge*4);
    const auto colour=CGColorSpaceCreateDeviceRGB();
    const auto context=CGBitmapContextCreate(texture.pixels.data(),texture.edge,texture.edge,8,texture.edge*4,colour,
        kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast);
    require(context,"Cannot decode RGBA atlas"); CGContextSetBlendMode(context,kCGBlendModeCopy);
    CGContextDrawImage(context,CGRectMake(0,0,texture.edge,texture.edge),image);
    CGContextRelease(context); CGColorSpaceRelease(colour); CGImageRelease(image); CFRelease(source);
    // The replay uses straight RGBA8. The five opaque cube tiles avoid colour
    // recovery ambiguity for partly transparent icon texels (excluded here).
    for (std::size_t i=0;i<texture.pixels.size();i+=4) {
        const auto alpha=texture.pixels[i+3];
        if (alpha && alpha < 255) for (unsigned c=0;c<3;++c)
            texture.pixels[i+c]=static_cast<unsigned char>(std::min(255u,(unsigned(texture.pixels[i+c])*255u+alpha/2u)/alpha));
    }
    glGenTextures(1,&texture.gl); glBindTexture(GL_TEXTURE_2D,texture.gl);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,texture.edge,texture.edge,0,GL_RGBA,GL_UNSIGNED_BYTE,texture.pixels.data());
    recordErrors("atlas-load"); return texture;
}
Texture loadArray(const std::string& path) {
    inputBytes(path); const auto payload=TerrainTextureArray::load(path); Texture texture;
    require(payload.edge == 64 && payload.layers == 256,"Expected frozen M1 64x64/256-layer payload");
    texture.edge=payload.edge; texture.layers=payload.layers;
    texture.pixels.assign(payload.rgba.begin(),payload.rgba.begin()+std::size_t(payload.edge)*payload.edge*payload.layers*4);
    glGenTextures(1,&texture.gl); glBindTexture(GL_TEXTURE_2D_ARRAY,texture.gl);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_REPEAT); glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,texture.edge,texture.edge,texture.layers,0,GL_RGBA,GL_UNSIGNED_BYTE,texture.pixels.data());
    recordErrors("array-load"); return texture;
}
void value(GLuint p,const char* name,float v) { glUniform1f(glGetUniformLocation(p,name),v); }
void uniforms(GLuint p,const Operation& op) {
    glUseProgram(p);
    for (const auto* sampler : {"terrainAtlas","terrainArray"}) glUniform1i(glGetUniformLocation(p,sampler),0);
    value(p,"atlasPixels",float(op.atlasPixels)); value(p,"tilePixels",float(op.tilePixels)); value(p,"tilesPerRow",float(op.tiles));
    value(p,"colourSaturation",1); value(p,"toneGamma",1); value(p,"greenSuppression",0); value(p,"greenRedShift",0);
    value(p,"playerExposure",1); value(p,"environmentLight",1); value(p,"surfaceLightingStrength",0); value(p,"sunIntensity",0);
    value(p,"fogDensity",0); value(p,"fogDirectionalStrength",0); value(p,"viewRangeStrength",0); value(p,"alphaCutoff",.4999f);
    for (const auto* name : {"fogColour","fogSunwardColour","sunDirection","sunColour","cameraPosition"})
        glUniform3f(glGetUniformLocation(p,name),0,0,0);
    glUniform2f(glGetUniformLocation(p,"viewRange"),0,0); glUniform2f(glGetUniformLocation(p,"viewRangeCentre"),0,0);
    matrix(p,"world",op.world); matrix(p,"worldView",op.world);
}
void upload(Operation& op) {
    for (auto& buffer : op.buffers) {
        glGenBuffers(1,&buffer.gl); glBindBuffer(GL_ARRAY_BUFFER,buffer.gl);
        glBufferData(GL_ARRAY_BUFFER,buffer.bytes.size(),buffer.bytes.data(),GL_STATIC_DRAW);
    }
    glGenBuffers(1,&op.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,op.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,op.indices.size(),op.indices.data(),GL_STATIC_DRAW);
    recordErrors(op.name+"/verbatim-upload");
}
void bindAttributes(GLuint p,const Operation& op) {
    for (const auto& entry : {std::pair<const char*,std::pair<unsigned,unsigned>>{"vertex",{1,0}},
         {"uv0",{7,0}},{"uv1",{7,1}},{"uv2",{7,2}},{"uv3",{7,3}}}) {
        const auto location=glGetAttribLocation(p,entry.first);
        require(location >= 0,"Production shader optimized required item input: " + std::string(entry.first));
        const auto& e=op.element(entry.second.first,entry.second.second); const auto& buffer=op.buffer(e.source);
        glBindBuffer(GL_ARRAY_BUFFER,buffer.gl); glEnableVertexAttribArray(GLuint(location));
        glVertexAttribPointer(GLuint(location),GLint(e.components),GL_FLOAT,GL_FALSE,GLsizei(buffer.stride),
                              reinterpret_cast<const void*>(std::uintptr_t(e.offset)));
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,op.ibo);
}
Mat faceProjection(const Operation& op,unsigned face) {
    const auto p0=transform(op.world,op.position(face*4)),p1=transform(op.world,op.position(face*4+1));
    const auto p2=transform(op.world,op.position(face*4+2)),p3=transform(op.world,op.position(face*4+3));
    const auto h=subtract(p3,p0),v=subtract(p0,p1); const auto hu=unit(h),vu=unit(v);
    require(std::abs(dot(hu,vu)) < 1e-5,"Actual world face is not rectangular");
    const Vec center{(p0[0]+p2[0])/2,(p0[1]+p2[1])/2,(p0[2]+p2[2])/2};
    const double hs=2*HalfClip/std::sqrt(dot(h,h)),vs=2*HalfClip/std::sqrt(dot(v,v));
    Mat projection{};
    for (unsigned c=0;c<3;++c) { projection[c]=hu[c]*hs; projection[4+c]=vu[c]*vs; }
    projection[3]=-dot(hu,center)*hs; projection[7]=-dot(vu,center)*vs; projection[15]=1;
    return multiply(projection,op.world);
}
struct Case { const char* name; int sideX,sideY,topX,topY; };
constexpr std::array<Case,5> Cases{{{"OakPlank",5,1,5,1},{"Cobblestone",7,1,7,1},
    {"Chest",0,1,0,1},{"Workbench",1,1,1,1},{"OakBark",4,0,5,0}}};
void validateAuthority(const fs::path& root,const Case& c) {
    // These expectations stay independent of emitted uv0 and renderer helpers.
    const auto text=inputText(root/"media/blocks"/(std::string(c.name)+".block"));
    std::istringstream stream(text); std::string key;
    std::map<std::string,std::pair<int,int>> coordinates;
    while (stream >> key) if (key == "TexAll" || key == "TexTop" || key == "TexSide" || key == "TexBottom") {
        int x=-1,y=-1; stream >> x >> y; require(bool(stream),"Bad actual block texture authority"); coordinates[key]={x,y};
    }
    const std::pair<int,int> side{c.sideX,c.sideY},top{c.topX,c.topY};
    if (coordinates.count("TexAll")) require(coordinates.at("TexAll") == side && side == top,"Frozen tile expectation differs from actual block authority");
    else require(coordinates.count("TexSide") && coordinates.count("TexTop") && coordinates.count("TexBottom") &&
        coordinates.at("TexSide") == side && coordinates.at("TexTop") == top && coordinates.at("TexBottom") == top,
        "Frozen per-face expectation differs from actual block authority");
}
struct Check {
    std::string name;
    bool pass;
    unsigned face,point,x,y;
    int tileX,tileY;
    double repeatU,repeatV,light;
    std::array<float,4> expected{},actual{};
};
std::vector<Check> checks;
std::string frozenProfile;
void drawChecks(GLuint program,Operation& op,const Texture& texture,const Case& c,bool array) {
    uniforms(program,op); bindAttributes(program,op);
    glBindTexture(array ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D,texture.gl);
    const std::array<std::array<double,2>,4> probes{{{{.21,.31}},{{.72,.23}},{{.24,.76}},{{.74,.69}}}};
    for (unsigned face=0;face<6;++face) {
        matrix(program,"worldViewProj",faceProjection(op,face));
        glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
        glDrawElementsBaseVertex(GL_TRIANGLES,6,op.indexType,
            reinterpret_cast<const void*>(std::uintptr_t((op.indexStart+face*6)*op.indexBytes)),GLint(op.vertexStart));
        std::vector<float> pixels(std::size_t(Edge)*Edge*4);
        glReadPixels(0,0,Edge,Edge,GL_RGBA,GL_FLOAT,pixels.data());
        recordErrors(op.name+"/"+(array ? "array" : "atlas")+"/face"+std::to_string(face));
        const int tx=face < 4 ? c.sideX : c.topX,ty=face < 4 ? c.sideY : c.topY;
        for (unsigned point=0;point<probes.size();++point) {
            const auto px=unsigned(std::lround((1-HalfClip+2*HalfClip*probes[point][0])*Edge/2-.5));
            const auto py=unsigned(std::lround((1+HalfClip-2*HalfClip*probes[point][1])*Edge/2-.5));
            // Pixel centre projected onto this original face. Expected repeat
            // orientation is the frozen corner contract, never read from uv1.
            const double u=((2*(px+.5)/Edge-1)+HalfClip)/(2*HalfClip);
            const double v=(HalfClip-(2*(py+.5)/Edge-1))/(2*HalfClip);
            const auto l0=op.attribute(face*4,7,2,0),l1=op.attribute(face*4+1,7,2,0);
            const auto l2=op.attribute(face*4+2,7,2,0),l3=op.attribute(face*4+3,7,2,0);
            const double light=v >= u ? (1-v)*l0+(v-u)*l1+u*l2 : (1-u)*l0+v*l2+(u-v)*l3;
            const unsigned texX=array ? unsigned(std::floor(u*texture.edge)) : unsigned(tx*int(op.tilePixels)+std::floor(.5+u*(op.tilePixels-1)));
            const unsigned texY=array ? unsigned(std::floor(v*texture.edge)) : unsigned(ty*int(op.tilePixels)+std::floor(.5+v*(op.tilePixels-1)));
            const unsigned layer=array ? unsigned(ty*16+tx) : 0; // Frozen HMT semantic stride is16.
            require(texX < texture.edge && texY < texture.edge && layer < texture.layers,"Independent expected texel outside resource");
            const auto source=(std::size_t(layer)*texture.edge*texture.edge+std::size_t(texY)*texture.edge+texX)*4;
            const float gain=float(.24+.76*std::clamp(light,0.,1.));
            Check check{op.name+"/"+(array ? "array" : "atlas")+"/face"+std::to_string(face)+"/point"+std::to_string(point),true,face,point,px,py,tx,ty,u,v,light,{},{}};
            const float alpha=texture.pixels[source+3]/255.f;
            for (unsigned channel=0;channel<4;++channel) {
                check.expected[channel]=alpha == 0 || (array && alpha < .4999f) ? 0.f :
                    texture.pixels[source+channel]/255.f*(channel < 3 ? gain : 1.f);
                check.actual[channel]=pixels[(std::size_t(py)*Edge+px)*4+channel];
                check.pass &= std::isfinite(check.actual[channel]) && std::abs(check.actual[channel]-check.expected[channel]) <= .002f;
            }
            checks.push_back(check);
            std::cout << "[ITEM_BINDING_GPU] " << (check.pass ? "PASS " : "FAIL ") << check.name << '\n';
        }
    }
}
void floatArray(std::ostream& out,const std::array<float,4>& values) {
    out << '['; for (unsigned i=0;i<4;++i) { if (i) out << ','; out << values[i]; } out << ']';
}
void receipt(const fs::path& output,const std::string& renderer,const std::string& version,const std::string& error,bool unchanged) {
    unsigned failures=0; for (const auto& check : checks) failures += !check.pass;
    const bool pass=error.empty() && !checks.empty() && !failures && glErrors.empty() && unchanged;
    std::ofstream out(output/"result.json"); require(bool(out),"Cannot write GPU receipt"); out << std::setprecision(9);
    out << "{\n  \"schema\":\"hellomine3d-item-binding-gpu-replay-v1\",\n  \"status\":" << quote(pass ? "PASS" : "FAIL")
        << ",\n  \"scope\":\"Actual exported held/drop cube VBO/IBO bytes and world transforms; production terrain GLSL; independent semantic tiles and four interior pixels per face. Software material stubs, no client bootstrap TUS, ImGui/map/gameplay, icon silhouette, or full48-chain acceptance.\",\n"
        << "  \"texture_decoder\":\"Effective source PNG ImageIO RGBA8 straight alpha; HMT validated by production bounded CPU loader. Diagnostic nearest base-level sampling, no Ogre texture-upload equivalence claim.\",\n"
        << "  \"geometry_policy\":\"Entire original source buffers uploaded byte-for-byte; original declaration/index type/starts bound; original six-index face slices drawn. Only camera projection and neutral diagnostic uniforms selected.\",\n"
        << "  \"neutral_uniforms\":\"surfaceLightingStrength=0,colourSaturation=1,toneGamma=1,greenSuppression=0,greenRedShift=0,playerExposure=1,environmentLight=1,sunIntensity=0,fogDensity=0,fogDirectionalStrength=0,viewRange=(0,0),viewRangeStrength=0; actual uv2 light gain .24+.76L\",\n"
        << "  \"shader_variant\":\"Source bytes unchanged; array variant inserts only #define TERRAIN_ARRAY 1 immediately after #version, matching production variant. Default array route plus atlas diagnostic; non-array profile atlas route.\",\n"
        << "  \"renderer\":" << quote(renderer) << ",\n  \"gl_version\":" << quote(version)
        << ",\n  \"frozen_profile\":" << (frozenProfile.empty() ? "null" : frozenProfile)
        << ",\n  \"checks\":" << checks.size() << ",\n  \"failures\":" << failures << ",\n  \"gl_errors\":" << glErrors.size()
        << ",\n  \"inputs_unchanged\":" << (unchanged ? "true" : "false") << ",\n  \"error\":" << quote(error) << ",\n  \"input_sha256\":{\n";
    bool first=true;
    for (const auto& input : inputs) { if (!first) out << ",\n"; first=false; out << "    " << quote(input.first) << ':' << quote(input.second); }
    out << "\n  },\n  \"gl_error_events\":[";
    first=true;
    for (const auto& event : glErrors) { if (!first) out << ','; first=false; out << "{\"stage\":" << quote(event.stage) << ",\"code\":" << event.code << '}'; }
    out << "],\n  \"samples\":[\n"; first=true;
    for (const auto& check : checks) {
        if (!first) out << ",\n"; first=false;
        out << "    {\"name\":" << quote(check.name) << ",\"pass\":" << (check.pass ? "true" : "false")
            << ",\"pixel\":[" << check.x << ',' << check.y << "],\"expected_tile\":[" << check.tileX << ',' << check.tileY
            << "],\"expected_repeat\":[" << check.repeatU << ',' << check.repeatV << "],\"actual_vertex_light\":" << check.light << ",\"expected_rgba\":";
        floatArray(out,check.expected); out << ",\"actual_rgba\":"; floatArray(out,check.actual); out << '}';
    }
    out << "\n  ]\n}\n"; require(bool(out),"GPU receipt write failed");
}
}

int main(int argc,char** argv) {
    fs::path output; std::string renderer,version,error; bool unchanged=true; CGLContextObj context=nullptr;
    try {
        require(argc == 4,"Usage: item-binding-gpu <repository-resources-root> <raw-operation-directory> <new-output-directory>");
        const fs::path root=fs::canonical(argv[1]),raw=fs::canonical(argv[2]),proposed=fs::absolute(argv[3]);
        require(!fs::exists(proposed),"Output directory must be new"); fs::create_directories(proposed); output=proposed;
        inputBytes(root/"tools/validate_item_material_binding_macos.cpp");
        inputBytes(root/"src/HelloMine3D/World/Block/TerrainTextureArray.h");
        const auto vertex=inputText(root/"media/ogre/HelloMine3DTerrain.vert"),fragment=inputText(root/"media/ogre/HelloMine3DTerrain.frag");
        std::vector<Operation> operations;
        for (const auto& c : Cases) {
            validateAuthority(root,c);
            for (const auto* producer : {"held","drop"}) {
                const std::string name=std::string(producer)+"-"+c.name;
                operations.push_back(loadOperation(raw/(name+".json")));
                require(operations.back().name == name,"Descriptor does not name the actual requested operation");
            }
        }
        const auto& first=operations.front();
        frozenProfile="{\"version\":"+std::to_string(first.profile)+",\"atlas_pixels\":"+std::to_string(first.atlasPixels)+
            ",\"tile_pixels\":"+std::to_string(first.tilePixels)+",\"tiles_per_row\":"+std::to_string(first.tiles)+
            ",\"uses_array\":"+(first.usesArray ? "true" : "false")+",\"atlas_source\":"+quote(first.atlas)+
            ",\"array_source\":"+quote(first.array)+",\"raw_operation_directory\":"+quote(raw.string())+"}";
        for (const auto& op : operations) require(op.atlas == first.atlas && op.array == first.array &&
            op.atlasPixels == first.atlasPixels && op.tilePixels == first.tilePixels && op.tiles == first.tiles &&
            op.profile == first.profile && op.usesArray == first.usesArray,"Operations do not share one frozen process profile");
        require(!first.usesArray || (first.profile == 2 && first.tiles == 16 && !first.array.empty()),
                "Standard array replay requires frozen existing16x16 semantic profile");
        const CGLPixelFormatAttribute attributes[]{kCGLPFAOpenGLProfile,
            static_cast<CGLPixelFormatAttribute>(kCGLOGLPVersion_3_2_Core),kCGLPFAAccelerated,static_cast<CGLPixelFormatAttribute>(0)};
        CGLPixelFormatObj format=nullptr; GLint count=0;
        require(CGLChoosePixelFormat(attributes,&format,&count) == kCGLNoError && format,"No accelerated GL3.2 core pixel format");
        const auto created=CGLCreateContext(format,nullptr,&context); CGLDestroyPixelFormat(format);
        require(created == kCGLNoError && context,"Cannot create diagnostic CGL context");
        require(CGLSetCurrentContext(context) == kCGLNoError,"Cannot select diagnostic CGL context");
        renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER)); version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
        recordErrors("context-startup");
        GLuint vao=0,fbo=0,colour=0; glGenVertexArrays(1,&vao); glBindVertexArray(vao);
        glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
        glGenRenderbuffers(1,&colour); glBindRenderbuffer(GL_RENDERBUFFER,colour);
        glRenderbufferStorage(GL_RENDERBUFFER,GL_RGBA32F,Edge,Edge);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,colour);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,"Incomplete RGBA32F diagnostic framebuffer");
        glViewport(0,0,Edge,Edge); glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_DITHER);
        glDisable(GL_FRAMEBUFFER_SRGB); glPixelStorei(GL_PACK_ALIGNMENT,1); glPixelStorei(GL_UNPACK_ALIGNMENT,1); glActiveTexture(GL_TEXTURE0);
        recordErrors("framebuffer-setup");
        const auto atlasProgram=createProgram(vertex,fragment,false),arrayProgram=first.usesArray ? createProgram(vertex,fragment,true) : 0;
        const auto atlas=loadAtlas(first.atlas,first.atlasPixels),array=first.usesArray ? loadArray(first.array) : Texture{};
        for (std::size_t i=0;i<operations.size();++i) {
            auto& op=operations[i]; upload(op);
            drawChecks(atlasProgram,op,atlas,Cases[i/2],false);
            if (first.usesArray) drawChecks(arrayProgram,op,array,Cases[i/2],true);
            for (auto& buffer : op.buffers) glDeleteBuffers(1,&buffer.gl);
            glDeleteBuffers(1,&op.ibo);
        }
        glDeleteTextures(1,&atlas.gl); if (array.gl) glDeleteTextures(1,&array.gl);
        glDeleteProgram(atlasProgram); if (arrayProgram) glDeleteProgram(arrayProgram);
        glDeleteRenderbuffers(1,&colour); glDeleteFramebuffers(1,&fbo); glDeleteVertexArrays(1,&vao);
        recordErrors("cleanup");
    } catch (const std::exception& exception) { error=exception.what(); std::cerr << error << '\n'; }
    if (context) { recordErrors("terminal"); CGLSetCurrentContext(nullptr); CGLDestroyContext(context); }
    for (const auto& input : inputs) {
        try { unchanged &= sha256(readBytes(input.first)) == input.second; } catch (...) { unchanged=false; }
    }
    unsigned failures=0; for (const auto& check : checks) failures += !check.pass;
    if (!output.empty() && fs::is_directory(output)) receipt(output,renderer,version,error,unchanged);
    std::cout << "[ITEM_BINDING_GPU] checks=" << checks.size() << " failures=" << failures << " gl_errors=" << glErrors.size()
              << " inputs_unchanged=" << unchanged << '\n';
    return error.empty() && !checks.empty() && !failures && glErrors.empty() && unchanged ? 0 : 1;
}
