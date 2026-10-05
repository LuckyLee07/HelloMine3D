#include "ShoreEditCapture.h"

#include <OgreGL3PlusPrerequisites.h>
#include <OgreGL3PlusHardwareIndexBuffer.h>
#include <OgreGL3PlusHardwareVertexBuffer.h>
#include <OgreVertexIndexData.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {
namespace fs = std::filesystem;
constexpr std::size_t OperationLimit = 16u * 1024u * 1024u;
constexpr std::size_t SessionLimit = 256u * 1024u * 1024u;
using Fields = std::vector<std::pair<std::string, std::string>>;
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error("Shore edit storage capture: " + message);
}
std::string quote(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4)
                              << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}
template<class T> std::string number(T value) {
    require(std::isfinite(static_cast<double>(value)), "nonfinite JSON number");
    std::ostringstream out; out << std::setprecision(10) << +value;
    return out.str();
}
std::string boolean(bool value) { return value ? "true" : "false"; }
std::string object(const Fields& fields) {
    std::string out = "{";
    for (const auto& field : fields) {
        if (out.size() > 1) out += ',';
        out += quote(field.first) + ':' + field.second;
    }
    return out + '}';
}
std::string array(const std::vector<std::string>& values) {
    std::string out = "[";
    for (const auto& value : values) { if (out.size() > 1) out += ','; out += value; }
    return out + ']';
}
std::string xyz(glm::ivec3 p) { return array({number(p.x), number(p.y), number(p.z)}); }
std::string matrix(const Ogre::Matrix4& value) {
    std::vector<std::string> values;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
        values.push_back(number(value[r][c]));
    return array(values);
}
// Comparison-only context snapshot. COPY_READ is the only binding mutated.
// In particular, never bind ARRAY/ELEMENT_ARRAY or a VAO to read original bytes.
struct ContextState {
    GLint copy = 0, program = 0, pipeline = 0, vao = 0, arrayBuffer = 0, ebo = 0;
    GLint activeTexture = 0, drawFbo = 0, readFbo = 0, readBuffer = 0, packBuffer = 0;
    std::array<GLint, 4> viewport{};
    bool havePipeline = false;
    ContextState() {
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
        require(major > 3 || (major == 3 && minor >= 3), "current GL3Plus context required");
        havePipeline = major > 4 || (major == 4 && minor >= 1);
        glGetIntegerv(GL_COPY_READ_BUFFER_BINDING, &copy);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        if (havePipeline) glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, &pipeline);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebo);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
        glGetIntegerv(GL_READ_BUFFER, &readBuffer);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
        glGetIntegerv(GL_VIEWPORT, viewport.data());
    }
    bool same(const ContextState& other) const noexcept {
        return copy == other.copy && program == other.program && pipeline == other.pipeline &&
            havePipeline == other.havePipeline && vao == other.vao &&
            arrayBuffer == other.arrayBuffer && ebo == other.ebo &&
            activeTexture == other.activeTexture && drawFbo == other.drawFbo &&
            readFbo == other.readFbo && readBuffer == other.readBuffer &&
            packBuffer == other.packBuffer && viewport == other.viewport;
    }
    std::string json() const {
        std::vector<std::string> view;
        for (auto value : viewport) view.push_back(number(value));
        return object({{"copy_read_buffer", number(copy)}, {"program", number(program)},
            {"pipeline", havePipeline ? number(pipeline) : "null"}, {"vao", number(vao)},
            {"array_buffer", number(arrayBuffer)}, {"vao_element_buffer", number(ebo)},
            {"active_texture", number(activeTexture)}, {"draw_framebuffer", number(drawFbo)},
            {"read_framebuffer", number(readFbo)}, {"read_buffer", number(readBuffer)},
            {"pack_buffer", number(packBuffer)}, {"viewport", array(view)}});
    }
};
struct CopyReadGuard {
    ContextState before;
    bool restored = false;
    void restore() noexcept {
        if (!restored) { glBindBuffer(GL_COPY_READ_BUFFER, static_cast<GLuint>(before.copy)); restored = true; }
    }
    ~CopyReadGuard() { restore(); }
};
std::size_t cpuBytes(const PackedTerrainRenderBatch& cpu) {
    static_assert(sizeof(TerrainRenderVertex) == 44, "current production terrain stride required");
    static_assert(sizeof(std::uint32_t) == 4, "current production index stride required");
    require(cpu.vertices.size() <= OperationLimit / sizeof(TerrainRenderVertex) &&
            cpu.indices.size() <= OperationLimit / sizeof(std::uint32_t), "CPU size exceeds operation bound");
    const auto bytes = cpu.vertices.size() * sizeof(TerrainRenderVertex) +
        cpu.indices.size() * sizeof(std::uint32_t);
    require(bytes > 0 && bytes <= OperationLimit && !cpu.vertices.empty() &&
            !cpu.indices.empty() && cpu.indices.size() % 3 == 0, "invalid bounded CPU triangle storage");
    for (auto index : cpu.indices) require(index < cpu.vertices.size(), "CPU index outside packed vertices");
    return bytes;
}
std::string partsJson(const ShoreEditCapture::Binding& binding) {
    require(!binding.parts.empty() && binding.parts.size() <= 4, "one to four original parts required");
    require(binding.layer == "water" || binding.layer == "solid", "unknown upload layer");
    if (binding.layer == "water") require(binding.parts.size() == 1 &&
        binding.parts.front().section == binding.origin, "Water must retain its direct section owner");
    std::vector<std::string> result;
    int previousY = -1;
    for (const auto& part : binding.parts) {
        const auto y = std::int64_t(part.section.y) - binding.origin.y;
        require(part.section.x == binding.origin.x && part.section.z == binding.origin.z &&
                y >= 0 && y < 4 && y > previousY, "parts differ from original ordered column group");
        previousY = static_cast<int>(y);
        require(part.liveKnown && part.gpuResident && !part.stillCpuReady &&
                part.liveRevision == part.uploadRevision, "original part is not acknowledged current resident upload");
        result.push_back(object({{"section", xyz(part.section)},
            {"upload_revision", number(part.uploadRevision)}, {"live_revision", number(part.liveRevision)},
            {"live_known", boolean(part.liveKnown)}, {"gpu_resident", boolean(part.gpuResident)},
            {"still_cpu_ready", boolean(part.stillCpuReady)}, {"incarnation", "null"},
            {"incarnation_known", "false"}}));
    }
    return array(result);
}
std::string declarationJson(const Ogre::VertexDeclaration& declaration) {
    const auto& elements = declaration.getElements();
    require(elements.size() == 5, "exact original five-element declaration required");
    const std::array<std::size_t, 5> offsets{{0, 12, 20, 28, 40}};
    const std::array<Ogre::VertexElementType, 5> types{{Ogre::VET_FLOAT3, Ogre::VET_FLOAT2,
        Ogre::VET_FLOAT2, Ogre::VET_FLOAT3, Ogre::VET_FLOAT1}};
    std::vector<std::string> result; std::size_t i = 0;
    for (const auto& element : elements) {
        require(element.getSource() == 0 && element.getOffset() == offsets[i] &&
            element.getType() == types[i] &&
            element.getSemantic() == (i == 0 ? Ogre::VES_POSITION : Ogre::VES_TEXTURE_COORDINATES) &&
            element.getIndex() == (i == 0 ? 0 : i - 1), "original declaration is not production44B");
        result.push_back(object({{"source", number(element.getSource())}, {"offset", number(element.getOffset())},
            {"type", number(element.getType())}, {"semantic", number(element.getSemantic())},
            {"semantic_index", number(element.getIndex())},
            {"components", number(Ogre::VertexElement::getTypeCount(element.getType()))},
            {"bytes", number(Ogre::VertexElement::getTypeSize(element.getType()))}}));
        ++i;
    }
    return array(result);
}
}

