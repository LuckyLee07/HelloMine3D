// Diagnostic fixture for the production renderer, not ordinary gameplay.
// Link this file and OgreCaveBoundaryRenderer.cpp against the current Ogre
// GL3Plus static libraries. Run: <tool> <repository-root> <ogre-log-path>
// It creates a hidden, non-activating native context and an offscreen target.
// GPU execution is deliberately separate from compiling this tool.
#include <Ogre.h>
#include <OgreGL3PlusPlugin.h>
#include <OgreGL3PlusHardwareVertexBuffer.h>
#include <OgreGL3PlusTexture.h>
#include <OgreSimpleRenderable.h>

#include "Ogre/OgreCaveBoundaryRenderer.h"
#include "World/Chunk/ChunkRuntime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using Renderer = OgreCaveBoundaryRenderer;
    constexpr std::size_t TargetEdge = 64;
    int checks = 0, failures = 0;
    std::size_t glErrors = 0;
    std::size_t startupGlErrors = 0, postStartupGlErrors = 0;
    bool startupComplete = false;

    void recordGlError()
    {
        ++glErrors;
        if (startupComplete) ++postStartupGlErrors;
        else ++startupGlErrors;
    }

    void glStage(const std::string& stage)
    {
        // Every consumed error is retained in the aggregate verdict. Staging
        // isolates an inherited startup error from fixture reads and production writes.
        bool clean = true;
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            clean = false;
            recordGlError();
            std::cout << "[CAVE_BOUNDARY_RENDERER_GL] stage=" << stage
                      << " error=0x" << std::hex << error << std::dec << '\n';
        }
        if (clean)
            std::cout << "[CAVE_BOUNDARY_RENDERER_GL] stage=" << stage << " error=0\n";
    }

    // Startup-only diagnostics wrap the actual GL3w driver entry points after
    // they are loaded. They neither replace engine initialization nor accept
    // errors. Each call is checked before and after so an untraced prior call
    // cannot be falsely attributed to the next traced call.
    void startupCallErrors(const char* call, std::initializer_list<GLenum> arguments)
    {
        for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
        {
            recordGlError();
            std::cout << "[CAVE_BOUNDARY_STARTUP_GL] call=" << call << " args=";
            for (const auto argument : arguments) std::cout << "0x" << std::hex << argument << ',';
            std::cout << " error=0x" << error << std::dec << '\n';
        }
    }

