#include "OgreBootstrap.h"
#include "OgreActorRenderer.h"
#include "ChunkSectionRenderable.h"
#include "OgreBlockOutline.h"
#include "OgreRenderCapture.h"
#include "OgreUserInterface.h"
#include "StartupErrorReporter.h"
#include "StartupResourcePreflight.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <OIS.h>
#include <Ogre.h>
#include <OgreCompositorManager.h>
#include <OgreGL3PlusPlugin.h>
#include <OgreWindowEventUtilities.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../Config.h"
#include "../Actor/EnemyRegistry.h"
#include "../Audio/AudioDefinitionRegistry.h"
#include "../Audio/AudioRuntime.h"
#include "../Audio/MusicDefinitionRegistry.h"
#include "../Audio/MusicRuntime.h"
#include "../Core/Camera.h"
#include "../Diagnostics/CrashDiagnostics.h"
#include "../Diagnostics/OperationPerformanceTiming.h"
#include "../Diagnostics/RuntimePerformanceCapture.h"
#include "../Diagnostics/RuntimeProfiler.h"
#include "../Gameplay/ObjectiveRegistry.h"
#include "../Item/FoodRegistry.h"
#include "../Item/RecipeRegistry.h"
#include "../Item/CraftingSession.h"
#include "../Item/ToolRegistry.h"
#include "../Item/SmeltingRegistry.h"
#include "../Player/Player.h"
#include "../Presentation/LocalizedTextRegistry.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../Sandbox/SandboxRuntime.h"
#include "../Util/ResourcePackResolver.h"
#include "../Util/ResourcePaths.h"
#include "../World/Chunk/Chunk.h"
#include "../World/Chunk/ChunkSection.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/ChestContainer.h"
#include "../World/Block/FurnaceContainer.h"
#include "../World/Block/TerrainMaterialProfile.h"
#include "../World/Environment/AtmosphereShaderContract.h"
#include "../World/World.h"
#include "../World/Storage/WorldManagementService.h"

namespace
{
    constexpr const char* ConfigFileName = "Mine.cfg";
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

    OIS::KeyCode toOisKey(GameplayKey key) noexcept
    {
        static constexpr OIS::KeyCode Keys[] = {
            OIS::KC_A, OIS::KC_B, OIS::KC_C, OIS::KC_D, OIS::KC_E,
            OIS::KC_F, OIS::KC_G, OIS::KC_H, OIS::KC_I, OIS::KC_J,
            OIS::KC_K, OIS::KC_L, OIS::KC_M, OIS::KC_N, OIS::KC_O,
            OIS::KC_P, OIS::KC_Q, OIS::KC_R, OIS::KC_S, OIS::KC_T,
            OIS::KC_U, OIS::KC_V, OIS::KC_W, OIS::KC_X, OIS::KC_Y,
            OIS::KC_Z, OIS::KC_SPACE, OIS::KC_LSHIFT,
            OIS::KC_LCONTROL, OIS::KC_UP, OIS::KC_DOWN, OIS::KC_LEFT,
            OIS::KC_RIGHT};
        const std::size_t index = static_cast<std::size_t>(key);
        return index < static_cast<std::size_t>(GameplayKey::Count)
                   ? Keys[index]
                   : OIS::KC_UNASSIGNED;
    }

    OIS::MouseButtonID toOisMouseButton(
        GameplayMouseButton button) noexcept
    {
        static constexpr OIS::MouseButtonID Buttons[] = {
            OIS::MB_Left, OIS::MB_Right, OIS::MB_Middle,
            OIS::MB_Button3, OIS::MB_Button4};
        const std::size_t index = static_cast<std::size_t>(button);
        return index < GameplayMouseButtonCount
                   ? Buttons[index]
                   : OIS::MB_Left;
    }

