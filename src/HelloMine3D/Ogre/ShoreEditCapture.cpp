#include "ShoreEditCapture.h"

#include <OgreGL3PlusPrerequisites.h>
#include <OgreGL3PlusHardwareIndexBuffer.h>
#include <OgreGL3PlusHardwareVertexBuffer.h>
#include <OgreVertexIndexData.h>
#include <Ogre.h>
#include <OgreAutoParamDataSource.h>
#include <GLSL/OgreGLSLShader.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
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
bool sameBinding(const ShoreEditCapture::Binding& a, const ShoreEditCapture::Binding& b) {
    if (a.renderable != b.renderable || a.origin != b.origin || a.layer != b.layer ||
        a.ownerKey != b.ownerKey || a.uploadSerial != b.uploadSerial || a.parts.size() != b.parts.size() ||
        a.cpu.vertices.size() != b.cpu.vertices.size() || a.cpu.indices.size() != b.cpu.indices.size()) return false;
    for (std::size_t i = 0; i < a.parts.size(); ++i) {
        const auto& x = a.parts[i]; const auto& y = b.parts[i];
        if (x.section != y.section || x.uploadRevision != y.uploadRevision || x.liveRevision != y.liveRevision ||
            x.liveKnown != y.liveKnown || x.gpuResident != y.gpuResident || x.stillCpuReady != y.stillCpuReady) return false;
    }
    return std::memcmp(a.cpu.vertices.data(), b.cpu.vertices.data(), a.cpu.vertices.size() * sizeof(TerrainRenderVertex)) == 0 &&
        std::memcmp(a.cpu.indices.data(), b.cpu.indices.data(), a.cpu.indices.size() * sizeof(std::uint32_t)) == 0;
}
}

