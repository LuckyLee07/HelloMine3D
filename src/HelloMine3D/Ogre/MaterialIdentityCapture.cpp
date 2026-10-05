#include "MaterialIdentityCapture.h"
#include "../Item/Material.h"
#include "../Util/ResourcePackResolver.h"
#include "../World/Block/TerrainMaterialProfile.h"

#include <Ogre.h>
#include <OgreGL3PlusPrerequisites.h>
#include <OgreGL3PlusHardwareVertexBuffer.h>
#include <OgreGL3PlusHardwareIndexBuffer.h>
#include <GLSL/OgreGLSLShader.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;
constexpr const char* Schema = "hellomine3d-material-identity-capture-v1";
constexpr std::size_t ByteLimit = 512u * 1024u * 1024u;
constexpr int ReplayEdge = 128;
MaterialIdentityCapture* currentCapture = nullptr;
void require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("Material identity capture: " + message);
}
std::string q(const std::string& value)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}
template<class T> std::string n(T value) { std::ostringstream out; out << std::setprecision(10) << +value; return out.str(); }
std::string b(bool value) { return value ? "true" : "false"; }
using Fields = std::vector<std::pair<std::string, std::string>>;
std::string object(const Fields& fields)
{
    std::string out = "{";
    for (const auto& field : fields) { if (out.size() > 1) out += ','; out += q(field.first) + ':' + field.second; }
    return out + '}';
}
std::string array(const std::vector<std::string>& values)
{
    std::string out = "[";
    for (const auto& value : values) { if (out.size() > 1) out += ','; out += value; }
    return out + ']';
}
template<class T, std::size_t N> std::string numbers(const std::array<T, N>& values)
{
    std::vector<std::string> out; for (const auto value : values) out.push_back(n(value)); return array(out);
}
std::string matrixJson(const Ogre::Matrix4& matrix)
{
    std::vector<std::string> values;
    for (unsigned row = 0; row < 4; ++row) for (unsigned col = 0; col < 4; ++col) {
        require(std::isfinite(matrix[row][col]), "non-finite observed matrix");
        values.push_back(n(matrix[row][col]));
    }
    return array(values);
}
std::string factsJson(const MaterialIdentityCapture::Facts& facts)
{
    return object({{"consumer", q(facts.consumer)}, {"material_name", q(facts.materialName)},
        {"material_id", n(facts.materialId)}, {"block_id", n(facts.blockId)}, {"slot", n(facts.slot)},
        {"amount", n(facts.amount)}, {"face", n(facts.face)}, {"tile_x", n(facts.tileX)}, {"tile_y", n(facts.tileY)},
        {"actor_id", n(facts.actorId)}, {"revision", n(facts.revision)}, {"map_cell", n(facts.mapCell)},
        {"height", n(facts.height)}, {"top", b(facts.top)}, {"known", b(facts.known)},
        {"world_x", n(facts.worldX)}, {"world_y", n(facts.worldY)}, {"world_z", n(facts.worldZ)}});
}
std::string sampleJson(const MaterialIdentityCapture::MapSample& sample, std::uint64_t revision)
{
    return object({{"cell", n(sample.cell)}, {"world_x", n(sample.worldX)}, {"world_z", n(sample.worldZ)},
        {"height", n(sample.height)}, {"block_id", n(sample.blockId)}, {"known", b(sample.known)},
        {"sample_available", b(sample.sampleAvailable)}, {"query_revision", n(revision)}});
}
std::string batchJson(const MaterialIdentityCapture::MapBatch& batch)
{
    return object({{"centre_x", n(batch.centreX)}, {"centre_z", n(batch.centreZ)}, {"count_x", n(batch.countX)},
        {"count_z", n(batch.countZ)}, {"step", n(batch.step)}, {"revision", n(batch.revision)},
        {"query_count", n(batch.queryCount)}, {"sample_count", n(batch.sampleCount)}, {"size_matched", b(batch.sizeMatched)}});
}
std::string parametersJson(const Ogre::GpuProgramParametersSharedPtr& parameters)
{
    require(!parameters.isNull(), "selected production program has no parameters");
    std::vector<std::string> floats, integers, definitions;
    require(parameters->getFloatConstantList().size() <= 4096 && parameters->getIntConstantList().size() <= 4096,
            "production parameter buffer exceeds diagnostic bound");
    for (const auto value : parameters->getFloatConstantList()) {
        require(std::isfinite(value), "production parameter buffer contains a non-finite value"); floats.push_back(n(value));
    }
    for (const auto value : parameters->getIntConstantList()) integers.push_back(n(value));
    for (const auto& entry : parameters->getConstantDefinitions().map) {
        const auto& definition = entry.second;
        definitions.push_back(object({{"name", q(entry.first)}, {"type", n(definition.constType)},
            {"physical_index", n(definition.physicalIndex)}, {"logical_index", n(definition.logicalIndex)},
            {"element_size", n(definition.elementSize)}, {"array_size", n(definition.arraySize)}}));
    }
    return object({{"floats", array(floats)}, {"integers", array(integers)}, {"definitions", array(definitions)}});
}
// Every changed GL state belongs to this guard. In particular, buffer reads use
// COPY_READ rather than changing the EBO of Ogre's currently bound VAO.
struct GlState {
    GLint program = 0, pipeline = 0, vao = 0, arrayBuffer = 0, elementBuffer = 0, copyRead = 0;
    GLint drawFbo = 0, readFbo = 0, renderbuffer = 0, readBuffer = 0, drawReadBuffer = 0, drawBuffer0 = 0, activeTexture = 0;
    GLint texture2d = 0, textureArray = 0, sampler = 0, packBuffer = 0;
    std::array<GLint, 4> viewport{}, scissor{};
    std::array<GLint, 2> polygon{};
    std::array<GLboolean, 4> colour{};
    GLboolean depthMask = GL_TRUE;
    const std::array<GLenum, 8> packNames{{GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_IMAGE_HEIGHT,
        GL_PACK_SKIP_PIXELS, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_IMAGES, GL_PACK_SWAP_BYTES, GL_PACK_LSB_FIRST}};
    const std::array<GLenum, 7> enableNames{{GL_BLEND, GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST,
        GL_STENCIL_TEST, GL_DITHER, GL_FRAMEBUFFER_SRGB}};
    std::array<GLint, 8> pack{};
    std::array<GLboolean, 7> enabled{};
    bool havePipeline = false, restored = false;
    GlState()
    {
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
        require(major > 3 || (major == 3 && minor >= 3), "current client context must support sampler objects");
        havePipeline = major > 4 || (major == 4 && minor >= 1);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        if (havePipeline) glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, &pipeline);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer); glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &copyRead);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &elementBuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer); glGetIntegerv(GL_READ_BUFFER, &readBuffer);
        glGetIntegerv(GL_DRAW_BUFFER0, &drawBuffer0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
        glGetIntegerv(GL_READ_BUFFER, &drawReadBuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFbo));
        glGetIntegerv(GL_VIEWPORT, viewport.data()); glGetIntegerv(GL_SCISSOR_BOX, scissor.data());
        glGetIntegerv(GL_POLYGON_MODE, polygon.data()); glGetBooleanv(GL_COLOR_WRITEMASK, colour.data());
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture); glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d); glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &textureArray);
        glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
        for (std::size_t i = 0; i < pack.size(); ++i) glGetIntegerv(packNames[i], &pack[i]);
        for (std::size_t i = 0; i < enabled.size(); ++i) enabled[i] = glIsEnabled(enableNames[i]);
    }
    void tightPack() const
    {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        for (std::size_t i = 0; i < pack.size(); ++i) glPixelStorei(packNames[i], i == 0 ? 1 : 0);
    }
    void restore()
    {
        if (restored) return;
        glUseProgram(static_cast<GLuint>(program));
        if (havePipeline) glBindProgramPipeline(static_cast<GLuint>(pipeline));
        glBindVertexArray(static_cast<GLuint>(vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
        glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(copyRead));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(drawFbo)); glReadBuffer(static_cast<GLenum>(drawReadBuffer));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFbo)); glReadBuffer(static_cast<GLenum>(readBuffer));
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]); glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        // Core profile requires FRONT_AND_BACK; both original entries are equal.
        glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(polygon[0]));
        glColorMask(colour[0], colour[1], colour[2], colour[3]); glDepthMask(depthMask);
        for (std::size_t i = 0; i < enabled.size(); ++i) {
            if (enabled[i]) glEnable(enableNames[i]); else glDisable(enableNames[i]);
        }
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2d));
        glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(textureArray)); glBindSampler(0, static_cast<GLuint>(sampler));
        glActiveTexture(static_cast<GLenum>(activeTexture));
        for (std::size_t i = 0; i < pack.size(); ++i) glPixelStorei(packNames[i], pack[i]);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(packBuffer));
        restored = true;
    }
    ~GlState() { restore(); }
    std::vector<std::string> differences(const GlState& observed) const
    {
        std::vector<std::string> result;
        const auto compare = [&](const char* field, const std::string& expected, const std::string& actual) {
            if (expected != actual) result.push_back(object({{"field", q(field)}, {"before", expected}, {"after", actual}}));
        };
        for (const auto& field : {std::pair<const char*, std::pair<GLint, GLint>>{"program", {program, observed.program}},
             {"pipeline", {pipeline, observed.pipeline}}, {"vao", {vao, observed.vao}}, {"array_buffer", {arrayBuffer, observed.arrayBuffer}},
             {"vao_element_buffer", {elementBuffer, observed.elementBuffer}}, {"copy_read_buffer", {copyRead, observed.copyRead}},
             {"draw_framebuffer", {drawFbo, observed.drawFbo}}, {"read_framebuffer", {readFbo, observed.readFbo}},
             {"renderbuffer", {renderbuffer, observed.renderbuffer}}, {"read_buffer", {readBuffer, observed.readBuffer}},
             {"draw_framebuffer_read_buffer", {drawReadBuffer, observed.drawReadBuffer}}, {"draw_buffer0", {drawBuffer0, observed.drawBuffer0}},
             {"active_texture", {activeTexture, observed.activeTexture}}, {"unit0_texture_2d", {texture2d, observed.texture2d}},
             {"unit0_texture_array", {textureArray, observed.textureArray}}, {"unit0_sampler", {sampler, observed.sampler}},
             {"pack_buffer", {packBuffer, observed.packBuffer}}, {"depth_mask", {depthMask, observed.depthMask}}})
            compare(field.first, n(field.second.first), n(field.second.second));
        compare("viewport", numbers(viewport), numbers(observed.viewport));
        compare("scissor", numbers(scissor), numbers(observed.scissor));
        compare("polygon_mode", numbers(polygon), numbers(observed.polygon));
        compare("colour_mask", numbers(colour), numbers(observed.colour));
        compare("pack_states", numbers(pack), numbers(observed.pack));
        compare("enabled_capabilities", numbers(enabled), numbers(observed.enabled));
        return result;
    }
};
struct GlOwned {
    GLuint program = 0, vao = 0, fbo = 0, colour = 0, sampler = 0;
    ~GlOwned() {
        if (program) glDeleteProgram(program);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        if (colour) glDeleteRenderbuffers(1, &colour);
        if (sampler) glDeleteSamplers(1, &sampler);
    }
};
GLuint shaderProgram(const std::string& vertex, const std::string& fragment)
{
    GlOwned owner; owner.program = glCreateProgram();
    for (const auto& entry : {std::pair<GLenum, std::string>{GL_VERTEX_SHADER, vertex}, {GL_FRAGMENT_SHADER, fragment}}) {
        require(!entry.second.empty(), "production program has no loaded source");
        const auto shader = glCreateShader(entry.first); const auto* source = entry.second.c_str();
        glShaderSource(shader, 1, &source, nullptr); glCompileShader(shader);
        GLint status = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        std::array<char, 16384> log{}; glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glAttachShader(owner.program, shader); glDeleteShader(shader);
        require(status == GL_TRUE, "loaded production shader compile failed: " + std::string(log.data()));
    }
    glLinkProgram(owner.program); GLint status = 0; glGetProgramiv(owner.program, GL_LINK_STATUS, &status);
    std::array<char, 16384> log{}; glGetProgramInfoLog(owner.program, static_cast<GLsizei>(log.size()), nullptr, log.data());
    require(status == GL_TRUE, "loaded production shader link failed: " + std::string(log.data()));
    const auto program = owner.program; owner.program = 0; return program;
}
void uniform(GLuint program, const char* name, float value) { glUniform1f(glGetUniformLocation(program, name), value); }
void matrixUniform(GLuint program, const char* name, const Ogre::Matrix4& value)
{
    std::array<float, 16> column{};
    for (unsigned row = 0; row < 4; ++row) for (unsigned col = 0; col < 4; ++col) column[col * 4 + row] = value[row][col];
    glUniformMatrix4fv(glGetUniformLocation(program, name), 1, GL_FALSE, column.data());
}
struct RawBuffer { unsigned source = 0; std::size_t stride = 0, vertices = 0; GLuint gl = 0; Bytes bytes; };
struct RawOperation {
    Ogre::RenderOperation operation;
    Ogre::Matrix4 world;
    std::vector<RawBuffer> buffers;
    Bytes indices;
    GLuint ibo = 0;
    std::size_t indexBytes = 0;
    const RawBuffer& buffer(unsigned source) const {
        const auto found = std::find_if(buffers.begin(), buffers.end(), [&](const auto& value) { return value.source == source; });
        require(found != buffers.end(), "declaration has no actual source binding"); return *found;
    }
    std::size_t index(std::size_t offset) const {
        const auto at = (operation.indexData->indexStart + offset) * indexBytes;
        require(at + indexBytes <= indices.size(), "original index range outside buffer");
        std::uint32_t value = 0; std::memcpy(&value, indices.data() + at, indexBytes);
        return operation.vertexData->vertexStart + value;
    }
    Ogre::Vector3 position(std::size_t indexValue) const {
        const auto* element = operation.vertexData->vertexDeclaration->findElementBySemantic(Ogre::VES_POSITION);
        require(element && element->getType() == Ogre::VET_FLOAT3, "only actual FLOAT3 production positions supported");
        const auto& source = buffer(element->getSource()); const auto at = indexValue * source.stride + element->getOffset();
        require(at + 3 * sizeof(float) <= source.bytes.size(), "original position outside buffer");
        std::array<float, 3> xyz{}; std::memcpy(xyz.data(), source.bytes.data() + at, sizeof(xyz));
        return world * Ogre::Vector3(xyz[0], xyz[1], xyz[2]);
    }
};
// Find a non-degenerate original triangle basis, then project its original four
// world-space corners. No replacement mesh or UV is manufactured.
Ogre::Matrix4 faceProjection(const RawOperation& raw, std::size_t first)
{
    std::vector<std::size_t> vertices;
    for (unsigned i = 0; i < 6; ++i) {
        const auto vertex = raw.index(first + i);
        if (std::find(vertices.begin(), vertices.end(), vertex) == vertices.end()) vertices.push_back(vertex);
    }
    require(vertices.size() == 4, "original six indices are not a quad face");
    const auto origin = raw.position(vertices[0]);
    auto u = raw.position(vertices[1]) - origin; require(u.squaredLength() > 1e-12f, "degenerate original face edge"); u.normalise();
    auto normal = u.crossProduct(raw.position(vertices[2]) - origin);
    require(normal.squaredLength() > 1e-12f, "degenerate original triangle"); normal.normalise();
    const auto v = normal.crossProduct(u);
    float minU = 1e30f, maxU = -1e30f, minV = 1e30f, maxV = -1e30f;
    for (const auto vertex : vertices) {
        const auto point = raw.position(vertex);
        require(std::abs(normal.dotProduct(point - origin)) < .001f, "nonplanar original face");
        minU = std::min(minU, u.dotProduct(point)); maxU = std::max(maxU, u.dotProduct(point));
        minV = std::min(minV, v.dotProduct(point)); maxV = std::max(maxV, v.dotProduct(point));
    }
    require(maxU - minU > 1e-6f && maxV - minV > 1e-6f, "empty original face extent");
    Ogre::Matrix4 projection = Ogre::Matrix4::ZERO;
    const auto su = 1.6f / (maxU - minU), sv = 1.6f / (maxV - minV);
    for (unsigned i = 0; i < 3; ++i) { projection[0][i] = u[i] * su; projection[1][i] = v[i] * sv; }
    projection[0][3] = -.8f - minU * su; projection[1][3] = -.8f - minV * sv; projection[3][3] = 1;
    return projection;
}
}

