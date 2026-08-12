#include "OgreBootstrap.h"
#include "OgreActorRenderer.h"
#include "ChunkSectionRenderable.h"
#include "OgreBlockOutline.h"
#include "OgreRenderCapture.h"
#include "OgreUserInterface.h"
#include "StartupErrorReporter.h"
#include "StartupResourcePreflight.h"

#include <OIS.h>
#include <Ogre.h>
#include <OgreGL3PlusPlugin.h>
#include <OgreWindowEventUtilities.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../Config.h"
#include "../Core/Camera.h"
#include "../Diagnostics/RuntimePerformanceCapture.h"
#include "../Player/Player.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/SandboxRuntime.h"
#include "../Util/ResourcePaths.h"
#include "../World/Chunk/Chunk.h"
#include "../World/Chunk/ChunkSection.h"
#include "../World/World.h"

namespace
{
    constexpr const char* ConfigFileName = "Mine.cfg";
    constexpr const char* ResourceFileName = "MineResources.cfg";
    constexpr const char* LogFileName = "MineOgre.log";
    constexpr const char* WindowTitle = "HelloMine3D";
    constexpr const char* SkyboxMaterial = "HelloMine3D/Skybox";

    bool isTrueValue(const char* value)
    {
        if (value == nullptr || value[0] == '\0')
        {
            return false;
        }
        const std::string text(value);
        return text != "0" && text != "false" && text != "FALSE" &&
               text != "False" && text != "off" && text != "OFF";
    }

    struct TerrainBuildSummary
    {
        std::size_t sectionCount = 0;
        std::size_t vertexCount = 0;
        std::size_t indexCount = 0;
        std::size_t transparentSectionCount = 0;
        std::size_t transparentVertexCount = 0;
        std::size_t transparentIndexCount = 0;
        std::size_t waterSectionCount = 0;
        std::size_t waterVertexCount = 0;
        std::size_t waterIndexCount = 0;
        std::size_t floraSectionCount = 0;
        std::size_t floraVertexCount = 0;
        std::size_t floraIndexCount = 0;

        TerrainBufferMetrics bufferMetrics() const noexcept
        {
            TerrainBufferMetrics metrics;
            metrics.add(vertexCount + transparentVertexCount +
                            waterVertexCount + floraVertexCount,
                        indexCount + transparentIndexCount +
                            waterIndexCount + floraIndexCount);
            return metrics;
        }
    };

    struct SectionVisual
    {
        Ogre::SceneNode* node = nullptr;
        std::vector<std::unique_ptr<ChunkSectionRenderable>> renderables;
    };

    std::string sectionKey(const glm::ivec3& location)
    {
        return std::to_string(location.x) + "_" +
               std::to_string(location.y) + "_" +
               std::to_string(location.z);
    }