struct ShoreEditCapture::Impl {
    fs::path directory;
    const std::thread::id owner = std::this_thread::get_id();
    std::size_t phases = 0, written = 0;
    bool failed = false;
    std::string prefix;
    std::vector<std::string> glErrors;
    explicit Impl(const std::string& output) : directory(output) {
        require(!output.empty() && !fs::exists(directory) && !fs::is_symlink(directory), "fresh output directory required");
        require(fs::create_directory(directory), "cannot create fresh output directory");
    }
    void write(const std::string& filename, const void* data, std::size_t bytes) {
        require(bytes <= SessionLimit - written, "256MiB observer write bound exceeded");
        const auto path = directory / filename;
        require(!fs::exists(path), "refusing to overwrite retained observation");
        std::ofstream out(path, std::ios::binary);
        require(bool(out), "cannot create observation file");
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
        out.close(); require(bool(out), "observation write failed"); written += bytes;
    }
    void textFile(const std::string& filename, const std::string& value) { write(filename, value.data(), value.size()); }
    bool retainGlErrors(const std::string& stage) {
        bool any = false;
        for (int i = 0; i < 32; ++i) {
            const auto error = glGetError(); if (error == GL_NO_ERROR) break;
            any = true; glErrors.push_back(object({{"stage", quote(stage)}, {"code", number(error)}}));
        }
        return any;
    }
    void checkGl(const std::string& stage) {
        require(!retainGlErrors(stage), "GL errors retained at " + stage);
    }
};