struct ShoreEditCapture::Impl {
    fs::path directory;
    const std::thread::id owner = std::this_thread::get_id();
    std::size_t phases = 0, written = 0;
    bool failed = false;
    std::string prefix;
    std::vector<std::string> glErrors;
    ShoreEditCapture& observer;
    Ogre::SceneManager& scene;
    Ogre::Camera& camera;
    const bool nativeDraw;
    bool frameOpen = false, nativeReady = false, nativeFailureWritten = false;
    std::uint64_t nativeFrameId = 0, attemptedFrames = 0;
    std::vector<Binding> nativeBindings;
    std::vector<std::string> openReasons;
    struct OpenHistory { std::uint64_t frames = 0, first = 0, last = 0; };
    std::map<std::string, OpenHistory> openHistory;
    std::size_t historyPhase = 0;
    struct Pending {
        const Ogre::Pass* pass = nullptr;
        const Ogre::Camera* callbackCamera = nullptr;
        GLuint query = 0;
        GLint conflict = 0;
        bool began = false;
        bool geometry = false, hull = false, domain = false, compute = false;
        std::size_t globalInstances = 0;
        std::size_t attempt = 0;
    };
    struct Attempt {
        ChunkSectionRenderable* object = nullptr;
        const Ogre::Camera* camera = nullptr;
        const Ogre::Pass* pass = nullptr;
        bool main = false, pre = false, post = false;
        std::string status = "OPEN_NO_PAIRED_NATIVE_CALLBACK", reason;
        std::string json() const {
            return ::object({{"object_id", number(reinterpret_cast<std::uintptr_t>(object))},
                {"camera_id", number(reinterpret_cast<std::uintptr_t>(camera))},
                {"pass_id", number(reinterpret_cast<std::uintptr_t>(pass))}, {"main_camera", boolean(main)},
                {"pre_called", boolean(pre)}, {"post_called", boolean(post)},
                {"status", quote(status)}, {"reason", quote(reason)}});
        }
    };
    std::vector<Attempt> attempts;
    struct NativeOperation {
        std::string json, file;
        std::vector<unsigned char> vertices, indices;
    };
    std::map<ChunkSectionRenderable*, Pending> pending;
    std::map<ChunkSectionRenderable*, NativeOperation> nativeOperations;
    Impl(ShoreEditCapture& o, const std::string& output, Ogre::SceneManager& sm, Ogre::Camera& c, bool draw)
        : directory(output), observer(o), scene(sm), camera(c), nativeDraw(draw) {
        require(!output.empty() && !fs::exists(directory) && !fs::is_symlink(directory), "fresh output directory required");
        require(fs::create_directory(directory), "cannot create fresh output directory");
        if (nativeDraw) scene.addRenderObjectListener(&observer);
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
    Binding* binding(ChunkSectionRenderable* renderable) {
        auto it = std::find_if(nativeBindings.begin(), nativeBindings.end(),
            [&](const auto& b) { return b.renderable == renderable; });
        return it == nativeBindings.end() ? nullptr : &*it;
    }
    void open(const std::string& reason) {
        const auto value = quote(reason);
        if (openReasons.size() < 32 && std::find(openReasons.begin(), openReasons.end(), value) == openReasons.end()) {
            openReasons.push_back(value);
            if (openHistory.size() < 32 || openHistory.count(reason)) {
                auto& history = openHistory[reason];
                if (!history.frames) history.first = nativeFrameId;
                if (!history.frames || history.last != nativeFrameId) ++history.frames;
                history.last = nativeFrameId;
            }
        }
    }
    void openAttempt(Pending& p, const std::string& reason) {
        open(reason);
        if (p.attempt < attempts.size()) { attempts[p.attempt].status = "OPEN"; attempts[p.attempt].reason = reason; }
    }
    void cleanupQuery(Pending& p) noexcept {
        if (!p.query) return;
        GLint active = 0; glGetQueryiv(GL_PRIMITIVES_GENERATED, GL_CURRENT_QUERY, &active);
        if (p.began && GLuint(active) == p.query) glEndQuery(GL_PRIMITIVES_GENERATED);
        glDeleteQueries(1, &p.query); p.query = 0; p.began = false;
    }
    void detachAll() noexcept {
        for (auto& item : pending) cleanupQuery(item.second);
        pending.clear();
        for (auto& b : nativeBindings) if (b.renderable) b.renderable->setNativeDrawObserver(nullptr);
        frameOpen = false;
    }
    std::string nativeJson(std::uint64_t uiFrameId, bool captured, bool uiFrameKnown = true) const {
        std::vector<std::string> ops, attemptJson, historyJson;
        for (const auto& b : nativeBindings) {
            const auto found = nativeOperations.find(b.renderable);
            if (found != nativeOperations.end()) ops.push_back(found->second.json);
        }
        for (const auto& attempt : attempts) attemptJson.push_back(attempt.json());
        for (const auto& entry : openHistory) historyJson.push_back(object({{"reason", quote(entry.first)},
            {"frames", number(entry.second.frames)}, {"first_frame", number(entry.second.first)},
            {"last_frame", number(entry.second.last)}}));
        return object({{"schema", quote("hellomine3d-shore-native-draw-v1")},
            {"status", quote(captured ? "CAPTURED" : "OPEN")}, {"frame_id", number(nativeFrameId)},
            {"main_camera_id", number(reinterpret_cast<std::uintptr_t>(&camera))},
            {"ui_frame_id", uiFrameKnown ? number(uiFrameId) : "null"}, {"attempted_frames", number(attemptedFrames)},
            {"operations", array(ops)}, {"draw_attempts", array(attemptJson)}, {"open_reasons", array(openReasons)},
            {"prior_and_current_open_history", array(historyJson)},
            {"gl_errors", array(glErrors)},
            {"atomic_incarnation_aba", quote("OPEN_PUBLIC_LIVE_VERSION_SNAPSHOT_HAS_NO_INCARNATION")},
            {"pixel_attribution", quote("OPEN_NOT_OBSERVED")}});
    }
    void writeNativeRaw() {
        for (const auto& b : nativeBindings) {
            const auto found = nativeOperations.find(b.renderable);
            if (found == nativeOperations.end()) continue;
            const auto& op = found->second;
            write(op.file + ".vbo0.bin", op.vertices.data(), op.vertices.size());
            write(op.file + ".ibo.bin", op.indices.data(), op.indices.size());
            write(op.file + ".cpu-vbo0.bin", b.cpu.vertices.data(), op.vertices.size());
            write(op.file + ".cpu-ibo.bin", b.cpu.indices.data(), op.indices.size());
        }
    }
};

ShoreEditCapture::ShoreEditCapture(const std::string& directory, Ogre::SceneManager& scene, Ogre::Camera& camera, bool nativeDraw)
    : m_impl(std::make_unique<Impl>(*this, directory, scene, camera, nativeDraw)) {}
ShoreEditCapture::~ShoreEditCapture() {
    if (m_impl && m_impl->nativeDraw) {
        m_impl->detachAll(); m_impl->scene.removeRenderObjectListener(this);
    }
}
std::size_t ShoreEditCapture::phaseCount() const noexcept { return m_impl->phases; }
std::string ShoreEditCapture::phaseJsonPath() const {
    require(!m_impl->prefix.empty(), "no phase path available"); return (m_impl->directory / (m_impl->prefix + ".json")).string();
}
std::string ShoreEditCapture::phasePngPath() const {
    require(!m_impl->prefix.empty(), "no phase path available"); return (m_impl->directory / (m_impl->prefix + ".png")).string();
}
bool ShoreEditCapture::nativeDrawEnabled() const noexcept { return m_impl->nativeDraw; }
void ShoreEditCapture::cancelNativeFrame() noexcept {
    auto& s = *m_impl; s.detachAll(); s.nativeReady = false;
}
void ShoreEditCapture::detachRenderable(ChunkSectionRenderable& renderable) noexcept {
    auto& s = *m_impl;
    if (!s.nativeDraw) return;
    renderable.setNativeDrawObserver(nullptr);
    const auto p = s.pending.find(&renderable);
    if (p != s.pending.end()) { s.cleanupQuery(p->second); s.pending.erase(p); }
    s.nativeOperations.erase(&renderable);
    for (auto& b : s.nativeBindings) if (b.renderable == &renderable) b.renderable = nullptr;
    s.nativeReady = false; s.open("original selected instance was detached or replaced");
}
void ShoreEditCapture::retainNativeFailure(const std::string& reason) noexcept {
    auto& s = *m_impl;
    if (!s.nativeDraw || s.nativeFailureWritten) return;
    s.nativeFailureWritten = true; s.failed = true; s.detachAll();
    try {
        s.retainGlErrors("native-draw-failure-after-cleanup");
        // Incomplete frames remain incomplete. Preserve any observed original
        // storage and metadata without claiming a six-phase successful capture.
        try { s.writeNativeRaw(); } catch (...) {}
        const auto packet = object({{"schema", quote("hellomine3d-shore-native-draw-failure-v1")},
            {"status", quote("FAIL")}, {"error", quote(reason)}, {"phase_index", number(s.phases)},
            {"native_draw_observation", s.nativeJson(0, false, false)}, {"gl_errors", array(s.glErrors)}});
        s.textFile("native-draw-failure.json", packet + '\n');
    } catch (...) {}
}
void ShoreEditCapture::beginNativeFrame(std::uint64_t frameId, std::vector<Binding> bindings) {
    auto& s = *m_impl;
    try {
    require(s.nativeDraw && !s.failed && std::this_thread::get_id() == s.owner && !s.frameOpen,
        "native frame requires the original render thread and enabled healthy observer");
    require(s.phases < 6 && !bindings.empty() && bindings.size() <= 8, "native phase/object bound exceeded");
    s.detachAll(); s.nativeReady = false; s.nativeOperations.clear(); s.nativeBindings.clear();
    s.openReasons.clear(); s.attempts.clear(); s.glErrors.clear(); s.nativeFrameId = frameId; ++s.attemptedFrames;
    if (s.historyPhase != s.phases) { s.openHistory.clear(); s.historyPhase = s.phases; }
    std::size_t rawBytes = 0; std::vector<ChunkSectionRenderable*> unique;
    for (const auto& b : bindings) {
        require(b.renderable && b.uploadSerial && std::find(unique.begin(), unique.end(), b.renderable) == unique.end(),
            "invalid/duplicate native original object");
        unique.push_back(b.renderable); partsJson(b); rawBytes += cpuBytes(b.cpu);
    }
    constexpr std::size_t metadataReserve = 128u * 1024u;
    require(SessionLimit - s.written >= metadataReserve &&
        rawBytes <= (SessionLimit - s.written - metadataReserve) / 4,
        "native and existing storage writes exceed remaining 256MiB session bound");
    s.checkGl("native-frame-before-attachment");
    s.nativeBindings = std::move(bindings); s.frameOpen = true;
    for (auto& b : s.nativeBindings) b.renderable->setNativeDrawObserver(this);
    } catch (const std::exception& e) { retainNativeFailure(e.what()); throw; }
}
void ShoreEditCapture::notifyRenderSingleObject(Ogre::Renderable* renderable, const Ogre::Pass* pass,
    const Ogre::AutoParamDataSource* source, const Ogre::LightList*, bool suppressed) {
    auto& s = *m_impl;
    if (!s.frameOpen) return;
    try {
    auto* instance = dynamic_cast<ChunkSectionRenderable*>(renderable);
    if (!instance || !s.binding(instance)) return;
    require(std::this_thread::get_id() == s.owner, "native listener thread mismatch");
    auto& p = s.pending[instance];
    // Every listener callback replaces the pairing, including excluded shadow
    // cameras. A previous main-camera pass cannot leak into another draw.
    require(!p.began && !p.query, "listener entered while an owned native query remains active");
    p.pass = nullptr; p.conflict = 0; p.callbackCamera = source ? source->getCurrentCamera() : nullptr;
    require(s.attempts.size() < 128, "native callback metadata exceeds bounded frame attempts");
    p.attempt = s.attempts.size();
    s.attempts.push_back({instance, p.callbackCamera, pass,
        !suppressed && p.callbackCamera == &s.camera && pass != nullptr, false, false,
        "OPEN_NO_PAIRED_NATIVE_CALLBACK", {}});
    if (suppressed || !source || source->getCurrentCamera() != &s.camera || !pass) {
        s.attempts.back().status = "EXCLUDED_SCOPE_NON_MAIN_CAMERA_OR_SUPPRESSED";
        s.attempts.back().reason = "listener excluded: suppressed, missing pass or non-main camera";
        s.open(s.attempts.back().reason); return;
    }
    p.pass = pass;
    } catch (const std::exception& e) { retainNativeFailure(e.what()); throw; }
}
void ShoreEditCapture::beforeNativeDraw(ChunkSectionRenderable& renderable, Ogre::SceneManager* scene,
                                      Ogre::RenderSystem* renderSystem) {
    auto& s = *m_impl;
    if (!s.frameOpen || !s.binding(&renderable)) return;
    try {
        require(std::this_thread::get_id() == s.owner && scene == &s.scene, "native pre scene/thread mismatch");
        auto& p = s.pending[&renderable]; require(!p.began && !p.query, "nested native primitive query");
        if (p.attempt < s.attempts.size()) s.attempts[p.attempt].pre = true;
        if (!p.pass) { s.open("no matching main-camera pass before actual draw"); return; }
        if (s.nativeOperations.count(&renderable)) { s.openAttempt(p, "multiple main-camera draws for original selected instance in one frame"); return; }
        p.geometry = renderSystem && renderSystem->isGpuProgramBound(Ogre::GPT_GEOMETRY_PROGRAM);
        p.hull = renderSystem && renderSystem->isGpuProgramBound(Ogre::GPT_HULL_PROGRAM);
        p.domain = renderSystem && renderSystem->isGpuProgramBound(Ogre::GPT_DOMAIN_PROGRAM);
        p.compute = renderSystem && renderSystem->isGpuProgramBound(Ogre::GPT_COMPUTE_PROGRAM);
        p.globalInstances = renderSystem ? renderSystem->getGlobalNumberOfInstances() : 0;
        if (!renderSystem || p.pass->getPassIterationCount() != 1 ||
            p.pass->getParent()->getNumPasses() != 1 || p.pass->hasGeometryProgram() ||
            p.pass->hasTessellationHullProgram() || p.pass->hasTessellationDomainProgram() || p.pass->hasComputeProgram() ||
            p.geometry || p.hull || p.domain || p.compute ||
            !renderSystem->getGlobalInstanceVertexBuffer().isNull() || renderSystem->getGlobalInstanceVertexBufferVertexDeclaration()) {
            s.openAttempt(p, "unsupported pass iteration, stages or global instance data"); p.pass = nullptr; return;
        }
        glGetQueryiv(GL_PRIMITIVES_GENERATED, GL_CURRENT_QUERY, &p.conflict);
        if (p.conflict) { s.openAttempt(p, "existing primitive query left untouched"); return; }
        s.checkGl("native-before-query");
        glGenQueries(1, &p.query); require(p.query, "native query allocation failed");
        glBeginQuery(GL_PRIMITIVES_GENERATED, p.query); p.began = true;
        s.checkGl("native-query-begin");
    } catch (const std::exception& e) { retainNativeFailure(e.what()); throw; }
}
void ShoreEditCapture::afterNativeDraw(ChunkSectionRenderable& renderable, Ogre::SceneManager* scene,
                                     Ogre::RenderSystem*) {
    auto& s = *m_impl;
    if (!s.frameOpen || !s.binding(&renderable)) return;
    try {
        require(std::this_thread::get_id() == s.owner && scene == &s.scene, "native post scene/thread mismatch");
        auto it = s.pending.find(&renderable);
        require(it != s.pending.end(), "unpaired original native callback");
        auto& p = it->second; const auto* pass = p.pass;
        if (p.attempt < s.attempts.size()) s.attempts[p.attempt].post = true;
        if (s.nativeOperations.count(&renderable)) { p.pass = nullptr; return; }
        GLint active = 0; glGetQueryiv(GL_PRIMITIVES_GENERATED, GL_CURRENT_QUERY, &active);
        const GLuint ownQuery = p.query; GLuint generated = 0, available = 0;
        const bool ended = p.began && ownQuery && GLuint(active) == ownQuery;
        if (ended) {
            glEndQuery(GL_PRIMITIVES_GENERATED); p.began = false;
            glGetQueryObjectuiv(ownQuery, GL_QUERY_RESULT_AVAILABLE, &available);
            glGetQueryObjectuiv(ownQuery, GL_QUERY_RESULT, &generated);
        }
        const GLint conflict = p.conflict; s.cleanupQuery(p); p.pass = nullptr;
        GLint queryAfterEnd = 0; glGetQueryiv(GL_PRIMITIVES_GENERATED, GL_CURRENT_QUERY, &queryAfterEnd);
        s.checkGl("native-query-end");
        if (!ended || !pass) {
            if (p.attempt < s.attempts.size() && s.attempts[p.attempt].main)
                s.openAttempt(p, "no owned complete main-camera primitive query");
            return;
        }
        require(queryAfterEnd == 0, "owned primitive query remained active after paired post");
        const ContextState entry; s.checkGl("native-post-entry");
        if (!entry.vao) { s.openAttempt(p, "actual post draw VAO is zero; await a genuine warm draw"); return; }
        require(glIsVertexArray(GLuint(entry.vao)), "actual post draw VAO no longer exists");
        if (entry.pipeline || !entry.program || !glIsProgram(GLuint(entry.program))) {
            s.openAttempt(p, "unsupported pipeline or missing actual monolithic program"); return;
        }
        GLint major = 0, minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major); glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (major < 4 || (major == 4 && minor < 1)) {
            s.openAttempt(p, "GL4.1 required for this native draw observation"); return;
        }
        // ARRAY_LONG became queryable in core 4.3 (spec bug 8272). On 4.1/4.2,
        // an actually queried FLOAT array cannot be an unconverted double:
        // VertexAttribLPointer requires DOUBLE. Record that derivation rather
        // than issuing an unsupported query or claiming a queried long flag.
        const bool longQuerySupported = major > 4 || (major == 4 && minor >= 3);
        GLint linked = 0, shaderCount = 0;
        glGetProgramiv(GLuint(entry.program), GL_LINK_STATUS, &linked);
        glGetProgramiv(GLuint(entry.program), GL_ATTACHED_SHADERS, &shaderCount);
        if (linked != GL_TRUE || shaderCount != 2 || !pass->hasVertexProgram() || !pass->hasFragmentProgram()) {
            s.openAttempt(p, "actual program is not a supported linked two-stage pass"); return;
        }
        const auto* vs = dynamic_cast<const Ogre::GLSLShader*>(pass->getVertexProgram().get());
        const auto* fsShader = dynamic_cast<const Ogre::GLSLShader*>(pass->getFragmentProgram().get());
        if (!vs || !fsShader) { s.openAttempt(p, "selected pass does not expose production GLSL stage handles"); return; }
        std::array<GLuint, 2> shaders{}; GLsizei actualShaders = 0;
        glGetAttachedShaders(GLuint(entry.program), 2, &actualShaders, shaders.data());
        std::vector<std::string> attached; bool stagesMatch = actualShaders == 2;
        for (auto shader : shaders) {
            GLint type = 0, compiled = 0;
            glGetShaderiv(shader, GL_SHADER_TYPE, &type); glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            const bool match = compiled == GL_TRUE &&
                ((type == GL_VERTEX_SHADER && shader == vs->getGLShaderHandle()) ||
                 (type == GL_FRAGMENT_SHADER && shader == fsShader->getGLShaderHandle()));
            stagesMatch = stagesMatch && match;
            attached.push_back(object({{"id", number(shader)}, {"type", number(type)},
                {"compiled", boolean(compiled == GL_TRUE)}, {"production_pass_match", boolean(match)}}));
        }
        if (!stagesMatch) { s.openAttempt(p, "actual attached stages do not match the selected main-camera pass"); return; }
        Ogre::RenderOperation op; renderable.getRenderOperation(op);
        require(op.srcRenderable == &renderable && op.operationType == Ogre::RenderOperation::OT_TRIANGLE_LIST && op.useIndexes &&
            op.numberOfInstances == 1 && op.vertexData && op.indexData && op.vertexData->vertexDeclaration &&
            op.vertexData->vertexBufferBinding && op.vertexData->vertexStart == 0 && op.indexData->indexStart == 0,
            "native original operation is not production single-instance triangles");
        declarationJson(*op.vertexData->vertexDeclaration);
        const auto effectiveInstances = op.useGlobalInstancingVertexBufferIsAvailable ? p.globalInstances : std::size_t(1);
        if (effectiveInstances != 1) { s.openAttempt(p, "unsupported actual global instance count"); return; }
        if (generated != op.indexData->indexCount / 3 || generated == 0) {
            s.openAttempt(p, "actual generated primitives do not cover one complete original operation"); return;
        }
        auto& binding = *s.binding(&renderable);
        const auto& buffers = op.vertexData->vertexBufferBinding->getBindings();
        require(buffers.size() == 1 && buffers.begin()->first == 0 && !buffers.begin()->second.isNull() &&
            !op.indexData->indexBuffer.isNull(), "native original source0 and u32 index storage required");
        const auto& hardware = buffers.begin()->second;
        auto* vertex = dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(hardware.get());
        auto* index = dynamic_cast<Ogre::GL3PlusHardwareIndexBuffer*>(op.indexData->indexBuffer.get());
        require(vertex && index && hardware->getVertexSize() == 44 &&
            op.indexData->indexBuffer->getType() == Ogre::HardwareIndexBuffer::IT_32BIT &&
            op.indexData->indexBuffer->getIndexSize() == 4, "native source0 must be production44B/u32");
        const auto vbytes = hardware->getSizeInBytes(), ibytes = op.indexData->indexBuffer->getSizeInBytes();
        require(vbytes <= OperationLimit && ibytes <= OperationLimit - vbytes &&
            vbytes == binding.cpu.vertices.size() * sizeof(TerrainRenderVertex) &&
            ibytes == binding.cpu.indices.size() * sizeof(std::uint32_t) &&
            op.vertexData->vertexCount == binding.cpu.vertices.size() && op.indexData->indexCount == binding.cpu.indices.size(),
            "native CPU and original operation extents differ");
        GLint attributeCount = 0, maximumName = 0;
        glGetProgramiv(GLuint(entry.program), GL_ACTIVE_ATTRIBUTES, &attributeCount);
        glGetProgramiv(GLuint(entry.program), GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maximumName);
        if (attributeCount < 1 || attributeCount > 5 || maximumName < 1 || maximumName > 128) {
            s.openAttempt(p, "unsupported actual active-input count or name extent"); return;
        }
        std::vector<std::string> attributes; bool attributesMatch = true; bool havePosition = false;
        const std::array<std::string, 5> names{{"vertex", "uv0", "uv1", "uv2", "uv3"}};
        const std::array<GLenum, 5> shaderTypes{{GL_FLOAT_VEC4, GL_FLOAT_VEC2, GL_FLOAT_VEC2, GL_FLOAT_VEC3, GL_FLOAT}};
        const std::array<GLint, 5> sizes{{3, 2, 2, 3, 1}};
        const std::array<std::size_t, 5> offsets{{0, 12, 20, 28, 40}};
        std::vector<GLint> locations;
        for (GLint a = 0; a < attributeCount; ++a) {
            std::array<char, 128> name{}; GLsizei length = 0; GLint shaderSize = 0; GLenum shaderType = 0;
            glGetActiveAttrib(GLuint(entry.program), GLuint(a), GLsizei(name.size()), &length, &shaderSize, &shaderType, name.data());
            const std::string input(name.data(), std::size_t(length));
            auto semantic = std::find(names.begin(), names.end(), input);
            const GLint location = glGetAttribLocation(GLuint(entry.program), input.c_str());
            if (semantic == names.end() || shaderSize != 1 || location < 0 ||
                std::find(locations.begin(), locations.end(), location) != locations.end()) {
                s.openAttempt(p, "unsupported active shader input or repeated native location"); return;
            }
            const auto slot = std::size_t(semantic - names.begin()); locations.push_back(location);
            if (shaderType != shaderTypes[slot]) { s.openAttempt(p, "active shader input type differs from production mapping"); return; }
            havePosition = havePosition || slot == 0;
            GLint enabled = 0, buffer = 0, type = 0, size = 0, stride = 0, normalized = 0, integer = 0, divisor = 0, longType = 0;
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &buffer);
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_TYPE, &type);
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &normalized);
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);
            glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &divisor);
            if (longQuerySupported)
                glGetVertexAttribiv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_LONG, &longType);
            void* pointer = nullptr;
            glGetVertexAttribPointerv(GLuint(location), GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
            const auto offset = reinterpret_cast<std::uintptr_t>(pointer);
            const auto expected = offsets[slot] + op.vertexData->vertexStart * hardware->getVertexSize();
            const bool longObserved = longQuerySupported || type == GL_FLOAT;
            const bool matches = enabled == GL_TRUE && GLuint(buffer) == vertex->getGLBufferId() && type == GL_FLOAT &&
                size == sizes[slot] && stride == 44 && !normalized && !integer && longObserved &&
                !longType && divisor == 0 && offset == expected;
            attributesMatch = attributesMatch && matches;
            attributes.push_back(object({{"name", quote(input)}, {"location", number(location)},
                {"shader_type", number(shaderType)}, {"shader_array_size", number(shaderSize)},
                {"enabled", boolean(enabled == GL_TRUE)}, {"buffer", number(buffer)}, {"type", number(type)},
                {"size", number(size)}, {"stride", number(stride)}, {"normalized", boolean(normalized != 0)},
                {"integer", boolean(integer != 0)}, {"long", longObserved ? boolean(longType != 0) : "null"},
                {"long_observation_mode", quote(longQuerySupported ? "QUERY_GL43_ARRAY_LONG" : "DERIVED_GL_FLOAT_EXCLUDES_DOUBLE")},
                {"divisor", number(divisor)},
                {"pointer_offset", number(offset)}, {"expected_offset", number(expected)},
                {"semantic", number(slot == 0 ? Ogre::VES_POSITION : Ogre::VES_TEXTURE_COORDINATES)},
                {"semantic_index", number(slot == 0 ? 0 : slot - 1)}, {"matches_declaration", boolean(matches)}}));
        }
        if (!havePosition) { s.openAttempt(p, "no active position input in the actual linked program"); return; }
        s.checkGl("native-active-input-reflection");
        Impl::NativeOperation captured;
        std::ostringstream file; file << "phase-" << std::setw(3) << std::setfill('0') << s.phases << "-native-op-"
            << std::distance(s.nativeBindings.begin(), std::find_if(s.nativeBindings.begin(), s.nativeBindings.end(),
                [&](const auto& b) { return b.renderable == &renderable; }));
        captured.file = file.str(); captured.vertices.resize(vbytes); captured.indices.resize(ibytes);
        CopyReadGuard state; s.checkGl("native-storage-entry");
        require(vertex->getGLBufferId() && index->getGLBufferId() && glIsBuffer(vertex->getGLBufferId()) &&
            glIsBuffer(index->getGLBufferId()), "native original buffers are missing");
        GLint64 allocated = 0;
        glBindBuffer(GL_COPY_READ_BUFFER, vertex->getGLBufferId());
        glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &allocated);
        require(allocated == GLint64(vbytes), "native original VBO allocation mismatch");
        glGetBufferSubData(GL_COPY_READ_BUFFER, 0, GLsizeiptr(vbytes), captured.vertices.data());
        glBindBuffer(GL_COPY_READ_BUFFER, index->getGLBufferId());
        glGetBufferParameteri64v(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &allocated);
        require(allocated == GLint64(ibytes), "native original IBO allocation mismatch");
        glGetBufferSubData(GL_COPY_READ_BUFFER, 0, GLsizeiptr(ibytes), captured.indices.data());
        s.checkGl("native-original-storage-read"); state.restore();
        const ContextState after; s.checkGl("native-post-state-restored");
        const bool restored = state.before.same(after);
        const bool equal = std::memcmp(captured.vertices.data(), binding.cpu.vertices.data(), vbytes) == 0 &&
            std::memcmp(captured.indices.data(), binding.cpu.indices.data(), ibytes) == 0;
        const bool eboMatch = GLuint(entry.ebo) == index->getGLBufferId();
        captured.json = object({{"object_name", quote(renderable.getName())},
            {"object_id", number(reinterpret_cast<std::uintptr_t>(&renderable))},
            {"camera_id", number(reinterpret_cast<std::uintptr_t>(p.callbackCamera))},
            {"upload_serial", number(binding.uploadSerial)}, {"material_name", quote(renderable.getMaterial()->getName())},
            {"pass", object({{"id", number(reinterpret_cast<std::uintptr_t>(pass))}, {"index", number(pass->getIndex())},
                {"iterations", number(pass->getPassIterationCount())}, {"vertex_program", quote(pass->getVertexProgramName())},
                {"fragment_program", quote(pass->getFragmentProgramName())}, {"vertex_shader_id", number(vs->getGLShaderHandle())},
                {"fragment_shader_id", number(fsShader->getGLShaderHandle())}, {"main_camera", "true"}})},
            {"render_system_stages", object({{"geometry", boolean(p.geometry)}, {"tessellation_hull", boolean(p.hull)},
                {"tessellation_domain", boolean(p.domain)}, {"compute", boolean(p.compute)}})},
            {"global_instance_buffer_present", "false"}, {"source0_instance_data", boolean(hardware->getIsInstanceData())},
            {"global_instance_count", number(p.globalInstances)}, {"effective_instances", number(effectiveInstances)},
            {"global_instancing_available", boolean(op.useGlobalInstancingVertexBufferIsAvailable)},
            {"program", number(entry.program)}, {"program_linked", boolean(linked == GL_TRUE)},
            {"gl_version", array({number(major), number(minor)})},
            {"attached_shaders", array(attached)}, {"pipeline", number(entry.pipeline)}, {"vao", number(entry.vao)},
            {"vbo", number(vertex->getGLBufferId())}, {"ebo", number(entry.ebo)}, {"expected_ebo", number(index->getGLBufferId())},
            {"vertex_start", number(op.vertexData->vertexStart)}, {"vertex_count", number(op.vertexData->vertexCount)},
            {"index_start", number(op.indexData->indexStart)}, {"index_count", number(op.indexData->indexCount)},
            {"index_type", quote("u32")}, {"instances", number(op.numberOfInstances)},
            {"primitive_query", object({{"target", number(GL_PRIMITIVES_GENERATED)}, {"id", number(ownQuery)},
                {"began", "true"}, {"ended", boolean(ended)}, {"conflicting_query", number(conflict)},
                {"available_before_blocking_read", boolean(available != 0)}, {"result", number(generated)},
                {"expected_whole_operation", number(op.indexData->indexCount / 3)}, {"current_query_after_end", number(queryAfterEnd)}})},
            {"active_attribute_count", number(attributeCount)}, {"active_attributes", array(attributes)},
            {"source0", object({{"stride", "44"}, {"vertices", number(op.vertexData->vertexCount)},
                {"vertex_bytes", number(vbytes)}, {"index_bytes", number(ibytes)}, {"native_cpu_bytes_equal", boolean(equal)},
                {"file", quote(captured.file + ".vbo0.bin")}, {"cpu_file", quote(captured.file + ".cpu-vbo0.bin")},
                {"index_file", quote(captured.file + ".ibo.bin")}, {"cpu_index_file", quote(captured.file + ".cpu-ibo.bin")}})},
            {"context_before", state.before.json()}, {"context_after", after.json()},
            {"state_restored", boolean(restored)}, {"gl_errors", array(s.glErrors)}});
        s.nativeOperations.emplace(&renderable, std::move(captured));
        if (p.attempt < s.attempts.size()) s.attempts[p.attempt].status = "CAPTURED";
        require(attributesMatch && eboMatch, "actual warm VAO attribute/index bindings differ from original production operation");
        require(!hardware->getIsInstanceData(), "native source0 unexpectedly carries instance data");
        require(restored, "native COPY_READ context restoration mismatch");
        require(equal, "native post-draw/actual uploader CPU bytes differ");
    } catch (const std::exception& e) { retainNativeFailure(e.what()); throw; }
}
bool ShoreEditCapture::finishNativeFrame(std::vector<Binding> endBindings) {
    auto& s = *m_impl;
    if (!s.frameOpen) return false;
    try {
        require(std::this_thread::get_id() == s.owner && !s.failed, "native endpoint thread/session mismatch");
        s.detachAll(); s.checkGl("native-frame-after-detachment");
        bool endpoints = endBindings.size() == s.nativeBindings.size();
        for (const auto& b : s.nativeBindings) {
            const auto found = std::find_if(endBindings.begin(), endBindings.end(),
                [&](const auto& x) { return x.renderable == b.renderable; });
            endpoints = endpoints && b.renderable && found != endBindings.end() && sameBinding(b, *found);
        }
        if (!endpoints) s.open("same-frame original owner, CPU, serial or revision endpoint differs");
        if (s.nativeOperations.size() != s.nativeBindings.size()) s.open("not every original selected object has a supported actual warm draw in this frame");
        bool actualPairs = true;
        for (const auto& b : s.nativeBindings) {
            std::size_t main = 0;
            for (const auto& attempt : s.attempts) if (attempt.object == b.renderable && attempt.main) {
                ++main;
                actualPairs = actualPairs && attempt.pre && attempt.post && attempt.status == "CAPTURED";
            }
            actualPairs = actualPairs && main == 1;
        }
        if (!actualPairs) s.open("this frame lacks exactly one complete supported main-camera pair for every selected object");
        s.nativeReady = endpoints && actualPairs && s.nativeOperations.size() == s.nativeBindings.size();
        return s.nativeReady;
    } catch (const std::exception& e) { retainNativeFailure(e.what()); throw; }
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
        if (s.nativeDraw) {
            require(s.nativeReady && !s.frameOpen && bindings.size() == s.nativeBindings.size(),
                "native phase has no complete original same-frame warm draw");
            for (std::size_t i = 0; i < bindings.size(); ++i)
                require(sameBinding(bindings[i], s.nativeBindings[i]), "native/capture original owner or CPU endpoint differs");
            // Only a fully joined original frame reaches disk. Failed/open
            // retries retained bounded metadata and never replaced these raws.
            s.writeNativeRaw();
        }
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
        Fields fields{{"schema", quote("hellomine3d-shore-edit-original-storage-v1")},
            {"status", quote("CAPTURED")}, {"phase", quote(phase)}, {"mode", quote(mode)},
            {"frame_id", number(frameId)}, {"operations", array(operations)}, {"gl_errors", array(s.glErrors)},
            {"scope", quote("actual original upload storage and caller-supplied revision/residency facts; independent World/map facts and original PNG are recorded by Bootstrap")},
            {"inner_draw_vertex_fetch", quote(s.nativeDraw ? "SCOPED_ACTUAL_WARM_VAO_BINDINGS" : "OPEN_NOT_OBSERVED")},
            {"pixel_attribution", quote("OPEN_NOT_OBSERVED")},
            {"atomic_incarnation_aba", quote("OPEN_PUBLIC_LIVE_VERSION_SNAPSHOT_HAS_NO_INCARNATION")},
            {"ordinary_input", quote("NOT_RUN")}};
        if (s.nativeDraw) {
            fields.push_back({"native_render_frame_id", number(s.nativeFrameId)});
            fields.push_back({"native_draw_observation", s.nativeJson(frameId, true)});
        }
        const auto packet = object(fields);
        s.textFile(s.prefix + ".json", packet + '\n'); ++s.phases;
        s.nativeReady = false;
    }
    catch (const std::exception& error) {
        s.failed = true;
        s.retainGlErrors("failure-after-copy-read-restoration");
        // Preserve partial raw files and the failure instead of replacing the
        // phase with an apparently successful empty packet.
        Fields fields{{"schema", quote("hellomine3d-shore-edit-original-storage-v1")},
            {"status", quote("FAIL")}, {"phase", quote(phase)}, {"mode", quote(mode)},
            {"frame_id", number(frameId)}, {"error", quote(error.what())},
            {"operations", array(operations)}, {"gl_errors", array(s.glErrors)}};
        if (s.nativeDraw) {
            fields.push_back({"native_render_frame_id", number(s.nativeFrameId)});
            fields.push_back({"native_draw_observation", s.nativeJson(frameId, s.nativeReady)});
        }
        const auto packet = object(fields);
        try { s.textFile(s.prefix + ".json", packet + '\n'); } catch (...) {}
        throw;
    }
}