#define TRACE_VOID(Name, Parameters, Arguments, Tokens) \
    decltype(gl3w##Name) real##Name = nullptr; \
    void trace##Name Parameters \
    { \
        startupCallErrors("before gl" #Name, {}); \
        real##Name Arguments; \
        startupCallErrors("gl" #Name, std::initializer_list<GLenum> Tokens); \
    }
    TRACE_VOID(GetIntegerv, (GLenum name, GLint* value), (name, value), ({name}))
    TRACE_VOID(GetFloatv, (GLenum name, GLfloat* value), (name, value), ({name}))
    TRACE_VOID(Enable, (GLenum capability), (capability), ({capability}))
    TRACE_VOID(Disable, (GLenum capability), (capability), ({capability}))
    TRACE_VOID(ProvokingVertex, (GLenum mode), (mode), ({mode}))
    TRACE_VOID(BindTexture, (GLenum target, GLuint texture), (target, texture), ({target, texture}))
    TRACE_VOID(TexParameteri, (GLenum target, GLenum name, GLint value), (target, name, value),
               ({target, name, static_cast<GLenum>(value)}))
    TRACE_VOID(TexImage2D, (GLenum target, GLint level, GLint internal, GLsizei width,
               GLsizei height, GLint border, GLenum format, GLenum type, const void* data),
               (target, level, internal, width, height, border, format, type, data),
               ({target, static_cast<GLenum>(internal), format, type}))
    TRACE_VOID(BindFramebuffer, (GLenum target, GLuint framebuffer), (target, framebuffer), ({target}))
    TRACE_VOID(BindRenderbuffer, (GLenum target, GLuint renderbuffer), (target, renderbuffer), ({target}))
    TRACE_VOID(RenderbufferStorage, (GLenum target, GLenum internal, GLsizei width, GLsizei height),
               (target, internal, width, height), ({target, internal}))
    TRACE_VOID(FramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textureTarget,
               GLuint texture, GLint level), (target, attachment, textureTarget, texture, level),
               ({target, attachment, textureTarget}))
    TRACE_VOID(FramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbufferTarget,
               GLuint renderbuffer), (target, attachment, renderbufferTarget, renderbuffer),
               ({target, attachment, renderbufferTarget}))
    TRACE_VOID(DrawBuffer, (GLenum mode), (mode), ({mode}))
    TRACE_VOID(ReadBuffer, (GLenum mode), (mode), ({mode}))
    TRACE_VOID(PixelStorei, (GLenum name, GLint value), (name, value), ({name, static_cast<GLenum>(value)}))
#undef TRACE_VOID

    PFNGLGETSTRINGPROC realGetString = nullptr;
    const GLubyte* traceGetString(GLenum name)
    {
        startupCallErrors("before glGetString", {});
        const auto* result = realGetString(name);
        startupCallErrors("glGetString", {name});
        return result;
    }

    class StartupTrace final : public Ogre::LogListener
    {
      public:
        StartupTrace() : m_log(Ogre::LogManager::getSingleton().getDefaultLog())
        {
            requireLog();
            m_log->addListener(this);
        }

        ~StartupTrace() override
        {
            m_log->removeListener(this);
            if (!m_installed) return;
#define RESTORE(Name) gl3w##Name = real##Name
            RESTORE(GetIntegerv); RESTORE(GetFloatv); RESTORE(GetString);
            RESTORE(Enable); RESTORE(Disable); RESTORE(ProvokingVertex);
            RESTORE(BindTexture); RESTORE(TexParameteri); RESTORE(TexImage2D);
            RESTORE(BindFramebuffer); RESTORE(BindRenderbuffer); RESTORE(RenderbufferStorage);
            RESTORE(FramebufferTexture2D); RESTORE(FramebufferRenderbuffer);
            RESTORE(DrawBuffer); RESTORE(ReadBuffer); RESTORE(PixelStorei);
#undef RESTORE
        }

        void messageLogged(const Ogre::String& message, Ogre::LogMessageLevel,
                           bool, const Ogre::String&, bool&) override
        {
            if (!gl3wGetError) return;
            if (message.find("GL_VERSION = ") == 0 && !m_installed)
            {
                glStage("startup checkpoint GL3w/context/version initialized");
#define INSTALL(Name) real##Name = gl3w##Name; gl3w##Name = trace##Name
                INSTALL(GetIntegerv); INSTALL(GetFloatv); INSTALL(GetString);
                INSTALL(Enable); INSTALL(Disable); INSTALL(ProvokingVertex);
                INSTALL(BindTexture); INSTALL(TexParameteri); INSTALL(TexImage2D);
                INSTALL(BindFramebuffer); INSTALL(BindRenderbuffer); INSTALL(RenderbufferStorage);
                INSTALL(FramebufferTexture2D); INSTALL(FramebufferRenderbuffer);
                INSTALL(DrawBuffer); INSTALL(ReadBuffer); INSTALL(PixelStorei);
#undef INSTALL
                m_installed = true;
            }
            if (message.find("GL3+: Using FBOs") == 0 || message.find("FBO ") == 0 ||
                message.find("[GL] : Valid FBO targets") == 0 || message == "RenderSystem capabilities")
                glStage("startup log checkpoint " + message.substr(0, 50));
        }

      private:
        void requireLog()
        {
            if (!m_log) throw std::runtime_error("Ogre startup tracing requires its actual log");
        }
        Ogre::Log* m_log;
        bool m_installed = false;
    };

    void require(bool value, const std::string& message)
    {
        if (!value) throw std::runtime_error(message);
    }

    void check(const std::string& name, bool value)
    {
        ++checks;
        failures += value ? 0 : 1;
        std::cout << "[CAVE_BOUNDARY_RENDERER_GPU] "
                  << (value ? "PASS " : "FAIL ") << name << '\n';
    }

    void sync(Renderer& renderer, const std::vector<WorldBoundaryMaskFace>& faces,
              bool uploadNewMasks = true)
    {
        static std::size_t sequence = 0;
        renderer.sync(faces, uploadNewMasks);
        glStage("production sync " + std::to_string(++sequence) +
                " uploadNewMasks=" + (uploadNewMasks ? "true" : "false"));
    }

    void clear(Renderer& renderer)
    {
        renderer.clear();
        glStage("production clear");
    }

    WorldBoundaryMaskFace fullFace(int x = 0, std::uint8_t direction = 3)
    {
        WorldBoundaryMaskFace face;
        face.location = {x, 0, 0};
        face.face = direction;
        face.blockRevision = 1;
        face.incarnation = 1;
        face.rows.fill(0xffff);
        return face;
    }

    WorldBoundaryMaskFace asymmetricFace()
    {
        auto face = fullFace();
        face.rows.fill(0);
        face.rows[2] = (1u << 1) | (1u << 11);
        face.rows[10] = 1u << 4;
        face.rows[15] = 0xf000;
        return face;
    }

    Ogre::SimpleRenderable& findRenderable(Ogre::SceneManager& scene)
    {
        auto* root = scene.getRootSceneNode();
        for (unsigned short i = 0; i < root->numChildren(); ++i)
        {
            auto* node = static_cast<Ogre::SceneNode*>(root->getChild(i));
            for (unsigned short j = 0; j < node->numAttachedObjects(); ++j)
            {
                auto* object = node->getAttachedObject(j);
                if (object->getName().find("HelloMine3D/CaveBoundaryInstance") == 0)
                    if (auto* result = dynamic_cast<Ogre::SimpleRenderable*>(object))
                        return *result;
            }
        }
        throw std::runtime_error("Production boundary renderable was not attached");
    }

    struct GpuResources
    {
        Ogre::SimpleRenderable* renderable;
        Ogre::HardwareVertexBufferSharedPtr vertices;
        Ogre::HardwareIndexBufferSharedPtr indices;
        Ogre::TexturePtr atlas;
        Ogre::MaterialPtr material;

        explicit GpuResources(Ogre::SceneManager& scene)
            : renderable(&findRenderable(scene)), material(renderable->getMaterial())
        {
            Ogre::RenderOperation operation;
            renderable->getRenderOperation(operation);
            require(operation.vertexData && operation.indexData, "Missing GPU geometry");
            vertices = operation.vertexData->vertexBufferBinding->getBuffer(0);
            indices = operation.indexData->indexBuffer;
            auto* unit = material->getTechnique(0)->getPass(0)->getTextureUnitState(0);
            atlas = Ogre::TextureManager::getSingleton().getByName(unit->getTextureName());
            require(!atlas.isNull(), "Missing production atlas");
        }

        std::vector<float> readVertices() const
        {
            std::vector<float> result(Renderer::VertexBytes / sizeof(float));
            // This buffer has no shadow copy. GL3Plus readData uses glGetBufferSubData.
            vertices->readData(0, Renderer::VertexBytes, result.data());
            glStage("fixture VBO readData");
            return result;
        }

        std::vector<unsigned char> readAtlas() const
        {
            std::vector<unsigned char> result(Renderer::AtlasBytes);
            atlas->getBuffer()->blitToMemory(Ogre::PixelBox(
                Renderer::AtlasWidth, Renderer::AtlasHeight, 1, Ogre::PF_L8,
                result.data()));
            glStage("fixture atlas blitToMemory");
            return result;
        }

        bool allVerticesZero() const
        {
            const auto data = readVertices();
            return std::all_of(data.begin(), data.end(), [](float v) { return v == 0; });
        }
    };

    bool tileMatches(const std::vector<unsigned char>& atlas, std::size_t slot,
                     const std::array<std::uint16_t, 16>& rows)
    {
        const std::size_t left = (slot % 32) * 16, top = (slot / 32) * 16;
        for (std::size_t y = 0; y < 16; ++y)
            for (std::size_t u = 0; u < 16; ++u)
                if (atlas[(top + y) * Renderer::AtlasWidth + left + u] !=
                    (((rows[y] >> u) & 1u) ? 255 : 0)) return false;
        return true;
    }

    class DrawProbe
    {
      public:
        explicit DrawProbe(Ogre::SceneManager& scene)
        {
            m_camera = scene.createCamera("CaveBoundaryFixtureCamera");
            m_camera->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
            m_camera->setOrthoWindow(16, 16);
            m_camera->setNearClipDistance(.1f);
            m_camera->setFarClipDistance(100);
            m_camera->setPosition(8, 8, 24);
            m_camera->lookAt(8, 8, 16);
            m_target = Ogre::TextureManager::getSingleton().createManual(
                "CaveBoundaryFixtureTarget", "General", Ogre::TEX_TYPE_2D,
                TargetEdge, TargetEdge, 0, Ogre::PF_BYTE_RGBA,
                // Ogre's PBO readback strips RTT flags and requires a valid
                // base buffer usage; RTT alone becomes zero and asserts in Debug.
                Ogre::TU_RENDERTARGET | Ogre::TU_STATIC_WRITE_ONLY);
            m_renderTarget = m_target->getBuffer()->getRenderTarget();
            m_renderTarget->setAutoUpdated(false);
            auto* viewport = m_renderTarget->addViewport(m_camera);
            viewport->setBackgroundColour(Ogre::ColourValue(.2f, .55f, .85f, 1));
            viewport->setOverlaysEnabled(false);
            viewport->setShadowsEnabled(false);
            glStage("fixture RTT allocation");
        }

        bool matches(const std::array<std::uint16_t, 16>& rows)
        {
            m_renderTarget->update();
            glStage("fixture RTT update (production draw)");
            std::vector<unsigned char> pixels(TargetEdge * TargetEdge * 4);
            m_target->getBuffer()->blitToMemory(Ogre::PixelBox(
                TargetEdge, TargetEdge, 1, Ogre::PF_BYTE_RGBA, pixels.data()));
            glStage("fixture RTT blitToMemory");
            bool result = true;
            for (std::size_t y = 0; y < TargetEdge; ++y)
                for (std::size_t x = 0; x < TargetEdge; ++x)
                {
                    // Ogre inverts projection Y for texture targets; readback is raw GL order.
                    const std::size_t worldY = m_renderTarget->requiresTextureFlipping()
                        ? TargetEdge - 1 - y : y;
                    const bool dark = (rows[worldY / 4] & (1u << (x / 4))) != 0;
                    const std::array<int, 3> expected = dark
                        ? std::array<int, 3>{9, 11, 14} : std::array<int, 3>{51, 140, 217};
                    for (std::size_t channel = 0; channel < 3; ++channel)
                        result &= std::abs(int(pixels[(y * TargetEdge + x) * 4 + channel])
                                           - expected[channel]) <= 3;
                }
            return result;
        }

      private:
        Ogre::Camera* m_camera = nullptr;
        Ogre::TexturePtr m_target;
        Ogre::RenderTexture* m_renderTarget = nullptr;
    };

    std::size_t instanceTextures()
    {
        std::size_t result = 0;
        auto iterator = Ogre::TextureManager::getSingleton().getResourceIterator();
        while (iterator.hasMoreElements())
            if (iterator.getNext()->getName().find("HelloMine3D/CaveBoundaryInstance") == 0)
                ++result;
        return result;
    }

    void guardCases(Ogre::SceneManager& scene)
    {
        auto source = Ogre::MaterialManager::getSingleton().getByName(Renderer::MaterialName);
        require(!source.isNull(), "Canonical material not parsed from current media");
        auto* pass = source->getTechnique(0)->getPass(0);
        const auto before = instanceTextures();
        auto reject = [&](const std::string& name, const std::function<void()>& change,
                          const std::function<void()>& restore)
        {
            change();
            bool rejected = false;
            try { Renderer invalid(scene); }
            catch (const std::runtime_error&) { rejected = true; }
            glStage("production constructor negative " + name);
            restore();
            check(name, rejected);
        };
        reject("reject extra technique", [&] { source->createTechnique(); },
               [&] { source->removeTechnique(1); });
        reject("reject extra pass", [&] { source->getTechnique(0)->createPass(); },
               [&] { source->getTechnique(0)->removePass(1); });
        reject("reject extra texture unit", [&] { pass->createTextureUnitState(); },
               [&] { pass->removeTextureUnitState(1); });
        reject("reject wrong unit name", [&] { pass->getTextureUnitState(0)->setName("wrong"); },
               [&] { pass->getTextureUnitState(0)->setName("caveBoundaryMask"); });
        reject("reject received shadows", [&] { source->setReceiveShadows(true); },
               [&] { source->setReceiveShadows(false); });
        reject("reject lighting", [&] { pass->setLightingEnabled(true); },
               [&] { pass->setLightingEnabled(false); });
        reject("reject depth check disabled", [&] { pass->setDepthCheckEnabled(false); },
               [&] { pass->setDepthCheckEnabled(true); });
        reject("reject depth writes", [&] { pass->setDepthWriteEnabled(true); },
               [&] { pass->setDepthWriteEnabled(false); });
        reject("reject culling", [&] { pass->setCullingMode(Ogre::CULL_CLOCKWISE); },
               [&] { pass->setCullingMode(Ogre::CULL_NONE); });
        check("rejected constructors release atlas", instanceTextures() == before);
    }

    void lifecycleCases(Ogre::SceneManager& scene)
    {
        guardCases(scene);
        DrawProbe draw(scene);
        const std::array<std::uint16_t, 16> empty{};
        std::string atlasName, materialName;
        const auto texturesBefore = instanceTextures();
        {
            Renderer renderer(scene);
            glStage("production valid constructor");
            GpuResources gpu(scene);
            atlasName = gpu.atlas->getName(); materialName = gpu.material->getName();
            auto* glBuffer = dynamic_cast<Ogre::GL3PlusHardwareVertexBuffer*>(gpu.vertices.get());
            auto* glTexture = dynamic_cast<Ogre::GL3PlusTexture*>(gpu.atlas.get());
            check("actual GL3Plus buffers without shadow copies", glBuffer && glTexture &&
                  !gpu.vertices->hasShadowBuffer() && !gpu.indices->hasShadowBuffer());
            check("696 KiB actual fixed resource sizes", renderer.stats().gpuBytes == 696 * 1024 &&
                  gpu.vertices->getSizeInBytes() == 163840 &&
                  gpu.indices->getSizeInBytes() == 24576 &&
                  gpu.atlas->getWidth() == 512 && gpu.atlas->getHeight() == 1024 &&
                  gpu.atlas->getFormat() == Ogre::PF_L8 && gpu.atlas->getNumMipmaps() == 0 &&
                  gpu.indices->getType() == Ogre::HardwareIndexBuffer::IT_16BIT);
            require(glTexture != nullptr, "Non-GL3Plus atlas");
            GLint oldTexture = 0, internalFormat = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
            glBindTexture(GL_TEXTURE_2D, glTexture->getGLID());
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
            glStage("fixture raw atlas internal-format query");
            check("actual atlas GL_R8 storage", internalFormat == GL_R8);
            auto* pass = gpu.material->getTechnique(0)->getPass(0);
            auto* unit = pass->getTextureUnitState(0);
            check("owned clone binds unit zero nearest clamp", gpu.material->getName() != Renderer::MaterialName &&
                  unit->getName() == "caveBoundaryMask" && unit->getTextureName() == atlasName &&
                  unit->getTextureFiltering(Ogre::FT_MIN) == Ogre::FO_POINT &&
                  unit->getTextureFiltering(Ogre::FT_MAG) == Ogre::FO_POINT &&
                  unit->getTextureFiltering(Ogre::FT_MIP) == Ogre::FO_NONE &&
                  unit->getTextureAddressingMode().u == Ogre::TextureUnitState::TAM_CLAMP);
            check("queue six no shadow caster", gpu.renderable->getRenderQueueGroup() ==
                  Ogre::RENDER_QUEUE_SKIES_EARLY + 1 && !gpu.renderable->getCastShadows());
            check("constructor VBO is degenerate", gpu.allVerticesZero());
            check("no world data draws only background", draw.matches(empty));
            std::vector<std::uint16_t> indices(Renderer::MaxFaces * 6);
            gpu.indices->readData(0, Renderer::IndexBytes, indices.data());
            glStage("fixture IB readData");
            check("static index reaches slot 2047 without overflow", indices[0] == 0 &&
                  indices[5] == 3 && indices[12282] == 8188 && indices[12287] == 8191);

            std::vector<WorldBoundaryMaskFace> faces;
            for (std::size_t i = 0; i < Renderer::MaxFaces; ++i)
                faces.push_back(fullFace(static_cast<int>(i * 2)));
            sync(renderer, faces, false);
            check("false sync assigns 2048 pending without upload", renderer.stats().liveFaces == 0 &&
                  renderer.stats().deferredFaces == 2048 && renderer.stats().updatesThisSync == 0 &&
                  renderer.stats().texturePatchBytesThisSync == 0 &&
                  renderer.stats().vertexPatchBytesThisSync == 0 && gpu.allVerticesZero());
            check("false new faces stay invisible on GPU", draw.matches(empty));
            bool bounded = true;
            for (std::size_t batch = 0; batch < 256; ++batch)
            {
                sync(renderer, faces);
                const auto& stats = renderer.stats();
                bounded &= stats.updatesThisSync == 8 && stats.texturePatchBytesThisSync == 2048 &&
                    stats.vertexPatchBytesThisSync == 640 && stats.liveFaces == (batch + 1) * 8 &&
                    stats.deferredFaces == 2048 - (batch + 1) * 8 && stats.gpuBytes == 712704;
            }
            check("2048 slots converge in 256 real eight-patch syncs", bounded);
            const auto fullAtlas = gpu.readAtlas();
            check("all actual GPU tiles contain uploaded full masks", std::all_of(
                  fullAtlas.begin(), fullAtlas.end(), [](unsigned char v) { return v == 255; }));
            check("production material draws active full face", draw.matches(faces[0].rows));
            sync(renderer, faces);
            check("unchanged identities cause zero GPU writes", renderer.stats().updatesThisSync == 0 &&
                  renderer.stats().texturePatchBytesThisSync == 0 && renderer.stats().vertexPatchBytesThisSync == 0);
            for (const auto slot : {31u, 32u, 2047u})
            {
                faces[slot].rows = asymmetricFace().rows;
                ++faces[slot].blockRevision;
            }
            sync(renderer, faces, false);
            sync(renderer, faces);
            const auto edgeAtlas = gpu.readAtlas();
            check("real tile patches cross atlas row and final-slot edges", renderer.stats().updatesThisSync == 3 &&
                  tileMatches(edgeAtlas, 0, faces[0].rows) && tileMatches(edgeAtlas, 31, faces[31].rows) &&
                  tileMatches(edgeAtlas, 32, faces[32].rows) && tileMatches(edgeAtlas, 2047, faces[2047].rows));
            const auto edgeVertices = gpu.readVertices();
            check("actual final-slot UVs address atlas last tile", edgeVertices[2047 * 20 + 3] == 31.f / 32 &&
                  edgeVertices[2047 * 20 + 4] == 63.f / 64 &&
                  edgeVertices[2047 * 20 + 13] == 1 && edgeVertices[2047 * 20 + 14] == 1);
            for (auto& face : faces) ++face.blockRevision;
            sync(renderer, faces, false);
            check("false confirmation immediately hides all dirty revisions", renderer.stats().liveFaces == 0 &&
                  renderer.stats().deferredFaces == 2048 && renderer.stats().hiddenFacesThisSync == 2048 &&
                  renderer.stats().vertexPatchBytesThisSync == 163840 &&
                  renderer.stats().texturePatchBytesThisSync == 0 && gpu.allVerticesZero());
            check("dirty revisions do not draw stale atlas", draw.matches(empty));
            clear(renderer);
            check("clear removes pending entries without additional GPU writes", renderer.stats().liveFaces == 0 &&
                  renderer.stats().deferredFaces == 0 && renderer.stats().removalsThisSync == 2048 &&
                  renderer.stats().vertexPatchBytesThisSync == 0);

            auto one = asymmetricFace();
            sync(renderer, {one});
            check("reuse uploads one tile before activation", renderer.stats().liveFaces == 1 &&
                  renderer.stats().updatesThisSync == 1 && renderer.stats().texturePatchBytesThisSync == 256 &&
                  renderer.stats().vertexPatchBytesThisSync == 80);
            check("asymmetric GPU atlas preserves y rows and u bits", tileMatches(gpu.readAtlas(), 0, one.rows));
            check("actual quad draw preserves asymmetric row and u orientation", draw.matches(one.rows));
            auto unchanged = one;
            ++one.incarnation;
            sync(renderer, {one}, false);
            check("same key replacement incarnation immediately degenerates VBO", renderer.stats().liveFaces == 0 &&
                  renderer.stats().deferredFaces == 1 && renderer.stats().hiddenFacesThisSync == 1 &&
                  renderer.stats().texturePatchBytesThisSync == 0 && renderer.stats().vertexPatchBytesThisSync == 80 &&
                  gpu.allVerticesZero());
            check("replacement incarnation hidden until upload", draw.matches(empty));
            sync(renderer, {one});
            check("replacement incarnation reactivates after tile patch", renderer.stats().updatesThisSync == 1 &&
                  renderer.stats().liveFaces == 1 && draw.matches(one.rows));
            ++one.blockRevision;
            one.rows.fill(0); one.rows[7] = 1u << 13;
            sync(renderer, {one}, false);
            check("revision and mask replacement hidden on confirmation", renderer.stats().hiddenFacesThisSync == 1 &&
                  renderer.stats().liveFaces == 0 && renderer.stats().texturePatchBytesThisSync == 0 && draw.matches(empty));
            sync(renderer, {one});
            check("replacement tile does not retain previous owner bits", tileMatches(gpu.readAtlas(), 0, one.rows) &&
                  draw.matches(one.rows));
            sync(renderer, {}, false);
            check("removed face immediately hides without texture patch", renderer.stats().removalsThisSync == 1 &&
                  renderer.stats().hiddenFacesThisSync == 1 && renderer.stats().liveFaces == 0 &&
                  renderer.stats().texturePatchBytesThisSync == 0 && gpu.allVerticesZero() && draw.matches(empty));
            sync(renderer, {unchanged}, false);
            check("removed slot new pending owner cannot expose old tile", renderer.stats().liveFaces == 0 &&
                  renderer.stats().deferredFaces == 1 && renderer.stats().updatesThisSync == 0 && draw.matches(empty));
            sync(renderer, {unchanged});
            check("removed slot reuse writes new owner tile", renderer.stats().updatesThisSync == 1 &&
                  tileMatches(gpu.readAtlas(), 0, unchanged.rows) && draw.matches(unchanged.rows));
            sync(renderer, {unchanged, unchanged});
            check("identical duplicates do not multiply slots or upload", renderer.stats().liveFaces == 1 &&
                  renderer.stats().updatesThisSync == 0);
            auto conflict = unchanged; ++conflict.blockRevision;
            bool rejected = false;
            try { sync(renderer, {unchanged, conflict}); }
            catch (const std::runtime_error&) { rejected = true; }
            check("conflicting duplicates reject before replacing live GPU owner", rejected &&
                  tileMatches(gpu.readAtlas(), 0, unchanged.rows) && draw.matches(unchanged.rows));
            auto invalid = fullFace(1, 4);
            sync(renderer, {unchanged, invalid});
            check("invalid face rejected without corrupting valid owner", renderer.stats().rejectedFacesThisSync == 1 &&
                  renderer.stats().liveFaces == 1 && renderer.stats().updatesThisSync == 0);
            auto zero = unchanged; zero.rows.fill(0);
            sync(renderer, {zero});
            check("zero mask removes visible entry without replacement upload", renderer.stats().liveFaces == 0 &&
                  renderer.stats().deferredFaces == 0 && renderer.stats().hiddenFacesThisSync == 1 &&
                  renderer.stats().texturePatchBytesThisSync == 0 && gpu.allVerticesZero());

            // Explicit coordinates are independent of the renderer's faceQuad implementation.
            const std::array<std::array<float, 12>, 4> positions{{
                {{-16,32,-48, -16,32,-32, -16,48,-32, -16,48,-48}},
                {{0,32,-48, 0,32,-32, 0,48,-32, 0,48,-48}},
                {{-16,32,-48, 0,32,-48, 0,48,-48, -16,48,-48}},
                {{-16,32,-32, 0,32,-32, 0,48,-32, -16,48,-32}}
            }};
            for (std::uint8_t direction = 0; direction < 4; ++direction)
            {
                clear(renderer);
                auto face = fullFace(0, direction); face.location = {-1, 2, -3};
                sync(renderer, {face});
                const auto vertices = gpu.readVertices();
                bool correct = true;
                for (std::size_t vertex = 0; vertex < 4; ++vertex)
                    for (std::size_t axis = 0; axis < 3; ++axis)
                        correct &= vertices[vertex * 5 + axis] == positions[direction][vertex * 3 + axis];
                check("exact negative-section plane face " + std::to_string(direction), correct);
            }
            clear(renderer);
            faces.push_back(fullFace(9000));
            sync(renderer, faces, false);
            check("over-capacity list retains at most 2048 slots", renderer.stats().deferredFaces == 2048 &&
                  renderer.stats().liveFaces == 0 && renderer.stats().rejectedFacesThisSync == 1);
            clear(renderer);
            faces.pop_back();
            for (std::size_t batch = 0; batch < 256; ++batch) sync(renderer, faces);
            clear(renderer);
            check("clear full active set immediately writes bounded 160 KiB", renderer.stats().hiddenFacesThisSync == 2048 &&
                  renderer.stats().removalsThisSync == 2048 && renderer.stats().vertexPatchBytesThisSync == 163840 &&
                  renderer.stats().texturePatchBytesThisSync == 0 && gpu.allVerticesZero());
            check("clear active set removes all visible draw", draw.matches(empty));
        }
        glStage("production renderer destructor");
        check("destructor removes per-instance texture and material", instanceTextures() == texturesBefore &&
              Ogre::TextureManager::getSingleton().getByName(atlasName).isNull() &&
              Ogre::MaterialManager::getSingleton().getByName(materialName).isNull());
        check("GPU fixture ends without GL error", glErrors == 0);
    }
}