struct MaterialIdentityCapture::Impl {
    fs::path directory;
    std::thread::id thread = std::this_thread::get_id();
    std::size_t written = 0;
    bool open = false, failed = false;
    std::uint64_t frame = 0;
    std::string phase, mode, prefix;
    std::vector<std::string> frameFiles, operations, errors;
    struct TextureRecord { std::string id, file; GLuint gl = 0; GLenum target = 0; };
    std::vector<TextureRecord> textures;
    struct PendingUi {
        UiRange range;
        std::size_t vertexEnd = 0, indexEnd = 0, commandEnd = 0;
        Facts facts;
        UiCallback nearest = nullptr, restore = nullptr;
    };
    std::vector<PendingUi> marks;
    MapBatch mapBatch;
    std::vector<MapSample> lastSamples;
    std::map<std::size_t, std::pair<MapSample, std::uint64_t>> mapCache;
    std::string resources, profile;

    void checkThread() const { require(thread == std::this_thread::get_id(), "observer used outside its render thread"); }
    void write(const std::string& file, const void* data, std::size_t size) {
        require(fs::path(file).filename() == fs::path(file), "output file must be a filename");
        require(size <= ByteLimit && written <= ByteLimit - size, "512 MiB output bound exceeded");
        require(!fs::exists(directory / file), "diagnostic output would overwrite an existing file");
        std::ofstream out(directory / file, std::ios::binary);
        require(bool(out), "cannot create " + file);
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size)); out.close();
        require(bool(out), "cannot complete " + file); written += size;
    }
    void writeText(const std::string& file, const std::string& value) { write(file, value.data(), value.size()); }
    void errorsAt(const std::string& stage) {
        for (unsigned i = 0; i < 64; ++i) {
            const auto error = glGetError(); if (error == GL_NO_ERROR) return;
            errors.push_back(object({{"stage", q(stage)}, {"code", n(error)}}));
        }
        require(false, "GL error queue exceeds bound");
    }
    void verifyState(GlState& before, const std::string& stage) {
        before.restore();
        GlState observed;
        // The query snapshot only activates unit0; restore its own original
        // active unit before comparison, so verification itself is read-only.
        observed.restore();
        const auto differences = before.differences(observed);
        errorsAt(stage + "-restore-verification");
        if (!differences.empty()) {
            failed = true;
            writeText("state-restoration-failure.json", object({{"stage", q(stage)}, {"state_restored", "false"}, {"differences", array(differences)}}));
            index();
            require(false, "native GL state did not restore at " + stage);
        }
    }
    std::string texture(GLuint gl, GLenum target, const std::string& name) {
        const auto found = std::find_if(textures.begin(), textures.end(), [&](const auto& old) { return old.gl == gl && old.target == target; });
        if (found != textures.end()) return found->id;
        require(textures.size() < 64 && gl != 0 && glIsTexture(gl), "invalid or excessive actual texture identities");
        require(target == GL_TEXTURE_2D || target == GL_TEXTURE_2D_ARRAY, "unsupported actual material texture target");
        GlState state; state.tightPack(); glActiveTexture(GL_TEXTURE0); glBindTexture(target, gl);
        const auto id = "texture-" + n(textures.size()); const auto file = id + ".json";
        GLint base = 0, max = 0; glGetTexParameteriv(target, GL_TEXTURE_BASE_LEVEL, &base); glGetTexParameteriv(target, GL_TEXTURE_MAX_LEVEL, &max);
        Fields params;
        for (const auto& entry : {std::pair<const char*, GLenum>{"min_filter", GL_TEXTURE_MIN_FILTER}, {"mag_filter", GL_TEXTURE_MAG_FILTER},
             {"wrap_s", GL_TEXTURE_WRAP_S}, {"wrap_t", GL_TEXTURE_WRAP_T}, {"wrap_r", GL_TEXTURE_WRAP_R}}) {
            GLint value = 0; glGetTexParameteriv(target, entry.second, &value); params.push_back({entry.first, n(value)});
        }
        std::vector<std::string> mips;
        for (int level = 0; level < 14; ++level) {
            GLint width = 0, height = 0, depth = 1, format = 0;
            glGetTexLevelParameteriv(target, level, GL_TEXTURE_WIDTH, &width);
            if (width == 0) break;
            glGetTexLevelParameteriv(target, level, GL_TEXTURE_HEIGHT, &height);
            if (target == GL_TEXTURE_2D_ARRAY) glGetTexLevelParameteriv(target, level, GL_TEXTURE_DEPTH, &depth);
            glGetTexLevelParameteriv(target, level, GL_TEXTURE_INTERNAL_FORMAT, &format);
            require(width > 0 && width <= 8192 && height > 0 && height <= 8192 && depth > 0 && depth <= 256,
                    "native material texture dimensions outside diagnostic bound");
            require(format == GL_RGBA8 || format == GL_SRGB8_ALPHA8 || format == GL_RGBA,
                    "native material texture is not RGBA8 storage");
            const auto size = std::size_t(width) * std::size_t(height) * std::size_t(depth) * 4;
            require(size <= 128u * 1024u * 1024u, "one texture mip exceeds 128 MiB bound");
            Bytes pixels(size); glGetTexImage(target, level, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const auto mipFile = id + "-mip-" + n(level) + ".rgba8"; write(mipFile, pixels.data(), pixels.size());
            mips.push_back(object({{"level", n(level)}, {"width", n(width)}, {"height", n(height)}, {"depth", n(depth)},
                {"internal_format", n(format)}, {"file", q(mipFile)}, {"bytes", n(size)}, {"format", q("rgba8")}, {"origin", q("native_storage")}}));
        }
        require(!mips.empty(), "actual texture has no populated level zero");
        verifyState(state, "native-texture-readback");
        writeText(file, object({{"id", q(id)}, {"gl_id", n(gl)}, {"target", n(target)}, {"ogre_name", q(name)},
            {"base_level", n(base)}, {"max_level", n(max)}, {"parameters", object(params)}, {"mips", array(mips)}, {"state_restored", "true"}}));
        textures.push_back({id, file, gl, target}); return id;
    }
    std::string profileResources() {
        const auto& p = runtimeTerrainMaterialProfile().parameters(); const auto& resolver = runtimeResourcePackResolver();
        std::vector<std::string> sources;
        Fields namedSources;
        const std::array<std::pair<const char*, std::string>, 4> selected{{
            {"profile", TerrainMaterialParameters::LogicalPath}, {"atlas", p.atlasTexture},
            {"array", p.arrayTexture}, {"layout", "media/materials/Base.terrain-atlas"}}};
        for (const auto& selectedSource : selected) {
            const auto& logical = selectedSource.second;
            if (logical.empty()) continue;
            const auto actual = resolver.resolve(logical);
            std::ifstream input(actual, std::ios::binary | std::ios::ate);
            require(bool(input), "cannot read effective resource " + actual);
            const auto size = input.tellg(); require(size >= 0 && size <= 32 * 1024 * 1024, "effective resource outside 32 MiB bound");
            Bytes bytes(static_cast<std::size_t>(size)); input.seekg(0);
            if (!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            require(bool(input), "truncated effective resource " + actual);
            const auto file = "resource-" + n(sources.size()) + fs::path(logical).extension().string();
            write(file, bytes.data(), bytes.size());
            std::string owner;
            for (const auto& resource : resolver.effectiveResources()) if (resource.logicalPath == logical) owner = resource.packName;
            const auto source = object({{"logical", q(logical)}, {"actual", q(actual)}, {"logical_path", q(logical)},
                {"resolved_file", q(actual)}, {"owner", q(owner)}, {"file", q(file)}, {"bytes", n(bytes.size())}});
            sources.push_back(source); namedSources.push_back({selectedSource.first, source});
        }
        writeText("effective-resource-manifest.txt", resolver.effectiveManifest());
        const auto frozen = object({{"format_version", n(p.formatVersion)}, {"atlas_pixels", n(p.atlasPixels)}, {"tile_pixels", n(p.tilePixels)},
            {"tiles_per_row", n(p.tilesPerRow)}, {"array_layer_pixels", n(p.arrayLayerPixels)}, {"uses_array", b(runtimeTerrainMaterialProfile().usesTextureArray())}});
        profile = frozen;
        namedSources.push_back({"frozen_profile", frozen});
        namedSources.push_back({"rendering_mode_reason", q(runtimeTerrainMaterialProfile().renderingModeReason())});
        namedSources.push_back({"profile_version", n(p.formatVersion)}); namedSources.push_back({"atlas_pixels", n(p.atlasPixels)});
        namedSources.push_back({"tile_pixels", n(p.tilePixels)}); namedSources.push_back({"tiles_per_row", n(p.tilesPerRow)});
        namedSources.push_back({"array_layer_pixels", n(p.arrayLayerPixels)}); namedSources.push_back({"uses_array", b(runtimeTerrainMaterialProfile().usesTextureArray())});
        namedSources.push_back({"sources", array(sources)}); namedSources.push_back({"effective_manifest", q("effective-resource-manifest.txt")});
        return object(namedSources);
    }
    void index() {
        std::vector<std::string> frames, refs;
        for (const auto& file : frameFiles) frames.push_back(q(file));
        for (const auto& value : textures) refs.push_back(object({{"id", q(value.id)}, {"file", q(value.file)}}));
        const auto value = object({{"schema", q(Schema)}, {"status", q(failed ? "FAIL" : (open ? "INCOMPLETE" : "CAPTURED"))},
            {"frames", array(frames)}, {"textures", array(refs)}, {"resources", resources}, {"profile", profile}, {"written_bytes", n(written)},
            {"scope", q("actual initialized client objects, native texture bytes, original render buffers and original ImGui commands; neutral production-source replay is diagnostic")}});
        // Only the session index is rewritten; all raw evidence is immutable.
        std::ofstream out(directory / "index.json", std::ios::binary | std::ios::trunc); out << value << '\n';
        require(bool(out), "cannot update session index");
    }
};

MaterialIdentityCapture::MaterialIdentityCapture(const std::string& output) : m_impl(std::make_unique<Impl>())
{
    require(currentCapture == nullptr, "only one capture session may be active");
    require(!output.empty() && !fs::exists(output), "capture requires a new output directory");
    m_impl->directory = fs::absolute(output); fs::create_directories(m_impl->directory);
    m_impl->resources = m_impl->profileResources(); m_impl->index(); currentCapture = this;
}
MaterialIdentityCapture::~MaterialIdentityCapture()
{
    if (currentCapture == this) currentCapture = nullptr;
    try { m_impl->index(); } catch (...) { /* diagnostic destructor never aborts the client */ }
}
MaterialIdentityCapture* MaterialIdentityCapture::active() noexcept { return currentCapture; }
bool MaterialIdentityCapture::isFrameOpen() const noexcept { return m_impl->open; }
std::size_t MaterialIdentityCapture::frameCount() const noexcept { return m_impl->frameFiles.size(); }
bool MaterialIdentityCapture::wantsMaterial(int id) const noexcept
{
    return id == Material::OakPlank || id == Material::Cobblestone || id == Material::Chest || id == Material::Workbench || id == Material::OakBark;
}
const std::vector<MaterialIdentityCapture::MapSample>& MaterialIdentityCapture::lastMapSamples() const noexcept { return m_impl->lastSamples; }
void MaterialIdentityCapture::beginFrame(const std::string& phase, const std::string& mode, std::uint64_t frame)
{
    m_impl->checkThread(); require(!m_impl->open && frameCount() < 24, "frame is already open or 24-frame bound reached");
    require(!phase.empty() && (mode == "standard" || mode == "compatibility"), "capture requires explicit phase and rendering mode");
    m_impl->phase = phase; m_impl->mode = mode; m_impl->frame = frame; m_impl->prefix = "frame-" + n(frameCount());
    m_impl->marks.clear(); m_impl->operations.clear(); m_impl->errors.clear(); m_impl->open = true;
    m_impl->errorsAt("begin-frame");
}
MaterialIdentityCapture::UiRange MaterialIdentityCapture::beginUiRange(const ImDrawList* list) const
{
    if (!m_impl->open || !list) return {};
    m_impl->checkThread(); return {list, std::size_t(list->VtxBuffer.Size), std::size_t(list->IdxBuffer.Size), std::size_t(list->CmdBuffer.Size)};
}
void MaterialIdentityCapture::recordUiRange(const UiRange& range, const Facts& facts, UiCallback nearest, UiCallback restore)
{
    if (!m_impl->open || !range.list || !wantsMaterial(facts.materialId)) return;
    m_impl->checkThread(); require(m_impl->marks.size() < 256, "256 selected UI range bound reached");
    require(range.vertexBegin <= std::size_t(range.list->VtxBuffer.Size) && range.indexBegin <= std::size_t(range.list->IdxBuffer.Size), "UI buffers changed before range end");
    m_impl->marks.push_back({range, std::size_t(range.list->VtxBuffer.Size), std::size_t(range.list->IdxBuffer.Size),
        std::size_t(range.list->CmdBuffer.Size), facts, nearest, restore});
}
void MaterialIdentityCapture::observeMapQueries(const MapBatch& batch, const std::vector<MapSample>& samples)
{
    m_impl->checkThread(); require(samples.size() <= 256 && batch.countX > 0 && batch.countX <= 129 && batch.countZ > 0 && batch.countZ <= 129,
        "actual map query batch outside diagnostic bound");
    const auto& old = m_impl->mapBatch;
    if (old.centreX != batch.centreX || old.centreZ != batch.centreZ || old.countX != batch.countX || old.countZ != batch.countZ || old.step != batch.step)
        m_impl->mapCache.clear();
    m_impl->mapBatch = batch; m_impl->lastSamples = samples;
    for (const auto& sample : samples) {
        require(sample.cell < std::size_t(batch.countX) * std::size_t(batch.countZ), "actual map cell outside region");
        m_impl->mapCache[sample.cell] = {sample, batch.revision};
    }
}

void MaterialIdentityCapture::observeRenderable(const Facts& facts, Ogre::Renderable& renderable,
    Ogre::Pass& pass, const Ogre::Camera& camera, const std::vector<std::size_t>& requested)
{
    if (!m_impl->open || !wantsMaterial(facts.materialId)) return;
    m_impl->checkThread(); require(m_impl->operations.size() < 32, "32 actual operation bound reached");
    require(renderable.getNumWorldTransforms() == 1, "observer requires one actual world transform");
    RawOperation raw; renderable.getRenderOperation(raw.operation); renderable.getWorldTransforms(&raw.world);
    require(raw.operation.operationType == Ogre::RenderOperation::OT_TRIANGLE_LIST && raw.operation.useIndexes && raw.operation.vertexData && raw.operation.indexData,
            "observer requires an actual indexed triangle-list operation");
    const auto& op = raw.operation; require(op.indexData->indexCount <= 4000000 && op.vertexData->vertexCount <= 1000000,
            "actual operation outside diagnostic bound");
    const auto opPrefix = m_impl->prefix + "-op-" + n(m_impl->operations.size());
    GlState state; state.tightPack();
    std::vector<std::string> bindings, elements, units;
    for (const auto& binding : op.vertexData->vertexBufferBinding->getBindings()) {
        const auto* native = dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(binding.second.get());
        require(native && binding.second->getSizeInBytes() <= 32u * 1024u * 1024u, "actual VBO is not bounded GL3Plus storage");
        RawBuffer buffer; buffer.source = binding.first; buffer.stride = binding.second->getVertexSize();
        buffer.vertices = binding.second->getNumVertices(); buffer.gl = native->getGLBufferId(); buffer.bytes.resize(binding.second->getSizeInBytes());
        glBindBuffer(GL_COPY_READ_BUFFER, buffer.gl); glGetBufferSubData(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(buffer.bytes.size()), buffer.bytes.data());
        const auto file = opPrefix + "-vbo-" + n(binding.first) + ".bin"; m_impl->write(file, buffer.bytes.data(), buffer.bytes.size());
        bindings.push_back(object({{"source", n(buffer.source)}, {"stride", n(buffer.stride)}, {"vertices", n(buffer.vertices)},
            {"bytes", n(buffer.bytes.size())}, {"file", q(file)}, {"gl_id", n(buffer.gl)}})); raw.buffers.push_back(std::move(buffer));
    }
    require(!raw.buffers.empty() && raw.buffers.size() <= 8, "invalid actual VBO binding count");
    for (const auto& element : op.vertexData->vertexDeclaration->getElements())
        elements.push_back(object({{"source", n(element.getSource())}, {"offset", n(element.getOffset())}, {"semantic", n(element.getSemantic())},
            {"semantic_index", n(element.getIndex())}, {"type", n(element.getType())}, {"components", n(Ogre::VertexElement::getTypeCount(element.getType()))},
            {"bytes", n(Ogre::VertexElement::getTypeSize(element.getType()))}}));
    const auto* index = dynamic_cast<Ogre::GL3PlusHardwareIndexBuffer*>(op.indexData->indexBuffer.get());
    require(index && op.indexData->indexBuffer->getSizeInBytes() <= 32u * 1024u * 1024u, "actual IBO is not bounded GL3Plus storage");
    raw.ibo = index->getGLBufferId(); raw.indexBytes = op.indexData->indexBuffer->getIndexSize(); raw.indices.resize(op.indexData->indexBuffer->getSizeInBytes());
    require(raw.indexBytes == 2 || raw.indexBytes == 4, "unsupported actual index type");
    glBindBuffer(GL_COPY_READ_BUFFER, raw.ibo); glGetBufferSubData(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(raw.indices.size()), raw.indices.data());
    const auto indexFile = opPrefix + "-ibo.bin"; m_impl->write(indexFile, raw.indices.data(), raw.indices.size());
    GLuint actualTexture = 0; GLenum actualTarget = 0; std::string textureId;
    for (unsigned unit = 0; unit < pass.getNumTextureUnitStates(); ++unit) {
        const auto* tus = pass.getTextureUnitState(unit); const auto texture = tus->_getTexturePtr();
        require(!texture.isNull() && texture->isLoaded(), "selected pass has no initialized actual texture");
        GLuint gl = 0; texture->getCustomAttribute("GLID", &gl);
        const auto target = texture->getTextureType() == Ogre::TEX_TYPE_2D_ARRAY ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
        require(texture->getTextureType() == Ogre::TEX_TYPE_2D_ARRAY || texture->getTextureType() == Ogre::TEX_TYPE_2D, "selected pass texture target unsupported");
        const auto id = m_impl->texture(gl, target, texture->getName()); const auto& address = tus->getTextureAddressingMode();
        units.push_back(object({{"unit", n(unit)}, {"texture_id", q(id)}, {"texture_name", q(texture->getName())}, {"coord_set", n(tus->getTextureCoordSet())},
            {"filter_min", n(tus->getTextureFiltering(Ogre::FT_MIN))}, {"filter_mag", n(tus->getTextureFiltering(Ogre::FT_MAG))},
            {"filter_mip", n(tus->getTextureFiltering(Ogre::FT_MIP))}, {"address_u", n(address.u)}, {"address_v", n(address.v)}, {"address_w", n(address.w)}}));
        if (unit == 0) { actualTexture = gl; actualTarget = target; textureId = id; }
    }
    require(actualTexture && pass.hasVertexProgram() && pass.hasFragmentProgram(), "selected production pass lacks terrain program or texture");
    const auto vp = pass.getVertexProgram(), fp = pass.getFragmentProgram();
    const auto* nativeVp = dynamic_cast<Ogre::GLSLShader*>(vp.get());
    const auto* nativeFp = dynamic_cast<Ogre::GLSLShader*>(fp.get());
    require(nativeVp && nativeFp && nativeVp->getGLShaderHandle() && nativeFp->getGLShaderHandle(), "selected pass does not reference compiled actual GLSL shader objects");
    const auto vertexFile = opPrefix + "-loaded-vertex.glsl", fragmentFile = opPrefix + "-loaded-fragment.glsl";
    m_impl->writeText(vertexFile, vp->getSource()); m_impl->writeText(fragmentFile, fp->getSource());
    const auto passJson = object({{"vertex_program", q(pass.getVertexProgramName())}, {"fragment_program", q(pass.getFragmentProgramName())},
        {"vertex_source", q(vertexFile)}, {"fragment_source", q(fragmentFile)}, {"texture_units", array(units)},
        {"vertex_shader_gl_id", n(nativeVp->getGLShaderHandle())}, {"fragment_shader_gl_id", n(nativeFp->getGLShaderHandle())},
        {"vertex_source_file", q(vp->getSourceFile())}, {"fragment_source_file", q(fp->getSourceFile())},
        {"vertex_defines", q(vp->getParameter("preprocessor_defines"))}, {"fragment_defines", q(fp->getParameter("preprocessor_defines"))},
        {"pass_index", n(pass.getIndex())}, {"vertex_parameters", parametersJson(pass.getVertexProgramParameters())},
        {"fragment_parameters", parametersJson(pass.getFragmentProgramParameters())},
        {"observed_current_program", n(state.program)}, {"observed_current_pipeline", n(state.pipeline)},
        {"observed_unit0_sampler", n(state.sampler)}, {"sampler_scope", q("GL3Plus production filtering is stored on texture; current GL binding is recorded separately")}});
    std::vector<std::size_t> faces = requested;
    if (faces.empty()) for (std::size_t first = 0; first + 6 <= op.indexData->indexCount; first += 6) {
        bool selected = true;
        if (facts.consumer == "world") for (unsigned i = 0; i < 6; ++i) {
            const auto position = raw.position(raw.index(first + i));
            if (position.x < facts.worldX - .001f || position.x > facts.worldX + 1.001f ||
                position.y < facts.worldY - .001f || position.y > facts.worldY + 1.001f ||
                position.z < facts.worldZ - .001f || position.z > facts.worldZ + 1.001f) selected = false;
        }
        if (selected) faces.push_back(first);
    }
    require(!faces.empty() && faces.size() <= 6, "expected bounded actual cube faces were not found");
    GlOwned replay; replay.program = shaderProgram(vp->getSource(), fp->getSource());
    glGenVertexArrays(1, &replay.vao); glBindVertexArray(replay.vao); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, raw.ibo);
    for (const auto& entry : {std::pair<const char*, std::pair<Ogre::VertexElementSemantic, unsigned short>>{"vertex", {Ogre::VES_POSITION, 0}},
         {"uv0", {Ogre::VES_TEXTURE_COORDINATES, 0}}, {"uv1", {Ogre::VES_TEXTURE_COORDINATES, 1}},
         {"uv2", {Ogre::VES_TEXTURE_COORDINATES, 2}}, {"uv3", {Ogre::VES_TEXTURE_COORDINATES, 3}}}) {
        const auto location = glGetAttribLocation(replay.program, entry.first);
        if (location < 0) continue;
        const auto* element = op.vertexData->vertexDeclaration->findElementBySemantic(entry.second.first, entry.second.second);
        require(element && element->getType() >= Ogre::VET_FLOAT1 && element->getType() <= Ogre::VET_FLOAT4, "replay input is not an actual declared FLOAT attribute");
        const auto& buffer = raw.buffer(element->getSource()); glBindBuffer(GL_ARRAY_BUFFER, buffer.gl);
        glEnableVertexAttribArray(static_cast<GLuint>(location));
        glVertexAttribPointer(static_cast<GLuint>(location), Ogre::VertexElement::getTypeCount(element->getType()), GL_FLOAT, GL_FALSE,
            static_cast<GLsizei>(buffer.stride), reinterpret_cast<const void*>(element->getOffset()));
    }
    glGenFramebuffers(1, &replay.fbo); glBindFramebuffer(GL_FRAMEBUFFER, replay.fbo);
    glGenRenderbuffers(1, &replay.colour); glBindRenderbuffer(GL_RENDERBUFFER, replay.colour);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA32F, ReplayEdge, ReplayEdge);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, replay.colour);
    glDrawBuffer(GL_COLOR_ATTACHMENT0); glReadBuffer(GL_COLOR_ATTACHMENT0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "diagnostic RGBA32F framebuffer incomplete");
    for (const auto capability : state.enableNames) glDisable(capability);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glDepthMask(GL_FALSE); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glViewport(0, 0, ReplayEdge, ReplayEdge); if (state.havePipeline) glBindProgramPipeline(0); glUseProgram(replay.program);
    glActiveTexture(GL_TEXTURE0); glBindTexture(actualTarget, actualTexture);
    // Diagnostic LOD-zero point sampler does not modify the actual texture's
    // production filter state. Native all-mip bytes and TUS filters are retained.
    glGenSamplers(1, &replay.sampler); glSamplerParameteri(replay.sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(replay.sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(replay.sampler, GL_TEXTURE_WRAP_S, GL_REPEAT); glSamplerParameteri(replay.sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glSamplerParameteri(replay.sampler, GL_TEXTURE_WRAP_R, GL_REPEAT); glBindSampler(0, replay.sampler);
    for (const auto name : {"terrainAtlas", "terrainArray"}) glUniform1i(glGetUniformLocation(replay.program, name), 0);
    const auto& profile = runtimeTerrainMaterialProfile().parameters();
    Fields uniformFields{{"atlasPixels", n(profile.atlasPixels)}, {"tilePixels", n(profile.tilePixels)}, {"tilesPerRow", n(profile.tilesPerRow)}};
    uniform(replay.program, "atlasPixels", float(profile.atlasPixels)); uniform(replay.program, "tilePixels", float(profile.tilePixels));
    uniform(replay.program, "tilesPerRow", float(profile.tilesPerRow));
    for (const auto& entry : {std::pair<const char*, float>{"colourSaturation", 1}, {"toneGamma", 1}, {"greenSuppression", 0}, {"greenRedShift", 0},
         {"playerExposure", 1}, {"environmentLight", 1}, {"surfaceLightingStrength", 0}, {"sunIntensity", 0}, {"fogDensity", 0},
         {"fogDirectionalStrength", 0}, {"viewRangeStrength", 0}, {"alphaCutoff", .4999f}}) {
        uniform(replay.program, entry.first, entry.second); uniformFields.push_back({entry.first, n(entry.second)});
    }
    for (const auto name : {"fogColour", "fogSunwardColour", "sunDirection", "sunColour", "cameraPosition"}) glUniform3f(glGetUniformLocation(replay.program, name), 0, 0, 0);
    glUniform2f(glGetUniformLocation(replay.program, "viewRange"), 0, 0); glUniform2f(glGetUniformLocation(replay.program, "viewRangeCentre"), 0, 0);
    matrixUniform(replay.program, "world", raw.world); matrixUniform(replay.program, "worldView", raw.world);
    std::vector<std::string> replays;
    for (const auto first : faces) {
        require(first + 6 <= op.indexData->indexCount, "requested face exceeds actual index count");
        const auto wvp = faceProjection(raw, first) * raw.world; matrixUniform(replay.program, "worldViewProj", wvp);
        const std::array<float, 4> clear{{-1, -1, -1, -1}}; glClearBufferfv(GL_COLOR, 0, clear.data());
        glDrawElementsBaseVertex(GL_TRIANGLES, 6, raw.indexBytes == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
            reinterpret_cast<const void*>((op.indexData->indexStart + first) * raw.indexBytes), static_cast<GLint>(op.vertexData->vertexStart));
        Bytes pixels(std::size_t(ReplayEdge) * ReplayEdge * 4 * sizeof(float)); glReadPixels(0, 0, ReplayEdge, ReplayEdge, GL_RGBA, GL_FLOAT, pixels.data());
        const auto file = opPrefix + "-face-" + n(first) + ".rgba32f"; m_impl->write(file, pixels.data(), pixels.size());
        replays.push_back(object({{"first_index", n(first)}, {"index_count", "6"}, {"texture_id", q(textureId)},
            {"world_view_projection_row_major", matrixJson(wvp)}, {"width", n(ReplayEdge)}, {"height", n(ReplayEdge)},
            {"file", q(file)}, {"bytes", n(pixels.size())}, {"format", q("rgba32f")}, {"origin", q("bottom_left")},
            {"uniforms", object(uniformFields)}, {"sampler", object({{"id", n(replay.sampler)}, {"min_filter", n(GL_NEAREST)}, {"mag_filter", n(GL_NEAREST)},
                {"wrap_s", n(GL_REPEAT)}, {"wrap_t", n(GL_REPEAT)}, {"scope", q("diagnostic LOD-zero point override")}})}, {"program", n(replay.program)}}));
    }
    m_impl->verifyState(state, "original-buffer-neutral-replay");
    m_impl->operations.push_back(object({{"facts", factsJson(facts)}, {"material_name", q(renderable.getMaterial()->getName())}, {"pass", passJson},
        {"vertex_start", n(op.vertexData->vertexStart)}, {"vertex_count", n(op.vertexData->vertexCount)}, {"index_start", n(op.indexData->indexStart)},
        {"index_count", n(op.indexData->indexCount)}, {"index_type", q(raw.indexBytes == 2 ? "u16" : "u32")},
        {"vertex_buffers", array(bindings)}, {"elements", array(elements)}, {"index_buffer", object({{"file", q(indexFile)}, {"bytes", n(raw.indices.size())}, {"gl_id", n(raw.ibo)}})},
        {"world_transform_row_major", matrixJson(raw.world)}, {"view_row_major", matrixJson(camera.getViewMatrix())},
        {"projection_row_major", matrixJson(camera.getProjectionMatrix())}, {"replays", array(replays)}, {"state_restored", "true"}}));
}

void MaterialIdentityCapture::finishFrame(const ImDrawData* drawData, bool captureFramebuffer)
{
    if (!m_impl->open) return;
    m_impl->checkThread();
    try {
        require(drawData && drawData->Valid && drawData->CmdListsCount <= 128, "actual ImGui draw data missing or outside bound");
        GlState state; state.tightPack();
        std::vector<std::string> lists, ranges;
        std::map<const ImDrawList*, std::size_t> listIndices;
        std::map<UiCallback, std::string> callbacks;
        for (const auto& mark : m_impl->marks) { if (mark.nearest) callbacks[mark.nearest] = "nearest"; if (mark.restore) callbacks[mark.restore] = "restore"; }
        for (int li = 0; li < drawData->CmdListsCount; ++li) {
            const auto* list = drawData->CmdLists[li]; listIndices[list] = static_cast<std::size_t>(li);
            require(list->VtxBuffer.size_in_bytes() <= 16 * 1024 * 1024 && list->IdxBuffer.size_in_bytes() <= 16 * 1024 * 1024 && list->CmdBuffer.Size <= 65536,
                "actual UI draw list exceeds bound");
            const auto vfile = m_impl->prefix + "-ui-" + n(li) + "-vertices.bin", ifile = m_impl->prefix + "-ui-" + n(li) + "-indices.bin";
            m_impl->write(vfile, list->VtxBuffer.Data, static_cast<std::size_t>(list->VtxBuffer.size_in_bytes()));
            m_impl->write(ifile, list->IdxBuffer.Data, static_cast<std::size_t>(list->IdxBuffer.size_in_bytes()));
            std::vector<std::string> commands;
            for (const auto& cmd : list->CmdBuffer) {
                std::string role = "none";
                if (cmd.UserCallback == ImDrawCallback_ResetRenderState) role = "reset";
                else if (cmd.UserCallback) { const auto found = callbacks.find(cmd.UserCallback); role = found == callbacks.end() ? "other" : found->second; }
                commands.push_back(object({{"index_offset", n(cmd.IdxOffset)}, {"vertex_offset", n(cmd.VtxOffset)}, {"count", n(cmd.ElemCount)},
                    {"clip", numbers(std::array<float, 4>{{cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w}})},
                    {"texture_id", n(cmd.GetTexID())}, {"callback", q(role)}}));
            }
            lists.push_back(object({{"vertices", object({{"file", q(vfile)}, {"bytes", n(list->VtxBuffer.size_in_bytes())}, {"count", n(list->VtxBuffer.Size)}})},
                {"indices", object({{"file", q(ifile)}, {"bytes", n(list->IdxBuffer.size_in_bytes())}, {"count", n(list->IdxBuffer.Size)}})}, {"commands", array(commands)}}));
        }
        std::set<GLuint> uiTextures;
        for (const auto& mark : m_impl->marks) {
            const auto found = listIndices.find(mark.range.list); require(found != listIndices.end(), "marked actual draw list missing at submission");
            require(mark.vertexEnd <= std::size_t(mark.range.list->VtxBuffer.Size) && mark.indexEnd <= std::size_t(mark.range.list->IdxBuffer.Size), "marked actual UI range no longer present");
            for (const auto& cmd : mark.range.list->CmdBuffer) if (!cmd.UserCallback && cmd.ElemCount &&
                std::size_t(cmd.IdxOffset) < mark.indexEnd && std::size_t(cmd.IdxOffset) + cmd.ElemCount > mark.range.indexBegin) {
                require(cmd.GetTexID() <= 0xffffffffu, "actual UI texture ID exceeds native GL name"); uiTextures.insert(static_cast<GLuint>(cmd.GetTexID()));
            }
            std::string query = "null";
            if (mark.facts.consumer == "map_3d" && mark.facts.mapCell >= 0) {
                const auto observation = m_impl->mapCache.find(static_cast<std::size_t>(mark.facts.mapCell));
                if (observation != m_impl->mapCache.end()) query = sampleJson(observation->second.first, observation->second.second);
            }
            ranges.push_back(object({{"facts", factsJson(mark.facts)}, {"list", n(found->second)}, {"vertex_begin", n(mark.range.vertexBegin)},
                {"vertex_end", n(mark.vertexEnd)}, {"index_begin", n(mark.range.indexBegin)}, {"index_end", n(mark.indexEnd)},
                {"command_begin", n(mark.range.commandBegin)}, {"command_end", n(mark.commandEnd)}, {"map_query", query}}));
        }
        for (const auto texture : uiTextures) m_impl->texture(texture, GL_TEXTURE_2D, "actual ImGui texture ID");
        std::string framebuffer = "null";
        if (captureFramebuffer) {
            require(state.viewport[2] > 0 && state.viewport[2] <= 8192 && state.viewport[3] > 0 && state.viewport[3] <= 8192, "actual framebuffer outside bound");
            // Read the actual draw target, regardless of a prior unrelated read FBO.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(state.drawFbo));
            glReadBuffer(static_cast<GLenum>(state.drawBuffer0));
            Bytes pixels(std::size_t(state.viewport[2]) * std::size_t(state.viewport[3]) * 4);
            glReadPixels(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            const auto file = m_impl->prefix + "-framebuffer.rgba8"; m_impl->write(file, pixels.data(), pixels.size());
            framebuffer = object({{"file", q(file)}, {"bytes", n(pixels.size())}, {"width", n(state.viewport[2])}, {"height", n(state.viewport[3])},
                {"format", q("rgba8")}, {"origin", q("bottom_left")}, {"framebuffer_srgb", b(state.enabled.back() == GL_TRUE)},
                {"draw_framebuffer", n(state.drawFbo)}, {"viewport", numbers(state.viewport)}});
        }
        Fields callbackSamplers;
        for (const auto& callback : callbacks) {
            const auto& platform = ImGui::GetPlatformIO();
            require(callback.first == platform.DrawCallback_SetSamplerNearest || callback.first == platform.DrawCallback_SetSamplerLinear,
                    "marked sampler callback is not the real ImGui backend callback");
            // These actual desktop backend callbacks only bind their existing
            // sampler object. Invoke once without a draw to read that object's
            // parameters; the enclosing guard restores the original binding.
            glActiveTexture(GL_TEXTURE0); callback.first(nullptr, nullptr);
            GLint sampler = 0; glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
            require(sampler > 0 && glIsSampler(static_cast<GLuint>(sampler)), "actual ImGui callback did not bind a sampler object");
            Fields params{{"id", n(sampler)}};
            for (const auto& entry : {std::pair<const char*, GLenum>{"min_filter", GL_TEXTURE_MIN_FILTER}, {"mag_filter", GL_TEXTURE_MAG_FILTER},
                 {"wrap_s", GL_TEXTURE_WRAP_S}, {"wrap_t", GL_TEXTURE_WRAP_T}}) {
                GLint value = 0; glGetSamplerParameteriv(static_cast<GLuint>(sampler), entry.second, &value); params.push_back({entry.first, n(value)});
            }
            callbackSamplers.push_back({callback.second, object(params)});
        }
        m_impl->verifyState(state, "finish-frame-readback");
        std::vector<std::string> samples;
        for (const auto& observation : m_impl->mapCache) samples.push_back(sampleJson(observation.second.first, observation.second.second));
        const auto ui = object({{"display_pos", numbers(std::array<float, 2>{{drawData->DisplayPos.x, drawData->DisplayPos.y}})},
            {"display_size", numbers(std::array<float, 2>{{drawData->DisplaySize.x, drawData->DisplaySize.y}})},
            {"framebuffer_scale", numbers(std::array<float, 2>{{drawData->FramebufferScale.x, drawData->FramebufferScale.y}})},
            {"vertex_stride", n(sizeof(ImDrawVert))}, {"pos_offset", n(offsetof(ImDrawVert, pos))}, {"uv_offset", n(offsetof(ImDrawVert, uv))},
            {"colour_offset", n(offsetof(ImDrawVert, col))}, {"colour_shifts", numbers(std::array<int, 4>{{IM_COL32_R_SHIFT, IM_COL32_G_SHIFT, IM_COL32_B_SHIFT, IM_COL32_A_SHIFT}})},
            {"index_type", q(sizeof(ImDrawIdx) == 2 ? "u16" : "u32")}, {"lists", array(lists)}, {"ranges", array(ranges)},
            {"callback_samplers", object(callbackSamplers)}});
        const auto file = m_impl->prefix + ".json";
        m_impl->writeText(file, object({{"schema", q(Schema)}, {"mode", q(m_impl->mode)}, {"phase", q(m_impl->phase)}, {"frame", n(m_impl->frame)},
            {"operations", array(m_impl->operations)}, {"ui", ui}, {"framebuffer", framebuffer}, {"map", object({{"batch", batchJson(m_impl->mapBatch)}, {"samples", array(samples)}})},
            {"gl_errors", array(m_impl->errors)}, {"state_restored", "true"} }));
        m_impl->frameFiles.push_back(file); m_impl->open = false; m_impl->index();
    } catch (...) {
        m_impl->failed = true; m_impl->open = false;
        try { m_impl->index(); } catch (...) {}
        throw;
    }
}