    const char *foodUseResultKey(FoodUseResult result) noexcept
    {
        switch (result)
        {
            case FoodUseResult::Consumed:
                return "food.feedback.consumed";
            case FoodUseResult::SimulationPaused:
                return "food.feedback.paused";
            case FoodUseResult::UiBusy:
                return "food.feedback.ui_busy";
            case FoodUseResult::PlayerUnavailable:
                return "food.feedback.player_unavailable";
            case FoodUseResult::PlayerDead:
                return "food.feedback.player_dead";
            case FoodUseResult::CoolingDown:
                return "food.feedback.cooldown";
            case FoodUseResult::EmptyHand:
                return "food.feedback.empty_hand";
            case FoodUseResult::NotFood:
                return "food.feedback.not_food";
            case FoodUseResult::FullHealth:
                return "food.feedback.full_health";
            case FoodUseResult::InventoryRejected:
                return "food.feedback.inventory_rejected";
        }
        return "food.feedback.failed";
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

    struct DirectionalShadowProfile
    {
        unsigned short textureSize = 0;
        float farDistance = 0.f;
        float fadeStart = 0.f;
        float bias = 0.f;
    };

    DirectionalShadowProfile directionalShadowProfile(
        DirectionalShadowQuality quality) noexcept
    {
        if (quality == DirectionalShadowQuality::High)
        {
            return {1024, 96.f, 72.f, 0.004f};
        }
        if (quality == DirectionalShadowQuality::Medium)
        {
            return {512, 64.f, 48.f, 0.008f};
        }
        return {};
    }

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
        explicit OgreBootstrap(
            std::vector<PendingCrashReport> crashReports = {})
            : m_pendingCrashReports(std::move(crashReports))
        {
        }

        ~OgreBootstrap() override
        {
            shutdown();
        }

        bool validate()
        {
            loadGameConfig();
            AudioDefinitionRegistry audioDefinitions =
                loadAudioDefinitions();
            std::unique_ptr<AudioRuntime> audioValidation =
                AudioRuntime::createDummy(
                    std::move(audioDefinitions), userSettings(m_config),
                    [](const std::string &logicalPath)
                    {
                        return runtimeResourcePackResolver().resolve(
                            logicalPath);
                    });
            std::cout << "[AUDIO_REGISTRY] frozen=1 definitions="
                      << audioValidation->definitions().definitions().size()
                      << " samples="
                      << audioValidation->samples().cueCount()
                      << " unique_samples="
                      << audioValidation->samples().uniqueSampleCount()
                      << " decoded_bytes="
                      << audioValidation->samples().decodedBytes()
                      << " degraded="
                      << ((m_audioDefinitionError.empty() &&
                           audioValidation->samples().cueCount() > 0)
                              ? 0
                              : 1)
                      << '\n';
            MusicDefinitionRegistry musicDefinitions =
                loadMusicDefinitions();
            std::unique_ptr<MusicRuntime> musicValidation =
                MusicRuntime::createDummy(
                    std::move(musicDefinitions), userSettings(m_config),
                    [](const std::string &logicalPath)
                    {
                        return runtimeResourcePackResolver().resolve(
                            logicalPath);
                    });
            std::cout << "[MUSIC_REGISTRY] frozen=1 tracks="
                      << musicValidation->definitions().tracks().size()
                      << " stream_bytes="
                      << musicValidation->stream().dataBytes
                      << " duration_ms="
                      << musicValidation->stream().durationMilliseconds
                      << " degraded="
                      << ((m_musicDefinitionError.empty() &&
                           musicValidation->streamAvailable())
                              ? 0
                              : 1)
                      << '\n';
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
            const OgreProjectileRendererValidation projectiles =
                OgreActorRenderer::validateProjectileSnapshots(
                    m_world->collectCombatProjectileSnapshots());
            if (!projectiles.valid)
            {
                throw std::runtime_error(
                    "Projectile validation failed: " +
                    projectiles.message);
            }

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
            std::cout << "[OGRE_VALIDATION] container_open="
                      << (userInterface.containerOpen ? "true" : "false")
                      << '\n';
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
            initializeAudio();
            initializeMusic();
            createRoot();
            configureResources();
            configureRenderSystem();
            const char* catalogueOverride =
                std::getenv("HELLOMINE3D_CATALOGUE_DIR");
            m_worldManagement =
                std::make_unique<WorldManagementService>(
                    catalogueOverride != nullptr &&
                            catalogueOverride[0] != '\0'
                        ? catalogueOverride
                        : ResourcePaths::bin("saves"));
            const char* saveOverride =
                std::getenv("HELLOMINE3D_SAVE_DIR");
            const bool directLaunch =
                (saveOverride != nullptr && saveOverride[0] != '\0') ||
                isTrueValue(std::getenv(
                    "HELLOMINE3D_SKIP_MAIN_MENU"));
            std::string initialSaveDirectory;
            if (directLaunch)
            {
                initialSaveDirectory =
                    saveOverride != nullptr && saveOverride[0] != '\0'
                        ? saveOverride
                        : ResourcePaths::bin("saves/default");
                m_applicationFlow.beginLoading("direct-launch");
            }
            runtimeOperationTimings().markLatestActive(
                RuntimeOperationKind::Startup);
            createWindowAndScene(initialSaveDirectory);
            if (directLaunch)
            {
                m_applicationFlow.completeLoading(true);
            }
            createInput();
            m_userInterface = std::make_unique<OgreUserInterface>(
                *m_window, *m_sceneManager, *m_camera, m_worldPlayer,
                m_world, m_applicationFlow, *m_worldManagement,
                userSettings(m_config),
                runtimeResourcePackResolver().resolve(
                    "media/fonts/NotoSansSC-VF.ttf"),
                [this]() {
                    if (m_audio != nullptr)
                    {
                        m_audio->emitUiClick();
                    }
                }, std::move(m_pendingCrashReports));
            if (m_audio != nullptr)
            {
                m_audio->setCaptionSink([this](std::string cueId,
                                               std::string caption)
                {
                    if (m_userInterface != nullptr)
                    {
                        m_userInterface->setAudioCaption(
                            std::move(cueId), std::move(caption));
                    }
                });
            }

            m_root->addFrameListener(this);
            Ogre::WindowEventUtilities::addWindowEventListener(m_window, this);
            m_listenersInstalled = true;
            m_root->startRendering();
            runtimeOperationTimings().completeLatestActive(
                RuntimeOperationKind::WorldEntry, m_frameCount > 0);
            runtimeOperationTimings().completeLatestActive(
                RuntimeOperationKind::Startup, m_frameCount > 0);
            return EXIT_SUCCESS;
        }

      private:
        void loadGameConfig()
        {
            m_config = loadRuntimeConfig(
                ResourcePaths::bin("config.txt"));
        }

        AudioDefinitionRegistry loadAudioDefinitions()
        {
            AudioDefinitionRegistry definitions;
            m_audioDefinitionError.clear();
            std::string error;
            const std::string path = runtimeResourcePackResolver().resolve(
                "media/audio/Base.audio");
            if (!definitions.tryFreezeFromFile(path, error))
            {
                m_audioDefinitionError = std::move(error);
            }
            return definitions;
        }

        MusicDefinitionRegistry loadMusicDefinitions()
        {
            MusicDefinitionRegistry definitions;
            m_musicDefinitionError.clear();
            std::string error;
            const std::string path = runtimeResourcePackResolver().resolve(
                "media/music/Base.music");
            if (!definitions.tryFreezeFromFile(path, error))
            {
                m_musicDefinitionError = std::move(error);
            }
            return definitions;
        }

        void initializeAudio()
        {
            AudioDefinitionRegistry definitions = loadAudioDefinitions();
            m_audio = AudioRuntime::create(
                std::move(definitions), userSettings(m_config),
                [](const std::string &logicalPath)
                {
                    return runtimeResourcePackResolver().resolve(logicalPath);
                });
            std::cout << "[AUDIO] backend=" << m_audio->backendName()
                      << " real=" << (m_audio->usesRealBackend() ? 1 : 0)
                      << " definitions="
                      << m_audio->definitions().definitions().size()
                      << " samples=" << m_audio->samples().cueCount()
                      << " unique_samples="
                      << m_audio->samples().uniqueSampleCount()
                      << " decoded_bytes="
                      << m_audio->samples().decodedBytes()
                      << " degraded="
                      << (m_audio->degradedReason().empty() ? 0 : 1);
            if (!m_audioDefinitionError.empty())
            {
                std::cout << " definition_error="
                          << m_audioDefinitionError;
            }
            else if (!m_audio->degradedReason().empty())
            {
                std::cout << " reason=" << m_audio->degradedReason();
            }
            std::cout << '\n';
        }

        void initializeMusic()
        {
            MusicDefinitionRegistry definitions = loadMusicDefinitions();
            m_music = MusicRuntime::create(
                std::move(definitions), userSettings(m_config),
                [](const std::string &logicalPath)
                {
                    return runtimeResourcePackResolver().resolve(logicalPath);
                });
            std::cout << "[MUSIC] backend=" << m_music->backendName()
                      << " real=" << (m_music->usesRealBackend() ? 1 : 0)
                      << " tracks="
                      << m_music->definitions().tracks().size()
                      << " stream_bytes=" << m_music->stream().dataBytes
                      << " duration_ms="
                      << m_music->stream().durationMilliseconds
                      << " state="
                      << musicPlaybackStateName(m_music->state())
                      << " degraded="
                      << (m_music->degradedReason().empty() ? 0 : 1);
            if (!m_musicDefinitionError.empty())
            {
                std::cout << " definition_error="
                          << m_musicDefinitionError;
            }
            else if (!m_music->degradedReason().empty())
            {
                std::cout << " reason=" << m_music->degradedReason();
            }
            std::cout << '\n';
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
            std::size_t locationCount = 0;
            for (const std::string &logicalDirectory :
                 {std::string("media/ogre"),
                  std::string("media/textures")})
            {
                for (const std::string &directory :
                     runtimeResourcePackResolver().resourceDirectories(
                         logicalDirectory))
                {
                    Ogre::ResourceGroupManager::getSingleton()
                        .addResourceLocation(directory, "FileSystem",
                                             "General", true);
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

        void createWindowAndScene(const std::string &initialSaveDirectory)
        {
            const bool hiddenWindow = isTrueValue(
                std::getenv("HELLOMINE3D_WINDOW_HIDDEN"));
            m_hiddenWindow = hiddenWindow;
            if (hiddenWindow)
            {
                m_root->initialise(false, WindowTitle);
                Ogre::NameValuePairList windowParameters;
                windowParameters["hidden"] = "true";
                windowParameters["noActivate"] = "true";
                m_window = m_root->createRenderWindow(
                    WindowTitle,
                    static_cast<unsigned int>(m_config.windowX),
                    static_cast<unsigned int>(m_config.windowY), false,
                    &windowParameters);
            }
            else
            {
                m_window = m_root->initialise(true, WindowTitle);
            }
            if (m_window == nullptr)
            {
                throw std::runtime_error("Ogre failed to create a window.");
            }
            runtimeOperationTimings().markLatestActive(
                RuntimeOperationKind::Startup);

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
            selectAtmosphereMode();
            syncTerrainMaterialParameters();
            m_sceneManager->setAmbientLight(
                Ogre::ColourValue(0.7f, 0.7f, 0.7f));
            m_sceneManager->setSkyBox(
                true, SkyboxMaterial, 5000.0f, true);
            if (!configureDirectionalShadows(
                    m_config.directionalShadowQuality))
            {
                m_config.directionalShadowQuality =
                    DirectionalShadowQuality::Off;
            }
            syncPostProcessingParameters();
            if (!configurePostProcessing(
                    m_config.postProcessingQuality))
            {
                m_config.postProcessingQuality =
                    PostProcessingQuality::Off;
            }

            m_actorRenderer =
                std::make_unique<OgreActorRenderer>(*m_sceneManager);
            m_actorRenderer->setCastShadows(
                m_directionalShadowQuality !=
                DirectionalShadowQuality::Off);
            m_blockOutline =
                std::make_unique<OgreBlockOutline>(*m_sceneManager);
            TerrainBuildSummary terrain;
            if (!initialSaveDirectory.empty())
            {
                terrain = buildTerrain(true, initialSaveDirectory);
                if (isTrueValue(std::getenv(
                        "HELLOMINE3D_SPAWN_VALIDATION_ACTORS")))
                {
                    spawnValidationActors();
                }
                syncActorVisuals();
            }
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

        TerrainBuildSummary buildTerrain(
            bool uploadToOgre,
            const std::string &mainSaveDirectory = std::string())
        {
            if (uploadToOgre)
            {
                runtimeOperationTimings().begin(
                    RuntimeOperationKind::WorldEntry);
            }
            Config config = m_config;
            if (!uploadToOgre)
            {
                config.renderDistance = 1;
            }
            m_logicCamera = std::make_unique<::Camera>(config);
            m_sandbox = std::make_unique<SandboxRuntime>(
                config, *m_logicCamera, false, 2, mainSaveDirectory);
            m_worldPlayer = &m_sandbox->getPlayer();
            m_world =
                m_sandbox->getWorldManager().getActiveWorld();
            if (m_world == nullptr)
            {
                throw std::runtime_error(
                    "Sandbox did not create an active world.");
            }
            if (m_audio != nullptr)
            {
                m_audio->attach(m_world->getEventBus());
            }

            if (!uploadToOgre ||
                isTrueValue(std::getenv(
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
                    "HELLOMINE3D_V10D_SHADOW_FIXTURE")))
            {
                const float yaw = glm::radians(
                    m_worldPlayer->rotation.y + 90.0f);
                const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                        -std::sin(yaw));
                const glm::vec3 fixtureCenter =
                    m_worldPlayer->position + forward * 8.0f;
                const int centerX =
                    World::toBlockCoord(fixtureCenter.x);
                const int centerZ =
                    World::toBlockCoord(fixtureCenter.z);
                const int floorY = World::toBlockCoord(
                    m_worldPlayer->position.y) - 2;

                for (int z = -9; z <= 9; ++z)
                {
                    for (int x = -9; x <= 9; ++x)
                    {
                        for (int y = 1; y <= 7; ++y)
                        {
                            m_world->setBlock(centerX + x, floorY + y,
                                              centerZ + z,
                                              BlockId::Air);
                        }
                        m_world->setBlock(centerX + x, floorY,
                                          centerZ + z,
                                          BlockId::Sand);
                    }
                }

                const auto placePillar =
                    [&](int offsetX, int offsetZ, int height,
                        BlockId block)
                    {
                        for (int y = 1; y <= height; ++y)
                        {
                            m_world->setBlock(centerX + offsetX,
                                              floorY + y,
                                              centerZ + offsetZ, block);
                        }
                    };
                placePillar(-3, 1, 5, BlockId::OakBark);
                placePillar(3, 1, 5, BlockId::OakBark);
                placePillar(0, 4, 3, BlockId::Stone);
                for (int x = -3; x <= 3; ++x)
                {
                    m_world->setBlock(centerX + x, floorY + 5,
                                      centerZ + 1,
                                      BlockId::OakBark);
                }
                m_world->setBlock(centerX - 5, floorY + 3,
                                  centerZ + 4, BlockId::Stone);
                m_world->setBlock(centerX + 5, floorY + 1,
                                  centerZ + 4, BlockId::Glass);
                std::cout << "[V10D_SHADOW_FIXTURE] center="
                          << centerX << ',' << floorY << ',' << centerZ
                          << " floor=19x19 casters=4\n";
            }

            const char *p11LightFixtureValue = std::getenv(
                "HELLOMINE3D_P11_LIGHT_FIXTURE");
            if (p11LightFixtureValue != nullptr &&
                p11LightFixtureValue[0] != '\0')
            {
                const std::string fixture(p11LightFixtureValue);
                if (fixture != "cave_before" &&
                    fixture != "cave_after" &&
                    fixture != "night_torch" &&
                    fixture != "furnace_lit")
                {
                    throw std::runtime_error(
                        "Unknown P11 light fixture: " + fixture);
                }

                const float yaw = glm::radians(
                    m_worldPlayer->rotation.y + 90.0f);
                const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                        -std::sin(yaw));
                const glm::vec3 right(-forward.z, 0.0f, forward.x);
                const int baseX = World::toBlockCoord(
                    m_worldPlayer->position.x);
                const int baseZ = World::toBlockCoord(
                    m_worldPlayer->position.z);
                const int forwardX = static_cast<int>(
                    std::lround(forward.x));
                const int forwardZ = static_cast<int>(
                    std::lround(forward.z));
                const int rightX = static_cast<int>(
                    std::lround(right.x));
                const int rightZ = static_cast<int>(
                    std::lround(right.z));
                const int floorY = World::toBlockCoord(
                    m_worldPlayer->position.y) - 2;
                const auto blockPosition =
                    [&](int forwardOffset, int rightOffset, int height)
                    {
                        return glm::ivec3{
                            baseX + forwardX * forwardOffset +
                                rightX * rightOffset,
                            floorY + height,
                            baseZ + forwardZ * forwardOffset +
                                rightZ * rightOffset};
                    };
                const auto setFixtureBlock =
                    [&](int forwardOffset, int rightOffset, int height,
                        ChunkBlock block)
                    {
                        const glm::ivec3 position = blockPosition(
                            forwardOffset, rightOffset, height);
                        m_world->setBlock(position.x, position.y,
                                          position.z, block);
                    };

                if (fixture == "night_torch")
                {
                    for (int forwardStep = -4; forwardStep <= 14;
                         ++forwardStep)
                    {
                        for (int rightStep = -6; rightStep <= 6;
                             ++rightStep)
                        {
                            setFixtureBlock(forwardStep, rightStep, 0,
                                            BlockId::Grass);
                        }
                    }
                    setFixtureBlock(7, 0, 1, BlockId::Torch);
                }
                else
                {
                    const int halfWidth =
                        fixture == "furnace_lit" ? 5 : 4;
                    for (int forwardStep = -3; forwardStep <= 12;
                         ++forwardStep)
                    {
                        for (int rightStep = -halfWidth;
                             rightStep <= halfWidth; ++rightStep)
                        {
                            setFixtureBlock(forwardStep, rightStep, 0,
                                            BlockId::Stone);
                            setFixtureBlock(forwardStep, rightStep, 6,
                                            BlockId::Stone);
                        }
                        for (int height = 1; height < 6; ++height)
                        {
                            setFixtureBlock(forwardStep, -halfWidth,
                                            height, BlockId::Stone);
                            setFixtureBlock(forwardStep, halfWidth,
                                            height, BlockId::Stone);
                        }
                    }
                    for (int rightStep = -halfWidth;
                         rightStep <= halfWidth; ++rightStep)
                    {
                        for (int height = 1; height < 6; ++height)
                        {
                            setFixtureBlock(-3, rightStep, height,
                                            BlockId::Stone);
                            setFixtureBlock(12, rightStep, height,
                                            BlockId::Stone);
                        }
                    }

                    if (fixture == "cave_before" ||
                        fixture == "cave_after")
                    {
                        setFixtureBlock(8, -halfWidth, 2,
                                        BlockId::CoalOre);
                        setFixtureBlock(9, halfWidth, 2,
                                        BlockId::IronOre);
                        if (fixture == "cave_after")
                        {
                            setFixtureBlock(7, 0, 1,
                                            BlockId::Torch);
                        }
                    }
                    else
                    {
                        const glm::ivec3 furnacePosition =
                            blockPosition(7, 0, 1);
                        m_world->setBlock(
                            furnacePosition.x, furnacePosition.y,
                            furnacePosition.z, BlockId::Furnace);
                        if (!FurnaceContainer::initialize(
                                *m_world, furnacePosition))
                        {
                            throw std::runtime_error(
                                "P11 light fixture failed to initialize "
                                "the furnace.");
                        }
                        FurnaceState state;
                        state.input = {
                            Material::ID::IronOre, 4, 0};
                        state.burnTicksRemaining = 160;
                        state.burnTicksTotal = 160;
                        if (!FurnaceContainer::shouldEmitLight(
                                state, runtimeSmeltingRegistry()) ||
                            !m_world->updateBlockEntity(
                                furnacePosition,
                                FurnaceContainer::serialize(state)))
                        {
                            throw std::runtime_error(
                                "P11 light fixture failed to persist an "
                                "active furnace.");
                        }
                        m_world->setBlock(
                            furnacePosition.x, furnacePosition.y,
                            furnacePosition.z,
                            ChunkBlock(
                                BlockId::Furnace,
                                BlockMetadata::Furnace::LitBit));
                        setFixtureBlock(9, -3, 1,
                                        BlockId::IronOre);
                        setFixtureBlock(9, 3, 1,
                                        BlockId::CoalOre);
                    }
                }
                std::cout << "[P11_LIGHT_FIXTURE] scene="
                          << fixture << " floor_y=" << floorY
                          << " source="
                          << (fixture == "cave_before" ? "none" :
                              fixture == "furnace_lit" ? "furnace13" :
                              "torch14")
                          << '\n';
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

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_CONTAINER_FIXTURE")))
            {
                const glm::ivec3 chestPosition{
                    World::toBlockCoord(m_worldPlayer->position.x) + 2,
                    World::toBlockCoord(m_worldPlayer->position.y),
                    World::toBlockCoord(m_worldPlayer->position.z) + 2};
                m_world->setBlock(chestPosition.x, chestPosition.y,
                                  chestPosition.z, BlockId::Air);
                m_world->setBlock(chestPosition.x, chestPosition.y,
                                  chestPosition.z, BlockId::Chest);
                if (!ChestContainer::initialize(*m_world, chestPosition))
                {
                    throw std::runtime_error(
                        "Container fixture failed to initialize the chest.");
                }
                ContainerInventory contents(ChestContainer::SlotCount);
                contents.addItem(Material::STONE_BLOCK, 32);
                contents.addItem(Material::IRON_ORE_BLOCK, 7);
                contents.addItem(Material::OAK_BARK_BLOCK, 12);
                if (!m_world->updateBlockEntity(chestPosition,
                                                contents.serialize()) ||
                    !ChestContainer::open(*m_world, *m_worldPlayer,
                                          chestPosition))
                {
                    throw std::runtime_error(
                        "Container fixture failed to open the chest.");
                }
                m_containerFixturePlaced = true;
            }

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_CRAFTING_FIXTURE")))
            {
                m_worldPlayer->addItem(Material::OAK_BARK_BLOCK, 8);
                m_worldPlayer->addItem(Material::GLASS_BLOCK, 2);
                m_worldPlayer->openCrafting(
                    CraftingSession::WorkbenchGridSize);
            }

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_COMBAT_FIXTURE")))
            {
                const float yaw = glm::radians(
                    m_worldPlayer->rotation.y + 90.0f);
                const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                        -std::sin(yaw));
                const ActorId mobId = m_world->spawnMob(
                    "hellomine:combat_fixture",
                    m_worldPlayer->position + forward * 4.0f);
                if (!m_world->damagePlayer(6.0f, mobId))
                {
                    throw std::runtime_error(
                        "Combat fixture failed to damage the player.");
                }
                m_combatFixturePlaced = true;
            }

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_HUD_FIXTURE")))
            {
                m_worldPlayer->addItem(Material::STONE_SWORD, 1);
                m_worldPlayer->addItem(Material::WHEAT_SEEDS, 9);
                m_worldPlayer->addItem(Material::DIRT_BLOCK, 32);
                m_worldPlayer->addItem(Material::BREAD, 3);
                m_worldPlayer->addItem(Material::IRON_ORE_BLOCK, 7);
                std::cout << "[HUD_FIXTURE] slots=5 selected=0\n";
            }

            const char *vertexLightingFixtureValue = std::getenv(
                "HELLOMINE3D_VERTEX_LIGHTING_FIXTURE");
            if (vertexLightingFixtureValue != nullptr &&
                vertexLightingFixtureValue[0] != '\0')
            {
                const std::string fixture(vertexLightingFixtureValue);
                if (fixture != "cave" && fixture != "canopy")
                {
                    throw std::runtime_error(
                        "Unknown vertex-lighting fixture: " + fixture);
                }

                const float yaw = glm::radians(
                    m_worldPlayer->rotation.y + 90.0f);
                const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                        -std::sin(yaw));
                const glm::vec3 right(-forward.z, 0.0f, forward.x);
                const int fixtureY =
                    World::toBlockCoord(m_worldPlayer->position.y);
                const auto blockPosition =
                    [&](int forwardOffset, int rightOffset, int height)
                    {
                        const glm::vec3 position =
                            m_worldPlayer->position +
                            forward * static_cast<float>(forwardOffset) +
                            right * static_cast<float>(rightOffset);
                        return glm::ivec3{
                            World::toBlockCoord(position.x),
                            fixtureY + height,
                            World::toBlockCoord(position.z)};
                    };
                const auto setFixtureBlock =
                    [&](int forwardOffset, int rightOffset, int height,
                        BlockId block)
                    {
                        const glm::ivec3 position = blockPosition(
                            forwardOffset, rightOffset, height);
                        m_world->setBlock(position.x, position.y,
                                          position.z, block);
                    };

                for (int forwardStep = 0; forwardStep <= 12;
                     ++forwardStep)
                {
                    for (int rightStep = -5; rightStep <= 5;
                         ++rightStep)
                    {
                        setFixtureBlock(forwardStep, rightStep, -1,
                                        fixture == "cave"
                                            ? BlockId::Stone
                                            : BlockId::Grass);
                        for (int height = 0; height <= 7; ++height)
                        {
                            setFixtureBlock(forwardStep, rightStep, height,
                                            BlockId::Air);
                        }
                    }
                }

                if (fixture == "cave")
                {
                    for (int forwardStep = 4; forwardStep <= 11;
                         ++forwardStep)
                    {
                        for (int rightStep = -3; rightStep <= 3;
                             ++rightStep)
                        {
                            setFixtureBlock(forwardStep, rightStep, -1,
                                            BlockId::Stone);
                            setFixtureBlock(forwardStep, rightStep, 4,
                                            BlockId::Stone);
                        }
                        for (int height = 0; height <= 4; ++height)
                        {
                            setFixtureBlock(forwardStep, -3, height,
                                            BlockId::Stone);
                            setFixtureBlock(forwardStep, 3, height,
                                            BlockId::Stone);
                        }
                    }
                    for (int rightStep = -3; rightStep <= 3;
                         ++rightStep)
                    {
                        for (int height = 0; height <= 4; ++height)
                        {
                            setFixtureBlock(11, rightStep, height,
                                            BlockId::Stone);
                        }
                    }
                    setFixtureBlock(3, -3, 0, BlockId::Stone);
                    setFixtureBlock(3, 3, 0, BlockId::Stone);
                }
                else
                {
                    for (int height = 0; height <= 4; ++height)
                    {
                        setFixtureBlock(6, 0, height,
                                        BlockId::OakBark);
                    }
                    for (int height = 4; height <= 6; ++height)
                    {
                        const int radius = height == 6 ? 2 : 4;
                        for (int forwardStep = 2;
                             forwardStep <= 10; ++forwardStep)
                        {
                            for (int rightStep = -4; rightStep <= 4;
                                 ++rightStep)
                            {
                                const int distance =
                                    std::abs(forwardStep - 6) +
                                    std::abs(rightStep);
                                if (distance <= radius + 2)
                                {
                                    setFixtureBlock(
                                        forwardStep, rightStep, height,
                                        BlockId::OakLeaf);
                                }
                            }
                        }
                    }
                    setFixtureBlock(5, -2, 3, BlockId::OakLeaf);
                    setFixtureBlock(5, 2, 3, BlockId::OakLeaf);
                    setFixtureBlock(7, -2, 3, BlockId::OakLeaf);
                    setFixtureBlock(7, 2, 3, BlockId::OakLeaf);
                }
                std::cout << "[VERTEX_LIGHTING_FIXTURE] mode="
                          << fixture << " ao="
                          << (isTrueValue(std::getenv(
                                  "HELLOMINE3D_DISABLE_VERTEX_AO"))
                                  ? "off"
                                  : "on")
                          << "\n";
            }

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_CROP_FIXTURE")))
            {
                const float yaw = glm::radians(
                    m_worldPlayer->rotation.y + 90.0f);
                const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                        -std::sin(yaw));
                const glm::vec3 right(-forward.z, 0.0f, forward.x);
                const glm::vec3 center =
                    m_worldPlayer->position + forward * 2.75f;
                const int cropY =
                    World::toBlockCoord(m_worldPlayer->position.y);
                for (int stage = 0; stage <= 3; ++stage)
                {
                    const float offset =
                        (static_cast<float>(stage) - 1.5f) * 1.1f;
                    const glm::vec3 location = center + right * offset;
                    const int x = World::toBlockCoord(location.x);
                    const int z = World::toBlockCoord(location.z);
                    m_world->setBlock(x, cropY - 1, z, BlockId::Dirt);
                    m_world->setBlock(x, cropY + 1, z, BlockId::Air);
                    m_world->setBlock(
                        x, cropY, z,
                        ChunkBlock(BlockId::WheatCrop,
                                   static_cast<BlockMetadata_t>(stage)));
                }
                m_cropFixturePlaced = true;
            }

            if (isTrueValue(std::getenv(
                    "HELLOMINE3D_VERTICAL_SLICE_FIXTURE")))
            {
                const float yaw = glm::radians(
                    m_worldPlayer->rotation.y + 90.0f);
                const glm::vec3 forward(-std::cos(yaw), 0.0f,
                                        -std::sin(yaw));
                const glm::vec3 right(-forward.z, 0.0f, forward.x);
                const int fixtureY =
                    World::toBlockCoord(m_worldPlayer->position.y);
                const auto blockPosition =
                    [&](float forwardOffset, float rightOffset)
                    {
                        const glm::vec3 position =
                            m_worldPlayer->position +
                            forward * forwardOffset + right * rightOffset;
                        return glm::ivec3{
                            World::toBlockCoord(position.x), fixtureY,
                            World::toBlockCoord(position.z)};
                    };

                for (int forwardStep = 2; forwardStep <= 7;
                     ++forwardStep)
                {
                    for (int rightStep = -3; rightStep <= 3;
                         ++rightStep)
                    {
                        const glm::ivec3 stage = blockPosition(
                            static_cast<float>(forwardStep),
                            static_cast<float>(rightStep));
                        m_world->setBlock(stage.x, fixtureY - 1, stage.z,
                                          BlockId::Stone);
                        for (int height = 0; height <= 3; ++height)
                        {
                            m_world->setBlock(stage.x, fixtureY + height,
                                              stage.z, BlockId::Air);
                        }
                    }
                }

                const glm::ivec3 plantedCrop = blockPosition(3.0f, -1.5f);
                const glm::ivec3 matureCrop = blockPosition(3.0f, -0.3f);
                for (const glm::ivec3 &crop : {plantedCrop, matureCrop})
                {
                    m_world->setBlock(crop.x, crop.y - 1, crop.z,
                                      BlockId::Dirt);
                    m_world->setBlock(crop.x, crop.y + 1, crop.z,
                                      BlockId::Air);
                }
                m_world->setBlock(
                    plantedCrop.x, plantedCrop.y, plantedCrop.z,
                    ChunkBlock(BlockId::WheatCrop,
                               BlockMetadata::WheatCrop::Planted));
                m_world->setBlock(
                    matureCrop.x, matureCrop.y, matureCrop.z,
                    ChunkBlock(BlockId::WheatCrop,
                               BlockMetadata::WheatCrop::Mature));

                const glm::ivec3 chest = blockPosition(3.0f, 1.5f);
                m_world->setBlock(chest.x, chest.y, chest.z,
                                  BlockId::Chest);
                if (!ChestContainer::initialize(*m_world, chest))
                {
                    throw std::runtime_error(
                        "Vertical-slice fixture failed to initialize the chest.");
                }
                ContainerInventory contents(ChestContainer::SlotCount);
                contents.addItem(Material::WHEAT, 1);
                if (!m_world->updateBlockEntity(chest,
                                                contents.serialize()) ||
                    !ChestContainer::open(*m_world, *m_worldPlayer, chest))
                {
                    throw std::runtime_error(
                        "Vertical-slice fixture failed to store or show the harvest.");
                }

                const glm::vec3 mobPosition =
                    m_worldPlayer->position + forward * 5.0f + right * 1.5f;
                m_world->spawnMob(World::NaturalMobType, mobPosition);
                m_world->spawnItemEntity(
                    Material::ID::Dirt, 1,
                    m_worldPlayer->position + forward * 4.0f - right * 1.5f);
                m_worldPlayer->addItem(Material::WHEAT_SEEDS, 1);
                m_worldPlayer->addItem(Material::DIRT_BLOCK, 1);
                m_verticalSliceFixturePlaced = true;
            }

            configureRcPerformanceFixture();

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
                            renderable->setCastShadows(
                                std::string(materialName) ==
                                "HelloMine3D/Terrain");
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
                        section->markMeshClean();
                        const std::string key =
                            sectionKey(sectionLocation);
                        if (visual.node != nullptr)
                        {
                            m_sectionVisuals.emplace(
                                key, std::move(visual));
                            m_sectionRenderStates[key] =
                                ChunkRenderState::GpuResident;
                        }
                        else
                        {
                            m_sectionRenderStates[key] =
                                ChunkRenderState::NotResident;
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
            if (uploadToOgre)
            {
                runtimeOperationTimings().markLatestActive(
                    RuntimeOperationKind::WorldEntry);
            }
            return summary;
        }

        void createInput()
        {
            std::size_t windowHandle = 0;
            m_window->getCustomAttribute("WINDOW", &windowHandle);
            m_nativeWindowHandle = static_cast<std::uintptr_t>(windowHandle);

            std::ostringstream handleText;
            handleText << windowHandle;
            OIS::ParamList parameters;
            parameters.insert({"WINDOW", handleText.str()});
#if defined(OIS_WIN32_PLATFORM)
            parameters.insert({"w32_mouse", m_hiddenWindow
                ? "DISCL_BACKGROUND" : "DISCL_FOREGROUND"});
            parameters.insert({"w32_mouse", "DISCL_NONEXCLUSIVE"});
            parameters.insert({"w32_keyboard", m_hiddenWindow
                ? "DISCL_BACKGROUND" : "DISCL_FOREGROUND"});
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

        bool clearActiveWorld(bool requireSave = true)
        {
            if (m_audio != nullptr)
            {
                m_audio->detach();
            }
            if (requireSave && m_sandbox != nullptr &&
                !m_sandbox->closeWorld())
            {
                if (m_audio != nullptr && m_world != nullptr)
                {
                    m_audio->attach(m_world->getEventBus());
                }
                return false;
            }
            if (m_userInterface != nullptr)
            {
                m_userInterface->setWorldContext(nullptr, nullptr);
            }
            if (m_blockOutline != nullptr)
            {
                m_blockOutline->update(nullptr);
            }
            for (auto &entry : m_sectionVisuals)
            {
                destroySectionVisual(entry.second);
            }
            m_sectionVisuals.clear();
            m_sectionRenderStates.clear();
            destroyDirectionalShadowResources();
            m_actorRenderer.reset();
            if (m_sceneManager != nullptr)
            {
                m_actorRenderer =
                    std::make_unique<OgreActorRenderer>(*m_sceneManager);
            }
            m_sandbox.reset();
            m_world = nullptr;
            m_worldPlayer = nullptr;
            m_logicCamera.reset();
            if (m_camera != nullptr)
            {
                m_camera->setPosition(0.0f, 1.0f, 5.0f);
                m_camera->lookAt(0.0f, 1.0f, 0.0f);
            }
            return true;
        }

        void processInterfaceAction()
        {
            if (m_userInterface == nullptr)
            {
                return;
            }
            const OgreUserInterfaceAction action =
                m_userInterface->consumeAction();
            switch (action.type)
            {
                case OgreUserInterfaceActionType::None:
                    return;
                case OgreUserInterfaceActionType::Quit:
                    if (!clearActiveWorld())
                    {
                        m_userInterface->setStatusMessage(
                            "World save failed; quit was cancelled.");
                        return;
                    }
                    m_shutdownRequested = true;
                    return;
                case OgreUserInterfaceActionType::ReturnToMainMenu:
                    if (!clearActiveWorld())
                    {
                        m_userInterface->setStatusMessage(
                            "World save failed; return to menu was cancelled.");
                        return;
                    }
                    m_applicationFlow.returnToMainMenu();
                    return;
                case OgreUserInterfaceActionType::ApplySettings:
                {
                    Config candidate = m_config;
                    userSettings(candidate) = action.settings;
                    std::string error;
                    if (!saveRuntimeConfig(
                            ResourcePaths::bin("config.txt"), candidate,
                            &error))
                    {
                        m_userInterface->reportSettingsApplied(
                            false, userSettings(m_config),
                            "Settings were not saved: " + error);
                        return;
                    }

                    bool shadowFallback = false;
                    bool postFallback = false;
                    if (candidate.directionalShadowQuality !=
                        m_config.directionalShadowQuality)
                    {
                        if (!configureDirectionalShadows(
                                candidate.directionalShadowQuality))
                        {
                            candidate.directionalShadowQuality =
                                DirectionalShadowQuality::Off;
                            shadowFallback = true;
                        }
                    }
                    if (candidate.postProcessingQuality !=
                        m_config.postProcessingQuality)
                    {
                        if (!configurePostProcessing(
                                candidate.postProcessingQuality))
                        {
                            candidate.postProcessingQuality =
                                PostProcessingQuality::Off;
                            postFallback = true;
                        }
                    }
                    if (shadowFallback || postFallback)
                    {
                        std::string fallbackSaveError;
                        if (!saveRuntimeConfig(
                                ResourcePaths::bin("config.txt"),
                                candidate, &fallbackSaveError))
                        {
                            std::cerr
                                << "[GRAPHICS_SETTINGS] "
                                   "fallback-save-failed="
                                << fallbackSaveError << '\n';
                        }
                    }
                    userSettings(m_config) = userSettings(candidate);
                    if (m_camera != nullptr)
                    {
                        m_camera->setFOVy(Ogre::Degree(
                            static_cast<Ogre::Real>(m_config.fov)));
                    }
                    if (m_sandbox != nullptr)
                    {
                        m_sandbox->applyUserSettings(
                            userSettings(m_config));
                    }
                    if (m_audio != nullptr)
                    {
                        m_audio->setUserSettings(userSettings(m_config));
                    }
                    if (m_music != nullptr)
                    {
                        m_music->setUserSettings(userSettings(m_config));
                    }
                    m_userInterface->reportSettingsApplied(
                        true, userSettings(m_config),
                        postFallback
                            ? "settings.post_fallback"
                            : (shadowFallback
                                   ? "settings.shadow_fallback"
                                   : std::string()));
                    return;
                }
                case OgreUserInterfaceActionType::ApplyDifficulty:
                {
                    if (m_world == nullptr)
                    {
                        m_userInterface->setStatusMessage(
                            "No active world can accept a difficulty change.");
                        return;
                    }
                    const DifficultyChangeResult result =
                        m_world->requestDifficulty(action.difficulty);
                    if (result == DifficultyChangeResult::Invalid)
                    {
                        m_userInterface->setStatusMessage(
                            "Difficulty change was rejected.");
                    }
                    else if (result == DifficultyChangeResult::Unchanged)
                    {
                        m_userInterface->setStatusMessage(
                            "Difficulty is already selected.");
                    }
                    else
                    {
                        m_userInterface->setStatusMessage(
                            "Difficulty queued for the next simulation tick.");
                    }
                    return;
                }
                case OgreUserInterfaceActionType::ClaimVictoryReward:
                    if (m_world != nullptr)
                    {
                        m_world->claimWaystoneReward(
                            m_applicationFlow.acceptsWorldSimulation());
                    }
                    return;
                case OgreUserInterfaceActionType::OpenWorld:
                    break;
            }

            if (!m_applicationFlow.beginLoading(action.worldId))
            {
                return;
            }
            const WorldManagementResult prepared =
                m_worldManagement->prepareWorldForOpen(action.worldId);
            if (!prepared.succeeded())
            {
                m_applicationFlow.completeLoading(false);
                m_userInterface->setStatusMessage(prepared.message);
                return;
            }
            m_pendingWorldDirectory = prepared.directoryPath;
            m_loadingRequestedFrame = m_frameCount;
        }

        void activatePendingWorld()
        {
            if (m_pendingWorldDirectory.empty())
            {
                return;
            }
            const std::string directory =
                std::move(m_pendingWorldDirectory);
            m_pendingWorldDirectory.clear();
            try
            {
                buildTerrain(true, directory);
                if (!configureDirectionalShadows(
                        m_config.directionalShadowQuality))
                {
                    m_config.directionalShadowQuality =
                        DirectionalShadowQuality::Off;
                }
                syncActorVisuals();
                m_userInterface->setWorldContext(m_worldPlayer, m_world);
                m_applicationFlow.completeLoading(true);
            }
            catch (const std::exception &exception)
            {
                clearActiveWorld(false);
                runtimeOperationTimings().completeLatestActive(
                    RuntimeOperationKind::WorldEntry, false);
                m_applicationFlow.completeLoading(false);
                m_userInterface->setStatusMessage(
                    std::string("World loading failed: ") +
                    exception.what());
            }
        }

        bool frameStarted(const Ogre::FrameEvent& event) override
        {
            HELLOMINE3D_PROFILE_FRAME();
            HELLOMINE3D_PROFILE_SCOPE("Ogre::frameStarted");
            HELLOMINE3D_PROFILE_PLOT(
                "Frame Delta (ms)",
                static_cast<double>(event.timeSinceLastFrame) * 1000.0);
            m_frameStart = std::chrono::steady_clock::now();
            Ogre::WindowEventUtilities::messagePump();
            if (m_shutdownRequested || m_window == nullptr ||
                m_window->isClosed())
            {
                return false;
            }

            syncInputFocus();
            updateNativeCursorCapture();
            if (!m_hiddenWindow)
            {
                m_keyboard->capture();
                m_mouse->capture();
            }
            processInterfaceAction();
            updateNativeCursorCapture();
            if (!m_pendingWorldDirectory.empty() &&
                m_frameCount > m_loadingRequestedFrame)
            {
                activatePendingWorld();
            }
            updateSandbox(event.timeSinceLastFrame);
            updateAudio(event.timeSinceLastFrame);

            m_frameWorldStats = collectRuntimeStats();
            if (m_world != nullptr)
            {
                syncEnvironment(m_frameWorldStats.environment);
            }
            if (m_userInterface != nullptr)
            {
                const MiningProgressSnapshot progress =
                    m_sandbox != nullptr
                        ? m_sandbox->getMiningProgress()
                        : MiningProgressSnapshot();
                const ActionFeedbackSnapshot actionFeedback =
                    m_sandbox != nullptr
                        ? m_sandbox->getActionFeedback()
                        : ActionFeedbackSnapshot();
                m_userInterface->beginFrame(event.timeSinceLastFrame,
                                            m_frameWorldStats, progress,
                                            actionFeedback);
            }
            m_updateEnd = std::chrono::steady_clock::now();
            return true;
        }

        bool frameRenderingQueued(const Ogre::FrameEvent&) override
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::frameRenderingQueued");
            ++m_frameCount;
            return true;
        }

        bool frameEnded(const Ogre::FrameEvent& event) override
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::frameEnded");
            if (m_renderCapture != nullptr)
            {
                m_renderCapture->update(event.timeSinceLastFrame);
            }
            emitDirectionalShadowDiagnostics();

            const auto frameEnd = std::chrono::steady_clock::now();
            const double frameMs =
                std::chrono::duration<double, std::milli>(
                    frameEnd - m_frameStart)
                    .count();
            RuntimePerformanceCapture::FrameTimings timings;
            timings.deltaMs =
                static_cast<double>(event.timeSinceLastFrame) * 1000.0;
            timings.updateMs =
                std::chrono::duration<double, std::milli>(
                    m_updateEnd - m_frameStart)
                    .count();
            timings.renderMs =
                std::chrono::duration<double, std::milli>(
                    frameEnd - m_updateEnd)
                    .count();
            timings.frameMs = frameMs;

            RuntimePerformanceCapture::recordFrame(timings,
                                                    m_frameWorldStats);

            if (m_frameCount == 1)
            {
                runtimeOperationTimings().markLatestActive(
                    RuntimeOperationKind::WorldEntry);
                runtimeOperationTimings().completeLatestActive(
                    RuntimeOperationKind::WorldEntry, true);
                runtimeOperationTimings().markLatestActive(
                    RuntimeOperationKind::Startup);
                runtimeOperationTimings().completeLatestActive(
                    RuntimeOperationKind::Startup, true);

                if (isControlledCrashRequested(
                        ControlledCrashPoint::AfterFirstFrame))
                {
                    if (m_world == nullptr || !m_world->save())
                    {
                        throw std::runtime_error(
                            "Unable to publish the active world before the "
                            "controlled crash.");
                    }
                    std::cout
                        << "[CRASH_DIAGNOSTICS] controlled_crash="
                        << controlledCrashPointName(
                               ControlledCrashPoint::AfterFirstFrame)
                        << " active_world_saved=1\n"
                        << std::flush;
                    std::cerr << std::flush;
                    triggerControlledCrashIfRequested(
                        ControlledCrashPoint::AfterFirstFrame);
                }
            }

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
            HELLOMINE3D_PROFILE_SCOPE("Ogre::updateSandbox");
            if (m_sandbox == nullptr)
            {
                return;
            }
            updateRcPerformanceScenario(deltaSeconds);
            if (!m_applicationFlow.acceptsWorldSimulation())
            {
                if (m_keyboard != nullptr)
                {
                    const GameplayInputBindings &bindings =
                        m_config.inputBindings;
                    m_movementModeTracker.update(
                        false,
                        m_keyboard->isKeyDown(toOisKey(bindings.get(
                            GameplayAction::Sprint))),
                        m_keyboard->isKeyDown(toOisKey(bindings.get(
                            GameplayAction::Sneak))),
                        m_config.sprintMode, m_config.sneakMode);
                }
                m_sandbox->cancelMiningProgress();
                clearTransientInput();
                return;
            }

            const bool keyboardCaptured =
                m_worldPlayer->hasOpenContainer() ||
                (m_userInterface != nullptr &&
                 m_userInterface->wantsKeyboardInput());
            const bool mouseCaptured =
                m_worldPlayer->hasOpenContainer() ||
                (m_userInterface != nullptr &&
                 m_userInterface->wantsMouseInput());
            const bool worldInputActive =
                m_focusGate.isFocused() && !m_focusTransitionFrame &&
                !keyboardCaptured && !mouseCaptured;

            SandboxInputState input;
            const GameplayInputBindings &bindings =
                m_config.inputBindings;
            const bool sprintDown = m_keyboard->isKeyDown(
                toOisKey(bindings.get(GameplayAction::Sprint)));
            const bool sneakDown = m_keyboard->isKeyDown(
                toOisKey(bindings.get(GameplayAction::Sneak)));
            const GameplayMovementModeState movementModes =
                m_movementModeTracker.update(
                    worldInputActive, sprintDown, sneakDown,
                    m_config.sprintMode, m_config.sneakMode);
            if (worldInputActive)
            {
                input.player.moveForward = m_keyboard->isKeyDown(
                    toOisKey(bindings.get(GameplayAction::MoveForward)));
                input.player.moveBackward = m_keyboard->isKeyDown(
                    toOisKey(bindings.get(GameplayAction::MoveBackward)));
                input.player.moveLeft = m_keyboard->isKeyDown(
                    toOisKey(bindings.get(GameplayAction::MoveLeft)));
                input.player.moveRight = m_keyboard->isKeyDown(
                    toOisKey(bindings.get(GameplayAction::MoveRight)));
                input.player.sprint = movementModes.sprint;
                input.player.jump = m_keyboard->isKeyDown(
                    toOisKey(bindings.get(GameplayAction::Jump)));
                input.player.descend = movementModes.sneak;
                input.player.toggleFlying = m_toggleFlying;
                input.player.hotbarDelta = m_hotbarDelta;
                input.player.hotbarSlot = m_hotbarSlot;
                input.resetMeshes = m_resetMeshes;
                input.useHeldFood = m_useHeldFood;
            }
            const OIS::MouseState &mouseState = m_mouse->getMouseState();
            bool anyMouseButtonDown = false;
            for (std::size_t buttonIndex = 0;
                 buttonIndex < GameplayMouseButtonCount; ++buttonIndex)
            {
                anyMouseButtonDown = anyMouseButtonDown ||
                    mouseState.buttonDown(toOisMouseButton(
                        static_cast<GameplayMouseButton>(buttonIndex)));
            }
            const bool worldMouseButtonsAllowed =
                worldInputActive &&
                m_focusGate.allowsWorldButtons(anyMouseButtonDown);
            if (worldInputActive && m_mouseLookEnabled &&
                m_focusGate.acceptsLookSample())
            {
                const GameplayLookDelta look = calculateGameplayLookDelta(
                    m_pendingLookDelta.x, m_pendingLookDelta.y,
                    m_config.mouseSensitivity, m_config.invertMouseY);
                input.player.lookDelta.x = look.yaw;
                input.player.lookDelta.y = look.pitch;
            }
            if (worldMouseButtonsAllowed)
            {
                const GameplayMouseBindings &mouseBindings =
                    m_config.mouseBindings;
                input.breakAttack = mouseState.buttonDown(
                    toOisMouseButton(mouseBindings.get(
                        GameplayWorldAction::BreakAttack)));
                input.useBlock = mouseState.buttonDown(
                    toOisMouseButton(mouseBindings.get(
                        GameplayWorldAction::Use)));
                input.placeBlock = mouseState.buttonDown(
                    toOisMouseButton(mouseBindings.get(
                        GameplayWorldAction::Place)));
                input.guardCombat = mouseState.buttonDown(
                    toOisMouseButton(mouseBindings.get(
                        GameplayWorldAction::Guard)));
            }

            const bool diagnosticsActive =
                (m_renderCapture != nullptr &&
                 m_renderCapture->isEnabled()) ||
                RuntimePerformanceCapture::isEnabled();
            const bool freezeValidationCapture =
                (m_validationActorsSpawned || m_oreFixturePlaced ||
                 m_containerFixturePlaced || m_combatFixturePlaced ||
                 m_cropFixturePlaced || m_verticalSliceFixturePlaced) &&
                m_renderCapture != nullptr &&
                m_renderCapture->isEnabled();
            m_sandbox->update(input,
                              freezeValidationCapture ? 0.0f : deltaSeconds,
                              !diagnosticsActive && worldInputActive);
            if (m_userInterface != nullptr &&
                m_sandbox->getFoodUseResult().has_value())
            {
                m_userInterface->setStatusMessage(
                    runtimeLocalizedTextRegistry().lookup(
                        m_config.locale, foodUseResultKey(
                            *m_sandbox->getFoodUseResult())));
            }
            if (m_userInterface != nullptr && m_world != nullptr)
            {
                const std::string feedback =
                    m_world->consumeWaystoneFeedbackKey();
                if (!feedback.empty())
                {
                    m_userInterface->setStatusMessage(
                        runtimeLocalizedTextRegistry().lookup(
                            m_config.locale, feedback));
                }
            }
            clearTransientInput();
            m_focusTransitionFrame = false;
            syncRenderCamera();
            syncSectionMeshes();
            syncActorVisuals();
            if (m_blockOutline != nullptr)
            {
                const auto& selection = m_sandbox->getBlockSelection();
                const MiningProgressSnapshot &progress =
                    m_sandbox->getMiningProgress();
                m_blockOutline->update(
                    selection.has_value() ? &*selection : nullptr,
                    progress.crackStage());
            }
        }

        void updateAudio(float deltaSeconds)
        {
            AudioListenerState listener;
            if (m_worldPlayer != nullptr)
            {
                listener.position = m_worldPlayer->position;
                const float yaw = glm::radians(m_worldPlayer->rotation.y);
                listener.forward = glm::vec3(
                    std::sin(yaw), 0.f, -std::cos(yaw));
            }
            const bool worldPaused =
                m_applicationFlow.state() == GameApplicationState::Paused;
            if (m_audio != nullptr)
            {
                m_audio->setWorldPaused(worldPaused);
                m_audio->update(deltaSeconds,
                                m_applicationFlow.acceptsWorldSimulation(),
                                listener);
            }
            if (m_music != nullptr)
            {
                m_music->update(deltaSeconds, m_world != nullptr,
                                worldPaused);
            }
        }

        void syncSectionMeshes()
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::syncSectionMeshes");
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
                const std::string key = sectionKey(location);
                liveSections.insert(key);
                m_sectionRenderStates.emplace(
                    key, ChunkRenderState::NotResident);
            }