int main(int argc, char** argv)
{
    const bool traceStartup = argc == 4 && std::string(argv[3]) == "--startup-trace";
    const bool startupOnly = traceStartup ||
        (argc == 4 && std::string(argv[3]) == "--startup-only");
    if (argc != 3 && !startupOnly)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <repository-root> <ogre-log-path> [--startup-only|--startup-trace]\n";
        return 2;
    }
    try
    {
        const auto repository = std::filesystem::absolute(argv[1]);
        const auto log = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(log.parent_path());
        // Root must be destroyed while its explicitly installed plugin still exists.
        auto plugin = std::make_unique<Ogre::GL3PlusPlugin>();
        auto root = std::make_unique<Ogre::Root>("", "", log.string());
        root->installPlugin(plugin.get());
        Ogre::RenderSystem* renderSystem = nullptr;
        for (auto* candidate : root->getAvailableRenderers())
            if (candidate && candidate->getName().find("OpenGL 3+") != Ogre::String::npos)
                renderSystem = candidate;
        require(renderSystem != nullptr, "No GL3Plus render system");
        root->setRenderSystem(renderSystem);
        for (const auto& option : std::array<std::pair<Ogre::String, Ogre::String>, 3>{{
                 {"Full Screen", "No"}, {"VSync", "No"}, {"FSAA", "0"}}})
            if (renderSystem->getConfigOptions().count(option.first))
                renderSystem->setConfigOption(option.first, option.second);
        root->initialise(false, "Cave Boundary Renderer Diagnostic");
        std::unique_ptr<StartupTrace> startupTrace;
        if (traceStartup) startupTrace = std::make_unique<StartupTrace>();
        Ogre::NameValuePairList windowParameters;
        windowParameters["hidden"] = "true";
        windowParameters["noActivate"] = "true";
        auto* window = root->createRenderWindow("Cave Boundary Renderer Diagnostic", 64, 64,
                                               false, &windowParameters);
        require(window != nullptr, "Hidden context window creation failed");
        window->setAutoUpdated(false);
        window->setDeactivateOnFocusChange(false);
        glStage("Ogre startup hidden context");
        std::cout << "[CAVE_BOUNDARY_RENDERER_GPU] diagnostic=1 gameplay=0 hidden=1 noActivate=1"
                  << " GL_VENDOR=" << glGetString(GL_VENDOR)
                  << " GL_RENDERER=" << glGetString(GL_RENDERER)
                  << " GL_VERSION=" << glGetString(GL_VERSION) << '\n';
        glStage("Ogre startup driver metadata");
        startupComplete = true;
        if (startupOnly)
        {
            std::cout << "[CAVE_BOUNDARY_STARTUP_BASELINE] media_loaded=0 scene_created=0"
                      << " boundary_renderer_created=0 gameplay=0 hidden=1 noActivate=1\n";
            std::cout << "[CAVE_BOUNDARY_STARTUP_BASELINE] driver_call_trace="
                      << (traceStartup ? 1 : 0) << '\n';
            glStage("minimal Ogre startup baseline final");
            check("minimal Ogre startup has no GL error", glErrors == 0);
            startupTrace.reset();
            root.reset();
            std::cout << "[CAVE_BOUNDARY_STARTUP_BASELINE] checks=" << checks
                      << " failures=" << failures << " gl_errors=" << glErrors
                      << " startup_gl_errors=" << startupGlErrors
                      << " post_startup_gl_errors=" << postStartupGlErrors << '\n';
            return failures || glErrors ? 1 : 0;
        }
        auto& resources = Ogre::ResourceGroupManager::getSingleton();
        resources.addResourceLocation((repository / "media/ogre").string(), "FileSystem", "General", true);
        resources.addResourceLocation((repository / "media/textures").string(), "FileSystem", "General", true);
        resources.initialiseAllResourceGroups();
        glStage("actual media scripts parsed");
        auto* scene = root->createSceneManager(Ogre::ST_GENERIC, "CaveBoundaryFixtureScene");
        glStage("fixture SceneManager creation");
        lifecycleCases(*scene);
        root->destroySceneManager(scene);
        glStage("fixture SceneManager destruction");
        check("actual media and boundary GPU stages without GL error", postStartupGlErrors == 0);
        root.reset();
        std::cout << "[CAVE_BOUNDARY_RENDERER_GPU] checks=" << checks
                  << " failures=" << failures << " gl_errors=" << glErrors
                  << " startup_gl_errors=" << startupGlErrors
                  << " post_startup_gl_errors=" << postStartupGlErrors << '\n';
        return failures || glErrors ? 1 : 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[CAVE_BOUNDARY_RENDERER_GPU] FATAL " << error.what()
                  << " checks=" << checks << " failures=" << failures << '\n';
        return 2;
    }
}