    class OgreBootstrap final : public Ogre::FrameListener,
                                public Ogre::WindowEventListener,
                                public OIS::KeyListener,
                                public OIS::MouseListener
    {
      public:
        ~OgreBootstrap() override
        {
            shutdown();
        }

        bool validate()
        {
            loadGameConfig();
            createRoot();
            const std::size_t resourceLocations = configureResources();
            Ogre::RenderSystem* renderSystem = configureRenderSystem();
            const TerrainBuildSummary terrain = buildTerrain(false);
            const TerrainBufferMetrics terrainBuffers =
                terrain.bufferMetrics();
            const OgreRenderCaptureValidation capture =
                OgreRenderCapture::validateConfiguration();
            const OgreUserInterfaceValidation userInterface =
                OgreUserInterface::validateConfiguration(*m_worldPlayer);
            spawnValidationActors();
            const OgreActorRendererValidation actors =
                OgreActorRenderer::validateSnapshots(
                    m_world->collectActorSnapshots());

            std::cout << "[OGRE_VALIDATION] renderer="
                      << renderSystem->getName() << '\n';
            std::cout << "[OGRE_VALIDATION] resource_locations="
                      << resourceLocations << '\n';
            std::cout << "[OGRE_VALIDATION] ois_version="
                      << OIS::InputManager::getVersionNumber() << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_sections="
                      << terrain.sectionCount << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_vertices="
                      << terrain.vertexCount << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_indices="
                      << terrain.indexCount << '\n';
            std::cout << "[OGRE_VALIDATION] transparent_sections="
                      << terrain.transparentSectionCount << '\n';
            std::cout << "[OGRE_VALIDATION] transparent_vertices="
                      << terrain.transparentVertexCount << '\n';
            std::cout << "[OGRE_VALIDATION] transparent_indices="
                      << terrain.transparentIndexCount << '\n';
            std::cout << "[OGRE_VALIDATION] water_sections="
                      << terrain.waterSectionCount << '\n';
            std::cout << "[OGRE_VALIDATION] water_vertices="
                      << terrain.waterVertexCount << '\n';
            std::cout << "[OGRE_VALIDATION] water_indices="
                      << terrain.waterIndexCount << '\n';
            std::cout << "[OGRE_VALIDATION] flora_sections="
                      << terrain.floraSectionCount << '\n';
            std::cout << "[OGRE_VALIDATION] flora_vertices="
                      << terrain.floraVertexCount << '\n';
            std::cout << "[OGRE_VALIDATION] flora_indices="
                      << terrain.floraIndexCount << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_vertex_stride_bytes="
                      << TerrainBufferMetrics::VertexStrideBytes << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_index_stride_bytes="
                      << TerrainBufferMetrics::IndexStrideBytes << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_resident_vertices_estimate="
                      << terrainBuffers.vertexCount << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_resident_indices_estimate="
                      << terrainBuffers.indexCount << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_vertex_buffer_bytes_estimate="
                      << terrainBuffers.vertexBytes() << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_index_buffer_bytes_estimate="
                      << terrainBuffers.indexBytes() << '\n';
            std::cout << "[OGRE_VALIDATION] terrain_buffer_bytes_estimate="
                      << terrainBuffers.totalBytes() << '\n';
            std::cout << "[OGRE_VALIDATION] capture_config="
                      << (capture.valid ? "valid" : "invalid") << '\n';
            std::cout << "[OGRE_VALIDATION] capture_enabled="
                      << (capture.enabled ? "true" : "false") << '\n';
            std::cout << "[OGRE_VALIDATION] capture_targets="
                      << capture.targetCount << '\n';
            std::cout << "[OGRE_VALIDATION] hud_config="
                      << (userInterface.valid ? "valid" : "invalid")
                      << '\n';
            std::cout << "[OGRE_VALIDATION] hud_slots="
                      << userInterface.hotbarSlots << '\n';
            std::cout << "[OGRE_VALIDATION] hud_selected_slot="
                      << userInterface.selectedSlot << '\n';
            std::cout << "[OGRE_VALIDATION] debug_panel_enabled="
                      << (userInterface.debugPanelVisible ? "true" : "false")
                      << '\n';
            std::cout << "[OGRE_VALIDATION] actor_config="
                      << (actors.valid ? "valid" : "invalid") << '\n';
            std::cout << "[OGRE_VALIDATION] actor_count="
                      << actors.actorCount << '\n';
            std::cout << "[OGRE_VALIDATION] mob_count="
                      << actors.mobCount << '\n';
            std::cout << "[OGRE_VALIDATION] item_count="
                      << actors.itemCount << '\n';

            return capture.valid && userInterface.valid &&
                   actors.valid && actors.mobCount > 0 &&
                   actors.itemCount > 0 &&
                   resourceLocations > 0 &&
                   terrain.sectionCount > 0 &&
                   terrain.vertexCount > 0 && terrain.indexCount > 0 &&
                   terrain.waterSectionCount > 0 &&
                   terrain.waterVertexCount > 0 &&
                   terrain.waterIndexCount > 0 &&
                   terrain.floraSectionCount > 0 &&
                   terrain.floraVertexCount > 0 &&
                   terrain.floraIndexCount > 0 &&
                   terrainBuffers.vertexCount > 0 &&
                   terrainBuffers.indexCount > 0 &&
                   terrainBuffers.totalBytes() ==
                       terrainBuffers.vertexCount *
                               TerrainBufferMetrics::VertexStrideBytes +
                           terrainBuffers.indexCount *
                               TerrainBufferMetrics::IndexStrideBytes &&
                   (!isTrueValue(std::getenv(
                        "HELLOMINE3D_TRANSPARENT_FIXTURE")) ||
                    (terrain.transparentSectionCount > 0 &&
                     terrain.transparentVertexCount > 0 &&
                     terrain.transparentIndexCount > 0));
        }

        int run()
        {
            loadGameConfig();
            createRoot();
            configureResources();
            configureRenderSystem();
            createWindowAndScene();
            createInput();
            m_userInterface = std::make_unique<OgreUserInterface>(
                *m_window, *m_sceneManager, *m_camera, *m_worldPlayer);

            m_root->addFrameListener(this);
            Ogre::WindowEventUtilities::addWindowEventListener(m_window, this);
            m_listenersInstalled = true;
            m_root->startRendering();
            return EXIT_SUCCESS;
        }

      private:
        void loadGameConfig()
        {
            m_config = loadRuntimeConfig(
                ResourcePaths::bin("config.txt"));
        }

        void createRoot()
        {
            m_root = std::make_unique<Ogre::Root>(
                "", ConfigFileName, LogFileName);
            m_gl3PlusPlugin = std::make_unique<Ogre::GL3PlusPlugin>();
            m_root->installPlugin(m_gl3PlusPlugin.get());
        }

        std::size_t configureResources()
        {
            Ogre::ConfigFile resourceConfig;
            resourceConfig.load(ResourceFileName);

            std::size_t locationCount = 0;
            Ogre::ConfigFile::SectionIterator sections =
                resourceConfig.getSectionIterator();
            while (sections.hasMoreElements())
            {
                const Ogre::String group = sections.peekNextKey();
                const Ogre::ConfigFile::SettingsMultiMap* settings =
                    sections.getNext();
                for (const auto& setting : *settings)
                {
                    Ogre::ResourceGroupManager::getSingleton()
                        .addResourceLocation(setting.second, setting.first,
                                             group, true);
                    ++locationCount;
                }
            }
            return locationCount;
        }

        Ogre::RenderSystem* configureRenderSystem()
        {
            Ogre::RenderSystem* selected = nullptr;
            if (m_root->restoreConfig())
            {
                selected = m_root->getRenderSystem();
            }

            if (selected == nullptr)
            {
                const Ogre::RenderSystemList& renderers =
                    m_root->getAvailableRenderers();
                for (Ogre::RenderSystem* renderer : renderers)
                {
                    if (renderer != nullptr &&
                        renderer->getName().find("OpenGL 3+") !=
                            Ogre::String::npos)
                    {
                        selected = renderer;
                        break;
                    }
                }
            }

            if (selected == nullptr)
            {
                throw std::runtime_error(
                    "OpenGL 3+ render system was not registered.");
            }

            m_root->setRenderSystem(selected);
            setOptionIfAvailable(*selected, "Full Screen",
                                 m_config.isFullscreen ? "Yes" : "No");
            setOptionIfAvailable(*selected, "VSync", "Yes");
            setOptionIfAvailable(*selected, "FSAA", "0");
            selectWindowSize(
                *selected, std::to_string(m_config.windowX) + " x " +
                               std::to_string(m_config.windowY));
            return selected;
        }

        static void setOptionIfAvailable(Ogre::RenderSystem& renderSystem,
                                         const Ogre::String& name,
                                         const Ogre::String& value)
        {
            const Ogre::ConfigOptionMap& options =
                renderSystem.getConfigOptions();
            if (options.find(name) != options.end())
            {
                renderSystem.setConfigOption(name, value);
            }
        }

        static void selectWindowSize(Ogre::RenderSystem& renderSystem,
                                     const Ogre::String& preferred)
        {
            const Ogre::ConfigOptionMap& options =
                renderSystem.getConfigOptions();
            const auto option = options.find("Video Mode");
            if (option == options.end())
            {
                return;
            }

            const Ogre::StringVector& values = option->second.possibleValues;
            if (std::find(values.begin(), values.end(), preferred) !=
                values.end())
            {
                renderSystem.setConfigOption("Video Mode", preferred);
            }
        }

        void createWindowAndScene()
        {
            m_window = m_root->initialise(true, WindowTitle);
            if (m_window == nullptr)
            {
                throw std::runtime_error("Ogre failed to create a window.");
            }

            m_window->setDeactivateOnFocusChange(false);
            m_sceneManager = m_root->createSceneManager(
                Ogre::ST_GENERIC, "HelloMine3DScene");
            m_camera = m_sceneManager->createCamera("PlayerCamera");
            m_camera->setPosition(0.0f, 1.0f, 5.0f);
            m_camera->lookAt(0.0f, 1.0f, 0.0f);
            m_camera->setNearClipDistance(0.1f);
            m_camera->setFarClipDistance(10000.0f);
            m_camera->setFOVy(
                Ogre::Degree(static_cast<Ogre::Real>(m_config.fov)));
            m_camera->setFixedYawAxis(true, Ogre::Vector3::UNIT_Y);

            Ogre::Viewport* viewport = m_window->addViewport(m_camera);
            viewport->setBackgroundColour(
                Ogre::ColourValue(0.2f, 0.55f, 0.85f));
            updateAspectRatio();

            Ogre::ResourceGroupManager::getSingleton()
                .initialiseAllResourceGroups();
            m_sceneManager->setAmbientLight(
                Ogre::ColourValue(0.7f, 0.7f, 0.7f));
            m_sceneManager->setSkyBox(
                true, SkyboxMaterial, 5000.0f, true);

            const TerrainBuildSummary terrain = buildTerrain(true);
            m_actorRenderer =
                std::make_unique<OgreActorRenderer>(*m_sceneManager);
            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_SPAWN_VALIDATION_ACTORS")))
            {
                spawnValidationActors();
            }
            syncActorVisuals();
            m_blockOutline =
                std::make_unique<OgreBlockOutline>(*m_sceneManager);
            std::cout << "[OGRE_TERRAIN] solid=" << terrain.sectionCount
                      << '/' << terrain.vertexCount << '/'
                      << terrain.indexCount << " transparent="
                      << terrain.transparentSectionCount << '/'
                      << terrain.transparentVertexCount << '/'
                      << terrain.transparentIndexCount << " water="
                      << terrain.waterSectionCount << '/'
                      << terrain.waterVertexCount << '/'
                      << terrain.waterIndexCount << " flora="
                      << terrain.floraSectionCount << '/'
                      << terrain.floraVertexCount << '/'
                      << terrain.floraIndexCount << '\n';
            m_renderCapture =
                std::make_unique<OgreRenderCapture>(*m_window);
            m_runtimeStarted = true;

            const char* exitFrames =
                std::getenv("HELLOMINE3D_EXIT_AFTER_FRAMES");
            if (exitFrames != nullptr)
            {
                m_exitAfterFrames = std::max(0, std::atoi(exitFrames));
            }
        }

        TerrainBuildSummary buildTerrain(bool uploadToOgre)
        {
            Config config = m_config;
            if (!uploadToOgre)
            {
                config.renderDistance = 1;
            }
            m_logicCamera = std::make_unique<::Camera>(config);
            m_sandbox = std::make_unique<SandboxRuntime>(
                config, *m_logicCamera, false, 2);
            m_worldPlayer = &m_sandbox->getPlayer();
            m_world =
                m_sandbox->getWorldManager().getActiveWorld();
            if (m_world == nullptr)
            {
                throw std::runtime_error(
                    "Sandbox did not create an active world.");
            }

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_TRANSPARENT_FIXTURE")))
            {
                const int centerX =
                    World::toBlockCoord(m_worldPlayer->position.x);
                const int centerY =
                    World::toBlockCoord(m_worldPlayer->position.y);
                const int centerZ =
                    World::toBlockCoord(m_worldPlayer->position.z) + 2;
                for (int y = 1; y <= 3; ++y)
                {
                    for (int x = -2; x <= 2; ++x)
                    {
                        const BlockId glass = (x + y) % 2 == 0
                                                  ? BlockId::Glass
                                                  : BlockId::GlassBorderless;
                        m_world->setBlock(centerX + x, centerY + y,
                                          centerZ, glass);
                    }
                }
                m_world->setBlock(centerX - 3, centerY + 1, centerZ,
                                  BlockId::OakLeaf);
                m_world->setBlock(centerX + 3, centerY + 1, centerZ,
                                  BlockId::OakLeaf);
                m_world->setBlock(centerX - 3, centerY + 1, centerZ + 1,
                                  BlockId::TallGrass);
                m_world->setBlock(centerX + 3, centerY + 1, centerZ + 1,
                                  BlockId::Rose);
            }

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_ORE_FIXTURE")))
            {
                const float yaw = glm::radians(
                    m_worldPlayer->rotation.y + 90.0f);
                const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                        -std::sin(yaw));
                const glm::vec3 right(-forward.z, 0.0f, forward.x);
                const glm::vec3 center =
                    m_worldPlayer->position + forward * 6.0f;
                const int centerY =
                    World::toBlockCoord(m_worldPlayer->position.y);

                for (int y = 0; y < 3; ++y)
                {
                    for (int side = 1; side <= 2; ++side)
                    {
                        const glm::vec3 coalPosition =
                            center - right * static_cast<float>(side);
                        const glm::vec3 ironPosition =
                            center + right * static_cast<float>(side);
                        m_world->setBlock(
                            World::toBlockCoord(coalPosition.x),
                            centerY + y,
                            World::toBlockCoord(coalPosition.z),
                            BlockId::CoalOre);
                        m_world->setBlock(
                            World::toBlockCoord(ironPosition.x),
                            centerY + y,
                            World::toBlockCoord(ironPosition.z),
                            BlockId::IronOre);
                    }
                }
                m_oreFixturePlaced = true;
            }

            const VectorXZ center = World::getChunkXZ(
                World::toBlockCoord(m_worldPlayer->position.x),
                World::toBlockCoord(m_worldPlayer->position.z));

            TerrainBuildSummary summary;
            for (auto &entry : m_world->getChunkManager().getChunks())
            {
                Chunk &chunk = entry.second;
                const glm::ivec2 chunkLocation = chunk.getLocation();
                if (std::abs(chunkLocation.x - center.x) >
                        config.renderDistance ||
                    std::abs(chunkLocation.y - center.z) >
                        config.renderDistance)
                {
                    continue;
                }

                for (std::size_t sectionIndex = 0;
                     sectionIndex < chunk.getSectionCount(); ++sectionIndex)
                {
                    ChunkSection *section =
                        chunk.findSection(static_cast<int>(sectionIndex));
                    if (section == nullptr)
                    {
                        continue;
                    }

                    section->makeMesh();
                    const glm::ivec3 sectionLocation =
                        section->getLocation();
                    std::ostringstream sectionName;
                    sectionName << "ChunkSection_" << sectionLocation.x
                                << '_' << sectionLocation.y << '_'
                                << sectionLocation.z;
                    Ogre::SceneNode *node = nullptr;
                    SectionVisual visual;
                    auto ensureNode = [&]() {
                        if (node != nullptr)
                        {
                            return node;
                        }
                        node = m_sceneManager->getRootSceneNode()
                                   ->createChildSceneNode(
                                       sectionName.str() + "_Node",
                                       Ogre::Vector3(
                                           static_cast<Ogre::Real>(
                                               sectionLocation.x *
                                               CHUNK_SIZE),
                                           static_cast<Ogre::Real>(
                                               sectionLocation.y *
                                               CHUNK_SIZE),
                                           static_cast<Ogre::Real>(
                                               sectionLocation.z *
                                               CHUNK_SIZE)));
                        visual.node = node;
                        return node;
                    };

                    auto processMesh =
                        [&](const ChunkMesh &mesh, const char *layerName,
                            const char *materialName,
                            std::uint8_t renderQueue,
                            std::size_t &sectionCount,
                            std::size_t &vertexCount,
                            std::size_t &indexCount) {
                            const ChunkMeshValidation validation =
                                ChunkSectionRenderable::validateCpuMesh(
                                    mesh, sectionLocation);
                            if (!validation.valid)
                            {
                                throw std::runtime_error(
                                    std::string(layerName) +
                                    " mesh validation failed: " +
                                    validation.message);
                            }
                            if (validation.indexCount == 0)
                            {
                                return;
                            }

                            ++sectionCount;
                            vertexCount += validation.vertexCount;
                            indexCount += validation.indexCount;
                            if (!uploadToOgre)
                            {
                                return;
                            }

                            auto renderable =
                                std::make_unique<ChunkSectionRenderable>(
                                    sectionName.str() + "_" + layerName,
                                    mesh, sectionLocation, materialName,
                                    renderQueue);
                            ensureNode()->attachObject(renderable.get());
                            visual.renderables.push_back(
                                std::move(renderable));
                        };

                    const ChunkMeshCollection &meshes =
                        section->getMeshes();
                    processMesh(
                        meshes.solidMesh, "Solid", "HelloMine3D/Terrain",
                        static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_MAIN),
                        summary.sectionCount, summary.vertexCount,
                        summary.indexCount);
                    processMesh(
                        meshes.transparentMesh, "Transparent",
                        "HelloMine3D/Transparent",
                        static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_8),
                        summary.transparentSectionCount,
                        summary.transparentVertexCount,
                        summary.transparentIndexCount);
                    processMesh(
                        meshes.waterMesh, "Water", "HelloMine3D/Water",
                        static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_8),
                        summary.waterSectionCount,
                        summary.waterVertexCount, summary.waterIndexCount);
                    processMesh(
                        meshes.floraMesh, "Flora", "HelloMine3D/Flora",
                        static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_6),
                        summary.floraSectionCount,
                        summary.floraVertexCount, summary.floraIndexCount);

                    if (uploadToOgre)
                    {
                        section->markGpuBuffered();
                        if (visual.node != nullptr)
                        {
                            m_sectionVisuals.emplace(
                                sectionKey(sectionLocation),
                                std::move(visual));
                        }
                    }
                }
            }

            if (uploadToOgre && summary.sectionCount > 0)
            {
                const glm::vec3 &position = m_worldPlayer->position;
                m_camera->setPosition(position.x, position.y + 10.0f,
                                      position.z + 14.0f);
                m_camera->lookAt(position.x, position.y, position.z);
                m_world->startBackgroundLoader();
            }
            return summary;
        }

        void createInput()
        {
            std::size_t windowHandle = 0;
            m_window->getCustomAttribute("WINDOW", &windowHandle);

            std::ostringstream handleText;
            handleText << windowHandle;
            OIS::ParamList parameters;
            parameters.insert({"WINDOW", handleText.str()});
#if defined(OIS_WIN32_PLATFORM)
            parameters.insert({"w32_mouse", "DISCL_FOREGROUND"});
            parameters.insert({"w32_mouse", "DISCL_NONEXCLUSIVE"});
            parameters.insert({"w32_keyboard", "DISCL_FOREGROUND"});
            parameters.insert({"w32_keyboard", "DISCL_NONEXCLUSIVE"});
#endif

            m_inputManager =
                OIS::InputManager::createInputSystem(parameters);
            m_keyboard = static_cast<OIS::Keyboard*>(
                m_inputManager->createInputObject(OIS::OISKeyboard, true));
            m_mouse = static_cast<OIS::Mouse*>(
                m_inputManager->createInputObject(OIS::OISMouse, true));
            m_keyboard->setEventCallback(this);
            m_mouse->setEventCallback(this);
            updateMouseBounds();
        }

        bool frameStarted(const Ogre::FrameEvent& event) override
        {
            m_frameStart = std::chrono::steady_clock::now();
            Ogre::WindowEventUtilities::messagePump();
            if (m_shutdownRequested || m_window == nullptr ||
                m_window->isClosed())
            {
                return false;
            }

            m_keyboard->capture();
            m_mouse->capture();
            updateSandbox(event.timeSinceLastFrame);

            m_frameWorldStats = collectRuntimeStats();
            syncEnvironment(m_frameWorldStats.environment);
            if (m_userInterface != nullptr)
            {
                m_userInterface->beginFrame(event.timeSinceLastFrame,
                                            m_frameWorldStats);
            }
            return true;
        }

        bool frameRenderingQueued(const Ogre::FrameEvent&) override
        {
            ++m_frameCount;
            return true;
        }

        bool frameEnded(const Ogre::FrameEvent& event) override
        {
            if (m_renderCapture != nullptr)
            {
                m_renderCapture->update(event.timeSinceLastFrame);
            }

            const auto frameEnd = std::chrono::steady_clock::now();
            const double frameMs =
                std::chrono::duration<double, std::milli>(
                    frameEnd - m_frameStart)
                    .count();
            RuntimePerformanceCapture::FrameTimings timings;
            timings.deltaMs =
                static_cast<double>(event.timeSinceLastFrame) * 1000.0;
            timings.renderMs = frameMs;
            timings.frameMs = frameMs;

            RuntimePerformanceCapture::recordFrame(timings,
                                                    m_frameWorldStats);

            const bool captureComplete =
                m_renderCapture != nullptr &&
                m_renderCapture->shouldCloseWindow();
            const bool frameLimitReached =
                m_exitAfterFrames > 0 &&
                m_frameCount >= m_exitAfterFrames;
            return !captureComplete && !frameLimitReached &&
                   !RuntimePerformanceCapture::shouldCloseWindow();
        }

        void updateSandbox(float deltaSeconds)
        {
            if (m_sandbox == nullptr)
            {
                return;
            }

            const bool keyboardCaptured =
                m_userInterface != nullptr &&
                m_userInterface->wantsKeyboardInput();
            const bool mouseCaptured =
                m_userInterface != nullptr &&
                m_userInterface->wantsMouseInput();

            SandboxInputState input;
            if (!keyboardCaptured)
            {
                input.player.moveForward =
                    m_keyboard->isKeyDown(OIS::KC_W);
                input.player.moveBackward =
                    m_keyboard->isKeyDown(OIS::KC_S);
                input.player.moveLeft =
                    m_keyboard->isKeyDown(OIS::KC_A);
                input.player.moveRight =
                    m_keyboard->isKeyDown(OIS::KC_D);
                input.player.sprint =
                    m_keyboard->isKeyDown(OIS::KC_LCONTROL) ||
                    m_keyboard->isKeyDown(OIS::KC_RCONTROL);
                input.player.jump =
                    m_keyboard->isKeyDown(OIS::KC_SPACE);
                input.player.descend =
                    m_keyboard->isKeyDown(OIS::KC_LSHIFT) ||
                    m_keyboard->isKeyDown(OIS::KC_RSHIFT);
                input.player.toggleFlying = m_toggleFlying;
                input.player.toggleSneaking = m_toggleSneaking;
                input.player.hotbarDelta = m_hotbarDelta;
                input.player.hotbarSlot = m_hotbarSlot;
                input.resetMeshes = m_resetMeshes;
            }
            if (!mouseCaptured && m_mouseLookEnabled)
            {
                input.player.lookDelta = m_pendingLookDelta;
                const OIS::MouseState &mouseState =
                    m_mouse->getMouseState();
                input.breakBlock =
                    mouseState.buttonDown(OIS::MB_Left);
                input.placeBlock =
                    mouseState.buttonDown(OIS::MB_Right);
            }

            const bool diagnosticsActive =
                (m_renderCapture != nullptr &&
                 m_renderCapture->isEnabled()) ||
                RuntimePerformanceCapture::isEnabled();
            const bool freezeValidationCapture =
                (m_validationActorsSpawned || m_oreFixturePlaced) &&
                m_renderCapture != nullptr &&
                m_renderCapture->isEnabled();
            m_sandbox->update(input,
                              freezeValidationCapture ? 0.0f : deltaSeconds,
                              !diagnosticsActive);
            clearTransientInput();
            syncRenderCamera();
            syncSectionMeshes();
            syncActorVisuals();
            if (m_blockOutline != nullptr)
            {
                const auto& selection = m_sandbox->getBlockSelection();
                m_blockOutline->update(
                    selection.has_value() ? &*selection : nullptr);
            }
        }

        void syncSectionMeshes()
        {
            if (m_world == nullptr || m_sceneManager == nullptr)
            {
                return;
            }

            WorldMeshSnapshot snapshot =
                m_world->collectSectionMeshSnapshot();
            std::unordered_set<std::string> liveSections;
            liveSections.reserve(snapshot.liveSections.size());
            for (const glm::ivec3& location : snapshot.liveSections)
            {
                liveSections.insert(sectionKey(location));
            }

            for (auto it = m_sectionVisuals.begin();
                 it != m_sectionVisuals.end();)
            {
                if (liveSections.find(it->first) != liveSections.end())
                {
                    ++it;
                    continue;
                }

                destroySectionVisual(it->second);
                it = m_sectionVisuals.erase(it);
            }

            std::vector<WorldSectionMeshVersion> uploaded;
            uploaded.reserve(snapshot.cpuReadySections.size());
            for (const WorldSectionMeshSnapshot& section :
                 snapshot.cpuReadySections)
            {
                uploadSectionVisual(section);
                uploaded.push_back(
                    {section.location, section.blockRevision});
            }
            m_world->acknowledgeSectionMeshUploads(uploaded);
        }

        void syncActorVisuals()
        {
            if (m_world == nullptr || m_actorRenderer == nullptr)
            {
                return;
            }
            m_actorRenderer->sync(m_world->collectActorSnapshots());
        }

        void spawnValidationActors()
        {
            if (m_validationActorsSpawned || m_world == nullptr ||
                m_worldPlayer == nullptr)
            {
                return;
            }

            const float yaw = glm::radians(
                m_worldPlayer->rotation.y + 90.0f);
            const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                    -std::sin(yaw));
            const glm::vec3 right(-forward.z, 0.0f, forward.x);
            const glm::vec3 origin = m_worldPlayer->position;

            const glm::vec3 itemPosition =
                origin + forward * 2.0f + right * 0.65f +
                glm::vec3(0.0f, 0.35f, 0.0f);
            const glm::vec3 mobPosition =
                origin + forward * 4.0f - right * 0.75f;
            m_world->spawnItemEntity(Material::ID::Stone, 1,
                                     itemPosition);
            m_world->spawnMob("validation_mob", mobPosition);
            m_validationActorsSpawned = true;
        }

        void uploadSectionVisual(
            const WorldSectionMeshSnapshot& section)
        {
            const std::string key = sectionKey(section.location);
            const auto existing = m_sectionVisuals.find(key);
            if (existing != m_sectionVisuals.end())
            {
                destroySectionVisual(existing->second);
                m_sectionVisuals.erase(existing);
            }

            const std::string objectName = "ChunkSection_" + key;
            SectionVisual visual;
            auto ensureNode = [&]() {
                if (visual.node == nullptr)
                {
                    visual.node = m_sceneManager->getRootSceneNode()
                                      ->createChildSceneNode(
                                          objectName + "_Node",
                                          Ogre::Vector3(
                                              static_cast<Ogre::Real>(
                                                  section.location.x *
                                                  CHUNK_SIZE),
                                              static_cast<Ogre::Real>(
                                                  section.location.y *
                                                  CHUNK_SIZE),
                                              static_cast<Ogre::Real>(
                                                  section.location.z *
                                                  CHUNK_SIZE)));
                }
                return visual.node;
            };

            auto uploadLayer =
                [&](const ChunkMesh& mesh, const char* layerName,
                    const char* materialName, std::uint8_t renderQueue) {
                    const ChunkMeshValidation validation =
                        ChunkSectionRenderable::validateCpuMesh(
                            mesh, section.location);
                    if (!validation.valid)
                    {
                        throw std::runtime_error(
                            std::string(layerName) +
                            " mesh validation failed: " +
                            validation.message);
                    }
                    if (validation.indexCount == 0)
                    {
                        return;
                    }

                    auto renderable =
                        std::make_unique<ChunkSectionRenderable>(
                            objectName + "_" + layerName, mesh,
                            section.location, materialName, renderQueue);
                    ensureNode()->attachObject(renderable.get());
                    visual.renderables.push_back(std::move(renderable));
                };

            uploadLayer(
                section.meshes.solidMesh, "Solid", "HelloMine3D/Terrain",
                static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_MAIN));
            uploadLayer(
                section.meshes.transparentMesh, "Transparent",
                "HelloMine3D/Transparent",
                static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_8));
            uploadLayer(
                section.meshes.waterMesh, "Water", "HelloMine3D/Water",
                static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_8));
            uploadLayer(
                section.meshes.floraMesh, "Flora", "HelloMine3D/Flora",
                static_cast<std::uint8_t>(Ogre::RENDER_QUEUE_6));

            if (visual.node != nullptr)
            {
                m_sectionVisuals.emplace(key, std::move(visual));
            }
        }

        void destroySectionVisual(SectionVisual& visual)
        {
            for (auto& renderable : visual.renderables)
            {
                if (renderable->isAttached())
                {
                    renderable->detachFromParent();
                }
            }
            visual.renderables.clear();
            if (visual.node != nullptr && m_sceneManager != nullptr)
            {
                m_sceneManager->destroySceneNode(visual.node);
                visual.node = nullptr;
            }
        }

        void clearTransientInput()
        {
            m_pendingLookDelta = glm::vec2(0.0f);
            m_toggleFlying = false;
            m_toggleSneaking = false;
            m_resetMeshes = false;
            m_hotbarDelta = 0;
            m_hotbarSlot = -1;
        }

        void syncRenderCamera()
        {
            if (m_logicCamera == nullptr || m_camera == nullptr)
            {
                return;
            }

            m_logicCamera->update();
            const glm::vec3 &position = m_logicCamera->position;
            const glm::vec3 &rotation = m_logicCamera->rotation;
            m_camera->setPosition(position.x, position.y, position.z);
            m_camera->setOrientation(Ogre::Quaternion::IDENTITY);
            m_camera->yaw(Ogre::Degree(-rotation.y));
            m_camera->pitch(Ogre::Degree(-rotation.x));
        }

        WorldDebugStats collectRuntimeStats()
        {
            WorldDebugStats stats;
            if (m_world != nullptr)
            {
                stats = m_world->collectDebugStats();
                stats.chunks.gpuBufferedSections =
                    m_sectionVisuals.size();
                for (const auto& visualEntry : m_sectionVisuals)
                {
                    for (const auto& renderable :
                         visualEntry.second.renderables)
                    {
                        stats.terrainBuffers.add(
                            renderable->vertexCount(),
                            renderable->indexCount());
                    }
                }
            }
            return stats;
        }

        Ogre::Pass* materialPass(const char* materialName)
        {
            Ogre::MaterialPtr material =
                Ogre::MaterialManager::getSingleton().getByName(
                    materialName);
            if (material.isNull())
            {
                throw std::runtime_error(
                    std::string("Missing environment material: ") +
                    materialName);
            }

            Ogre::Technique* technique = material->getBestTechnique();
            if (technique == nullptr || technique->getNumPasses() == 0)
            {
                throw std::runtime_error(
                    std::string("Environment material has no pass: ") +
                    materialName);
            }
            return technique->getPass(0);
        }

        void syncEnvironment(const WorldEnvironmentState& state)
        {
            if (m_sceneManager == nullptr)
            {
                return;
            }

            const Ogre::ColourValue fog(
                state.fogColour.r, state.fogColour.g,
                state.fogColour.b);
            const Ogre::Vector3 fogVector(
                state.fogColour.r, state.fogColour.g,
                state.fogColour.b);
            const Ogre::Vector3 skyVector(
                state.skyTint.r, state.skyTint.g, state.skyTint.b);

            m_sceneManager->setFog(Ogre::FOG_EXP2, fog,
                                   state.fogDensity);
            if (m_camera != nullptr && m_camera->getViewport() != nullptr)
            {
                m_camera->getViewport()->setBackgroundColour(fog);
            }

            const char* terrainMaterials[] = {
                "HelloMine3D/Terrain", "HelloMine3D/Water",
                "HelloMine3D/Transparent", "HelloMine3D/Flora"};
            for (const char* materialName : terrainMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant(
                    "environmentLight", state.daylight);
                parameters->setNamedConstant("fogColour", fogVector);
                parameters->setNamedConstant(
                    "fogDensity", state.fogDensity);
            }

            const char* actorMaterials[] = {
                "HelloMine3D/ActorMob", "HelloMine3D/ActorItem"};
            for (const char* materialName : actorMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant(
                    "environmentLight", state.daylight);
                parameters->setNamedConstant("fogColour", fogVector);
                parameters->setNamedConstant(
                    "fogDensity", state.fogDensity);
            }

            materialPass(SkyboxMaterial)
                ->getFragmentProgramParameters()
                ->setNamedConstant("skyTint", skyVector);
        }

        bool keyPressed(const OIS::KeyEvent& event) override
        {
            if (m_userInterface != nullptr)
            {
                m_userInterface->keyEvent(event, true, *m_keyboard);
            }
            if (event.key == OIS::KC_ESCAPE)
            {
                m_shutdownRequested = true;
            }
            if (m_userInterface != nullptr &&
                m_userInterface->wantsKeyboardInput())
            {
                return true;
            }

            switch (event.key)
            {
                case OIS::KC_F:
                    m_toggleFlying = true;
                    break;
                case OIS::KC_LSHIFT:
                case OIS::KC_RSHIFT:
                    m_toggleSneaking = true;
                    break;
                case OIS::KC_L:
                    m_mouseLookEnabled = !m_mouseLookEnabled;
                    break;
                case OIS::KC_C:
                    m_resetMeshes = true;
                    break;
                case OIS::KC_DOWN:
                    m_hotbarDelta = 1;
                    break;
                case OIS::KC_UP:
                    m_hotbarDelta = -1;
                    break;
                case OIS::KC_1:
                case OIS::KC_2:
                case OIS::KC_3:
                case OIS::KC_4:
                case OIS::KC_5:
                    m_hotbarSlot =
                        static_cast<int>(event.key) -
                        static_cast<int>(OIS::KC_1);
                    break;
                default:
                    break;
            }
            return true;
        }

        bool keyReleased(const OIS::KeyEvent& event) override
        {
            if (m_userInterface != nullptr)
            {
                m_userInterface->keyEvent(event, false, *m_keyboard);
            }
            return true;
        }

        bool mouseMoved(const OIS::MouseEvent& event) override
        {
            if (m_userInterface != nullptr)
            {
                m_userInterface->mouseMoved(event);
            }
            if (m_userInterface == nullptr ||
                !m_userInterface->wantsMouseInput())
            {
                m_pendingLookDelta.x +=
                    static_cast<float>(event.state.X.rel);
                m_pendingLookDelta.y +=
                    static_cast<float>(event.state.Y.rel);
            }
            return true;
        }

        bool mousePressed(const OIS::MouseEvent& event,
                          OIS::MouseButtonID button) override
        {
            if (m_userInterface != nullptr)
            {
                m_userInterface->mouseButton(event, button, true);
            }
            return true;
        }

        bool mouseReleased(const OIS::MouseEvent& event,
                           OIS::MouseButtonID button) override
        {
            if (m_userInterface != nullptr)
            {
                m_userInterface->mouseButton(event, button, false);
            }
            return true;
        }

        void windowResized(Ogre::RenderWindow*) override
        {
            updateAspectRatio();
            updateMouseBounds();
        }

        void windowClosed(Ogre::RenderWindow* window) override
        {
            if (window == m_window)
            {
                m_shutdownRequested = true;
            }
        }

        void updateAspectRatio()
        {
            if (m_window == nullptr || m_camera == nullptr ||
                m_window->getNumViewports() == 0)
            {
                return;
            }

            const Ogre::Viewport* viewport = m_window->getViewport(0);
            if (viewport->getActualHeight() > 0)
            {
                m_camera->setAspectRatio(
                    static_cast<Ogre::Real>(viewport->getActualWidth()) /
                    static_cast<Ogre::Real>(viewport->getActualHeight()));
            }
        }

        void updateMouseBounds()
        {
            if (m_window == nullptr || m_mouse == nullptr)
            {
                return;
            }

            unsigned int width = 0;
            unsigned int height = 0;
            unsigned int colourDepth = 0;
            int left = 0;
            int top = 0;
            m_window->getMetrics(width, height, colourDepth, left, top);
            const OIS::MouseState& state = m_mouse->getMouseState();
            state.width = static_cast<int>(width);
            state.height = static_cast<int>(height);
        }

        void shutdown()
        {
            if (m_listenersInstalled && m_root != nullptr)
            {
                m_root->removeFrameListener(this);
                Ogre::WindowEventUtilities::removeWindowEventListener(
                    m_window, this);
                m_listenersInstalled = false;
            }

            if (m_inputManager != nullptr)
            {
                if (m_mouse != nullptr)
                {
                    m_inputManager->destroyInputObject(m_mouse);
                    m_mouse = nullptr;
                }
                if (m_keyboard != nullptr)
                {
                    m_inputManager->destroyInputObject(m_keyboard);
                    m_keyboard = nullptr;
                }
                OIS::InputManager::destroyInputSystem(m_inputManager);
                m_inputManager = nullptr;
            }

            if (m_runtimeStarted)
            {
                RuntimePerformanceCapture::shutdown();
                m_runtimeStarted = false;
            }
            m_renderCapture.reset();
            m_userInterface.reset();
            m_blockOutline.reset();
            m_actorRenderer.reset();

            for (auto &entry : m_sectionVisuals)
            {
                destroySectionVisual(entry.second);
            }
            m_sectionVisuals.clear();
            m_world = nullptr;
            m_worldPlayer = nullptr;
            m_sandbox.reset();
            m_logicCamera.reset();

            m_camera = nullptr;
            m_sceneManager = nullptr;
            m_window = nullptr;
            m_root.reset();
            m_gl3PlusPlugin.reset();
        }

        std::unique_ptr<Ogre::Root> m_root;
        Config m_config;
        std::unique_ptr<Ogre::GL3PlusPlugin> m_gl3PlusPlugin;
        Ogre::RenderWindow* m_window = nullptr;
        Ogre::SceneManager* m_sceneManager = nullptr;
        Ogre::Camera* m_camera = nullptr;
        OIS::InputManager* m_inputManager = nullptr;
        OIS::Keyboard* m_keyboard = nullptr;
        OIS::Mouse* m_mouse = nullptr;
        std::unique_ptr<OgreRenderCapture> m_renderCapture;
        std::unique_ptr<OgreUserInterface> m_userInterface;
        std::unique_ptr<OgreBlockOutline> m_blockOutline;
        std::unique_ptr<OgreActorRenderer> m_actorRenderer;
        Player* m_worldPlayer = nullptr;
        std::unique_ptr<::Camera> m_logicCamera;
        std::unique_ptr<SandboxRuntime> m_sandbox;
        World* m_world = nullptr;
        std::unordered_map<std::string, SectionVisual> m_sectionVisuals;
        bool m_listenersInstalled = false;
        bool m_shutdownRequested = false;
        bool m_runtimeStarted = false;
        int m_exitAfterFrames = 0;
        int m_frameCount = 0;
        WorldDebugStats m_frameWorldStats;
        std::chrono::steady_clock::time_point m_frameStart;
        glm::vec2 m_pendingLookDelta{0.0f};
        bool m_toggleFlying = false;
        bool m_toggleSneaking = false;
        bool m_resetMeshes = false;
        bool m_mouseLookEnabled = true;
        bool m_validationActorsSpawned = false;
        bool m_oreFixturePlaced = false;
        int m_hotbarDelta = 0;
        int m_hotbarSlot = -1;
    };
}

int runOgreBootstrap(bool validateOnly)
{
    try
    {
        const std::string root = ResourcePaths::projectRoot();
        validateStartupResources(root, loadStartupResourceManifest(root));
        OgreBootstrap bootstrap;
        if (validateOnly)
        {
            return bootstrap.validate() ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        return bootstrap.run();
    }
    catch (const Ogre::Exception& exception)
    {
        const std::string diagnostic =
            "Ogre bootstrap failed: " + exception.getFullDescription();
        std::cerr << diagnostic << '\n';
        StartupErrorReporter::present(diagnostic, !validateOnly);
    }
    catch (const OIS::Exception& exception)
    {
        const std::string diagnostic =
            "OIS bootstrap failed: " + std::string(exception.eText);
        std::cerr << diagnostic << '\n';
        StartupErrorReporter::present(diagnostic, !validateOnly);
    }
    catch (const std::exception& exception)
    {
        const std::string diagnostic =
            "Ogre bootstrap failed: " + std::string(exception.what());
        std::cerr << diagnostic << '\n';
        StartupErrorReporter::present(diagnostic, !validateOnly);
    }
    return EXIT_FAILURE;
}