ShoreEditCapture::ShoreEditCapture(const std::string& directory) : m_impl(std::make_unique<Impl>(directory)) {}
ShoreEditCapture::~ShoreEditCapture() = default;
std::size_t ShoreEditCapture::phaseCount() const noexcept { return m_impl->phases; }
std::string ShoreEditCapture::phaseJsonPath() const {
    require(!m_impl->prefix.empty(), "no phase path available"); return (m_impl->directory / (m_impl->prefix + ".json")).string();
}
std::string ShoreEditCapture::phasePngPath() const {
    require(!m_impl->prefix.empty(), "no phase path available"); return (m_impl->directory / (m_impl->prefix + ".png")).string();
}
void ShoreEditCapture::capturePhase(const std::string& phase, const std::string& mode,
                                  std::uint64_t frameId, const std::vector<Binding>& bindings) {
    auto& s = *m_impl;
    require(std::this_thread::get_id() == s.owner && !s.failed, "render thread required; failed session cannot resume");
    require(s.phases < 6 && !bindings.empty() && bindings.size() <= 8, "phase/object bound exceeded");
    require(!phase.empty() && phase.size() <= 64 && (mode == "standard" || mode == "compatibility"), "invalid phase/profile");
    std::ostringstream name; name << "phase-" << std::setw(3) << std::setfill('0') << s.phases;
    s.prefix = name.str(); s.glErrors.clear();
    std::vector<std::string> operations;
    try {
        std::vector<ChunkSectionRenderable*> unique;
        std::size_t rawBytes = 0;
        for (const auto& binding : bindings) {
            require(binding.renderable && binding.uploadSerial > 0 && !binding.ownerKey.empty() && binding.ownerKey.size() <= 128 &&
                std::find(unique.begin(), unique.end(), binding.renderable) == unique.end(), "invalid/duplicate original object");
            unique.push_back(binding.renderable); partsJson(binding);
            rawBytes += cpuBytes(binding.cpu);
        }
        constexpr std::size_t metadataReserve = 64u * 1024u;
        require(SessionLimit - s.written >= metadataReserve &&
            rawBytes <= (SessionLimit - s.written - metadataReserve) / 2,
            "CPU/native raw writes exceed remaining session bound");
        s.checkGl("before-storage-read");
        for (const auto& binding : bindings) {
            auto& renderable = *binding.renderable;
            const auto material = renderable.getMaterial();
            require(renderable.isAttached() && !material.isNull() && material->getName() == (binding.layer == "water" ?
                "HelloMine3D/Water" : "HelloMine3D/Terrain"), "original object material/layer mismatch");
            Ogre::RenderOperation op; renderable.getRenderOperation(op);
            require(op.srcRenderable == &renderable && op.operationType == Ogre::RenderOperation::OT_TRIANGLE_LIST && op.useIndexes &&
                op.numberOfInstances == 1 && op.vertexData && op.indexData &&
                op.vertexData->vertexDeclaration && op.vertexData->vertexBufferBinding &&
                op.vertexData->vertexStart == 0 && op.indexData->indexStart == 0,
                "operation differs from original terrain triangle upload");
            const auto elements = declarationJson(*op.vertexData->vertexDeclaration);
            const auto& buffers = op.vertexData->vertexBufferBinding->getBindings();
            require(buffers.size() == 1 && buffers.begin()->first == 0 && !buffers.begin()->second.isNull() &&
                !op.indexData->indexBuffer.isNull(), "original source0 VBO/u32 IBO required");
            const auto& hardware = buffers.begin()->second;
            auto* vertex = dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(hardware.get());
            auto* index = dynamic_cast<Ogre::GL3PlusHardwareIndexBuffer*>(op.indexData->indexBuffer.get());
            require(vertex && index && hardware->getVertexSize() == 44 &&
                op.indexData->indexBuffer->getType() == Ogre::HardwareIndexBuffer::IT_32BIT &&
                op.indexData->indexBuffer->getIndexSize() == 4, "original native storage is not GL3Plus44B/u32");
            const auto vertexBytes = hardware->getSizeInBytes(), indexBytes = op.indexData->indexBuffer->getSizeInBytes();
            require(vertexBytes <= OperationLimit && indexBytes <= OperationLimit - vertexBytes &&
                op.vertexData->vertexCount == binding.cpu.vertices.size() &&
                op.indexData->indexCount == binding.cpu.indices.size() &&
                vertexBytes == binding.cpu.vertices.size() * sizeof(TerrainRenderVertex) &&
                indexBytes == binding.cpu.indices.size() * sizeof(std::uint32_t), "original CPU/native extents differ");
            Ogre::Matrix4 transform; renderable.getWorldTransforms(&transform);
            Ogre::Matrix4 expected = Ogre::Matrix4::IDENTITY;
            expected.setTrans(Ogre::Vector3(float(binding.origin.x) * CHUNK_SIZE,
                float(binding.origin.y) * CHUNK_SIZE, float(binding.origin.z) * CHUNK_SIZE));
            for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
                require(transform[r][c] == expected[r][c], "original node differs from registered constructor origin");
            std::vector<unsigned char> vertices(vertexBytes), indices(indexBytes);
            CopyReadGuard state; s.checkGl("storage-state-entry");
            require(vertex->getGLBufferId() && index->getGLBufferId() &&
                glIsBuffer(vertex->getGLBufferId()) && glIsBuffer(index->getGLBufferId()), "original native GL buffer missing");
            GLint64 allocated = 0;
            glBindBuffer(GL_COPY_READ_BUFFER, vertex->getGLBufferId());
            glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &allocated);
            require(allocated == static_cast<GLint64>(vertexBytes), "original native VBO allocation differs");
            glGetBufferSubData(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(vertexBytes), vertices.data());
            glBindBuffer(GL_COPY_READ_BUFFER, index->getGLBufferId());
            glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &allocated);
            require(allocated == static_cast<GLint64>(indexBytes), "original native IBO allocation differs");
            glGetBufferSubData(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(indexBytes), indices.data());
            s.checkGl("original-storage-read"); state.restore();
            const ContextState after; s.checkGl("storage-state-restored");
            const bool restored = state.before.same(after);
            const bool equal = std::memcmp(vertices.data(), binding.cpu.vertices.data(), vertexBytes) == 0 &&
                std::memcmp(indices.data(), binding.cpu.indices.data(), indexBytes) == 0;
            const auto prefix = s.prefix + "-op-" + number(operations.size());
            s.write(prefix + ".vbo0.bin", vertices.data(), vertexBytes);
            s.write(prefix + ".ibo.bin", indices.data(), indexBytes);
            s.write(prefix + ".cpu-vbo0.bin", binding.cpu.vertices.data(), vertexBytes);
            s.write(prefix + ".cpu-ibo.bin", binding.cpu.indices.data(), indexBytes);
            operations.push_back(object({{"object_name", quote(renderable.getName())},
                {"object_id", number(reinterpret_cast<std::uintptr_t>(&renderable))},
                {"upload_serial", number(binding.uploadSerial)}, {"layer", quote(binding.layer)},
                {"owner_key", quote(binding.ownerKey)}, {"origin", xyz(binding.origin)},
                {"material_name", quote(material->getName())}, {"parts", partsJson(binding)},
                {"operation_type", number(op.operationType)}, {"instances", number(op.numberOfInstances)},
                {"vertex_start", number(op.vertexData->vertexStart)}, {"vertex_count", number(op.vertexData->vertexCount)},
                {"index_start", number(op.indexData->indexStart)}, {"index_count", number(op.indexData->indexCount)},
                {"index_type", quote("u32")}, {"elements", elements},
                {"vertex_buffers", array({object({{"source", "0"}, {"stride", "44"},
                    {"gl_id", number(vertex->getGLBufferId())}, {"vertices", number(hardware->getNumVertices())},
                    {"bytes", number(vertexBytes)}, {"file", quote(prefix + ".vbo0.bin")},
                    {"cpu_file", quote(prefix + ".cpu-vbo0.bin")}})})},
                {"index_buffer", object({{"gl_id", number(index->getGLBufferId())}, {"bytes", number(indexBytes)},
                    {"file", quote(prefix + ".ibo.bin")}, {"cpu_file", quote(prefix + ".cpu-ibo.bin")}})},
                {"node_world_row_major", matrix(transform)}, {"native_cpu_bytes_equal", boolean(equal)},
                {"context_before", state.before.json()}, {"context_after", after.json()},
                {"state_restored", boolean(restored)}}));
            require(restored, "COPY_READ context restoration mismatch");
            require(equal, "original native/actual uploader CPU bytes differ");
        }
        s.checkGl("after-storage-read");
        const auto packet = object({{"schema", quote("hellomine3d-shore-edit-original-storage-v1")},
            {"status", quote("CAPTURED")}, {"phase", quote(phase)}, {"mode", quote(mode)},
            {"frame_id", number(frameId)}, {"operations", array(operations)}, {"gl_errors", array(s.glErrors)},
            {"scope", quote("actual original upload storage and caller-supplied revision/residency facts; independent World/map facts and original PNG are recorded by Bootstrap")},
            {"inner_draw_vertex_fetch", quote("OPEN_NOT_OBSERVED")}, {"pixel_attribution", quote("OPEN_NOT_OBSERVED")},
            {"atomic_incarnation_aba", quote("OPEN_PUBLIC_LIVE_VERSION_SNAPSHOT_HAS_NO_INCARNATION")},
            {"ordinary_input", quote("NOT_RUN")}});
        s.textFile(s.prefix + ".json", packet + '\n'); ++s.phases;
    }
    catch (const std::exception& error) {
        s.failed = true;
        s.retainGlErrors("failure-after-copy-read-restoration");
        // Preserve partial raw files and the failure instead of replacing the
        // phase with an apparently successful empty packet.
        const auto packet = object({{"schema", quote("hellomine3d-shore-edit-original-storage-v1")},
            {"status", quote("FAIL")}, {"phase", quote(phase)}, {"mode", quote(mode)},
            {"frame_id", number(frameId)}, {"error", quote(error.what())},
            {"operations", array(operations)}, {"gl_errors", array(s.glErrors)}});
        try { s.textFile(s.prefix + ".json", packet + '\n'); } catch (...) {}
        throw;
    }
}