            for (auto it = m_sectionRenderStates.begin();
                 it != m_sectionRenderStates.end();)
            {
                if (liveSections.find(it->first) != liveSections.end())
                {
                    ++it;
                    continue;
                }

                const auto visual = m_sectionVisuals.find(it->first);
                if (visual != m_sectionVisuals.end())
                {
                    destroySectionVisual(visual->second);
                    m_sectionVisuals.erase(visual);
                }
                if (it->second != ChunkRenderState::NotResident)
                {
                    transitionRenderState(it->first,
                                          ChunkRenderState::NotResident);
                }
                it = m_sectionRenderStates.erase(it);
            }

            std::vector<WorldSectionMeshVersion> uploaded;
            uploaded.reserve(snapshot.cpuReadySections.size());
            for (const WorldSectionMeshSnapshot& section :
                 snapshot.cpuReadySections)
            {
                const std::string key = sectionKey(section.location);
                ChunkRenderState state = m_sectionRenderStates[key];
                if (state == ChunkRenderState::GpuResident)
                {
                    transitionRenderState(key, ChunkRenderState::Stale);
                    state = ChunkRenderState::Stale;
                }
                else if (state == ChunkRenderState::UploadPending)
                {
                    transitionRenderState(key, ChunkRenderState::Stale);
                    state = ChunkRenderState::Stale;
                }
                if (state == ChunkRenderState::NotResident ||
                    state == ChunkRenderState::Stale)
                {
                    transitionRenderState(
                        key, ChunkRenderState::UploadPending);
                }
                uploadSectionVisual(section);
                if (m_fastStreamingPending &&
                    section.location.x == m_fastStreamingTarget.x &&
                    section.location.z == m_fastStreamingTarget.z)
                {
                    const double visibleMilliseconds =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() -
                            m_fastStreamingStarted)
                            .count();
                    RuntimePerformanceCapture::recordStreamingLatency(
                        visibleMilliseconds);
                    m_fastStreamingPending = false;
                    m_nextFastStreamingMoveSeconds =
                        m_rcPerformanceElapsedSeconds + 2.0f;
                }
                uploaded.push_back(
                    {section.location, section.blockRevision});
            }
            m_world->acknowledgeSectionMeshUploads(uploaded);
            const WorldMeshSnapshot acknowledged =
                m_world->collectSectionMeshSnapshot();
            std::unordered_map<std::string, std::uint32_t>
                currentRevisions;
            currentRevisions.reserve(
                acknowledged.liveSectionVersions.size());
            for (const WorldSectionMeshVersion& version :
                 acknowledged.liveSectionVersions)
            {
                currentRevisions.emplace(
                    sectionKey(version.location), version.blockRevision);
            }
            std::unordered_set<std::string> stillCpuReady;
            stillCpuReady.reserve(
                acknowledged.cpuReadySections.size());
            for (const WorldSectionMeshSnapshot& section :
                 acknowledged.cpuReadySections)
            {
                stillCpuReady.insert(sectionKey(section.location));
            }
            for (const WorldSectionMeshVersion& version : uploaded)
            {
                const std::string key = sectionKey(version.location);
                const bool acceptedCurrent =
                    currentRevisions.find(key) !=
                        currentRevisions.end() &&
                    currentRevisions[key] == version.blockRevision &&
                    stillCpuReady.find(key) == stillCpuReady.end();
                const bool hasVisual =
                    m_sectionVisuals.find(key) != m_sectionVisuals.end();
                if (!acceptedCurrent && hasVisual)
                {
                    auto visual = m_sectionVisuals.find(key);
                    destroySectionVisual(visual->second);
                    m_sectionVisuals.erase(visual);
                }
                transitionRenderState(
                    key, acceptedCurrent && hasVisual
                             ? ChunkRenderState::GpuResident
                             : ChunkRenderState::NotResident);
            }
        }

        void syncActorVisuals()
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::syncActorVisuals");
            if (m_world == nullptr || m_actorRenderer == nullptr ||
                m_logicCamera == nullptr)
            {
                return;
            }
            m_actorRenderer->sync(m_world->collectActorSnapshots(),
                                  m_logicCamera->position);
            m_actorRenderer->syncProjectiles(
                m_world->collectCombatProjectileSnapshots());
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

        void configureRcPerformanceFixture()
        {
            const char *profile =
                std::getenv("HELLOMINE3D_RC_PERF_PROFILE");
            if (profile == nullptr || profile[0] == '\0' ||
                m_world == nullptr || m_worldPlayer == nullptr)
            {
                return;
            }

            const std::string profileName(profile);
            if (profileName == "fast-streaming")
            {
                m_fastStreamingEnabled = true;
                m_fastStreamingOrigin = World::getChunkXZ(
                    World::toBlockCoord(m_worldPlayer->position.x),
                    World::toBlockCoord(m_worldPlayer->position.z));
                std::cout << "[RC_PERF] profile=fast-streaming path=v1\n";
                return;
            }
            if (profileName != "scaled-gameplay")
            {
                throw std::runtime_error(
                    "Unknown RC performance profile: " + profileName);
            }

            const int centerX =
                World::toBlockCoord(m_worldPlayer->position.x);
            const int centerY =
                World::toBlockCoord(m_worldPlayer->position.y);
            const int centerZ =
                World::toBlockCoord(m_worldPlayer->position.z);
            std::size_t crops = 0;
            std::size_t chests = 0;
            std::size_t capEvents = 0;

            for (int z = 0; z < 8; ++z)
            {
                for (int x = 0; x < 8; ++x)
                {
                    const int blockX = centerX + 10 + x;
                    const int blockZ = centerZ + 10 + z;
                    m_world->setBlock(blockX, centerY - 1, blockZ,
                                      BlockId::Dirt);
                    m_world->setBlock(blockX, centerY + 1, blockZ,
                                      BlockId::Air);
                    m_world->setBlock(
                        blockX, centerY, blockZ,
                        ChunkBlock(BlockId::WheatCrop,
                                   BlockMetadata::WheatCrop::Mature));
                    ++crops;
                }
            }

            for (int index = 0; index < 8; ++index)
            {
                const glm::ivec3 chest{
                    centerX - 12 + index * 2, centerY, centerZ + 10};
                m_world->setBlock(chest.x, chest.y - 1, chest.z,
                                  BlockId::Stone);
                m_world->setBlock(chest.x, chest.y, chest.z,
                                  BlockId::Chest);
                if (ChestContainer::initialize(*m_world, chest))
                {
                    ++chests;
                }
                else
                {
                    ++capEvents;
                }
            }

            for (int index = 0; index < 8; ++index)
            {
                const glm::vec3 position =
                    m_worldPlayer->position +
                    glm::vec3(6.f + static_cast<float>(index % 4), 0.f,
                              5.f + static_cast<float>(index / 4) * 2.f);
                if (m_world->spawnMob("hellomine:scaled_fixture",
                                      position) == InvalidActorId)
                {
                    ++capEvents;
                }
            }
            for (int index = 0; index < 16; ++index)
            {
                const glm::vec3 position =
                    m_worldPlayer->position +
                    glm::vec3(18.f + static_cast<float>(index % 8), 3.f,
                              12.f + static_cast<float>(index / 8) * 2.f);
                if (m_world->spawnItemEntity(Material::ID::Stone, 1,
                                             position) == InvalidActorId)
                {
                    ++capEvents;
                }
            }

            const WorldDebugStats stats = m_world->collectDebugStats();
            const std::size_t items =
                m_world->getActorManager().countActorsByType("item");
            RuntimePerformanceCapture::recordScenarioPopulation(
                stats.actorCount, items, crops, chests, capEvents);
            std::cout << "[RC_PERF] profile=scaled-gameplay actors="
                      << stats.actorCount << " items=" << items
                      << " crops=" << crops << " chests=" << chests
                      << " cap_events=" << capEvents << '\n';
        }

        void updateRcPerformanceScenario(float deltaSeconds)
        {
            if (!m_fastStreamingEnabled || m_fastStreamingPending ||
                m_world == nullptr || m_worldPlayer == nullptr)
            {
                if (m_fastStreamingEnabled)
                {
                    m_rcPerformanceElapsedSeconds += deltaSeconds;
                }
                return;
            }

            m_rcPerformanceElapsedSeconds += deltaSeconds;
            if (m_rcPerformanceElapsedSeconds <
                m_nextFastStreamingMoveSeconds)
            {
                return;
            }
            if (m_fastStreamingMoveIndex >= 4)
            {
                return;
            }

            static const std::array<VectorXZ, 8> Offsets = {
                VectorXZ{12, 0}, VectorXZ{12, 12},
                VectorXZ{0, 12}, VectorXZ{-12, 12},
                VectorXZ{-12, 0}, VectorXZ{-12, -12},
                VectorXZ{0, -12}, VectorXZ{12, -12}};
            const VectorXZ offset =
                Offsets[m_fastStreamingMoveIndex % Offsets.size()];
            ++m_fastStreamingMoveIndex;
            m_fastStreamingTarget = {
                m_fastStreamingOrigin.x + offset.x,
                m_fastStreamingOrigin.z + offset.z};
            m_worldPlayer->position = {
                static_cast<float>(m_fastStreamingTarget.x * CHUNK_SIZE +
                                   CHUNK_SIZE / 2),
                m_worldPlayer->position.y,
                static_cast<float>(m_fastStreamingTarget.z * CHUNK_SIZE +
                                   CHUNK_SIZE / 2)};
            m_worldPlayer->velocity = glm::vec3(0.f);
            m_worldPlayer->box.update(m_worldPlayer->position);
            m_logicCamera->update();
            m_world->preloadAround(m_worldPlayer->position);
            m_fastStreamingStarted = std::chrono::steady_clock::now();
            m_fastStreamingPending = true;
        }

        bool uploadSectionVisual(
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
                    renderable->setCastShadows(
                        std::string(materialName) ==
                        "HelloMine3D/Terrain");
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
                return true;
            }
            return false;
        }

        bool transitionRenderState(const std::string& key,
                                   ChunkRenderState state)
        {
            const auto found = m_sectionRenderStates.find(key);
            if (found == m_sectionRenderStates.end())
            {
                assert(false && "missing Ogre render-state owner");
                return false;
            }
            const bool legal = canTransition(found->second, state);
            assert(legal && "illegal Ogre render-state transition");
            if (!legal)
            {
                return false;
            }
            found->second = state;
            return true;
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
            m_resetMeshes = false;
            m_useHeldFood = false;
            m_hotbarDelta = 0;
            m_hotbarSlot = -1;
        }

        void syncRenderCamera()
        {
            if (m_logicCamera == nullptr || m_camera == nullptr)
            {
                return;
            }

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
                for (const auto& state : m_sectionRenderStates)
                {
                    switch (state.second)
                    {
                    case ChunkRenderState::NotResident:
                        ++stats.chunks.renderNotResidentSections;
                        break;
                    case ChunkRenderState::UploadPending:
                        ++stats.chunks.renderUploadPendingSections;
                        break;
                    case ChunkRenderState::GpuResident:
                        ++stats.chunks.gpuResidentSections;
                        ++stats.chunks.gpuBufferedSections;
                        break;
                    case ChunkRenderState::Stale:
                        ++stats.chunks.renderStaleSections;
                        break;
                    }
                }
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

        Ogre::Pass* materialPass(const Ogre::String& materialName)
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
            material->load();

            Ogre::Technique* technique = material->getBestTechnique();
            if (technique == nullptr || technique->getNumPasses() == 0)
            {
                throw std::runtime_error(
                    std::string("Environment material has no pass: ") +
                    materialName);
            }
            return technique->getPass(0);
        }

        void ensureDirectionalShadowReceiver(
            const char* materialName)
        {
            Ogre::Pass* pass = materialPass(materialName);
            for (unsigned short index = 0;
                 index < pass->getNumTextureUnitStates(); ++index)
            {
                if (pass->getTextureUnitState(index)->getContentType() ==
                    Ogre::TextureUnitState::CONTENT_SHADOW)
                {
                    return;
                }
            }
            Ogre::TextureUnitState* shadow =
                pass->createTextureUnitState();
            shadow->setContentType(
                Ogre::TextureUnitState::CONTENT_SHADOW);
            shadow->setTextureAddressingMode(
                Ogre::TextureUnitState::TAM_CLAMP);
            shadow->setTextureFiltering(Ogre::TFO_NONE);
        }

        void setDirectionalShadowReceiverPrograms(bool enabled)
        {
            struct ReceiverPrograms
            {
                const char* material;
                const char* vertex;
                const char* shadowVertex;
                const char* fragment;
                const char* shadowFragment;
            };
            const ReceiverPrograms receivers[] = {
                {"HelloMine3D/Terrain", "HelloMine3D/TerrainVertex",
                 "HelloMine3D/TerrainShadowVertex",
                 "HelloMine3D/TerrainFragment",
                 "HelloMine3D/TerrainShadowFragment"},
                {"HelloMine3D/Transparent", "HelloMine3D/TerrainVertex",
                 "HelloMine3D/TerrainShadowVertex",
                 "HelloMine3D/TerrainFragment",
                 "HelloMine3D/TerrainShadowFragment"},
                {"HelloMine3D/Flora", "HelloMine3D/FloraVertex",
                 "HelloMine3D/FloraShadowVertex",
                 "HelloMine3D/TerrainFragment",
                 "HelloMine3D/TerrainShadowFragment"},
                {"HelloMine3D/ActorMob", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/ActorStalker", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/ActorBrute", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/ActorSpitter", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/ActorItem", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/CombatProjectile", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"}};
            for (const ReceiverPrograms& receiver : receivers)
            {
                Ogre::Pass* pass = materialPass(receiver.material);
                pass->setVertexProgram(
                    enabled ? receiver.shadowVertex : receiver.vertex);
                pass->setFragmentProgram(
                    enabled ? receiver.shadowFragment : receiver.fragment);
                if (enabled)
                {
                    ensureDirectionalShadowReceiver(receiver.material);
                    continue;
                }
                for (int index =
                         static_cast<int>(pass->getNumTextureUnitStates()) - 1;
                     index >= 0; --index)
                {
                    if (pass->getTextureUnitState(
                            static_cast<unsigned short>(index))
                            ->getContentType() ==
                        Ogre::TextureUnitState::CONTENT_SHADOW)
                    {
                        pass->removeTextureUnitState(
                            static_cast<unsigned short>(index));
                    }
                }
            }
            syncTerrainMaterialParameters();
        }

        void syncDirectionalShadowMaterialParameters(float strength)
        {
            m_directionalShadowStrength = strength;
            if (m_directionalShadowQuality ==
                DirectionalShadowQuality::Off)
            {
                return;
            }
            const DirectionalShadowProfile profile =
                directionalShadowProfile(m_directionalShadowQuality);
            const float enabled = 1.f;
            const char* terrainMaterials[] = {
                "HelloMine3D/Terrain", "HelloMine3D/Transparent",
                "HelloMine3D/Flora"};
            for (const char* materialName : terrainMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant(
                    "directionalShadowEnabled", enabled);
                parameters->setNamedConstant(
                    "directionalShadowBias", profile.bias);
                parameters->setNamedConstant(
                    "directionalShadowStrength", strength);
                parameters->setNamedConstant(
                    "directionalShadowFadeStart", profile.fadeStart);
                parameters->setNamedConstant(
                    "directionalShadowFadeEnd", profile.farDistance);
            }

            const char* actorMaterials[] = {
                "HelloMine3D/ActorMob", "HelloMine3D/ActorStalker",
                "HelloMine3D/ActorBrute", "HelloMine3D/ActorSpitter",
                "HelloMine3D/ActorItem",
                "HelloMine3D/CombatProjectile"};
            for (const char* materialName : actorMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant(
                    "directionalShadowEnabled", enabled);
                parameters->setNamedConstant(
                    "directionalShadowBias", profile.bias);
                parameters->setNamedConstant(
                    "directionalShadowStrength", strength);
                parameters->setNamedConstant(
                    "directionalShadowFadeStart", profile.fadeStart);
                parameters->setNamedConstant(
                    "directionalShadowFadeEnd", profile.farDistance);
            }
        }

        void emitDirectionalShadowDiagnostics()
        {
            if (m_directionalShadowDiagnosticsEmitted ||
                m_frameCount < 3 ||
                !isTrueValue(std::getenv(
                    "HELLOMINE3D_V10D_SHADOW_DIAGNOSTICS")))
            {
                return;
            }
            m_directionalShadowDiagnosticsEmitted = true;
            std::cout << "[V10D_SHADOW_DIAGNOSTICS] active="
                      << directionalShadowQualityToken(
                             m_directionalShadowQuality)
                      << " strength=" << m_directionalShadowStrength
                      << " light_attached="
                      << (m_directionalSunLight != nullptr &&
                                  m_directionalSunLight->isAttached()
                              ? 1
                              : 0);
            if (m_sceneManager == nullptr ||
                m_directionalShadowQuality ==
                    DirectionalShadowQuality::Off ||
                m_sceneManager->getShadowTextureCount() == 0)
            {
                std::cout << " texture=none\n";
                return;
            }
            try
            {
                const Ogre::TexturePtr& texture =
                    m_sceneManager->getShadowTexture(0);
                const Ogre::uint32 width = texture->getWidth();
                const Ogre::uint32 height = texture->getHeight();
                std::vector<float> pixels(width * height, 1.f);
                Ogre::PixelBox destination(
                    width, height, 1, Ogre::PF_FLOAT32_R,
                    pixels.data());
                texture->getBuffer()->blitToMemory(destination);
                float minimum = std::numeric_limits<float>::max();
                float maximum = std::numeric_limits<float>::lowest();
                double sum = 0.0;
                for (float value : pixels)
                {
                    minimum = std::min(minimum, value);
                    maximum = std::max(maximum, value);
                    sum += value;
                }
                std::cout << " texture=" << width << 'x' << height
                          << " min=" << minimum
                          << " max=" << maximum
                          << " mean=" << (sum / pixels.size()) << '\n';
            }
            catch (const std::exception& exception)
            {
                std::cout << " texture=read-failed detail="
                          << exception.what() << '\n';
            }
        }

        void destroyDirectionalShadowResources()
        {
            if (m_sceneManager == nullptr)
            {
                m_directionalSunLight = nullptr;
                m_directionalSunNode = nullptr;
                m_directionalShadowQuality =
                    DirectionalShadowQuality::Off;
                return;
            }
            m_sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_NONE);
            m_directionalShadowDiagnosticsEmitted = false;
            if (m_directionalSunLight != nullptr)
            {
                if (m_directionalSunNode != nullptr)
                {
                    m_directionalSunNode->detachObject(
                        m_directionalSunLight);
                }
                m_sceneManager->destroyLight(m_directionalSunLight);
                m_directionalSunLight = nullptr;
            }
            if (m_directionalSunNode != nullptr)
            {
                m_sceneManager->destroySceneNode(m_directionalSunNode);
                m_directionalSunNode = nullptr;
            }
            m_directionalShadowQuality =
                DirectionalShadowQuality::Off;
            m_directionalShadowStrength = 0.f;
            setDirectionalShadowReceiverPrograms(false);
            if (m_actorRenderer != nullptr)
            {
                m_actorRenderer->setCastShadows(false);
            }
        }

        bool configureDirectionalShadows(
            DirectionalShadowQuality requestedQuality)
        {
            if (m_sceneManager == nullptr)
            {
                return false;
            }

            m_sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_NONE);
            if (m_directionalSunLight != nullptr)
            {
                m_directionalSunLight->setCastShadows(false);
            }
            m_directionalShadowQuality =
                DirectionalShadowQuality::Off;
            m_directionalShadowStrength = 0.f;
            setDirectionalShadowReceiverPrograms(false);
            if (m_actorRenderer != nullptr)
            {
                m_actorRenderer->setCastShadows(false);
            }

            if (requestedQuality == DirectionalShadowQuality::Off)
            {
                destroyDirectionalShadowResources();
                std::cout << "[V10D_SHADOW] requested=off active=off "
                             "fallback=0 reason=disabled texture=0 "
                             "distance=0 pcf=0\n";
                return true;
            }

            const bool forcedFallback = isTrueValue(
                std::getenv("HELLOMINE3D_V10D_SHADOW_FALLBACK"));
            Ogre::RenderSystem* renderSystem =
                m_root != nullptr ? m_root->getRenderSystem() : nullptr;
            const Ogre::RenderSystemCapabilities* capabilities =
                renderSystem != nullptr
                    ? renderSystem->getCapabilities()
                    : nullptr;
            const bool supported =
                !forcedFallback && capabilities != nullptr &&
                capabilities->hasCapability(
                    Ogre::RSC_HWRENDER_TO_TEXTURE) &&
                capabilities->hasCapability(Ogre::RSC_TEXTURE_FLOAT) &&
                capabilities->hasCapability(Ogre::RSC_VERTEX_PROGRAM) &&
                capabilities->hasCapability(Ogre::RSC_FRAGMENT_PROGRAM) &&
                Ogre::GpuProgramManager::getSingleton()
                    .isSyntaxSupported("glsl150");
            if (!supported)
            {
                destroyDirectionalShadowResources();
                std::cout << "[V10D_SHADOW] requested="
                          << directionalShadowQualityToken(requestedQuality)
                          << " active=off fallback=1 reason="
                          << (forcedFallback
                                  ? "forced"
                                  : "unsupported-capability")
                          << " texture=0 distance=0 pcf=0\n";
                return false;
            }

            try
            {
                setDirectionalShadowReceiverPrograms(true);

                if (m_directionalSunLight == nullptr)
                {
                    m_directionalSunLight =
                        m_sceneManager->createLight(
                            "HelloMine3D_DirectionalSun");
                    m_directionalSunLight->setType(
                        Ogre::Light::LT_DIRECTIONAL);
                    m_directionalSunLight->setDiffuseColour(
                        Ogre::ColourValue::White);
                    m_directionalSunLight->setSpecularColour(
                        Ogre::ColourValue::White);
                    m_directionalSunNode =
                        m_sceneManager->getRootSceneNode()
                            ->createChildSceneNode(
                                "HelloMine3D_DirectionalSun_Node");
                    m_directionalSunNode->attachObject(
                        m_directionalSunLight);
                }

                const DirectionalShadowProfile profile =
                    directionalShadowProfile(requestedQuality);
                m_sceneManager->setShadowTextureSettings(
                    profile.textureSize, 1, Ogre::PF_FLOAT32_R);
                m_sceneManager->setShadowTextureCountPerLightType(
                    Ogre::Light::LT_DIRECTIONAL, 1);
                m_sceneManager->setShadowTextureCountPerLightType(
                    Ogre::Light::LT_POINT, 0);
                m_sceneManager->setShadowTextureCountPerLightType(
                    Ogre::Light::LT_SPOTLIGHT, 0);
                m_sceneManager->setShadowFarDistance(
                    profile.farDistance);
                m_sceneManager->setShadowDirectionalLightExtrusionDistance(
                    profile.farDistance);
                m_directionalSunLight->setShadowNearClipDistance(0.5f);
                m_directionalSunLight->setShadowFarClipDistance(
                    profile.farDistance * 2.f);
                m_sceneManager->setShadowDirLightTextureOffset(0.55f);
                m_sceneManager->setShadowTextureSelfShadow(true);
                m_sceneManager->setShadowCasterRenderBackFaces(false);
                m_sceneManager->setShadowTextureCasterMaterial(
                    "HelloMine3D/DirectionalShadowCaster");
                m_sceneManager->setShadowTechnique(
                    Ogre::SHADOWTYPE_TEXTURE_MODULATIVE_INTEGRATED);
                m_directionalSunLight->setCastShadows(true);
                m_directionalShadowQuality = requestedQuality;
                if (m_actorRenderer != nullptr)
                {
                    m_actorRenderer->setCastShadows(true);
                }
                syncDirectionalShadowMaterialParameters(0.f);
                std::cout << "[V10D_SHADOW] requested="
                          << directionalShadowQualityToken(requestedQuality)
                          << " active="
                          << directionalShadowQualityToken(
                                 m_directionalShadowQuality)
                          << " fallback=0 reason=supported texture="
                          << profile.textureSize << " distance="
                          << profile.farDistance
                          << " pcf=2x2 bias=" << profile.bias << '\n';
                return true;
            }
            catch (const std::exception& exception)
            {
                m_sceneManager->setShadowTechnique(
                    Ogre::SHADOWTYPE_NONE);
                if (m_directionalSunLight != nullptr)
                {
                    m_directionalSunLight->setCastShadows(false);
                }
                m_directionalShadowQuality =
                    DirectionalShadowQuality::Off;
                destroyDirectionalShadowResources();
                std::cout << "[V10D_SHADOW] requested="
                          << directionalShadowQualityToken(requestedQuality)
                          << " active=off fallback=1 reason=setup-failed "
                             "texture=0 distance=0 pcf=0 detail="
                          << exception.what() << '\n';
                return false;
            }
        }

        void destroyPostProcessingResources() noexcept
        {
            if (m_postProcessingInstalled && m_window != nullptr &&
                m_window->getNumViewports() > 0)
            {
                try
                {
                    Ogre::Viewport* viewport = m_window->getViewport(0);
                    Ogre::CompositorManager::getSingleton()
                        .setCompositorEnabled(
                            viewport, "HelloMine3D/PostProcess", false);
                    Ogre::CompositorManager::getSingleton()
                        .removeCompositor(
                            viewport, "HelloMine3D/PostProcess");
                }
                catch (...)
                {
                }
            }
            m_postProcessingInstalled = false;
            m_postProcessingQuality = PostProcessingQuality::Off;
        }

        void syncPostProcessingParameters()
        {
            const bool fixtureEnabled = isTrueValue(
                std::getenv("HELLOMINE3D_V10E_POST_FIXTURE"));
            Ogre::GpuProgramParametersSharedPtr parameters =
                materialPass("HelloMine3D/PostProcess")
                    ->getFragmentProgramParameters();
            parameters->setNamedConstant(
                "fixtureMode", fixtureEnabled ? 1.f : 0.f);
            std::cout << "[V10E_POST_FIXTURE] enabled="
                      << (fixtureEnabled ? 1 : 0)
                      << " bands=16 ranges=dark-full-bright\n";
        }

        bool configurePostProcessing(
            PostProcessingQuality requestedQuality)
        {
            destroyPostProcessingResources();
            if (requestedQuality == PostProcessingQuality::Off)
            {
                std::cout << "[V10E_POST] requested=off active=off "
                             "fallback=0 reason=disabled passes=0\n";
                return true;
            }

            const bool forcedFallback = isTrueValue(
                std::getenv("HELLOMINE3D_V10E_POST_FALLBACK"));
            Ogre::RenderSystem* renderSystem =
                m_root != nullptr ? m_root->getRenderSystem() : nullptr;
            const Ogre::RenderSystemCapabilities* capabilities =
                renderSystem != nullptr
                    ? renderSystem->getCapabilities()
                    : nullptr;
            const bool supported =
                !forcedFallback && capabilities != nullptr &&
                capabilities->hasCapability(
                    Ogre::RSC_HWRENDER_TO_TEXTURE) &&
                capabilities->hasCapability(Ogre::RSC_VERTEX_PROGRAM) &&
                capabilities->hasCapability(Ogre::RSC_FRAGMENT_PROGRAM) &&
                Ogre::GpuProgramManager::getSingleton()
                    .isSyntaxSupported("glsl150") &&
                m_window != nullptr && m_window->getNumViewports() > 0;
            if (!supported)
            {
                std::cout << "[V10E_POST] requested=on active=off "
                             "fallback=1 reason="
                          << (forcedFallback
                                  ? "forced"
                                  : "unsupported-capability")
                          << " passes=0\n";
                return false;
            }

            try
            {
                Ogre::Viewport* viewport = m_window->getViewport(0);
                Ogre::CompositorInstance* instance =
                    Ogre::CompositorManager::getSingleton()
                        .addCompositor(
                            viewport, "HelloMine3D/PostProcess");
                if (instance == nullptr)
                {
                    throw std::runtime_error(
                        "compositor definition was not available");
                }
                m_postProcessingInstalled = true;
                Ogre::CompositorManager::getSingleton()
                    .setCompositorEnabled(
                        viewport, "HelloMine3D/PostProcess", true);
                m_postProcessingQuality = PostProcessingQuality::On;
                std::cout << "[V10E_POST] requested=on active=on "
                             "fallback=0 reason=supported passes=1\n";
                return true;
            }
            catch (const std::exception& exception)
            {
                destroyPostProcessingResources();
                std::cout << "[V10E_POST] requested=on active=off "
                             "fallback=1 reason=setup-failed passes=0 "
                             "detail="
                          << exception.what() << '\n';
                return false;
            }
            catch (...)
            {
                destroyPostProcessingResources();
                std::cout << "[V10E_POST] requested=on active=off "
                             "fallback=1 reason=setup-failed passes=0 "
                             "detail=unknown\n";
                return false;
            }
        }

        void selectAtmosphereMode()
        {
            const bool forcedFallback = isTrueValue(
                std::getenv("HELLOMINE3D_V10C_FALLBACK"));
            Ogre::RenderSystem* renderSystem =
                m_root != nullptr ? m_root->getRenderSystem() : nullptr;
            const Ogre::RenderSystemCapabilities* capabilities =
                renderSystem != nullptr
                    ? renderSystem->getCapabilities()
                    : nullptr;
            const bool programCapabilities =
                capabilities != nullptr &&
                capabilities->hasCapability(Ogre::RSC_VERTEX_PROGRAM) &&
                capabilities->hasCapability(Ogre::RSC_FRAGMENT_PROGRAM);
            const bool syntaxSupported =
                Ogre::GpuProgramManager::getSingleton()
                    .isSyntaxSupported("glsl150");
            m_v10cAtmosphereEnabled =
                !forcedFallback && programCapabilities && syntaxSupported;
            const char* reason = m_v10cAtmosphereEnabled
                ? "supported"
                : (forcedFallback ? "forced-fs2"
                                  : "unsupported-program-capability");
            std::cout << "[V10C_ATMOSPHERE] enabled="
                      << (m_v10cAtmosphereEnabled ? 1 : 0)
                      << " fallback="
                      << (m_v10cAtmosphereEnabled ? 0 : 1)
                      << " reason=" << reason << '\n';
        }

        void syncTerrainMaterialParameters()
        {
            const TerrainMaterialParameters &profile =
                runtimeTerrainMaterialProfile().parameters();
            const char *terrainMaterials[] = {
                "HelloMine3D/Terrain", "HelloMine3D/Transparent",
                "HelloMine3D/Flora"};
            for (const char *materialName : terrainMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant(
                    "atlasPixels",
                    static_cast<float>(profile.atlasPixels));
                parameters->setNamedConstant(
                    "tilePixels",
                    static_cast<float>(profile.tilePixels));
                parameters->setNamedConstant(
                    "tilesPerRow",
                    static_cast<float>(profile.tilesPerRow));
                parameters->setNamedConstant(
                    "colourSaturation", profile.colourSaturation);
                parameters->setNamedConstant(
                    "greenSuppression", profile.greenSuppression);
                parameters->setNamedConstant(
                    "greenRedShift", profile.greenRedShift);
                parameters->setNamedConstant(
                    "toneGamma", profile.toneGamma);
            }
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
            const Ogre::Vector3 fogSunwardColour(
                state.fogSunwardColour.r,
                state.fogSunwardColour.g,
                state.fogSunwardColour.b);
            const Ogre::Vector3 skyZenith(
                state.skyZenithColour.r, state.skyZenithColour.g,
                state.skyZenithColour.b);
            const Ogre::Vector3 skyHorizon(
                state.skyHorizonColour.r, state.skyHorizonColour.g,
                state.skyHorizonColour.b);
            const Ogre::Vector3 sunDirection(
                state.sunDirection.x, state.sunDirection.y,
                state.sunDirection.z);
            const Ogre::Vector3 sunColour(
                state.sunColour.r, state.sunColour.g,
                state.sunColour.b);
            const Ogre::Vector3 cloudLightColour(
                state.cloudLightColour.r, state.cloudLightColour.g,
                state.cloudLightColour.b);
            const Ogre::Vector3 cloudShadowColour(
                state.cloudShadowColour.r, state.cloudShadowColour.g,
                state.cloudShadowColour.b);
            const Ogre::Vector3 waterShallowColour(
                state.waterShallowColour.r, state.waterShallowColour.g,
                state.waterShallowColour.b);
            const Ogre::Vector3 waterDeepColour(
                state.waterDeepColour.r, state.waterDeepColour.g,
                state.waterDeepColour.b);
            const float directionalStrength =
                m_v10cAtmosphereEnabled
                    ? state.fogDirectionalStrength
                    : 0.f;
            const bool shadowActive =
                m_directionalShadowQuality !=
                DirectionalShadowQuality::Off;
            const float shadowStrength = shadowActive
                ? std::max(0.f, std::min(0.42f,
                      state.sunIntensity * 0.42f))
                : 0.f;
            if (m_directionalSunLight != nullptr)
            {
                m_directionalSunLight->setDirection(-sunDirection);
                m_directionalSunLight->setDiffuseColour(
                    Ogre::ColourValue(state.sunColour.r,
                                      state.sunColour.g,
                                      state.sunColour.b));
                m_directionalSunLight->setCastShadows(
                    shadowActive && state.sunIntensity > 0.02f);
            }
            syncDirectionalShadowMaterialParameters(shadowStrength);

            m_sceneManager->setFog(Ogre::FOG_EXP2, fog,
                                   state.fogDensity);
            if (m_camera != nullptr && m_camera->getViewport() != nullptr)
            {
                m_camera->getViewport()->setBackgroundColour(fog);
            }

            const char* terrainMaterials[] = {
                "HelloMine3D/Terrain", "HelloMine3D/Transparent",
                "HelloMine3D/Flora"};
            for (const char* materialName : terrainMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant(
                    "environmentLight", state.daylight);
                parameters->setNamedConstant("fogColour", fogVector);
                parameters->setNamedConstant(
                    "fogSunwardColour", fogSunwardColour);
                parameters->setNamedConstant(
                    "sunDirection", sunDirection);
                parameters->setNamedConstant(
                    "fogDirectionalStrength", directionalStrength);
                parameters->setNamedConstant(
                    "fogDensity", state.fogDensity);
            }

            Ogre::GpuProgramParametersSharedPtr waterParameters =
                materialPass("HelloMine3D/Water")
                    ->getFragmentProgramParameters();
            waterParameters->setNamedConstant(
                "environmentLight", state.daylight);
            waterParameters->setNamedConstant("fogColour", fogVector);
            waterParameters->setNamedConstant(
                "fogSunwardColour", fogSunwardColour);
            waterParameters->setNamedConstant(
                "fogDirectionalStrength", directionalStrength);
            waterParameters->setNamedConstant(
                "fogDensity", state.fogDensity);
            waterParameters->setNamedConstant(
                "skyZenithColour", skyZenith);
            waterParameters->setNamedConstant(
                "skyHorizonColour", skyHorizon);
            waterParameters->setNamedConstant(
                "sunDirection", sunDirection);
            waterParameters->setNamedConstant("sunColour", sunColour);
            waterParameters->setNamedConstant(
                "sunIntensity", state.sunIntensity);
            waterParameters->setNamedConstant(
                "waterShallowColour", waterShallowColour);
            waterParameters->setNamedConstant(
                "waterDeepColour", waterDeepColour);

            const char* actorMaterials[] = {
                "HelloMine3D/ActorMob", "HelloMine3D/ActorStalker",
                "HelloMine3D/ActorBrute", "HelloMine3D/ActorSpitter",
                "HelloMine3D/ActorItem",
                "HelloMine3D/CombatProjectile"};
            for (const char* materialName : actorMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant(
                    "environmentLight", state.daylight);
                parameters->setNamedConstant("fogColour", fogVector);
                parameters->setNamedConstant(
                    "fogSunwardColour", fogSunwardColour);
                parameters->setNamedConstant(
                    "sunDirection", sunDirection);
                parameters->setNamedConstant(
                    "fogDirectionalStrength", directionalStrength);
                parameters->setNamedConstant(
                    "fogDensity", state.fogDensity);
            }

            const auto syncSkyParameters =
                [&](Ogre::GpuProgramParametersSharedPtr parameters)
                {
                    parameters->setNamedConstant(
                        "skyZenithColour", skyZenith);
                    parameters->setNamedConstant(
                        "skyHorizonColour", skyHorizon);
                    parameters->setNamedConstant(
                        "sunDirection", sunDirection);
                    parameters->setNamedConstant(
                        "sunColour", sunColour);
                    parameters->setNamedConstant(
                        "sunIntensity", state.sunIntensity);
                    parameters->setNamedConstant(
                        "moonIntensity", state.moonIntensity);
                    parameters->setNamedConstant(
                        "starIntensity", state.starIntensity);
                    parameters->setNamedConstant(
                        "cloudLightColour", cloudLightColour);
                    parameters->setNamedConstant(
                        "cloudShadowColour", cloudShadowColour);
                    parameters->setNamedConstant(
                        "cloudCoverage", state.cloudCoverage);
                    parameters->setNamedConstant(
                        "fogSunwardColour", fogSunwardColour);
                    parameters->setNamedConstant(
                        "fogDirectionalStrength", directionalStrength);
                    parameters->setNamedConstant(
                        "cloudLayerEnabled",
                        m_v10cAtmosphereEnabled ? 1.f : 0.f);
                    parameters->setNamedConstant(
                        "cloudBaseHeight", state.cloudBaseHeight);
                    parameters->setNamedConstant(
                        "cloudThickness", state.cloudThickness);
                    parameters->setNamedConstant(
                        "cloudHorizontalScale",
                        state.cloudHorizontalScale);
                    parameters->setNamedConstant(
                        "cloudVelocity",
                        Ogre::Vector2(state.cloudVelocity.x,
                                      state.cloudVelocity.y));
                    parameters->setNamedConstant(
                        "cloudMaxDistance", state.cloudMaxDistance);
                };
            syncSkyParameters(
                materialPass(SkyboxMaterial)
                    ->getFragmentProgramParameters());

            // Without a cube texture Ogre builds the skybox from six cloned
            // plane materials. Update those live clones as well as the source
            // material so the procedural cycle reaches the actual draw calls.
            for (int face = 0; face < 6; ++face)
            {
                const Ogre::String materialName =
                    m_sceneManager->getName() + "SkyBoxPlane" +
                    Ogre::StringConverter::toString(face);
                syncSkyParameters(
                    materialPass(materialName)
                        ->getFragmentProgramParameters());
            }
        }

        bool keyPressed(const OIS::KeyEvent& event) override
        {
            if (m_userInterface != nullptr)
            {
                m_userInterface->keyEvent(event, true, *m_keyboard);
            }
            if (!m_focusGate.isFocused() || m_focusTransitionFrame)
            {
                return true;
            }
            if (event.key == OIS::KC_ESCAPE)
            {
                if (m_userInterface != nullptr &&
                    m_userInterface->hasBlockingModal())
                {
                    return true;
                }
                if (m_userInterface != nullptr &&
                    m_userInterface->dismissSettings())
                {
                    return true;
                }
                if (m_worldPlayer != nullptr &&
                    m_worldPlayer->hasOpenCrafting())
                {
                    m_worldPlayer->closeCrafting();
                    return true;
                }
                if (m_worldPlayer != nullptr &&
                    m_worldPlayer->hasOpenContainer())
                {
                    m_worldPlayer->closeContainer();
                    return true;
                }
                switch (m_applicationFlow.state())
                {
                    case GameApplicationState::Playing:
                        m_applicationFlow.pause();
                        break;
                    case GameApplicationState::Paused:
                        m_applicationFlow.resume();
                        break;
                    case GameApplicationState::WorldList:
                        m_applicationFlow.returnToMainMenu();
                        break;
                    case GameApplicationState::MainMenu:
                        m_shutdownRequested = true;
                        break;
                    case GameApplicationState::Loading:
                        break;
                }
                return true;
            }
            if (m_userInterface != nullptr &&
                m_userInterface->wantsKeyboardInput())
            {
                return true;
            }

            if (event.key == toOisKey(m_config.inputBindings.get(
                                 GameplayAction::ConsumeFood)))
            {
                m_useHeldFood = true;
                return true;
            }
            if (event.key == toOisKey(m_config.inputBindings.get(
                                 GameplayAction::OpenCrafting)))
            {
                if (m_worldPlayer != nullptr &&
                    !m_worldPlayer->hasOpenContainer())
                {
                    m_worldPlayer->openCrafting(
                        CraftingSession::PlayerGridSize);
                }
                return true;
            }

            switch (event.key)
            {
                case OIS::KC_F:
                    m_toggleFlying = true;
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
            if (!m_focusGate.isFocused() || m_focusTransitionFrame)
            {
                return true;
            }
            if (m_userInterface == nullptr ||
                (!m_userInterface->wantsMouseInput() &&
                 (m_worldPlayer == nullptr ||
                  !m_worldPlayer->hasOpenContainer())))
            {
                if (event.state.Z.rel > 0)
                {
                    m_hotbarDelta = -1;
                }
                else if (event.state.Z.rel < 0)
                {
                    m_hotbarDelta = 1;
                }
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
            refreshNativeCursorClip();
        }

        void windowMoved(Ogre::RenderWindow*) override
        {
            refreshNativeCursorClip();
        }

        void windowFocusChange(Ogre::RenderWindow*) override
        {
            syncInputFocus();
            updateNativeCursorCapture();
        }

        bool runtimeWindowFocused() const
        {
            if (m_hiddenWindow || m_window == nullptr)
            {
                return false;
            }
#if defined(_WIN32)
            return m_nativeWindowHandle != 0 &&
                   GetForegroundWindow() == reinterpret_cast<HWND>(
                       m_nativeWindowHandle);
#else
            return m_window->isActive();
#endif
        }

        void syncInputFocus()
        {
            const bool focused = runtimeWindowFocused();
            if (focused == m_focusGate.isFocused())
            {
                return;
            }
            m_focusGate.setFocused(focused);
            m_focusTransitionFrame = true;
            clearTransientInput();
            if (!focused)
            {
                releaseNativeCursorCapture();
            }
        }

        void windowClosed(Ogre::RenderWindow* window) override
        {
            if (window == m_window)
            {
                releaseNativeCursorCapture();
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
            const float viewPointScale =
                std::max(1.0f, m_window->getViewPointToPixelScale());
            const OIS::MouseState& state = m_mouse->getMouseState();
            state.width = static_cast<int>(
                static_cast<float>(width) / viewPointScale);
            state.height = static_cast<int>(
                static_cast<float>(height) / viewPointScale);
        }

        bool shouldCaptureNativeCursor() const
        {
            if (m_hiddenWindow || m_window == nullptr ||
                m_nativeWindowHandle == 0 || m_worldPlayer == nullptr ||
                m_applicationFlow.state() != GameApplicationState::Playing ||
                !m_mouseLookEnabled || m_worldPlayer->hasOpenContainer() ||
                m_worldPlayer->hasOpenCrafting() ||
                (m_userInterface != nullptr &&
                 m_userInterface->hasBlockingModal()))
            {
                return false;
            }
#if defined(_WIN32)
            return GetForegroundWindow() == reinterpret_cast<HWND>(
                m_nativeWindowHandle);
#else
            return m_window->isActive();
#endif
        }

        void refreshNativeCursorClip()
        {
#if defined(_WIN32)
            if (!m_nativeCursorCaptured || m_nativeWindowHandle == 0)
            {
                return;
            }
            const HWND handle = reinterpret_cast<HWND>(m_nativeWindowHandle);
            RECT client{};
            if (!GetClientRect(handle, &client))
            {
                return;
            }
            POINT upperLeft{client.left, client.top};
            POINT lowerRight{client.right, client.bottom};
            if (!ClientToScreen(handle, &upperLeft) ||
                !ClientToScreen(handle, &lowerRight))
            {
                return;
            }
            const RECT screenBounds{upperLeft.x, upperLeft.y,
                                    lowerRight.x, lowerRight.y};
            ClipCursor(&screenBounds);
#endif
        }

        void updateNativeCursorCapture()
        {
#if defined(_WIN32)
            const bool shouldCapture = shouldCaptureNativeCursor();
            if (shouldCapture == m_nativeCursorCaptured)
            {
                return;
            }
            if (!shouldCapture)
            {
                releaseNativeCursorCapture();
                return;
            }

            m_nativeCursorCaptured = true;
            refreshNativeCursorClip();
            do
            {
                ++m_cursorHideAdjustments;
            }
            while (ShowCursor(FALSE) >= 0 &&
                   m_cursorHideAdjustments < 16);
#endif
        }

        void releaseNativeCursorCapture()
        {
#if defined(_WIN32)
            if (!m_nativeCursorCaptured && m_cursorHideAdjustments == 0)
            {
                return;
            }
            ClipCursor(nullptr);
            while (m_cursorHideAdjustments > 0)
            {
                ShowCursor(TRUE);
                --m_cursorHideAdjustments;
            }
            m_nativeCursorCaptured = false;
#endif
        }

        void shutdown()
        {
            releaseNativeCursorCapture();
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
            destroyPostProcessingResources();
            m_blockOutline.reset();
            m_actorRenderer.reset();

            for (auto &entry : m_sectionVisuals)
            {
                destroySectionVisual(entry.second);
            }
            m_sectionVisuals.clear();
            m_sectionRenderStates.clear();
            destroyDirectionalShadowResources();
            if (m_audio != nullptr)
            {
                m_audio->detach();
            }
            if (m_music != nullptr)
            {
                m_music->stopImmediately();
            }
            m_world = nullptr;
            m_worldPlayer = nullptr;
            m_sandbox.reset();
            m_logicCamera.reset();
            m_music.reset();
            m_audio.reset();

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
        Ogre::Light* m_directionalSunLight = nullptr;
        Ogre::SceneNode* m_directionalSunNode = nullptr;
        OIS::InputManager* m_inputManager = nullptr;
        OIS::Keyboard* m_keyboard = nullptr;
        OIS::Mouse* m_mouse = nullptr;
        std::uintptr_t m_nativeWindowHandle = 0;
        std::unique_ptr<OgreRenderCapture> m_renderCapture;
        std::unique_ptr<OgreUserInterface> m_userInterface;
        std::unique_ptr<AudioRuntime> m_audio;
        std::unique_ptr<MusicRuntime> m_music;
        std::vector<PendingCrashReport> m_pendingCrashReports;
        std::string m_audioDefinitionError;
        std::string m_musicDefinitionError;
        GameApplicationFlow m_applicationFlow;
        std::unique_ptr<WorldManagementService> m_worldManagement;
        std::unique_ptr<OgreBlockOutline> m_blockOutline;
        std::unique_ptr<OgreActorRenderer> m_actorRenderer;
        Player* m_worldPlayer = nullptr;
        std::unique_ptr<::Camera> m_logicCamera;
        std::unique_ptr<SandboxRuntime> m_sandbox;
        World* m_world = nullptr;
        std::unordered_map<std::string, SectionVisual> m_sectionVisuals;
        std::unordered_map<std::string, ChunkRenderState>
            m_sectionRenderStates;
        bool m_listenersInstalled = false;
        bool m_shutdownRequested = false;
        bool m_runtimeStarted = false;
        std::string m_pendingWorldDirectory;
        int m_loadingRequestedFrame = -1;
        int m_exitAfterFrames = 0;
        int m_frameCount = 0;
        WorldDebugStats m_frameWorldStats;
        std::chrono::steady_clock::time_point m_frameStart;
        std::chrono::steady_clock::time_point m_updateEnd;
        glm::vec2 m_pendingLookDelta{0.0f};
        GameplayMovementModeTracker m_movementModeTracker;
        GameplayFocusGate m_focusGate;
        bool m_focusTransitionFrame = false;
        bool m_toggleFlying = false;
        bool m_resetMeshes = false;
        bool m_useHeldFood = false;
        bool m_mouseLookEnabled = true;
        bool m_hiddenWindow = false;
        bool m_v10cAtmosphereEnabled = true;
        bool m_directionalShadowDiagnosticsEmitted = false;
        float m_directionalShadowStrength = 0.f;
        DirectionalShadowQuality m_directionalShadowQuality =
            DirectionalShadowQuality::Off;
        PostProcessingQuality m_postProcessingQuality =
            PostProcessingQuality::Off;
        bool m_postProcessingInstalled = false;
        bool m_nativeCursorCaptured = false;
        int m_cursorHideAdjustments = 0;
        bool m_validationActorsSpawned = false;
        bool m_oreFixturePlaced = false;
        bool m_containerFixturePlaced = false;
        bool m_combatFixturePlaced = false;
        bool m_cropFixturePlaced = false;
        bool m_verticalSliceFixturePlaced = false;
        bool m_fastStreamingEnabled = false;
        bool m_fastStreamingPending = false;
        VectorXZ m_fastStreamingOrigin{0, 0};
        VectorXZ m_fastStreamingTarget{0, 0};
        std::chrono::steady_clock::time_point m_fastStreamingStarted;
        float m_rcPerformanceElapsedSeconds = 0.f;
        float m_nextFastStreamingMoveSeconds = 4.f;
        std::size_t m_fastStreamingMoveIndex = 0;
        int m_hotbarDelta = 0;
        int m_hotbarSlot = -1;
    };
}

int runOgreBootstrap(bool validateOnly,
                     std::vector<PendingCrashReport> crashReports)
{
    HELLOMINE3D_PROFILE_THREAD("Main Thread");
    std::cout << "[TRACY] enabled="
              << (RuntimeProfiler::isEnabled() ? 1 : 0);
    if (RuntimeProfiler::isEnabled())
    {
        std::cout << " on_demand=1 version=0.13.1";
    }
    std::cout << '\n';

    if (!validateOnly)
    {
        runtimeOperationTimings().begin(RuntimeOperationKind::Startup);
    }

    try
    {
        const std::string root = ResourcePaths::projectRoot();
        const std::vector<StartupResourceRequirement> startupResources =
            loadStartupResourceManifest(root);
        std::vector<ResourcePackRequirement> resourceRequirements;
        resourceRequirements.reserve(startupResources.size());
        for (const StartupResourceRequirement &resource : startupResources)
        {
            resourceRequirements.push_back(
                {resource.category, resource.relativePath});
        }
        runtimeResourcePackResolver().freezeFromEnvironment(
            root, resourceRequirements);
        validateStartupResources(root, startupResources);
        runtimeTerrainMaterialProfile().freezeFromResourceView(
            runtimeResourcePackResolver());
        validateAtmosphereShaderContract(
            runtimeResourcePackResolver());
        validateDirectionalShadowShaderContract(
            runtimeResourcePackResolver());
        validatePostProcessingShaderContract(
            runtimeResourcePackResolver());
        BlockDatabase::get();
        runtimeRecipeRegistry().freezeFromResourceView(
            runtimeResourcePackResolver());
        runtimeToolRegistry().freezeFromResourceView(
            runtimeResourcePackResolver());
        runtimeSmeltingRegistry().freezeFromResourceView(
            runtimeResourcePackResolver());
        runtimeFoodRegistry().freezeFromResourceView(
            runtimeResourcePackResolver());
        runtimeEnemyRegistry().freezeFromResourceView(
            runtimeResourcePackResolver());
        runtimeObjectiveRegistry().freezeFromResourceView(
            runtimeResourcePackResolver());
        runtimeLocalizedTextRegistry().freezeFromResourceView(
            runtimeResourcePackResolver());
        const char *manifestOutput =
            std::getenv("HELLOMINE3D_EFFECTIVE_MANIFEST_OUT");
        if (manifestOutput != nullptr && manifestOutput[0] != '\0')
        {
            std::ofstream output(manifestOutput,
                                 std::ios::binary | std::ios::trunc);
            if (!output)
            {
                throw std::runtime_error(
                    "Unable to write effective resource manifest to '" +
                    std::string(manifestOutput) + "'.");
            }
            output << runtimeResourcePackResolver().effectiveManifest();
        }
        std::cout << "[RESOURCE_PACK] enabled="
                  << runtimeResourcePackResolver().packs().size()
                  << " overrides="
                  << runtimeResourcePackResolver().overrideCount()
                  << " effective="
                  << runtimeResourcePackResolver()
                         .effectiveResources().size()
                  << '\n';
        const TerrainMaterialParameters &terrainMaterial =
            runtimeTerrainMaterialProfile().parameters();
        std::cout << "[TERRAIN_MATERIAL] frozen=1 version="
                  << TerrainMaterialParameters::ContractVersion
                  << " atlas=" << terrainMaterial.atlasTexture
                  << " atlas_pixels=" << terrainMaterial.atlasPixels
                  << " tile_pixels=" << terrainMaterial.tilePixels
                  << " tiles_per_row=" << terrainMaterial.tilesPerRow
                  << " colour_saturation="
                  << terrainMaterial.colourSaturation
                  << " green_suppression="
                  << terrainMaterial.greenSuppression
                  << " green_red_shift="
                  << terrainMaterial.greenRedShift
                  << " tone_gamma=" << terrainMaterial.toneGamma
                  << '\n';
        std::cout << "[RECIPE_REGISTRY] frozen=1 recipes="
                  << runtimeRecipeRegistry().recipes().size() << '\n';
        std::cout << "[TOOL_REGISTRY] frozen=1 tools="
                  << runtimeToolRegistry().tools().size() << '\n';
        std::cout << "[SMELTING_REGISTRY] frozen=1 recipes="
                  << runtimeSmeltingRegistry().recipes().size()
                  << " fuels="
                  << runtimeSmeltingRegistry().fuels().size() << '\n';
        std::cout << "[FOOD_REGISTRY] frozen=1 foods="
                  << runtimeFoodRegistry().foods().size() << '\n';
        std::cout << "[ENEMY_REGISTRY] frozen=1 enemies="
                  << runtimeEnemyRegistry().enemies().size()
                  << " natural="
                  << runtimeEnemyRegistry().naturalEnemies().size()
                  << '\n';
        std::cout << "[OBJECTIVE_REGISTRY] frozen=1 version="
                  << runtimeObjectiveRegistry().definitionVersion()
                  << " objectives="
                  << runtimeObjectiveRegistry().definitions().size()
                  << '\n';
        std::cout << "[TEXT_REGISTRY] frozen=1 locales="
                  << (runtimeLocalizedTextRegistry().hasLocale("en-US") ? 1 : 0) +
                         (runtimeLocalizedTextRegistry().hasLocale("zh-CN") ? 1 : 0)
                  << " keys="
                  << runtimeLocalizedTextRegistry().keys("en-US").size()
                  << '\n';
        runtimeOperationTimings().markLatestActive(
            RuntimeOperationKind::Startup);
        OgreBootstrap bootstrap(std::move(crashReports));
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
        runtimeOperationTimings().completeLatestActive(
            RuntimeOperationKind::WorldEntry, false);
        runtimeOperationTimings().completeLatestActive(
            RuntimeOperationKind::Startup, false);
        RuntimePerformanceCapture::shutdown();
        StartupErrorReporter::present(diagnostic, !validateOnly);
    }
    catch (const OIS::Exception& exception)
    {
        const std::string diagnostic =
            "OIS bootstrap failed: " + std::string(exception.eText);
        std::cerr << diagnostic << '\n';
        runtimeOperationTimings().completeLatestActive(
            RuntimeOperationKind::WorldEntry, false);
        runtimeOperationTimings().completeLatestActive(
            RuntimeOperationKind::Startup, false);
        RuntimePerformanceCapture::shutdown();
        StartupErrorReporter::present(diagnostic, !validateOnly);
    }
    catch (const std::exception& exception)
    {
        const std::string diagnostic =
            "Ogre bootstrap failed: " + std::string(exception.what());
        std::cerr << diagnostic << '\n';
        runtimeOperationTimings().completeLatestActive(
            RuntimeOperationKind::WorldEntry, false);
        runtimeOperationTimings().completeLatestActive(
            RuntimeOperationKind::Startup, false);
        RuntimePerformanceCapture::shutdown();
        StartupErrorReporter::present(diagnostic, !validateOnly);
    }
    return EXIT_FAILURE;
}
