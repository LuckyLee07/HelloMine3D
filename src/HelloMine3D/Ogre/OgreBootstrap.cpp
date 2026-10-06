#include "OgreBootstrap.h"
#include "OgreActorRenderer.h"
#include "OgrePlayerRenderer.h"
#include "OgreThirdPersonCameraRig.h"
#include "OgreCameraDiagnostics.h"
#include "OgreCaveBoundaryRenderer.h"
#include "HdrPipeline.h"
#include "PlanarWaterReflection.h"
#include "RenderLifecycleDiagnostics.h"
#include "ReferenceWorldEditDiagnostics.h"
#include "ReferenceResidencyDiagnostics.h"
#include "ReferenceSettingsRestartDiagnostics.h"
#include <GLSL/OgreGLSLShader.h>
#include "HdrShaderContract.h"
#include "../Actor/EnemyPresentationGallery.h"
#include "../Presentation/DirectionalShadowPresentation.h"
#include "ChunkSectionRenderable.h"
#include "OgreBlockFeedback.h"
#include "OgreRenderCapture.h"
#include "OgreUserInterface.h"
#include "MaterialIdentityCapture.h"
#include "FloraWindCapture.h"
#include "PauseNotificationCapture.h"
#include "ShoreEditCapture.h"
#include "../Presentation/LocalizedPresentation.h"
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
#include <OgreDepthBuffer.h>
#include <OgreGL3PlusPlugin.h>
#include <OgreGL3PlusPrerequisites.h>
#include <OgreWindowEventUtilities.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

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
#include "../Diagnostics/VisualCameraSweep.h"
#include "../Diagnostics/ReferenceVisualScene.h"
#include "../Gameplay/ObjectiveRegistry.h"
#include "../Item/FoodRegistry.h"
#include "../Item/RecipeRegistry.h"
#include "../Item/CraftingSession.h"
#include "../Item/ToolRegistry.h"
#include "../Item/SmeltingRegistry.h"
#include "../Player/Player.h"
#include "../Presentation/LocalizedTextRegistry.h"
#include "../Presentation/AdventureAudioPresentation.h"
#include "../Presentation/AdventureAudioWorldAdapter.h"
#include "../Presentation/PlayerAvatarPresentation.h"
#include "../Presentation/PlayerHandPresentation.h"
#include "../Presentation/ThirdPersonCameraPresentation.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../Sandbox/SandboxRuntime.h"
#include "../Util/ResourcePackResolver.h"
#include "../Util/ResourcePaths.h"
#include "../World/Chunk/Chunk.h"
#include "../World/Chunk/ChunkSection.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockData.h"
#include "../World/Block/ChestContainer.h"
#include "../World/Block/CrusherContainer.h"
#include "../World/Block/FurnaceContainer.h"
#include "../World/Block/TerrainMaterialProfile.h"
#include "../World/Block/TerrainTextureArray.h"
#include "../World/Block/ReferenceSurfaceProfile.h"
#include "../World/Environment/AtmosphereShaderContract.h"
#include "../World/Environment/RegionalAtmosphere.h"
#include "../World/Generation/Terrain/TerrainGenerator.h"
#include "../World/World.h"
#include "../World/Command/PlayerBlockInteractionCommand.h"
#include "../World/Storage/WorldManagementService.h"

namespace
{
    constexpr const char* ConfigFileName = "Mine.cfg";
    constexpr const char* LogFileName = "MineOgre.log";
    constexpr const char* WindowTitle = "HelloMine3D";
    constexpr const char* SkyboxMaterial = "HelloMine3D/Skybox";

    class TerrainArrayLoader final : public Ogre::ManualResourceLoader
    {
      public:
        explicit TerrainArrayLoader(TerrainTextureArray payload)
            : data(std::move(payload)) {}

        void loadResource(Ogre::Resource *resource) override
        {
            auto &texture = *static_cast<Ogre::Texture *>(resource);
            texture.createInternalResources();
            std::size_t offset = 0;
            for (unsigned mip = 0; mip < data.mipCount; ++mip)
            {
                const unsigned edge = data.edge >> mip;
                Ogre::PixelBox pixels(edge, edge, data.layers, Ogre::PF_BYTE_RGBA,
                                      data.rgba.data() + offset);
                texture.getBuffer(0, mip)->blitFromMemory(pixels);
                offset += std::size_t(edge) * edge * data.layers * 4u;
            }
        }

        TerrainTextureArray data;
    };

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

    void setDiagnosticEnvironment(const char* name, const std::string& value)
    {
#if defined(_WIN32)
        if (_putenv_s(name, value.c_str()) != 0)
#else
        if (setenv(name, value.c_str(), 1) != 0)
#endif
        {
            throw std::runtime_error(std::string("Cannot set diagnostic ") + name);
        }
    }

    struct E2BatchPhase
    {
        std::string name;
        int terrainVersion = 0;
        std::string scene;
        std::string position;
        std::string rotation;
        bool streaming = false;
        std::string saveDirectory;
        std::string outputDirectory;
        double warmupMs = 0.0;
        double durationMs = 0.0;
    };

    std::vector<E2BatchPhase> readE2BatchManifest(const std::string& path)
    {
        std::ifstream input(path);
        std::string line;
        if (!input || !std::getline(input, line) || line != "E2_BATCH_V1")
        {
            throw std::runtime_error("Invalid E2 batch manifest header");
        }
        std::vector<E2BatchPhase> phases;
        std::unordered_set<std::string> names;
        while (std::getline(input, line))
        {
            if (line.empty())
            {
                continue;
            }
            std::vector<std::string> fields;
            std::size_t start = 0;
            for (;;)
            {
                const std::size_t end = line.find('\t', start);
                fields.push_back(line.substr(start, end - start));
                if (end == std::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            if (fields.size() != 10)
            {
                throw std::runtime_error("E2 batch phase requires ten fields");
            }
            E2BatchPhase phase;
            phase.name = fields[0];
            phase.terrainVersion = std::stoi(fields[1]);
            phase.scene = fields[2];
            phase.position = fields[3];
            phase.rotation = fields[4];
            phase.streaming = fields[5] == "1";
            phase.saveDirectory = fields[6];
            phase.outputDirectory = fields[7];
            phase.warmupMs = std::stod(fields[8]);
            phase.durationMs = std::stod(fields[9]);
            if (phase.name.empty() || !names.insert(phase.name).second ||
                phase.terrainVersion < ForestEcologyTerrainGenerationVersion ||
                phase.terrainVersion > CurrentTerrainGenerationVersion ||
                (phase.scene != "forest" && phase.scene != "shore" &&
                 phase.scene != "meadow" && phase.scene != "relief") ||
                (fields[5] != "0" && fields[5] != "1") ||
                phase.saveDirectory.empty() || phase.outputDirectory.empty() ||
                !std::isfinite(phase.warmupMs) ||
                !std::isfinite(phase.durationMs) ||
                phase.warmupMs <= 0.0 || phase.durationMs <= 0.0)
            {
                throw std::runtime_error("Invalid E2 batch phase values");
            }
            for (const std::string* coordinates :
                 {&phase.position, &phase.rotation})
            {
                std::istringstream values(*coordinates);
                double x = 0.0, y = 0.0, z = 0.0;
                std::string trailing;
                if (!(values >> x >> y >> z) || (values >> trailing) ||
                    !std::isfinite(x) || !std::isfinite(y) ||
                    !std::isfinite(z))
                {
                    throw std::runtime_error("Invalid E2 batch coordinates");
                }
            }
            phases.push_back(std::move(phase));
            if (phases.size() > 48)
            {
                throw std::runtime_error("E2 batch phase limit exceeded");
            }
        }
        if (phases.empty())
        {
            throw std::runtime_error("E2 batch manifest is empty");
        }
        return phases;
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
        glm::ivec3 location{0};
        std::unique_ptr<ChunkMeshCollection> batchMeshes;
        // Optional diagnostic copy from the same safe CPU upload input; normal null.
        std::unique_ptr<ChunkMesh> fernDiagnosticFlora;
        // Only the isolated shore observer retains direct upload inputs.
        std::unique_ptr<ChunkMesh> shoreDiagnosticWater, shoreDiagnosticSolid;
        std::uint64_t shoreUploadSerial = 0;
    };

    // Own only the solar light's projection, leaving Ogre's other lights alone.
    class StableSolarShadowCamera final : public Ogre::ShadowCameraSetup
    {
        void getShadowCamera(const Ogre::SceneManager* scene,
                             const Ogre::Camera* camera, const Ogre::Viewport*,
                             const Ogre::Light* light, Ogre::Camera* shadow,
                             size_t) const override
        {
            const float distance = light->getShadowFarDistance();
            const Ogre::Vector3 target = camera->getDerivedPosition() +
                camera->getDerivedDirection() *
                    (distance * scene->getShadowDirLightTextureOffset());
            const Ogre::Vector3 sun = -light->getDerivedDirection();
            const auto frame = DirectionalShadowPresentation::cameraFrame(
                {target.x, target.y, target.z}, {sun.x, sun.y, sun.z},
                distance, shadow->getViewport()->getActualWidth());
            auto vector = [](const glm::vec3& value) {
                return Ogre::Vector3(value.x, value.y, value.z);
            };
            Ogre::Quaternion orientation;
            orientation.FromAxes(vector(frame.right), vector(frame.up), vector(frame.back));
            shadow->setCustomViewMatrix(false);
            shadow->setCustomProjectionMatrix(false);
            shadow->setProjectionType(Ogre::PT_ORTHOGRAPHIC);
            shadow->setOrthoWindow(distance * 2.f, distance * 2.f);
            shadow->setNearClipDistance(light->_deriveShadowNearClipDistance(camera));
            shadow->setFarClipDistance(light->_deriveShadowFarClipDistance(camera));
            shadow->setPosition(vector(frame.position));
            shadow->setOrientation(orientation);
        }
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
            return {2048, 96.f, 72.f, 0.002f};
        }
        if (quality == DirectionalShadowQuality::Medium)
        {
            return {1024, 64.f, 48.f, 0.004f};
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
            m_referenceRestartOutput=ReferenceSettingsRestartObservation::validateConfig(m_config);
            if (m_config.renderPipeline == RenderPipeline::LinearHdr)
                validateHdrSceneShaderContract(runtimeResourcePackResolver());
            const auto lifecycleDirectory = RenderLifecycleProbe::validateEnvironment(
                m_config.renderPipeline == RenderPipeline::LinearHdr,
                m_config.visualDetail == VisualDetail::Standard, m_config.isFullscreen,
                unsigned(m_config.windowX), unsigned(m_config.windowY));
            if (!lifecycleDirectory.empty()) {
                m_lifecycleProbe=std::make_unique<RenderLifecycleProbe>(lifecycleDirectory);
                m_lifecycleWorldDirectory=std::getenv("HELLOMINE3D_SAVE_DIR");
            }
            m_referenceEditOutput=ReferenceWorldEditProbe::validateEnvironment(
                m_config.renderPipeline==RenderPipeline::LinearHdr,
                m_config.visualDetail==VisualDetail::Standard,m_config.isFullscreen,
                unsigned(m_config.windowX),unsigned(m_config.windowY),unsigned(m_config.renderDistance),
                m_config.directionalShadowQuality==DirectionalShadowQuality::Medium);
            m_referenceResidencyOutput=ReferenceResidencyProbe::validateEnvironment(
                m_config.renderPipeline==RenderPipeline::LinearHdr,
                m_config.visualDetail==VisualDetail::Standard,m_config.isFullscreen,
                unsigned(m_config.windowX),unsigned(m_config.windowY),unsigned(m_config.renderDistance),
                m_config.directionalShadowQuality==DirectionalShadowQuality::Medium);
            initializeAudio();
            initializeMusic();
            createRoot();
            configureResources();
            configureRenderSystem();
            const char* catalogueOverride =
                std::getenv("HELLOMINE3D_CATALOGUE_DIR");
            const char* saveOverride = std::getenv("HELLOMINE3D_SAVE_DIR");
            m_referenceVisualRequested = isTrueValue(
                std::getenv("HELLOMINE3D_REFERENCE_VISUAL_SCENE"));
            if (m_referenceVisualRequested)
            {
                // Scene edits are allowed only once, inside an explicit fresh
                // save. Reject before catalogue/Sandbox construction can write.
                if (!saveOverride || !saveOverride[0] ||
                    !catalogueOverride || !catalogueOverride[0])
                    throw std::runtime_error("Reference visual scene requires explicit fresh save and catalogue directories.");
                const auto save = std::filesystem::weakly_canonical(saveOverride);
                const auto catalogue = std::filesystem::weakly_canonical(catalogueOverride);
                const auto fresh = [](const std::filesystem::path& path) {
                    return !std::filesystem::exists(path) ||
                        (std::filesystem::is_directory(path) && std::filesystem::is_empty(path));
                };
                const auto contains = [](const std::filesystem::path& parent,
                                         const std::filesystem::path& child) {
                    const auto relative = child.lexically_relative(parent);
                    return !relative.empty() && *relative.begin() != "..";
                };
                if (!fresh(save) || !fresh(catalogue) ||
                    contains(save, catalogue) || contains(catalogue, save))
                    throw std::runtime_error("Reference visual scene save/catalogue must be fresh, separate directories; saved worlds are never edited.");
            }
            // Reject existing/shared paths and all other diagnostics before
            // WorldManagementService or any actual World can create files.
            m_pauseNotificationOutput = PauseNotificationCapture::validateEnvironment(userSettings(m_config));
            const char* cameraDiagnosticDirectory = std::getenv(
                "HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR");
            if (cameraDiagnosticDirectory != nullptr && cameraDiagnosticDirectory[0] != '\0')
            {
                if (saveOverride == nullptr || saveOverride[0] == '\0' ||
                    catalogueOverride == nullptr || catalogueOverride[0] == '\0' ||
                    std::filesystem::exists(saveOverride) ||
                    std::filesystem::exists(catalogueOverride) ||
                    std::filesystem::exists(cameraDiagnosticDirectory))
                    throw std::runtime_error("Camera diagnostics require fresh save, catalogue and output directories.");
                const auto parent = std::filesystem::weakly_canonical(
                    std::filesystem::path(cameraDiagnosticDirectory).parent_path());
                if (parent != std::filesystem::weakly_canonical(
                        std::filesystem::path(saveOverride).parent_path()) ||
                    parent != std::filesystem::weakly_canonical(
                        std::filesystem::path(catalogueOverride).parent_path()))
                    throw std::runtime_error("Camera diagnostic directories must share the new session directory.");
            }
            const char* fernDirectory = std::getenv("HELLOMINE3D_FERN_WIND_CAPTURE_DIR");
            if (fernDirectory != nullptr && fernDirectory[0] != '\0')
            {
                if (saveOverride == nullptr || saveOverride[0] == '\0' ||
                    catalogueOverride == nullptr || catalogueOverride[0] == '\0' ||
                    std::filesystem::exists(saveOverride) ||
                    std::filesystem::exists(catalogueOverride) ||
                    std::filesystem::exists(fernDirectory))
                    throw std::runtime_error("Fern wind capture requires fresh save, catalogue and output directories.");
                const auto parent = std::filesystem::weakly_canonical(
                    std::filesystem::path(fernDirectory).parent_path());
                if (parent != std::filesystem::weakly_canonical(std::filesystem::path(saveOverride).parent_path()) ||
                    parent != std::filesystem::weakly_canonical(std::filesystem::path(catalogueOverride).parent_path()))
                    throw std::runtime_error("Fern diagnostic directories must share the new session directory.");
            }
            const char* shoreDirectory = std::getenv("HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR");
            if (shoreDirectory && shoreDirectory[0])
            {
                if (!saveOverride || !saveOverride[0] || !catalogueOverride || !catalogueOverride[0])
                    throw std::runtime_error("Shore edit requires explicit fresh save, catalogue and output paths.");
                const auto save = std::filesystem::weakly_canonical(saveOverride);
                const auto catalogue = std::filesystem::weakly_canonical(catalogueOverride);
                const auto output = std::filesystem::weakly_canonical(shoreDirectory);
                if (save == catalogue || save == output || catalogue == output ||
                    save.parent_path() != output.parent_path() || catalogue.parent_path() != output.parent_path() ||
                    std::filesystem::exists(save) || std::filesystem::exists(catalogue) || std::filesystem::exists(output))
                    throw std::runtime_error("Shore edit paths must be distinct, nonexistent siblings in the new session directory.");
            }
            m_worldManagement =
                std::make_unique<WorldManagementService>(
                    catalogueOverride != nullptr &&
                            catalogueOverride[0] != '\0'
                        ? catalogueOverride
                        : ResourcePaths::bin("saves"));
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
                    "media/fonts/HelloMineUI-Medium.ttf"),
                [this]() {
                    if (m_audio != nullptr)
                    {
                        m_audio->emitUiClick();
                    }
                }, std::move(m_pendingCrashReports));
            m_userInterface->setRenderPipelineFallback(m_hdrPipeline->fallback());
            if (!m_pauseNotificationOutput.empty())
            {
                m_pauseNotificationCapture = std::make_unique<PauseNotificationCapture>(
                    m_pauseNotificationOutput, m_config.locale);
                m_userInterface->setPauseNotificationCapture(m_pauseNotificationCapture.get());
                std::cout << "[PAUSE_NOTIFICATIONS] normal_input=0 fresh_isolated_world=1 maximum_frames=12 timeout_seconds=45\n";
            }
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

            configureE2BatchCapture();

            m_root->addFrameListener(this);
            Ogre::WindowEventUtilities::addWindowEventListener(m_window, this);
            m_listenersInstalled = true;
            try {
                m_root->startRendering();
                if(m_referenceEditProbe) {
                    ReferenceEdit::require(m_world!=nullptr,"World disappeared before completion");
                    m_referenceEditProbe->finish(*m_world);
                }
                if(m_referenceResidencyProbe) {
                    ReferenceResidency::require(m_referenceResidencyProbe->complete() && m_world,"residency phases incomplete");
                    ReferenceResidency::require(m_world->save(),"normal World save failed");
                    m_referenceResidencyProbe->event("normal-save",ReferenceResidency::object({{"saved","true"},{"cells",referenceResidencyCells()}}));
                    ReferenceResidency::require(clearActiveWorld(true),"normal clear/save/join failed");
                    m_lifecycleCloseReturned=true;
                    m_referenceResidencyProbe->worldDestroyed();
                    ReferenceResidency::require(!m_world && !m_sandbox && !m_logicCamera && !m_worldPlayer && m_sectionVisuals.empty() && m_terrainBatchVisuals.empty() && m_dirtyTerrainBatches.empty() && m_sectionRenderStates.empty() && m_lastLiveSections.empty() && m_localLights.count==0,"normal clear retained world/cache/light");
                    m_referenceResidencyProbe->event("world-cleared",lifecycleSnapshot());
                    m_referenceResidencyProbe->detachDraws();
                    shutdown();m_referenceResidencyProbe->finish();m_referenceResidencyProbe.reset();
                }
                if(m_lifecycleProbe) {
                    if(m_lifecycleProbe->stage()!=11) throw std::runtime_error("Lifecycle rendering ended before all resize/world cycles.");
                    shutdown();
                    m_lifecycleProbe->finish();
                }
            } catch(const std::exception& error) {
                if(m_referenceEditProbe)m_referenceEditProbe->fail(error.what(),m_world);
                if(m_referenceResidencyProbe){std::string facts="null";try{facts=referenceResidencySnapshot(false);}catch(...){}m_referenceResidencyProbe->fail(error.what(),facts);}
                if(m_lifecycleProbe) {
                    // Retain actual post-operation facts, including Manager
                    // names, when a strict lifecycle gate rejects the state.
                    std::string snapshot="null";
                    try {snapshot=lifecycleSnapshot();} catch(...) {}
                    m_lifecycleProbe->fail(error.what(),snapshot);
                }
                throw;
            }
            runtimeOperationTimings().completeLatestActive(
                RuntimeOperationKind::WorldEntry, m_frameCount > 0);
            runtimeOperationTimings().completeLatestActive(
                RuntimeOperationKind::Startup, m_frameCount > 0);
            return EXIT_SUCCESS;
        }

      private:
        void logE2BatchEvent(const char* event)
        {
            if (!m_e2BatchEvents || m_e2BatchIndex >= m_e2BatchPhases.size())
            {
                return;
            }
            const auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
            const E2BatchPhase& phase = m_e2BatchPhases[m_e2BatchIndex];
            m_e2BatchEvents << phase.name << '\t' << m_e2BatchIndex
                            << '\t' << event << '\t' << milliseconds
                            << '\t'
#if defined(_WIN32)
                            << _getpid()
#else
                            << getpid()
#endif
                            << '\t' << phase.terrainVersion << '\t'
                            << phase.scene << '\n' << std::flush;
        }

        void configureE2BatchCapture()
        {
            const char* manifest =
                std::getenv("HELLOMINE3D_E2_BATCH_MANIFEST");
            if (manifest == nullptr || manifest[0] == '\0')
            {
                return;
            }
            m_e2BatchPhases = readE2BatchManifest(manifest);
            const E2BatchPhase& first = m_e2BatchPhases.front();
            const char* save = std::getenv("HELLOMINE3D_SAVE_DIR");
            const char* performance = std::getenv("HELLO_PERF_CAPTURE_DIR");
            const char* frames = std::getenv("HELLO_RENDER_CAPTURE_DIR");
            const char* events = std::getenv("HELLOMINE3D_E2_BATCH_EVENTS");
            const char* position =
                std::getenv("HELLOMINE3D_PLAYER_POSITION");
            const char* rotation =
                std::getenv("HELLOMINE3D_PLAYER_ROTATION");
            const char* seed = std::getenv("HELLOMINE3D_SEED");
            const char* worldTime = std::getenv("HELLOMINE3D_WORLD_TIME");
            const char* profile =
                std::getenv("HELLOMINE3D_RC_PERF_PROFILE");
            if (m_world == nullptr || m_renderCapture == nullptr ||
                !m_renderCapture->isEnabled() ||
                !RuntimePerformanceCapture::isEnabled() ||
                save == nullptr || first.saveDirectory != save ||
                performance == nullptr ||
                first.outputDirectory + "/performance" != performance ||
                frames == nullptr ||
                first.outputDirectory + "/frames" != frames ||
                position == nullptr || first.position != position ||
                rotation == nullptr || first.rotation != rotation ||
                seed == nullptr || std::string(seed) != "20260807" ||
                worldTime == nullptr || std::string(worldTime) != "6000" ||
                profile == nullptr ||
                std::string(profile) !=
                    (first.streaming ? "fast-streaming" : "") ||
                events == nullptr || events[0] == '\0' ||
                std::ifstream(events).good() ||
                isTrueValue(std::getenv("HELLO_PERF_CAPTURE_EXIT")) ||
                isTrueValue(std::getenv("HELLO_RENDER_CAPTURE_EXIT")))
            {
                throw std::runtime_error("E2 batch launch configuration differs");
            }
            m_e2BatchEvents.open(events, std::ios::out | std::ios::trunc);
            if (!m_e2BatchEvents)
            {
                throw std::runtime_error("Cannot open E2 batch events");
            }
            m_e2BatchEvents
                << "run\tindex\tevent\tunix_ms\tpid\tversion\tscene\n";
            m_e2BatchEnabled = true;
            m_renderPhaseDiagnostics = isTrueValue(
                std::getenv("HELLOMINE3D_E2_RENDER_PHASES"));
            logE2BatchEvent("started");
            std::cout << "[E2_BATCH] phases=" << m_e2BatchPhases.size()
                      << " manifest=" << manifest << '\n';
        }

        void advanceE2BatchCapture()
        {
            const E2BatchPhase& current = m_e2BatchPhases[m_e2BatchIndex];
            if (m_renderCapture == nullptr ||
                !m_renderCapture->isComplete() ||
                m_frameWorldStats.terrainSeed != 20260807 ||
                m_frameWorldStats.terrainGenerationVersion !=
                    current.terrainVersion)
            {
                throw std::runtime_error("E2 batch phase evidence differs");
            }
            logE2BatchEvent("completed");
            if (!clearActiveWorld())
            {
                throw std::runtime_error("E2 batch world save failed");
            }
            if (++m_e2BatchIndex == m_e2BatchPhases.size())
            {
                m_shutdownRequested = true;
                return;
            }

            const E2BatchPhase& next = m_e2BatchPhases[m_e2BatchIndex];
            setDiagnosticEnvironment("HELLOMINE3D_SAVE_DIR",
                                     next.saveDirectory);
            setDiagnosticEnvironment("HELLOMINE3D_SEED", "20260807");
            setDiagnosticEnvironment("HELLOMINE3D_PLAYER_POSITION",
                                     next.position);
            setDiagnosticEnvironment("HELLOMINE3D_PLAYER_ROTATION",
                                     next.rotation);
            setDiagnosticEnvironment("HELLOMINE3D_WORLD_TIME", "6000");
            setDiagnosticEnvironment("HELLOMINE3D_RC_PERF_PROFILE",
                                     next.streaming ? "fast-streaming" : "");
            setDiagnosticEnvironment("HELLO_RENDER_CAPTURE_DIR",
                                     next.outputDirectory + "/frames");
            setDiagnosticEnvironment("HELLO_PERF_CAPTURE_DIR",
                                     next.outputDirectory + "/performance");

            m_fastStreamingEnabled = false;
            m_fastStreamingPending = false;
            m_fastStreamingMoveIndex = 0;
            m_rcPerformanceElapsedSeconds = 0.f;
            m_nextFastStreamingMoveSeconds = 4.f;
            buildTerrain(true, next.saveDirectory);
            if (!configureDirectionalShadows(
                    m_config.directionalShadowQuality))
            {
                throw std::runtime_error("E2 batch shadow setup failed");
            }
            syncActorVisuals();
            m_userInterface->setWorldContext(m_worldPlayer, m_world);
            RuntimePerformanceCapture::startDiagnosticSegment(
                next.outputDirectory + "/performance",
                next.warmupMs, next.durationMs);
            m_renderCapture =
                std::make_unique<OgreRenderCapture>(*m_window);
            m_e2BatchEntryPending = true;
            logE2BatchEvent("started");
        }

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
            const bool requestedMsaa4=HdrPipeline::preferMsaa4(m_config.renderPipeline);
            const auto& rendererOptions=selected->getConfigOptions();
            const auto sampleOption=rendererOptions.find("FSAA");
            const bool fourAdvertised=sampleOption!=rendererOptions.end() &&
                std::find(sampleOption->second.possibleValues.begin(),sampleOption->second.possibleValues.end(),"4")!=
                    sampleOption->second.possibleValues.end();
            const bool fourAvailable=requestedMsaa4 && fourAdvertised && HdrPipeline::msaa4WindowSupported();
            setOptionIfAvailable(*selected,"FSAA",fourAvailable?"4":"0");
            if(requestedMsaa4) std::cout<<"[HDR_MSAA4_WINDOW] requested=4 selected="<<(fourAvailable?4:0)
                <<" configuration_only=1 reason="<<(fourAvailable?"native-pixel-format-supported":"window-format-unavailable")<<'\n';
            // Both pipelines output display encoded RGB explicitly. HUD shares
            // that window, so hardware gamma must not encode either one again.
            setOptionIfAvailable(*selected, "sRGB Gamma Conversion", "No");
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
            m_visualCameraSweep = VisualCameraSweep::parse(
                std::getenv("HELLOMINE3D_VISUAL_CAMERA_SWEEP"),
                isTrueValue(std::getenv("HELLOMINE3D_WINDOW_HIDDEN")) &&
                    isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")),
                !initialSaveDirectory.empty(),
                RuntimePerformanceCapture::isEnabled() ||
                    std::getenv("HELLOMINE3D_RC_PERF_PROFILE") != nullptr ||
                    std::getenv("HELLOMINE3D_E2_BATCH_MANIFEST") != nullptr,
                std::getenv("HELLOMINE3D_VISUAL_CAMERA_PATH"));
            if (m_visualCameraSweep.enabled)
                std::cout << "[VISUAL_CAMERA_SWEEP] enabled=1 evidence=developer-diagnostic normal_input=0 player_unchanged=1\n";
            if (const char* fixture = std::getenv("HELLOMINE3D_PLAYER_MOTION_CAPTURE")) {
                m_playerMotionCapture = fixture;
                if (!isTrueValue(std::getenv("HELLOMINE3D_WINDOW_HIDDEN")) ||
                    !isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) ||
                    initialSaveDirectory.empty() || RuntimePerformanceCapture::isEnabled() ||
                    std::getenv("HELLOMINE3D_RC_PERF_PROFILE") != nullptr ||
                    std::getenv("HELLOMINE3D_E2_BATCH_MANIFEST") != nullptr ||
                    m_visualCameraSweep.enabled ||
                    (m_playerMotionCapture != "forward" && m_playerMotionCapture != "backward" &&
                     m_playerMotionCapture != "left" && m_playerMotionCapture != "right"))
                    throw std::runtime_error("Player motion fixture requires hidden world render capture without performance/camera fixtures and a valid direction.");
                std::cout << "[PLAYER_MOTION_CAPTURE] direction=" << m_playerMotionCapture
                    << " evidence=developer-diagnostic normal_input=0 player_unchanged=1\n";
            }
            if (const char* fixture = std::getenv("HELLOMINE3D_ACTOR_VISUAL_CAPTURE")) {
                m_actorVisualCapture = fixture;
                if ((!isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) &&
                     !RuntimePerformanceCapture::isEnabled()) ||
                    (m_actorVisualCapture != "idle" && m_actorVisualCapture != "windup" &&
                     m_actorVisualCapture != "recover" && m_actorVisualCapture != "walk" &&
                     m_actorVisualCapture != "cycle" && m_actorVisualCapture != "projectiles" &&
                     m_actorVisualCapture != "projectile-flight" &&
                     m_actorVisualCapture != "wildlife-cycle"))
                    throw std::runtime_error("Actor visual fixture requires diagnostic capture and a valid pose.");
                std::cout << "[ACTOR_VISUAL_CAPTURE] pose=" << m_actorVisualCapture
                    << " evidence=developer-diagnostic normal_input=0\n";
            }
            if (const char* distance = std::getenv("HELLOMINE3D_ACTOR_VISUAL_DISTANCE")) {
                const std::string value(distance);
                if (m_actorVisualCapture.empty() ||
                    (value != "6" && value != "12" && value != "24"))
                    throw std::runtime_error("Actor visual distance requires a diagnostic gallery and 6, 12 or 24 metres.");
                m_actorVisualDistance = std::stof(value);
                std::cout << "[ACTOR_VISUAL_CAPTURE] distance=" << value << '\n';
            }
            const char* identityOutput = std::getenv(
                "HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR");
            if (identityOutput != nullptr && identityOutput[0] != '\0')
            {
                if (!isTrueValue(std::getenv("HELLOMINE3D_WINDOW_HIDDEN")) ||
                    !isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) ||
                    initialSaveDirectory.empty() ||
                    std::getenv("HELLOMINE3D_SAVE_DIR") == nullptr ||
                    std::getenv("HELLOMINE3D_CATALOGUE_DIR") == nullptr ||
                    RuntimePerformanceCapture::isEnabled() ||
                    std::getenv("HELLOMINE3D_RC_PERF_PROFILE") != nullptr ||
                    std::getenv("HELLOMINE3D_E2_BATCH_MANIFEST") != nullptr ||
                    m_visualCameraSweep.enabled || !m_playerMotionCapture.empty() ||
                    !m_actorVisualCapture.empty() ||
                    isTrueValue(std::getenv("HELLOMINE3D_HUD_FIXTURE")) ||
                    std::getenv("HELLOMINE3D_HUD_PAGE_FIXTURE") != nullptr ||
                    runtimeTerrainMaterialProfile().parameters().formatVersion != 2)
                    throw std::runtime_error("Material identity capture requires a hidden isolated v2 world capture without other fixtures.");
                for (const char* name : {"HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE",
                         "HELLOMINE3D_COMBAT_FIXTURE", "HELLOMINE3D_CONTAINER_FIXTURE",
                         "HELLOMINE3D_CRAFTING_FIXTURE", "HELLOMINE3D_CROP_FIXTURE",
                         "HELLOMINE3D_MACHINE_FIXTURE", "HELLOMINE3D_ORE_FIXTURE",
                         "HELLOMINE3D_SPAWN_VALIDATION_ACTORS", "HELLOMINE3D_TRANSPARENT_FIXTURE",
                         "HELLOMINE3D_VERTEX_LIGHTING_FIXTURE", "HELLOMINE3D_VERTICAL_SLICE_FIXTURE"})
                    if (std::getenv(name) != nullptr)
                        throw std::runtime_error(std::string("Material identity capture cannot combine fixture ") + name);
                m_materialIdentityOutput = identityOutput;
                std::cout << "[MATERIAL_IDENTITY_CAPTURE] evidence=developer-diagnostic normal_input=0 fixture=resident-query-columns simulation_delta=0\n";
            }
            const char* cameraOutput = std::getenv(
                "HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR");
            if (cameraOutput != nullptr && cameraOutput[0] != '\0')
            {
                if (!isTrueValue(std::getenv("HELLOMINE3D_WINDOW_HIDDEN")) ||
                    !isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) ||
                    initialSaveDirectory.empty() ||
                    std::getenv("HELLOMINE3D_SAVE_DIR") == nullptr ||
                    std::getenv("HELLOMINE3D_CATALOGUE_DIR") == nullptr ||
                    RuntimePerformanceCapture::isEnabled() ||
                    std::getenv("HELLOMINE3D_RC_PERF_PROFILE") != nullptr ||
                    std::getenv("HELLOMINE3D_E2_BATCH_MANIFEST") != nullptr ||
                    m_visualCameraSweep.enabled || !m_playerMotionCapture.empty() ||
                    !m_actorVisualCapture.empty() || !m_materialIdentityOutput.empty())
                    throw std::runtime_error("Camera diagnostics require a hidden isolated world without other diagnostics.");
                for (const char* name : {"HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE",
                         "HELLOMINE3D_COMBAT_FIXTURE", "HELLOMINE3D_CONTAINER_FIXTURE",
                         "HELLOMINE3D_CRAFTING_FIXTURE", "HELLOMINE3D_CROP_FIXTURE",
                         "HELLOMINE3D_MACHINE_FIXTURE", "HELLOMINE3D_ORE_FIXTURE",
                         "HELLOMINE3D_SPAWN_VALIDATION_ACTORS", "HELLOMINE3D_TRANSPARENT_FIXTURE",
                         "HELLOMINE3D_VERTEX_LIGHTING_FIXTURE", "HELLOMINE3D_VERTICAL_SLICE_FIXTURE",
                         "HELLOMINE3D_HUD_FIXTURE", "HELLOMINE3D_HUD_PAGE_FIXTURE"})
                    if (std::getenv(name) != nullptr)
                        throw std::runtime_error(std::string("Camera diagnostics cannot combine fixture ") + name);
                m_cameraDiagnosticOutput = cameraOutput;
                std::cout << "[CAMERA_DIAGNOSTICS] normal_input=0 isolated_world=1 simulation_delta=0 camera_sweep=0\n";
            }
            const char* fernOutput = std::getenv(
                "HELLOMINE3D_FERN_WIND_CAPTURE_DIR");
            if (fernOutput != nullptr && fernOutput[0] != '\0')
            {
                if (!isTrueValue(std::getenv("HELLOMINE3D_WINDOW_HIDDEN")) ||
                    !isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) ||
                    initialSaveDirectory.empty() ||
                    std::getenv("HELLOMINE3D_SAVE_DIR") == nullptr ||
                    std::getenv("HELLOMINE3D_CATALOGUE_DIR") == nullptr ||
                    RuntimePerformanceCapture::isEnabled() ||
                    std::getenv("HELLOMINE3D_RC_PERF_PROFILE") != nullptr ||
                    std::getenv("HELLOMINE3D_E2_BATCH_MANIFEST") != nullptr ||
                    m_visualCameraSweep.enabled || !m_playerMotionCapture.empty() ||
                    !m_actorVisualCapture.empty() || !m_materialIdentityOutput.empty() ||
                    !m_cameraDiagnosticOutput.empty())
                    throw std::runtime_error("Fern wind capture require a hidden isolated world without other diagnostics.");
                for (const char* name : {"HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE",
                         "HELLOMINE3D_COMBAT_FIXTURE", "HELLOMINE3D_CONTAINER_FIXTURE",
                         "HELLOMINE3D_CRAFTING_FIXTURE", "HELLOMINE3D_CROP_FIXTURE",
                         "HELLOMINE3D_MACHINE_FIXTURE", "HELLOMINE3D_ORE_FIXTURE",
                         "HELLOMINE3D_SPAWN_VALIDATION_ACTORS", "HELLOMINE3D_TRANSPARENT_FIXTURE",
                         "HELLOMINE3D_VERTEX_LIGHTING_FIXTURE", "HELLOMINE3D_VERTICAL_SLICE_FIXTURE",
                         "HELLOMINE3D_HUD_FIXTURE", "HELLOMINE3D_HUD_PAGE_FIXTURE",
                         "HELLOMINE3D_RESOURCE_PACKS", "HELLOMINE3D_TERRAIN_FALLBACK", "HELLOMINE3D_V10C_FALLBACK"})
                    if (std::getenv(name) != nullptr)
                        throw std::runtime_error(std::string("Fern wind capture cannot combine fixture ") + name);
                auto exact = [](const char* key, const char* value) {
                    const char* actual = std::getenv(key);
                    return actual != nullptr && std::string(actual) == value;
                };
                if (!exact("HELLOMINE3D_SEED", "20260807") ||
                    !exact("HELLOMINE3D_WORLD_TIME", "6000") ||
                    !exact("HELLOMINE3D_PLAYER_POSITION", "966.5 81 -21.5") ||
                    !exact("HELLOMINE3D_PLAYER_ROTATION", "30 0 0") ||
                    m_config.renderDistance != 1 || m_config.fov != 90 ||
                    m_config.cameraPerspective != CameraPerspective::FirstPerson)
                    throw std::runtime_error("Fern wind capture requires its fixed natural World camera and default resources.");
                m_fernWindOutput = fernOutput;
                std::cout << "[FERN_WIND_CAPTURE] normal_input=0 isolated_world=1 simulation_delta=0 camera_sweep=0\n";
            }
            const char* shoreOutput = std::getenv("HELLOMINE3D_SHORE_EDIT_CAPTURE_DIR");
            const char* shoreNativeDraw = std::getenv("HELLOMINE3D_SHORE_NATIVE_DRAW");
            if (shoreNativeDraw && (std::string(shoreNativeDraw) != "1" || !shoreOutput || !shoreOutput[0]))
                throw std::runtime_error("Shore native draw requires explicit value1 and the existing isolated shore capture entry.");
            m_shoreNativeDraw = shoreNativeDraw != nullptr;
            if (shoreOutput && shoreOutput[0])
            {
                if (!isTrueValue(std::getenv("HELLOMINE3D_WINDOW_HIDDEN")) ||
                    !isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) ||
                    initialSaveDirectory.empty() ||
                    std::getenv("HELLOMINE3D_SAVE_DIR") == nullptr ||
                    std::getenv("HELLOMINE3D_CATALOGUE_DIR") == nullptr ||
                    std::filesystem::exists(std::filesystem::path(initialSaveDirectory) / "world.meta") ||
                    RuntimePerformanceCapture::isEnabled() ||
                    m_visualCameraSweep.enabled || !m_playerMotionCapture.empty() ||
                    !m_actorVisualCapture.empty() || !m_materialIdentityOutput.empty() ||
                    !m_cameraDiagnosticOutput.empty() || !m_fernWindOutput.empty() ||
                    m_config.renderDistance != 1 || m_config.fov != 90 ||
                    m_config.minimapRange != 256)
                    throw std::runtime_error("Shore edit capture requires a hidden fresh isolated world and RD1/FOV90/minimap256.");
                for (const char* key : {"HELLOMINE3D_RC_PERF_PROFILE", "HELLOMINE3D_E2_BATCH_MANIFEST",
                         "HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR", "HELLOMINE3D_RESOURCE_PACKS",
                         "HELLOMINE3D_TERRAIN_FALLBACK", "HELLOMINE3D_V10C_FALLBACK",
                         "HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE", "HELLOMINE3D_COMBAT_FIXTURE",
                         "HELLOMINE3D_CONTAINER_FIXTURE", "HELLOMINE3D_CRAFTING_FIXTURE",
                         "HELLOMINE3D_CROP_FIXTURE", "HELLOMINE3D_MACHINE_FIXTURE",
                         "HELLOMINE3D_ORE_FIXTURE", "HELLOMINE3D_SPAWN_VALIDATION_ACTORS",
                         "HELLOMINE3D_TRANSPARENT_FIXTURE", "HELLOMINE3D_VERTEX_LIGHTING_FIXTURE",
                         "HELLOMINE3D_VERTICAL_SLICE_FIXTURE", "HELLOMINE3D_HUD_FIXTURE",
                         "HELLOMINE3D_HUD_PAGE_FIXTURE", "HELLOMINE3D_FORCE_LEGACY_TERRAIN",
                         "HELLOMINE3D_P11_LIGHT_FIXTURE", "HELLOMINE3D_DISABLE_VERTEX_AO",
                         "HELLOMINE3D_V10E_SETTINGS_FIXTURE", "HELLOMINE3D_V10D_SHADOW_FIXTURE",
                         "HELLOMINE3D_V10D_SHADOW_FALLBACK", "HELLOMINE3D_V10E_POST_FIXTURE",
                         "HELLOMINE3D_V10E_POST_FALLBACK", "HELLOMINE3D_CONTROLLED_CRASH",
                         "HELLOMINE3D_EXIT_AFTER_FRAMES", "HELLOMINE3D_HUD_INSPECT_SLOT",
                         "HELLOMINE3D_VISUAL_CAMERA_PATH", "HELLOMINE3D_E2_BATCH_EVENTS",
                         "HELLOMINE3D_E2_RENDER_PHASES"})
                    if (std::getenv(key))
                        throw std::runtime_error(std::string("Shore edit capture cannot combine ") + key);
                auto exact = [](const char* key, const char* value) {
                    const char* actual = std::getenv(key);
                    return actual && std::string(actual) == value;
                };
                const std::string site = std::getenv("HELLOMINE3D_SHORE_EDIT_SITE")
                    ? std::getenv("HELLOMINE3D_SHORE_EDIT_SITE") : "river";
                const char* position = nullptr; const char* rotation = nullptr;
                if (site == "river") { position = "212 70 -192"; rotation = "10 90 0"; }
                if (site == "lake") { position = "196 72 -144"; rotation = "10 90 0"; }
                if (site == "sea") { position = "-89 67 602"; rotation = "10 -75.6186 0"; }
                if (site == "wetland") { position = "-101 67 312"; rotation = "10 -126.027 0"; }
                if (!position || !exact("HELLOMINE3D_PLAYER_POSITION", position) ||
                    !exact("HELLOMINE3D_PLAYER_ROTATION", rotation) ||
                    !exact("HELLOMINE3D_SEED", "42") || !exact("HELLOMINE3D_WORLD_TIME", "7000"))
                    throw std::runtime_error("Shore edit capture requires a frozen V06b site and seed42/time7000.");
                const char* target = std::getenv("HELLOMINE3D_SHORE_EDIT_TARGET");
                std::istringstream parsed(target ? target : ""); std::string extra;
                if (!(parsed >> m_shoreTarget.x >> m_shoreTarget.y >> m_shoreTarget.z) ||
                    (parsed >> extra) || m_shoreTarget.y != 64 ||
                    m_shoreTarget.x % 4 != 0 || m_shoreTarget.z % 4 != 0)
                    throw std::runtime_error("Shore edit capture requires an exact canonical4 Water64 target.");
                std::istringstream cameraPosition(position); glm::ivec3 centre(0);
                cameraPosition >> centre.x >> centre.y >> centre.z;
                if (m_shoreTarget.x < centre.x - 16 || m_shoreTarget.x >= centre.x + 16 ||
                    m_shoreTarget.z < centre.z - 16 || m_shoreTarget.z >= centre.z + 16)
                    throw std::runtime_error("Shore edit target is outside the frozen bounded locator region.");
                m_shoreEditOutput = shoreOutput;
                std::cout << "[SHORE_EDIT_CAPTURE] normal_input=0 isolated_world=1 simulation_delta=0 site=" << site << '\n';
            }
            const bool hiddenWindow = isTrueValue(
                std::getenv("HELLOMINE3D_WINDOW_HIDDEN"));
            m_hiddenWindow = hiddenWindow;
            if (hiddenWindow)
            {
                m_root->initialise(false, WindowTitle);
                Ogre::NameValuePairList windowParameters;
                windowParameters["hidden"] = "true";
                windowParameters["noActivate"] = "true";
                windowParameters["gamma"] = "false";
                const auto& rendererOptions = m_root->getRenderSystem()->getConfigOptions();
                const auto fsaaOption = rendererOptions.find("FSAA");
                windowParameters["FSAA"] = fsaaOption == rendererOptions.end()
                    ? "0" : fsaaOption->second.currentValue;
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
            // Windowed sizes need not be monitor video modes. Apply the saved
            // dimensions even when Ogre's fullscreen mode list omits them.
            if (!m_config.isFullscreen &&
                (m_window->getWidth() != static_cast<unsigned int>(m_config.windowX) ||
                 m_window->getHeight() != static_cast<unsigned int>(m_config.windowY)))
                m_window->resize(m_config.windowX, m_config.windowY);
            runtimeOperationTimings().markLatestActive(
                RuntimeOperationKind::Startup);

            m_window->setDeactivateOnFocusChange(false);
            m_sceneManager = m_root->createSceneManager(
                Ogre::ST_GENERIC, "HelloMine3DScene");
            if(m_lifecycleProbe)m_lifecycleProbe->rootIdentity(m_root.get(),m_sceneManager);
            m_camera = m_sceneManager->createCamera("PlayerCamera");
            m_camera->setPosition(0.0f, 1.0f, 5.0f);
            m_camera->lookAt(0.0f, 1.0f, 0.0f);
            m_camera->setNearClipDistance(m_nominalCameraNearClipDistance);
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
            m_hdrPipeline = std::make_unique<HdrPipeline>();
            if(m_lifecycleProbe) m_hdrPipeline->setLifecycleReleaseObserver(
                [this](const char* owner,const std::string& facts,bool pass){m_lifecycleProbe->release(owner,facts,pass);},
                std::getenv("HELLOMINE3D_LIFECYCLE_FAULT")!=nullptr);
            m_hdrPipeline->initialize(*viewport, *m_root->getRenderSystem(),
                                      m_config.renderPipeline);
            configureWaterBoundaryPins();
            configureTerrainAppearance();
            m_waterReflection = std::make_unique<PlanarWaterReflection>();
            if(m_lifecycleProbe) m_waterReflection->setLifecycleReleaseObserver(
                [this](const char* owner,const std::string& facts,bool pass){m_lifecycleProbe->release(owner,facts,pass);});
            m_waterReflection->initialize(*m_sceneManager, *m_root->getRenderSystem());
            if(!m_referenceEditOutput.empty()) {
                ReferenceEdit::require(m_hdrPipeline->active() && m_referenceSurfaceEnabled,
                    "actual HDR/current surface profile required");
                m_referenceEditProbe=std::make_unique<ReferenceWorldEditProbe>(
                    m_referenceEditOutput,*m_sceneManager,*m_camera,*m_window);
            }
            selectAtmosphereMode();
            syncTerrainMaterialParameters();
            if (!m_materialIdentityOutput.empty())
                m_materialIdentityCapture = std::make_unique<MaterialIdentityCapture>(
                    m_materialIdentityOutput);
            if (!m_fernWindOutput.empty())
                m_floraWindCapture = std::make_unique<FloraWindCapture>(
                    m_fernWindOutput, *m_sceneManager, *m_camera, *m_window);
            if (!m_cameraDiagnosticOutput.empty())
                m_cameraDiagnostics = std::make_unique<OgreCameraDiagnostics>(
                    m_cameraDiagnosticOutput, *m_sceneManager);
            if (!m_shoreEditOutput.empty())
                m_shoreEditCapture = std::make_unique<ShoreEditCapture>(
                    m_shoreEditOutput, *m_sceneManager, *m_camera, m_shoreNativeDraw);
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
            m_playerRenderer =
                std::make_unique<OgrePlayerRenderer>(*m_sceneManager);
            m_playerRenderer->setCastShadows(
                m_directionalShadowQuality !=
                DirectionalShadowQuality::Off);
            m_caveBoundaryRenderer =
                std::make_unique<OgreCaveBoundaryRenderer>(*m_sceneManager);
            m_blockFeedback =
                std::make_unique<OgreBlockFeedback>(*m_sceneManager);
            m_hdrPipeline->applySceneParameters();
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

        // Select once before any World, SectionMeshInput or workers exist.
        // A complete older Water override remains valid and uses its old mesh.
        void configureWaterBoundaryPins()
        {
            auto* pass = materialPass("HelloMine3D/Water");
            auto vertex = pass->getVertexProgram();
            auto fragment = pass->getFragmentProgram();
            if (vertex.isNull() || fragment.isNull() ||
                vertex->getType() != Ogre::GPT_VERTEX_PROGRAM ||
                fragment->getType() != Ogre::GPT_FRAGMENT_PROGRAM)
                throw std::runtime_error("Invalid Water shader program stages.");
            auto* vs = dynamic_cast<Ogre::GLSLShader*>(vertex.get());
            auto* fs = dynamic_cast<Ogre::GLSLShader*>(fragment.get());
            if (!vs || !fs)
                throw std::runtime_error("Water requires the active GLSL backend.");
            vertex->load(); fragment->load();
            if (!vs->compile(true) || !fs->compile(true) ||
                vertex->hasCompileError() || fragment->hasCompileError())
                throw std::runtime_error("Invalid compiled Water shader resources.");
            const GLuint certificate = glCreateProgram();
            if (!certificate)
                throw std::runtime_error("Cannot create Water shader capability certificate.");
            bool guardActive = false, pinActive = false, available = false;
            try
            {
                // Attach Ogre's actual preprocessed/compiled shaders, including
                // child objects. This temporary link never binds draw state.
                vs->attachToProgramObject(certificate);
                fs->attachToProgramObject(certificate);
                glLinkProgram(certificate);
                GLint linked = 0;
                glGetProgramiv(certificate, GL_LINK_STATUS, &linked);
                if (!linked)
                {
                    char log[4096] = {};
                    glGetProgramInfoLog(certificate, sizeof(log), nullptr, log);
                    throw std::runtime_error(std::string("Invalid linked Water shader resources: ") + log);
                }
                GLint count = 0;
                glGetProgramiv(certificate, GL_ACTIVE_UNIFORMS, &count);
                for (GLint i = 0; i < count; ++i)
                {
                    char name[256] = {};
                    GLint size = 0; GLenum type = 0;
                    glGetActiveUniform(certificate, static_cast<GLuint>(i),
                        sizeof(name), nullptr, &size, &type, name);
                    if (std::string(name) == "waterBoundaryPinsV1")
                        guardActive = type == GL_FLOAT && size == 1;
                }
                glGetProgramiv(certificate, GL_ACTIVE_ATTRIBUTES, &count);
                for (GLint i = 0; i < count; ++i)
                {
                    char name[256] = {};
                    GLint size = 0; GLenum type = 0;
                    glGetActiveAttrib(certificate, static_cast<GLuint>(i),
                        sizeof(name), nullptr, &size, &type, name);
                    if (std::string(name) == "uv3")
                        pinActive = type == GL_FLOAT && size == 1;
                }
                auto parameters = pass->getVertexProgramParameters();
                const bool namedGuard = parameters->_findNamedConstantDefinition(
                    "waterBoundaryPinsV1", false) != nullptr;
                available = guardActive && pinActive && namedGuard;
                if (namedGuard)
                    parameters->setNamedConstant("waterBoundaryPinsV1", available ? 1.f : 0.f);
                glDeleteProgram(certificate);
            }
            catch (...)
            {
                glDeleteProgram(certificate);
                throw;
            }
            BlockDatabase::get().setWaterBoundaryPinsAvailable(available);
            std::cout << "[WATER_BOUNDARY_PINS] available=" << available
                      << " guard_active=" << guardActive << " pin_attribute_active=" << pinActive
                      << " linked=1 startup_only=1 optional_interface=1 mesh="
                      << (available ? "opaque-compound-clipped" : "legacy-uncut") << '\n';
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
            m_regionalAtmosphere.reset();
            if (m_world == nullptr)
            {
                throw std::runtime_error(
                    "Sandbox did not create an active world.");
            }
            if(uploadToOgre && !m_referenceRestartOutput.empty()) {
                ReferenceEdit::require(!m_referenceRestartObservation,"Settings restart single loaded World required");
                m_referenceRestartObservation=std::make_unique<ReferenceSettingsRestartObservation>(
                    m_referenceRestartOutput,*m_world,*m_worldPlayer,*m_sceneManager,*m_camera,
                    m_config,ResourcePaths::bin("config.txt"));
            }
            if (uploadToOgre && m_referenceVisualRequested && !m_referenceVisualApplied)
            {
                m_referenceVisualApplied = buildReferenceVisualScene(
                    *m_world, *m_worldPlayer, *m_logicCamera);
            }
            if(m_lifecycleProbe) {
                ++m_lifecycleWorldEpoch; m_lifecycleWorldDirectory=mainSaveDirectory; m_lifecycleCloseReturned=false;
                lifecycleContext();
            }
            resetAdventureAudioPresentation(true);
            if (m_audio != nullptr)
            {
                m_audio->attach(m_world->getEventBus());
            }

            // Explicit developer capture only: set a small stage, then feed
            // holds/releases through SandboxRuntime's actual mining path.
            const char *feedbackCapture = std::getenv("HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE");
            if (uploadToOgre && feedbackCapture != nullptr && feedbackCapture[0] != '\0')
            {
                if (!isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) &&
                    !RuntimePerformanceCapture::isEnabled())
                    throw std::runtime_error("Block feedback fixture requires an enabled diagnostic capture.");
                const std::string mode(feedbackCapture);
                BlockId id = BlockId::Stone;
                if (mode == "flower") id = BlockId::Rose;
                else if (mode == "grass") id = BlockId::TallGrass;
                else if (mode == "crop") id = BlockId::WheatCrop;
                else if (mode == "door") id = BlockId::OakDoorOpen;
                else if (mode != "stone") throw std::runtime_error("Unknown block feedback capture: " + mode);
                const glm::ivec3 target(World::toBlockCoord(m_worldPlayer->position.x),
                    World::toBlockCoord(m_worldPlayer->position.y),
                    World::toBlockCoord(m_worldPlayer->position.z) - 3);
                for (int x = -4; x <= 4; ++x)
                for (int z = -4; z <= 5; ++z)
                {
                    m_world->setBlock(target.x+x, target.y-1, target.z+z, BlockId::Grass);
                    for (int y=0; y<=4; ++y)
                        m_world->setBlock(target.x+x, target.y+y, target.z+z, BlockId::Air);
                }
                m_world->setBlock(target.x, target.y, target.z, ChunkBlock(id, 0));
                m_worldPlayer->position = glm::vec3(target) + glm::vec3(0.5f,1.01f,3.8f);
                m_worldPlayer->rotation = {19.f,0.f,0.f};
                m_worldPlayer->resetInterpolation();
                m_logicCamera->update();
                m_blockFeedbackCapture = true;
                m_blockFeedbackCaptureTarget = target;
                m_blockFeedbackCaptureId = id;
                m_blockFeedbackCaptureSeconds = 0.f;
                std::cout << "[BLOCK_FEEDBACK_CAPTURE] diagnostic=1 mode=" << mode
                          << " target=" << target.x << ',' << target.y << ',' << target.z << '\n';
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
                if (!uploadToOgre)
                {
                    // The validation-only mesh contract covers every render
                    // pass, even when the current terrain seed is dry land.
                    m_world->setBlock(centerX, centerY + 5, centerZ + 2,
                                      BlockId::Water);
                    m_world->setBlock(centerX, centerY + 6, centerZ + 2,
                                      BlockId::Air);
                }
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

            const char* machineFixture = std::getenv(
                "HELLOMINE3D_MACHINE_FIXTURE");
            if (machineFixture != nullptr && machineFixture[0] != '\0')
            {
                const std::string kind(machineFixture);
                const char* fixtureSave = std::getenv("HELLOMINE3D_SAVE_DIR");
                if ((kind != "furnace" && kind != "crusher") ||
                    fixtureSave == nullptr || fixtureSave[0] == '\0')
                {
                    throw std::runtime_error(
                        "Machine fixture requires furnace or crusher and an "
                        "explicit diagnostic save directory.");
                }
                const bool crusher = kind == "crusher";
                const glm::ivec3 position{
                    World::toBlockCoord(m_worldPlayer->position.x) + 2,
                    World::toBlockCoord(m_worldPlayer->position.y),
                    World::toBlockCoord(m_worldPlayer->position.z) + 2};
                m_world->setBlock(position.x, position.y, position.z,
                                  BlockId::Air);
                m_world->setBlock(position.x, position.y, position.z,
                                  crusher ? BlockId::Crusher : BlockId::Furnace);
                const bool initialized = crusher
                    ? CrusherContainer::initialize(*m_world, position)
                    : FurnaceContainer::initialize(*m_world, position);
                const bool opened = initialized && (crusher
                    ? CrusherContainer::open(*m_world, *m_worldPlayer, position)
                    : FurnaceContainer::open(*m_world, *m_worldPlayer, position,
                                             runtimeSmeltingRegistry()));
                if (!opened)
                {
                    throw std::runtime_error("Machine fixture failed to open.");
                }
                // Preparation only; processing and transfers use the real UI.
                m_worldPlayer->addItem(crusher ? Material::COBBLESTONE_BLOCK
                                              : Material::IRON_ORE_BLOCK, 4);
                m_worldPlayer->addItem(Material::COAL_ORE_BLOCK, 3);
                m_worldPlayer->addItem(Material::SAND_BLOCK, 2);
                m_worldPlayer->addItem(Material::DIRT_BLOCK, 8);
                m_worldPlayer->addItem(Material::GLASS_BLOCK, 2);
                m_containerFixturePlaced = true;
                std::cout << "[MACHINE_FIXTURE] kind=" << kind
                          << " preparation=diagnostic normal_input=0\n";
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

            if(!m_referenceResidencyOutput.empty()) {
                ReferenceResidency::require(!m_referenceResidencyProbe && m_hdrPipeline->active() && m_referenceSurfaceEnabled,"single current HDR World required");
                m_referenceResidencyProbe=std::make_unique<ReferenceResidencyProbe>(
                    m_referenceResidencyOutput,*m_world,*m_sceneManager,*m_camera,*m_window);
                m_hdrPipeline->setLifecycleReleaseObserver([this](const char* owner,const std::string& facts,bool pass){m_referenceResidencyProbe->event("component-release",ReferenceResidency::object({{"owner",ReferenceResidency::quote(owner)},{"facts",facts},{"runtime_pass",ReferenceResidency::boolean(pass)}}));});
                m_waterReflection->setLifecycleReleaseObserver([this](const char* owner,const std::string& facts,bool pass){m_referenceResidencyProbe->event("component-release",ReferenceResidency::object({{"owner",ReferenceResidency::quote(owner)},{"facts",facts},{"runtime_pass",ReferenceResidency::boolean(pass)}}));});
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
                    visual.location = sectionLocation;
                    if (m_shoreEditCapture) visual.shoreUploadSerial = ++m_shoreUploadSerial;
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

                            if (m_floraWindCapture && std::string(materialName)=="HelloMine3D/Flora" &&
                                sectionLocation==glm::ivec3(60,5,-2))
                            {
                                if (validation.vertexCount*sizeof(TerrainRenderVertex)+validation.indexCount*sizeof(std::uint32_t)>16u*1024u*1024u)
                                    throw std::runtime_error("Startup natural Flora diagnostic copy exceeds raw bound.");
                                visual.fernDiagnosticFlora=std::make_unique<ChunkMesh>(mesh);
                            }
                            auto renderable =
                                std::make_unique<ChunkSectionRenderable>(
                                    sectionName.str() + "_" + layerName,
                                    mesh, sectionLocation, materialName,
                                    renderQueue);
                            retainShoreUploadInput(visual, mesh, materialName, sectionLocation);
                            if(m_referenceEditProbe)
                                m_referenceEditProbe->retainUpload(*renderable,mesh,sectionLocation,section->getBlockRevision());
                            if(m_referenceResidencyProbe) {
                                m_referenceResidencyProbe->uploaded(sectionLocation,section->getBlockRevision());
                                m_referenceResidencyProbe->retain(*renderable,{{sectionLocation,&mesh}},sectionLocation);
                            }
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
                        const auto startupMeshState=section->getMeshState();
                        section->markMeshClean();
                        const std::string key =
                            sectionKey(sectionLocation);
                        if (visual.node != nullptr)
                        {
                            m_sectionVisuals.emplace(
                                key, std::move(visual));
                            m_sectionRenderStates[key] =
                                ChunkRenderState::GpuResident;
                            if(m_referenceResidencyProbe)m_referenceResidencyProbe->witnessUploaded(sectionLocation,section->getBlockRevision(),chunk.getIncarnation(),startupMeshState,"cpu-ready","startup");
                            if (m_materialIdentityCapture || m_floraWindCapture || m_shoreEditCapture || m_referenceEditProbe)
                                m_materialIdentityMeshRevisions[key] = section->getBlockRevision();
                        }
                        else
                        {
                            m_sectionRenderStates[key] =
                                ChunkRenderState::NotResident;
                            // Startup validated every empty layer above and
                            // completed the real CPU mesh before this record.
                            m_emptySectionUploadIdentities[key] = {
                                sectionLocation, section->getBlockRevision(),
                                chunk.getIncarnation(), ChunkMeshState::Clean};
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
                m_adventureAmbientReplayCountdown.fill(0.f);
                m_adventureAmbientReplaySuspended = true;
                return false;
            }
            if (m_userInterface != nullptr)
            {
                m_userInterface->setWorldContext(nullptr, nullptr);
            }
            if(m_referenceResidencyProbe && requireSave)m_referenceResidencyProbe->worldDestroyed();
            if (m_shoreEditCapture) m_shoreEditCapture->cancelNativeFrame();
            if (m_waterReflection) m_waterReflection->resetWorld();
            m_localLights = {};
            if (m_blockFeedback != nullptr)
            {
                m_blockFeedback->clear();
            }
            for (auto &entry : m_sectionVisuals)
            {
                destroySectionVisual(entry.second);
            }
            m_sectionVisuals.clear();
            clearTerrainBatches();
            m_sectionRenderStates.clear();
            m_emptySectionUploadIdentities.clear();
            m_materialIdentityMeshRevisions.clear();
            m_lastLiveSections.clear();
            if (m_caveBoundaryRenderer != nullptr)
            {
                m_caveBoundaryRenderer->clear();
            }
            destroyDirectionalShadowResources();
            m_actorRenderer.reset();
            m_playerRenderer.reset();
            if (m_sceneManager != nullptr)
            {
                m_actorRenderer =
                    std::make_unique<OgreActorRenderer>(*m_sceneManager);
                m_playerRenderer =
                    std::make_unique<OgrePlayerRenderer>(*m_sceneManager);
            }
            resetPlayerPresentation();
            m_sandbox.reset();
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->worldDestroyed();
            m_world = nullptr;
            m_regionalAtmosphere.reset();
            m_worldPlayer = nullptr;
            resetAdventureAudioPresentation(false);
            m_logicCamera.reset();
            m_visualCameraSweep = {};
            m_blockFeedbackCapture = false;
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
                case OgreUserInterfaceActionType::SelectHotbar:
                    if (m_worldPlayer != nullptr && m_userInterface->wantsHudPointer() &&
                        action.hotbarSlot >= 0 && action.hotbarSlot < m_worldPlayer->getInventorySlotCount())
                    {
                        PlayerInputState input;
                        input.hotbarSlot = action.hotbarSlot;
                        m_worldPlayer->applyInput(input);
                    }
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
                case OgreUserInterfaceActionType::OpenWorldBackups:
                    if (!clearActiveWorld())
                    {
                        m_userInterface->setStatusMessage(LocalizedPresentation::text(
                            userSettings(m_config).locale, "map.backup_save_failed"));
                        return;
                    }
                    m_applicationFlow.returnToMainMenu();
                    m_applicationFlow.showWorldList();
                    m_userInterface->showWorldBackups(action.worldId);
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
                    const bool audioCaptionsWereEnabled =
                        userSettings(m_config).audioCaptions;
                    userSettings(m_config) = userSettings(candidate);
                    if (!audioCaptionsWereEnabled &&
                        userSettings(m_config).audioCaptions)
                    {
                        // A disabled caption timeline is cleared by the UI.
                        // Let each still-audible environment layer announce
                        // itself once when captions are enabled again.
                        m_adventureAmbientCaptionAnnounced.fill(false);
                    }
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
                        m_userInterface->setStatusMessage(LocalizedPresentation::text(
                            userSettings(m_config).locale, "pause.difficulty_no_world"));
                        return;
                    }
                    const DifficultyChangeResult result =
                        m_world->requestDifficulty(action.difficulty);
                    if (result == DifficultyChangeResult::Invalid)
                    {
                        m_userInterface->setStatusMessage(LocalizedPresentation::text(
                            userSettings(m_config).locale, "pause.difficulty_rejected"));
                    }
                    else if (result == DifficultyChangeResult::Unchanged)
                    {
                        m_userInterface->setStatusMessage(LocalizedPresentation::text(
                            userSettings(m_config).locale, "pause.difficulty_unchanged"));
                    }
                    else
                    {
                        m_userInterface->setStatusMessage(LocalizedPresentation::text(
                            userSettings(m_config).locale, "pause.difficulty_queued"));
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
            if (m_visualCameraSweep.enabled && std::isfinite(event.timeSinceLastFrame) &&
                event.timeSinceLastFrame > 0.f)
                m_visualCameraSweepElapsed = std::min(
                    m_visualCameraSweepElapsed + event.timeSinceLastFrame,
                    VisualCameraSweep::WarmupSeconds + m_visualCameraSweep.durationSeconds + 1.0);
            if (!m_actorVisualCapture.empty())
                m_actorVisualCaptureSeconds += std::clamp(event.timeSinceLastFrame, 0.f, .25f);
            HELLOMINE3D_PROFILE_FRAME();
            HELLOMINE3D_PROFILE_SCOPE("Ogre::frameStarted");
            HELLOMINE3D_PROFILE_PLOT(
                "Frame Delta (ms)",
                static_cast<double>(event.timeSinceLastFrame) * 1000.0);
            m_frameStart = std::chrono::steady_clock::now();
            if (m_renderPhaseDiagnostics)
            {
                m_sceneRenderEnd = {};
                m_swapEnd = {};
            }
            Ogre::WindowEventUtilities::messagePump();
            if(m_referenceResidencyProbe) {
                // Only pure values are retained for this frame. A failed
                // best-effort map observation must never survive as absence.
                m_referenceResidencyObservationFrame=unsigned(m_frameCount);
                m_referenceResidencyColumnsAvailable=false;
                m_referenceResidencyWitnessCellsAvailable=false;
                m_referenceResidencyColumnFacts="[]";
                m_referenceResidencyWitnessCellFacts={{"{}","{}"}};
                m_referenceResidencyProbe->beginFrame(unsigned(m_frameCount),m_referenceResidencyInputs,[this]{return referenceResidencySnapshot(false);});
            }
            if(m_referenceEditProbe && m_world) {
                m_referenceEditProbe->beginFrame(*m_world,unsigned(m_frameCount),m_referenceEditInputEvents);
                if(m_referenceEditProbe->mutationPending()) {
                    bool ready=false;const auto sections=referenceEditSections(ready);
                    m_referenceEditProbe->mutationFacts(ReferenceEdit::object({
                        {"blocks",m_referenceEditProbe->blocks(*m_world)},
                        {"world_visual_revision",ReferenceEdit::number(m_world->visualRevision())},
                        {"sections",sections},{"sections_ready",ReferenceEdit::boolean(ready)},
                        {"section_readiness",m_referenceEditProbe->sectionReadinessFacts()}}));
                }
            }
            if (m_shutdownRequested || m_window == nullptr ||
                m_window->isClosed())
            {
                return false;
            }

            if (m_hdrPipeline)
            {
                m_hdrPipeline->beforeFrame();
                syncReferenceSurfaceMode();
                if (m_userInterface)
                    m_userInterface->setRenderPipelineFallback(m_hdrPipeline->fallback());
            }

            if (m_e2BatchEnabled &&
                RuntimePerformanceCapture::isComplete())
            {
                advanceE2BatchCapture();
                if (m_shutdownRequested)
                {
                    return false;
                }
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
            prepareMaterialIdentityCapture(event.timeSinceLastFrame);
            prepareCameraDiagnostics(event.timeSinceLastFrame);
            prepareFloraWindCapture(event.timeSinceLastFrame);
            prepareShoreEditCapture();
            const bool sandboxAdvanced =
                updateSandbox(event.timeSinceLastFrame);
            if (!sandboxAdvanced && m_sandbox != nullptr)
            {
                // Paused menus and the HUD pointer still need an immediate
                // perspective preview, but must not advance animation time.
                syncRenderCamera(0.f);
                syncPlayerPresentation(0.f);
            }
            updateAudio(event.timeSinceLastFrame);

            m_frameWorldStats = collectRuntimeStats();
            if (m_world != nullptr)
            {
                if (m_referenceSurfaceEnabled && m_camera)
                    m_localLights=m_world->observeLocalLights(glm::vec3(
                        m_camera->getDerivedPosition().x,m_camera->getDerivedPosition().y,
                        m_camera->getDerivedPosition().z));
                syncEnvironment(m_frameWorldStats.environment, m_referenceEditProbe?0.f:event.timeSinceLastFrame);
                if (m_playerRenderer != nullptr)
                {
                    const float exposure = PlayerHandPresentation::updateLighting(
                        m_playerLighting, m_frameWorldStats.playerLocalLightKnown,
                        lightLevelToBrightness(m_frameWorldStats.playerSunlight),
                        lightLevelToBrightness(m_frameWorldStats.playerBlockLight),
                        m_frameWorldStats.environment.daylight,
                        event.timeSinceLastFrame);
                    m_playerRenderer->setLighting(exposure);
                }
            }
            if(m_referenceEditProbe) {
                m_referenceEditProbe->freezeTime();
                bool ready=false;referenceEditSections(ready);
                m_referenceEditProbe->arm(m_localLights,ready);
            }
            if(m_referenceResidencyProbe) {
                bool ready=false,witnessReady=false;
                referenceResidencySections(ready);
                referenceResidencyWitnessSections(witnessReady);
                const bool observationsReady=m_referenceResidencyProbe->phase()!=1 &&
                    m_referenceResidencyProbe->warm() && ready && witnessReady &&
                    prepareReferenceResidencyFrameObservations(false);
                m_referenceResidencyProbe->arm(m_localLights,ready && witnessReady && observationsReady);
            }
            if(m_referenceRestartObservation && m_world)
                m_referenceRestartObservation->beginFrame(unsigned(m_frameCount),*m_world,sandboxAdvanced);
            if (m_waterReflection && m_world && m_camera && m_camera->getViewport())
            {
                const auto position=m_camera->getDerivedPosition();
                const glm::vec3 eye(position.x,position.y,position.z);
                const auto plane=m_world->observeWaterSurfacePlane(eye);
                if (plane) m_waterReflection->selectPlaneY(*plane);
                else m_waterReflection->clearSelection();
                PlanarWaterReflection::FrameInput reflection;
                reflection.enabled=m_config.visualDetail==VisualDetail::Standard &&
                    !isTrueValue(std::getenv("HELLOMINE3D_PLANAR_REFLECTION_OFF"));
                reflection.linearHdr=m_hdrPipeline && m_hdrPipeline->active();
                reflection.cameraUnderwater=m_world->getBlock(World::toBlockCoord(eye.x),
                    World::toBlockCoord(eye.y),World::toBlockCoord(eye.z))==BlockId::Water;
                reflection.frameSerial=static_cast<std::uint64_t>(m_frameCount);
                reflection.sceneRevision=m_world->visualRevision();
                if(m_referenceEditProbe)
                    m_referenceEditFrameSceneRevision=reflection.sceneRevision;
                if(m_referenceResidencyProbe)m_referenceResidencyFrameRevision=reflection.sceneRevision;
                reflection.authoredBackground=m_camera->getViewport()->getBackgroundColour();
                reflection.bindViewParameters=[this](const Ogre::String&,Ogre::Pass &pass,
                                                       const Ogre::Camera&) {
                    // Mirror is a virtual air view, even though its eye lies
                    // below the plane. Main frame already supplied dry-air fog;
                    // underwater frames are rejected before this callback.
                    bindLocalLightParameters(pass.getFragmentProgramParameters());
                };
                if(!m_referenceEditProbe || !m_referenceEditProbe->skipReflection())
                    m_waterReflection->render(*m_camera,*m_camera->getViewport(),reflection);
                m_waterReflection->bindWaterPass(*materialPass("HelloMine3D/Water"));
                if (!m_planarDiagnosticCaptured && m_hiddenWindow && m_frameCount >= 240 &&
                    m_waterReflection->statistics().active &&
                    isTrueValue(std::getenv("HELLO_RENDER_CAPTURE")) &&
                    isTrueValue(std::getenv("HELLOMINE3D_PLANAR_DIAGNOSTIC")))
                {
                    const char* directory = std::getenv("HELLO_RENDER_CAPTURE_DIR");
                    if (directory && directory[0])
                    {
                        m_waterReflection->captureDiagnostic(
                            std::string(directory) + "/planar-diagnostic");
                        m_planarDiagnosticCaptured = true;
                    }
                }
                if (m_frameCount%240==0)
                {
                    const auto &stats=m_waterReflection->statistics();
                    std::cout<<"[REFERENCE_FRAME] frame="<<m_frameCount<<" reflection="<<stats.active
                             <<" reason="<<stats.reason<<" updates="<<stats.updateCount
                             <<" colour_bytes="<<stats.colourBytes<<" depth_stencil_bytes="<<stats.depthStencilBytes
                             <<" private_passes="<<stats.privatePasses<<" batches="<<stats.colourBatches
                             <<" shadow_updates="<<stats.shadowUpdates<<" shadow_batches="<<stats.shadowBatches
                             <<" cpu_ms="<<stats.cpuMilliseconds<<" sources="<<m_localLights.count
                             <<" source_sections="<<m_localLights.inspectedSections
                             <<" source_cells="<<m_localLights.inspectedCells<<'\n';
                }
            }
            observeMaterialIdentityGeometry();
            observeCameraDiagnostics();
            observeFloraWindCapture();
            observeShoreNativeDraw();
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
                if (m_pauseNotificationCapture != nullptr)
                    m_pauseNotificationCapture->drive(*m_userInterface,
                        m_applicationFlow, runtimeWindowFocused());
                m_userInterface->beginFrame(event.timeSinceLastFrame,
                                            m_frameWorldStats, progress,
                                            actionFeedback);
            }
            // Focus suppression lasts one input frame, including paused/HUD
            // frames. Tying it to world simulation can permanently block the
            // very Escape/Tab press needed to resume after switching windows.
            m_focusTransitionFrame = false;
            m_updateEnd = std::chrono::steady_clock::now();
            return true;
        }

        bool frameRenderingQueued(const Ogre::FrameEvent&) override
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::frameRenderingQueued");
            if (m_renderPhaseDiagnostics)
            {
                // Ogre fires this callback after _updateAllRenderTargets and
                // immediately before _swapAllRenderTargetBuffers.
                m_sceneRenderEnd = std::chrono::steady_clock::now();
            }
            if (m_materialIdentityCapture && m_materialIdentityFramePending)
            {
                if (m_materialIdentityCapture->isFrameOpen())
                    throw std::runtime_error("Material identity frame did not reach the actual ImGui backend.");
                m_window->writeContentsToFile(m_materialIdentityOutput +
                    "/client-" + std::to_string(m_materialIdentityPhase) + ".png");
                ++m_materialIdentityPhase;
                m_materialIdentityPhaseSeconds = 0.f;
                m_materialIdentityFramePending = false;
            }
            if (m_cameraDiagnostics && m_cameraDiagnosticFramePending)
            {
                m_window->writeContentsToFile(m_cameraDiagnostics->framePngPath());
                const auto error = glGetError();
                std::cout << "[CAMERA_DIAGNOSTICS] phase=" << m_cameraDiagnosticPhase
                    << " gl_error=" << error << '\n';
                if (error != GL_NO_ERROR)
                    throw std::runtime_error("Camera diagnostic backend reported a GL error.");
                m_cameraDiagnostics->finishFrame(
                    m_userInterface->isFirstPersonPresentationVisible());
                ++m_cameraDiagnosticPhase;
                m_cameraDiagnosticPhaseSeconds = 0.f;
                m_cameraDiagnosticPhasePlaced = false;
                m_cameraDiagnosticFramePending = false;
            }
            if (m_floraWindCapture && m_floraWindCapture->isFrameOpen())
            {
                m_window->writeContentsToFile(m_floraWindCapture->framePngPath());
                auto end = collectFloraWindBindings();
                if (end.empty()) throw std::runtime_error("Fern source endpoint lost its actual resident GPU object.");
                m_floraWindCapture->finishFrame(std::move(end));
                m_fernWindPhaseSeconds = 0.f;
                m_fernWindPhaseApplied = false;
            }
            if (m_pauseNotificationCapture && m_pauseNotificationCapture->isFramePending())
            {
                m_window->writeContentsToFile(m_pauseNotificationCapture->framePngPath());
                const auto error = glGetError();
                if (error != GL_NO_ERROR)
                    throw std::runtime_error("Pause notification actual PNG/backend reported GL error " + std::to_string(error));
                m_pauseNotificationCapture->finishFrame();
            }
            finishShoreEditFrame();
            finishReferenceEditFrame();
            finishReferenceResidencyFrame();
            if(m_referenceRestartObservation && m_referenceRestartObservation->ready()) {
                std::optional<float> spatialAaStrength;
                if(m_hdrPipeline && m_hdrPipeline->active()) {
                    const auto parameters=materialPass("HelloMine3D/HdrResolve")->getFragmentProgramParameters();
                    const auto* definition=parameters->_findNamedConstantDefinition("spatialAaStrength",false);
                    ReferenceEdit::require(definition!=nullptr,"Settings restart actual HDR spatial parameter missing");
                    float strength=0;parameters->_readRawConstants(definition->physicalIndex,1,&strength);
                    ReferenceEdit::require(std::isfinite(strength),"Settings restart actual HDR spatial parameter nonfinite");
                    spatialAaStrength=strength;
                }
                auto planar=m_waterReflection->lifecycleFacts().json();
                planar=planar.substr(0,planar.size()-1)+",\"frame\":"+ReferenceEdit::number(m_waterReflection->statistics().frameSerial)+"}";
                m_referenceRestartObservation->finish(*m_world,*m_worldPlayer,*m_window,m_hdrPipeline->lifecycleFacts().json(),planar,spatialAaStrength);
            }
            ++m_frameCount;
            return true;
        }

        bool frameEnded(const Ogre::FrameEvent& event) override
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::frameEnded");
            if (m_renderPhaseDiagnostics)
            {
                // Ogre's final buffer swap and LOD event processing completed
                // before frameEnded.
                m_swapEnd = std::chrono::steady_clock::now();
            }
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
            if (m_renderPhaseDiagnostics &&
                m_sceneRenderEnd >= m_updateEnd &&
                m_swapEnd >= m_sceneRenderEnd &&
                frameEnd >= m_swapEnd)
            {
                timings.renderDrawMs =
                    std::chrono::duration<double, std::milli>(
                        m_sceneRenderEnd - m_updateEnd).count();
                timings.renderPostDrawMs =
                    std::chrono::duration<double, std::milli>(
                        m_swapEnd - m_sceneRenderEnd).count();
                timings.renderEndedMs =
                    std::chrono::duration<double, std::milli>(
                        frameEnd - m_swapEnd).count();
                timings.renderPhaseValid = true;
            }
            timings.frameMs = frameMs;

            RuntimePerformanceCapture::recordFrame(timings,
                                                    m_frameWorldStats);

            if (m_e2BatchEntryPending)
            {
                runtimeOperationTimings().completeLatestActive(
                    RuntimeOperationKind::WorldEntry, true);
                m_e2BatchEntryPending = false;
            }

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

            if(m_lifecycleProbe) {
                advanceRenderLifecycleProbe();
                if(m_lifecycleProbe->stage()==11) return false;
            }
            const bool captureComplete =
                m_renderCapture != nullptr &&
                m_renderCapture->shouldCloseWindow();
            const bool frameLimitReached =
                m_exitAfterFrames > 0 &&
                m_frameCount >= m_exitAfterFrames;
            return !(m_materialIdentityCapture && m_materialIdentityPhase >= 9) &&
                   !(m_cameraDiagnostics && m_cameraDiagnosticPhase >= 6) &&
                   !(m_floraWindCapture && m_floraWindCapture->isComplete()) &&
                   !(m_pauseNotificationCapture && m_pauseNotificationCapture->isComplete()) &&
                   !m_shoreComplete &&
                   !(m_referenceEditProbe && m_referenceEditProbe->complete()) &&
                   !(m_referenceResidencyProbe && m_referenceResidencyProbe->complete()) &&
                   !captureComplete && !frameLimitReached &&
                   !RuntimePerformanceCapture::shouldCloseWindow();
        }


        std::string referenceResidencyCells() const {
            using namespace ReferenceResidency;std::vector<std::string> cells;
            for(const auto& sample:ReferenceResidencyProbe::Samples){const auto b=m_world->getBlock(sample.p.x,sample.p.y,sample.p.z);cells.push_back(object({{"position",xyz(sample.p)},{"id",number(b.id)},{"metadata",number(b.metadata)}}));require(b==sample.block,"current target ID/meta differs from template");}return array(cells);
        }
        std::string referenceResidencySections(bool& ready) const {
            using namespace ReferenceResidency;ready=false;if(!m_world || !m_referenceResidencyProbe)return "[]";
            const auto snapshot=m_world->collectSectionMeshSnapshot(false);std::vector<std::string> sections;bool all=true;
            for(const auto& v:snapshot.liveSectionVersions)if(m_referenceResidencyProbe->selected(v.location)) {
                const auto state=m_sectionRenderStates.find(sectionKey(v.location));const auto upload=m_referenceResidencyProbe->uploadedRevision(v.location);
                const bool gpu=state!=m_sectionRenderStates.end() && state->second==ChunkRenderState::GpuResident;
                const bool offered=std::any_of(snapshot.cpuReadySections.begin(),snapshot.cpuReadySections.end(),[&](const auto& s){return s.location==v.location;});
                const bool current=upload && *upload==v.blockRevision;all=all && gpu && current && !offered;
                sections.push_back(object({{"location",xyz(v.location)},{"live_revision",number(v.blockRevision)},{"uploaded_revision",upload?number(*upload):"null"},{"gpu_resident",boolean(gpu)},{"offered_cpu_ready",boolean(offered)}}));
            }
            ready=all && sections.size()==2;
            return array(sections);
        }
        bool prepareReferenceResidencyFrameObservations(bool departed) {
            using namespace ReferenceResidency;
            require(m_referenceResidencyProbe && m_world &&
                m_referenceResidencyObservationFrame==unsigned(m_frameCount),
                "same-frame observation owner missing");
            const std::vector<VectorXZ> coords{{201,-186},{199,-196}};
            const auto values=m_world->observeSurfaceMap(coords);
            // {} means try-lock unavailable, not known=false or data absent.
            if(values.empty())return false;
            require(values.size()==coords.size(),"incomplete target column batch");
            std::vector<std::string> columns;
            bool expectedKnown=true;
            for(std::size_t i=0;i<values.size();++i) {
                columns.push_back(object({{"position",array({number(coords[i].x),number(coords[i].z)})},
                    {"known",boolean(values[i].known)},{"height",number(values[i].height)},
                    {"material",number(unsigned(values[i].material))},
                    {"observation",quote("World.observeSurfaceMap-try-lock-complete-batch")},
                    {"observed_frame",number(m_frameCount)}}));
                expectedKnown=expectedKnown && values[i].known==!departed;
            }
            m_referenceResidencyColumnFacts=array(columns);
            m_referenceResidencyColumnsAvailable=true;
            if(!expectedKnown)return false;
            if(departed)return true; // Never query old target/witness cells here.
            const std::array<glm::ivec3,2> positions{{{225,67,-175},{225,72,-177}}};
            const std::array<ChunkBlock,2> expected{{ChunkBlock(Block_t(4),2),ChunkBlock(Block_t(5),2)}};
            bool cellsReady=true;
            for(std::size_t i=0;i<positions.size();++i) {
                const auto& pos=positions[i];
                // Existing blocking World lock + find-only accessor. Absent or
                // unloaded data returns Air; these fixed witnesses are non-Air.
                const auto block=m_world->getBlock(pos.x,pos.y,pos.z);
                const bool known=block.id!=static_cast<Block_t>(BlockId::Air);
                cellsReady=cellsReady && known && block==expected[i];
                m_referenceResidencyWitnessCellFacts[i]=object({{"position",xyz(pos)},
                    {"id",number(block.id)},{"metadata",number(block.metadata)},
                    {"known",boolean(known)},
                    {"observation",quote("blocking-World.getBlock-find-only-nonAir")},
                    {"observed_frame",number(m_frameCount)}});
            }
            m_referenceResidencyWitnessCellsAvailable=cellsReady;
            return cellsReady;
        }
        std::string referenceResidencyWitnessSections(bool& ready,bool withCells=false) const {
            using namespace ReferenceResidency;ready=false;if(!m_world || !m_referenceResidencyProbe)return "[]";
            const auto snapshot=m_world->collectSectionMeshSnapshot(false);std::vector<std::string> fields;bool all=true;
            for(const auto& v:snapshot.liveSectionVersions)if(ReferenceResidencyProbe::witness(v.location)) {
                const auto u=m_referenceResidencyProbe->witnessUpload(v.location);const auto state=m_sectionRenderStates.find(sectionKey(v.location));
                const bool gpu=state!=m_sectionRenderStates.end() && state->second==ChunkRenderState::GpuResident;
                const bool offered=std::any_of(snapshot.cpuReadySections.begin(),snapshot.cpuReadySections.end(),[&](const auto& candidate){return candidate.location==v.location;});
                const bool current=u && u->revision==v.blockRevision && u->incarnation==v.incarnation && v.meshState==ChunkMeshState::Clean;const auto floor=m_referenceResidencyProbe->returnWitnessFloor();
                const bool fresh=u && (m_referenceResidencyProbe->phase()!=2 || (u->serial>floor && u->frame>m_referenceResidencyProbe->returnMovementFrame()));
                all=all && gpu && current && !offered && fresh;
                std::vector<std::string> cells;
                if(withCells && m_referenceResidencyProbe->phase()!=1) {
                    require(m_referenceResidencyWitnessCellsAvailable &&
                        m_referenceResidencyObservationFrame==unsigned(m_frameCount),
                        "same-frame known witness cells not observed before draw");
                    cells.push_back(m_referenceResidencyWitnessCellFacts[v.location.z==-11?0:1]);
                }
                fields.push_back(object({{"cells",array(cells)},{"location",xyz(v.location)},{"mesh_state",quote(chunkMeshStateName(v.meshState))},{"incarnation",number(v.incarnation)},{"upload_source",u?quote(u->source):"null"},{"upload_domain",u?quote(u->domain):"null"},{"upload_source_mesh_state",u?quote(chunkMeshStateName(u->sourceState)):"null"},{"upload_source_incarnation",u?number(u->incarnation):"null"},{"upload_frame_facts",u?u->frameFacts:"null"},{"live_revision",number(v.blockRevision)},{"uploaded_revision",u?number(u->revision):"null"},{"upload_serial",u?number(u->serial):"null"},{"uploaded_frame",u?number(u->frame):"null"},{"return_upload_serial_floor",number(floor)},{"gpu_resident",boolean(gpu)},{"offered_cpu_ready",boolean(offered)}}));
            }
            ready=all && fields.size()==2;return array(fields);
        }
        std::string referenceResidencySnapshot(bool withCurrentCells) const {
            using namespace ReferenceResidency;bool ready=false;const auto sections=referenceResidencySections(ready);bool witnessReady=false;const auto witnessSections=referenceResidencyWitnessSections(witnessReady,withCurrentCells);
            const auto snapshot=m_world?m_world->collectSectionMeshSnapshot(false):WorldMeshSnapshot{};
            std::vector<std::string> live,states,visuals,batches,dirty;
            for(const auto& v:snapshot.liveSectionVersions)if(column(v.location))live.push_back(xyz(v.location));
            for(const auto& v:m_sectionRenderStates)for(const auto& loc:m_lastLiveSections)if(column(loc) && v.first==sectionKey(loc))states.push_back(object({{"location",xyz(loc)},{"state",number(unsigned(v.second))}}));
            // Keys are evidence too: a deliberately retained render-state can
            // remain after its live location disappeared.
            std::vector<std::string> stateKeys;for(const auto& v:m_sectionRenderStates)stateKeys.push_back(quote(v.first));
            for(const auto& v:m_sectionVisuals)if(column(v.second.location))visuals.push_back(object({{"location",xyz(v.second.location)},{"key",quote(v.first)},{"node",quote(v.second.node?v.second.node->getName():"")},{"renderables",number(v.second.renderables.size())},{"batch_cpu_owned",boolean(bool(v.second.batchMeshes))}}));
            for(const auto& v:m_terrainBatchVisuals)if(column(v.second.location))batches.push_back(object({{"origin",xyz(v.second.location)},{"key",quote(v.first)},{"renderables",number(v.second.renderables.size())}}));
            for(const auto& v:m_dirtyTerrainBatches)if(column(v.second))dirty.push_back(xyz(v.second));
            // Reuse the one actual observation from this frame, never issue a
            // second best-effort query after drawing or native readback.
            const bool columnsAvailable=m_referenceResidencyColumnsAvailable &&
                m_referenceResidencyObservationFrame==unsigned(m_frameCount);
            const auto columns=columnsAvailable?m_referenceResidencyColumnFacts:"[]";
            std::string worldId;std::ifstream meta(std::filesystem::path(m_referenceResidencyOutput).parent_path()/"save/world.meta");for(std::string line;std::getline(meta,line);)if(line.compare(0,9,"world_id ")==0){worldId=line.substr(9);break;}
            const auto actualIdentity=m_world?m_world->observeWorldIdentity():WorldIdentityObservation{};
            const auto& p=m_worldPlayer;const auto pFacts=p?object({{"position",xyz(p->position)},{"rotation",xyz(p->rotation)},{"velocity",xyz(p->velocity)},{"interpolation_epoch",number(p->getInterpolationEpoch())}}):"null";
            const auto logic=m_logicCamera?object({{"position",xyz(m_logicCamera->position)},{"rotation",xyz(m_logicCamera->rotation)}}):"null";
            const auto planar=m_waterReflection && m_waterReflection->statistics().active?m_waterReflection->worldEditDiagnosticFacts():"null";
            return object({{"identity",object({{"root_instance",lifecycleAddress(m_root.get())},{"scene_instance",lifecycleAddress(m_sceneManager)},{"window_instance",lifecycleAddress(m_window)},{"world_instance",lifecycleAddress(m_world)},{"world_id",quote(actualIdentity.worldId)},{"actual_world_id",quote(actualIdentity.worldId)},{"disk_world_id",quote(worldId)},{"actual_seed",number(actualIdentity.seed)},{"actual_terrain_generation_version",number(actualIdentity.terrainGenerationVersion)},{"save_directory",quote(ReferenceResidency::value("HELLOMINE3D_SAVE_DIR"))}})},
                {"player",pFacts},{"logic_camera",logic},{"main_camera",m_camera?camera(*m_camera):"null"},{"world_time",number(m_world?m_world->getWorldTime():0.f)},{"simulation_delta",number(m_referenceResidencyDelta)},
                {"frame_input_scene_revision",number(m_referenceResidencyFrameRevision)},{"later_current_world_visual_revision",number(m_world?m_world->visualRevision():0)},
                {"chunk_event_counts",m_referenceResidencyProbe->aggregateEventFacts()},{"target_chunks",m_referenceResidencyProbe->targetFacts()},{"native_objects",m_referenceResidencyProbe->nativeFacts()},{"cells",withCurrentCells && m_world?referenceResidencyCells():"[]"},{"columns",columns},{"column_observation_available",boolean(columnsAvailable)},{"sections",sections},{"sections_ready",boolean(ready)},{"view_witness_sections",witnessSections},{"view_witness_ready",boolean(witnessReady)},{"return_upload_serial_floor",number(m_referenceResidencyProbe->returnWitnessFloor())},{"return_movement_frame",number(m_referenceResidencyProbe->returnMovementFrame())},
                {"global_cpu_ready_total",number(snapshot.cpuReadyTotal)},{"global_cpu_ready_deferred",number(snapshot.cpuReadyDeferred)},{"offered_cpu_ready_total",number(snapshot.cpuReadySections.size())},
                {"cache",object({{"target_live_sections",array(live)},{"target_render_states",array(states)},{"render_state_keys",array(stateKeys)},{"target_section_visuals",array(visuals)},{"target_batches",array(batches)},{"target_dirty_batches",array(dirty)}})},
                {"draws",m_referenceResidencyProbe->drawFacts()},{"render_object_access",m_referenceResidencyProbe->accessFacts()},{"local_lights",lights(m_localLights)},{"hdr",m_hdrPipeline?m_hdrPipeline->lifecycleFacts().json():"null"},{"planar",planar},{"lifecycle",lifecycleSnapshot()}});
        }
        void finishReferenceResidencyFrame() {
            if(!m_referenceResidencyProbe)return;using namespace ReferenceResidency;
            const auto phase=m_referenceResidencyProbe->phase();if(!m_referenceResidencyProbe->warm())return;
            if(phase==1) {
                if(!m_referenceResidencyProbe->targetsAbsent())return;
                if(!prepareReferenceResidencyFrameObservations(true))return;
                const auto facts=referenceResidencySnapshot(false);
                // Facts are retained before any failure, including the real
                // fault's intentionally live old visual/buffer.
                m_referenceResidencyProbe->event("departed-observation",facts);
                require(m_referenceResidencyColumnsAvailable &&
                    m_referenceResidencyObservationFrame==unsigned(m_frameCount),
                    "departed observation not the original complete batch");
                const auto live=m_world->collectSectionMeshSnapshot(false);for(const auto& v:live.liveSections)require(!column(v),"old target still Near");
                for(const auto& v:m_sectionVisuals)require(!column(v.second.location),"old target visual retained");for(const auto& v:m_terrainBatchVisuals)require(!column(v.second.location),"old target batch retained");
                for(int z:{-12,-13})for(int y=0;y<64;++y)require(!m_sectionRenderStates.count(sectionKey({12,y,z})),"old render state retained");
                m_referenceResidencyProbe->checkInventory();
                m_referenceResidencyProbe->checkpoint(facts);
                m_referenceResidencyProbe->markReturnMovement();
                const auto from=m_worldPlayer->position,to=m_referenceResidencyProbe->originPosition();m_worldPlayer->rotation=m_referenceResidencyProbe->originRotation();
                require(m_sandbox->getWorldManager().teleportPlayer(*m_worldPlayer,to),"return teleport rejected");m_logicCamera->update();
                m_referenceResidencyProbe->event("teleport",object({{"direction",quote("return")},{"return_upload_serial_floor",number(m_referenceResidencyProbe->returnWitnessFloor())},{"return_movement_frame",number(m_referenceResidencyProbe->returnMovementFrame())},{"from",xyz(from)},{"requested",xyz(to)},{"accepted","true"},{"actual_player",xyz(m_worldPlayer->position)},{"logic_camera",xyz(m_logicCamera->position)}}));return;
            }
            if(!m_referenceResidencyProbe->armed())return;
            bool ready=false;referenceResidencySections(ready);require(ready,"selected actual upload changed during observed frame");
            if(phase==2)require(m_referenceResidencyProbe->returned(),"return lacks actual storage/new incarnation");
            bool witnesses=false;referenceResidencyWitnessSections(witnesses);
            require(witnesses,"visible witness upload changed during observed frame");
            const auto& r=m_waterReflection->statistics();require(r.active && r.frameSerial==static_cast<std::uint64_t>(m_frameCount) && r.sceneRevision==m_referenceResidencyFrameRevision,"RTT not current original frame");
            lifecycleLiveReady(); // Numeric production native/storage/ownership guards; input counter remains separately checked.
            require(m_referenceResidencyColumnsAvailable && m_referenceResidencyWitnessCellsAvailable &&
                m_referenceResidencyObservationFrame==unsigned(m_frameCount),
                "same-frame observations missing before native readback");
            // All authority/current-upload/pose observations are materialized
            // before consuming either bounded readback. They contain values,
            // not World/Ogre references or resource lifetime extensions.
            const auto actual=referenceResidencySnapshot(true);
            if(phase!=0) {const auto& a=m_referenceResidencyProbe->originPosition();const auto& rot=m_referenceResidencyProbe->originRotation();require(m_worldPlayer->position.x==a.x && m_worldPlayer->position.z==a.z && m_worldPlayer->rotation==rot,"actual returned horizontal pose differs");}
            const auto prefix=m_referenceResidencyProbe->prefix();m_waterReflection->captureDiagnostic(prefix);m_window->writeContentsToFile(m_referenceResidencyProbe->mainPng());require(glGetError()==GL_NO_ERROR,"main readback GL error");
            m_referenceResidencyProbe->checkInventory();
            const auto facts=object({{"frame",number(m_frameCount)},{"actual",actual},{"planar_prefix",quote(std::filesystem::path(prefix).filename().string())},{"main_png",quote(std::filesystem::path(m_referenceResidencyProbe->mainPng()).filename().string())},{"main_readback_gl_error","0"}});
            if(phase==0)m_referenceResidencyProbe->anchor(*m_worldPlayer);
            m_referenceResidencyProbe->checkpoint(facts);
            if(phase==0) {
                const auto from=m_worldPlayer->position;glm::vec3 to=from+glm::vec3(192.f,0.f,0.f);
                require(m_sandbox->getWorldManager().teleportPlayer(*m_worldPlayer,to),"depart teleport rejected");
                const auto surface=m_world->observeSurfaceMap({{World::toBlockCoord(to.x),World::toBlockCoord(to.z)}});require(surface.size()==1 && surface[0].known,"far target preload did not provide actual surface");
                to.y=float(surface[0].height)+1.02f;require(m_sandbox->getWorldManager().teleportPlayer(*m_worldPlayer,to),"safe far teleport rejected");m_logicCamera->update();
                m_referenceResidencyProbe->event("teleport",object({{"direction",quote("depart")},{"from",xyz(from)},{"requested",xyz(to)},{"accepted","true"},{"far_surface_height",number(surface[0].height)},{"actual_player",xyz(m_worldPlayer->position)},{"logic_camera",xyz(m_logicCamera->position)}}));
            }
        }

        std::string referenceEditSections(bool& ready)
        {
            using namespace ReferenceEdit;
            ready=false;if(!m_referenceEditProbe || !m_world)return "[]";
            const auto snapshot=m_world->collectSectionMeshSnapshot(false);
            // Global worker offers include unrelated sections. Current selected
            // input revisions, resident uploads and selected offers define this
            // observation; the global queue remains diagnostic evidence only.
            std::vector<std::string> sections;bool all=true;
            for(const auto& section:snapshot.liveSectionVersions) {
                if(!m_referenceEditProbe->sectionSelected(section.location))continue;
                const auto key=sectionKey(section.location);
                const auto uploaded=m_materialIdentityMeshRevisions.find(key);
                const auto resident=m_sectionRenderStates.find(key);
                const bool cpu=std::any_of(snapshot.cpuReadySections.begin(),snapshot.cpuReadySections.end(),
                    [&](const auto& v){return v.location==section.location;});
                const bool gpu=resident!=m_sectionRenderStates.end() && resident->second==ChunkRenderState::GpuResident;
                const bool known=uploaded!=m_materialIdentityMeshRevisions.end();
                const bool current=known && uploaded->second==section.blockRevision;
                all=all && current && gpu && !cpu;
                // cpuReadySections is the budgeted offered subset, not a full
                // CPU state query; input invalidation advances blockRevision.
                sections.push_back(object({{"location",xyz(section.location)},{"live_revision",number(section.blockRevision)},
                    {"uploaded_revision",known?number(uploaded->second):"null"},{"gpu_resident",boolean(gpu)},
                    {"cpu_ready",boolean(cpu)},{"offered_cpu_ready",boolean(cpu)},
                    {"upload_known",boolean(known)},{"revision_current",boolean(current)}}));
            }
            ready=all && !sections.empty();const auto facts=array(sections);
            m_referenceEditProbe->recordSectionReadiness(object({{"selected_ready",boolean(ready)},{"sections",facts},
                {"global_cpu_ready_total",number(snapshot.cpuReadyTotal)},
                {"global_cpu_ready_deferred",number(snapshot.cpuReadyDeferred)},
                {"offered_cpu_ready_total",number(snapshot.cpuReadySections.size())}}));
            return facts;
        }
        void finishReferenceEditFrame()
        {
            if(!m_referenceEditProbe || !m_referenceEditProbe->frameArmed())return;
            using namespace ReferenceEdit;
            bool ready=false;const auto sections=referenceEditSections(ready);
            require(ready,"actual section upload readiness lost within original frame");
            m_referenceEditProbe->requireExpected(*m_world);
            require(m_window->getWidth()==2560 && m_window->getHeight()==1440,"actual physical window must be2560x1440");
            require(m_hdrPipeline->active() && m_referenceSurfaceEnabled,"actual HDR/surface path lost");
            const auto hdr=m_hdrPipeline->lifecycleFacts();
            require(RenderLifecycle::valid(hdr.native,2560,1440,4),"actual HDR4 storage required");
            const auto planar=m_waterReflection->worldEditDiagnosticFacts();
            const auto prefix=m_referenceEditProbe->prefix();
            m_waterReflection->captureDiagnostic(prefix);
            m_window->writeContentsToFile(m_referenceEditProbe->mainPng());
            const auto error=glGetError();require(error==GL_NO_ERROR,"main readback GL error");
            std::vector<std::string> actors;for(const auto& actor:m_frameActorSnapshots)
                actors.push_back(object({{"id",number(actor.id)},{"type",quote(actor.type)},{"position",xyz(actor.position)},
                    {"rotation",xyz(actor.rotation)},{"wildlife_motion_seconds",number(actor.wildlifeMotionSeconds)},
                    {"item_age_seconds",number(actor.itemAgeSeconds)}}));
            const glm::ivec3 lightProbe(201,68,-186);
            const auto sample=m_world->getBlock(lightProbe.x,lightProbe.y,lightProbe.z);
            const glm::ivec3 receiver(201,68,-192);const auto receiverBlock=m_world->getBlock(receiver.x,receiver.y,receiver.z);
            require(receiverBlock==ChunkBlock(Block_t(35),0),"fixed unedited receiver block/meta differs");
            std::vector<std::string> receiverAir;
            for(int x=201;x<=202;++x)for(int y=68;y<=69;++y) {
                const auto b=m_world->getBlock(x,y,-191);require(b==ChunkBlock(Block_t(0),0),"receiver neighbouring light sample must be actual Air");
                receiverAir.push_back(object({{"position",xyz(glm::ivec3(x,y,-191))},{"id",number(b.id)},{"metadata",number(b.metadata)},
                    {"sky",number(m_world->getSunlight(x,y,-191))},{"block",number(m_world->getBlockLight(x,y,-191))}}));
            }
            const auto receiverFacts=object({{"cell",xyz(receiver)},{"id",number(receiverBlock.id)},{"metadata",number(receiverBlock.metadata)},
                {"face",quote("positive-z")},{"corners",array({xyz(glm::ivec3(201,68,-191)),xyz(glm::ivec3(202,68,-191)),xyz(glm::ivec3(202,69,-191)),xyz(glm::ivec3(201,69,-191))})},
                {"air_samples",array(receiverAir)}});
            // The worker may advance global revision after this frame's actual
            // upload/light/reflection input. Keep those observation clocks apart.
            const auto laterWorldRevision=m_world->visualRevision();
            const auto facts=object({{"frame",number(m_frameCount)},{"normal_input","false"},{"input_event_count",number(m_referenceEditInputEvents)},
                {"simulation_delta","0"},{"animation_time","4"},{"blocks",m_referenceEditProbe->blocks(*m_world)},
                {"world_instance",lifecycleAddress(m_world)},{"root_instance",lifecycleAddress(m_root.get())},{"scene_instance",lifecycleAddress(m_sceneManager)},
                {"world_visual_revision",number(laterWorldRevision)},
                {"frame_input_scene_revision",number(m_referenceEditFrameSceneRevision)},
                {"frame_local_light_revision",number(m_localLights.revision)},
                {"later_current_world_visual_revision",number(laterWorldRevision)},
                {"world_time",number(m_frameWorldStats.worldTime)},
                {"save_directory",quote(std::getenv("HELLOMINE3D_SAVE_DIR"))},{"sections",sections},{"sections_ready",boolean(ready)},
                {"section_readiness",m_referenceEditProbe->sectionReadinessFacts()},
                {"local_lights",lights(m_localLights)},{"receiver",receiverFacts},{"light_probe",object({{"position",xyz(lightProbe)},{"id",number(sample.id)},{"metadata",number(sample.metadata)},
                    {"sky",number(m_world->getSunlight(lightProbe.x,lightProbe.y,lightProbe.z))},{"block",number(m_world->getBlockLight(lightProbe.x,lightProbe.y,lightProbe.z))}})},
                {"camera",camera(*m_camera)},{"actors",array(actors)},{"physical_window",array({"2560","1440"})},
                {"hdr",hdr.json()},{"planar",planar},{"draws",m_referenceEditProbe->drawFacts()},
                {"main_png",quote(std::filesystem::path(m_referenceEditProbe->mainPng()).filename().string())},
                {"planar_prefix",quote(std::filesystem::path(prefix).filename().string())},{"main_readback_gl_error",number(error)}});
            m_referenceEditProbe->checkpoint(facts);
            const auto& reflection=m_waterReflection->statistics();
            require(reflection.frameSerial==static_cast<std::uint64_t>(m_frameCount) && reflection.sceneRevision==m_referenceEditFrameSceneRevision,
                "reflection did not update this original frame/revision");
        }

        static void lifecycleRequire(bool condition,const char* reason)
        { if(!condition)throw std::runtime_error(std::string("Render lifecycle: ")+reason); }
        static std::string lifecycleAddress(const void* object)
        { std::ostringstream out;out<<object;return RenderLifecycle::quote(out.str()); }
        void lifecycleContext()
        {
            if(!m_lifecycleProbe)return;
            std::string id;std::ifstream input(std::filesystem::path(m_lifecycleWorldDirectory)/"world.meta");
            for(std::string line;std::getline(input,line);)if(line.compare(0,9,"world_id ")==0){id=line.substr(9);break;}
            lifecycleRequire(!id.empty(),"Actual clone world identity missing.");
            m_lifecycleProbe->context(m_lifecycleWorldEpoch,m_world,m_lifecycleWorldDirectory,id);
        }
        unsigned lifecycleCameraCount() const
        {
            if(!m_sceneManager)return 0;
            unsigned count=0;auto it=m_sceneManager->getCameraIterator();while(it.hasMoreElements()){it.getNext();++count;}return count;
        }
        static std::vector<std::string> lifecycleManagerNames(Ogre::ResourceManager& manager)
        {
            std::vector<std::string> names;auto it=manager.getResourceIterator();
            while(it.hasMoreElements()){const auto resource=it.getNext();names.push_back(resource->getGroup()+":"+resource->getName());}
            std::sort(names.begin(),names.end());return names;
        }
        std::string lifecycleSnapshot() const
        {
            std::ostringstream o;o<<std::boolalpha;
            o<<"{\"root_alive\":"<<bool(m_root)<<",\"scene_alive\":"<<(m_sceneManager!=nullptr)
             <<",\"root_instance\":"<<lifecycleAddress(m_root.get())<<",\"scene_instance\":"<<lifecycleAddress(m_sceneManager)
             <<",\"window_instance\":"<<lifecycleAddress(m_window)<<",\"world_present\":"<<(m_world!=nullptr)
             <<",\"sandbox_present\":"<<bool(m_sandbox)<<",\"sandbox_instance\":"<<lifecycleAddress(m_sandbox.get())
             <<",\"loader_lifetime_owner_present\":"<<bool(m_sandbox)<<",\"normal_close_save_and_join_returned\":"<<m_lifecycleCloseReturned<<",\"hidden\":"<<m_hiddenWindow
             <<",\"input_event_count\":"<<m_lifecycleInputEvents<<",\"perf_enabled\":"<<RuntimePerformanceCapture::isEnabled()
             <<",\"requested_points\":["<<m_lifecyclePointWidth<<','<<m_lifecyclePointHeight<<']'
             <<",\"window_pixels\":["<<(m_window?m_window->getWidth():0)<<','<<(m_window?m_window->getHeight():0)<<']';
            const auto* viewport=m_window && m_window->getNumViewports()?m_window->getViewport(0):nullptr;
            o<<",\"main_viewport_bound\":"<<(m_camera && viewport && m_camera->getViewport()==viewport)
             <<",\"viewport_pixels\":["<<(viewport?viewport->getActualWidth():0)<<','<<(viewport?viewport->getActualHeight():0)<<']'
             <<",\"hdr_component\":"<<bool(m_hdrPipeline)<<",\"planar_component\":"<<bool(m_waterReflection)
             <<",\"hdr\":"<<(m_hdrPipeline?m_hdrPipeline->lifecycleFacts():RenderLifecycleTargetFacts{}).json()
             <<",\"planar\":"<<(m_waterReflection?m_waterReflection->lifecycleFacts():RenderLifecycleTargetFacts{}).json()
             <<",\"scene_camera_count\":"<<lifecycleCameraCount()
             <<",\"cache\":{\"sections\":"<<m_sectionVisuals.size()<<",\"batches\":"<<m_terrainBatchVisuals.size()
             <<",\"dirty_batches\":"<<m_dirtyTerrainBatches.size()<<",\"render_states\":"<<m_sectionRenderStates.size()
             <<",\"last_live_sections\":"<<m_lastLiveSections.size()
             <<",\"empty_section_upload_identities\":"<<m_emptySectionUploadIdentities.size()
             <<",\"material_identity_revisions\":"<<m_materialIdentityMeshRevisions.size()
             <<",\"local_lights\":"<<m_localLights.count
             <<",\"dynamic_shadow_off\":"<<(m_directionalShadowQuality==DirectionalShadowQuality::Off &&
                 !m_directionalSunLight && !m_directionalSunNode &&
                 (!m_sceneManager || m_sceneManager->getShadowTechnique()==Ogre::SHADOWTYPE_NONE))<<'}';
            o<<",\"managers_available\":"<<bool(m_root)<<",\"manager\":";
            if(!m_root)o<<"null";
            else {
                o<<'{';bool comma=false;
                const auto manager=[&](const char* name,Ogre::ResourceManager& value){if(comma)o<<',';comma=true;const auto names=lifecycleManagerNames(value);o<<RenderLifecycle::quote(name)<<":{\"count\":"<<names.size()<<",\"memory_bytes\":"<<value.getMemoryUsage()<<",\"names\":"<<RenderLifecycle::namesJson(names)<<'}';};
                manager("texture",Ogre::TextureManager::getSingleton());manager("material",Ogre::MaterialManager::getSingleton());
                manager("mesh",Ogre::MeshManager::getSingleton());manager("program",Ogre::HighLevelGpuProgramManager::getSingleton());
                manager("compositor",Ogre::CompositorManager::getSingleton());o<<'}';
            }
            o<<'}';return o.str();
        }
        void lifecycleLiveReady() const
        {
            lifecycleRequire(m_world && m_sandbox && m_hdrPipeline && m_waterReflection,"Actual world/components missing.");
            const auto h=m_hdrPipeline->lifecycleFacts(),p=m_waterReflection->lifecycleFacts();
            const auto* viewport=m_window->getViewport(0);
            lifecycleRequire(m_camera->getViewport()==viewport,"Main camera retained a non-main viewport.");
            lifecycleRequire(h.active && h.targetCount==1 && h.depthCount==1 && h.ownedDepthAttached && h.depthPool==Ogre::DepthBuffer::POOL_NO_DEPTH && h.native.colour.samples==4 &&
                RenderLifecycle::valid(h.native,unsigned(viewport->getActualWidth()),unsigned(viewport->getActualHeight()),4),"Actual HDR attachment/4samples invalid.");
            lifecycleRequire(p.active && p.updateCount>0 && p.targetCount==1 && p.depthCount==1 && p.ownedDepthAttached && p.depthPool==Ogre::DepthBuffer::POOL_NO_DEPTH && p.cameraCount==1 && p.privateMaterials>0 &&
                p.privateMaterials<=PlanarWaterReflection::MaximumPrivateMaterials && p.privatePasses<=PlanarWaterReflection::MaximumPrivatePasses &&
                p.waterSamplerBound && p.binderBound && !p.listenersActive && !h.observerFailures && !p.observerFailures,"Real reflected residents or scoped ownership absent.");
            lifecycleRequire(RenderLifecycle::valid(p.native,p.width,p.height,0) && p.width==(h.width+1)/2 && p.height==(h.height+1)/2 &&
                std::uint64_t(h.width)*h.height<=8294400 && std::uint64_t(p.width)*p.height<=PlanarWaterReflection::MaximumPixels,"Real target dimensions/budget invalid.");
            lifecycleRequire(m_lifecycleInputEvents==0 && m_hiddenWindow,"Ordinary input encountered in hidden probe.");
        }
        void lifecycleEmptyReady(bool establishBaseline)
        {
            lifecycleRequire(!m_world && !m_sandbox && !m_logicCamera && !m_worldPlayer,"Normal World/Sandbox unload did not complete.");
            lifecycleRequire(m_sectionVisuals.empty() && m_terrainBatchVisuals.empty() && m_dirtyTerrainBatches.empty() &&
                m_sectionRenderStates.empty() && m_lastLiveSections.empty() && m_emptySectionUploadIdentities.empty() && m_materialIdentityMeshRevisions.empty() &&
                m_localLights.count==0,"Resident visual/cache/local light survived world close.");
            lifecycleRequire(m_directionalShadowQuality==DirectionalShadowQuality::Off && !m_directionalSunLight &&
                !m_directionalSunNode && m_sceneManager->getShadowTechnique()==Ogre::SHADOWTYPE_NONE,
                "Dynamic shadow resources survived world close.");
            const auto p=m_waterReflection->lifecycleFacts();
            lifecycleRequire(!p.active && p.targetCount==0 && p.depthCount==0 && p.privateMaterials==0 && p.privatePasses==0 && p.cameraCount==1 &&
                !p.selected && !p.binderBound && !p.waterSamplerBound && !p.lodCameraBound && !p.listenersActive && !p.observerFailures,"Planar world-reset retained owned resources/references.");
            lifecycleRequire(m_hdrPipeline->active(),"Menu must retain the active HDR target.");
            lifecycleRequire(m_camera && m_camera->getViewport()==m_window->getViewport(0),
                "Menu camera retained a non-main viewport.");
            const std::array<Ogre::ResourceManager*,5> managers{{&Ogre::TextureManager::getSingleton(),&Ogre::MaterialManager::getSingleton(),&Ogre::MeshManager::getSingleton(),&Ogre::HighLevelGpuProgramManager::getSingleton(),&Ogre::CompositorManager::getSingleton()}};
            for(std::size_t i=0;i<managers.size();++i) {
                const auto names=lifecycleManagerNames(*managers[i]);
                if(establishBaseline)m_lifecycleEmptyManagerNames[i]=names;
                else if(names!=m_lifecycleEmptyManagerNames[i]) {
                    constexpr std::array<const char*,5> kinds{{"texture","material","mesh","program","compositor"}};
                    std::vector<std::string> added,removed;
                    std::set_difference(names.begin(),names.end(),m_lifecycleEmptyManagerNames[i].begin(),
                        m_lifecycleEmptyManagerNames[i].end(),std::back_inserter(added));
                    std::set_difference(m_lifecycleEmptyManagerNames[i].begin(),m_lifecycleEmptyManagerNames[i].end(),
                        names.begin(),names.end(),std::back_inserter(removed));
                    std::cerr<<"[RENDER_LIFECYCLE_MANAGER_DIFFERENCE] kind="<<kinds[i]
                             <<" added="<<RenderLifecycle::namesJson(added)
                             <<" removed="<<RenderLifecycle::namesJson(removed)<<'\n';
                    lifecycleRequire(false,"Warm menu ResourceManager names changed after world cycle.");
                }
            }
        }
        void advanceRenderLifecycleProbe()
        {
            auto& probe=*m_lifecycleProbe;probe.observeFrame(unsigned(m_frameCount));lifecycleContext();
            const unsigned stage=probe.stage();if(stage>=11)return;
            if(!probe.begun()) {
                probe.begin(unsigned(m_frameCount),lifecycleSnapshot());
                if(stage>=1 && stage<=3) {
                    const auto old=m_hdrPipeline->lifecycleFacts();m_lifecycleExpectedHdrGeneration=old.generation+1;
                    const unsigned ratio=unsigned(m_window->getWidth())/m_lifecyclePointWidth;
                    lifecycleRequire((ratio==1 || ratio==2) && m_window->getHeight()==m_lifecyclePointHeight*ratio,"Unknown native window point/pixel scale.");
                    m_lifecyclePointWidth=stage==1?960u:stage==2?1600u:1280u;
                    m_lifecyclePointHeight=stage==1?540u:stage==2?900u:720u;
                    m_lifecycleExpectedWidth=m_lifecyclePointWidth*ratio;m_lifecycleExpectedHeight=m_lifecyclePointHeight*ratio;
                    m_window->resize(m_lifecyclePointWidth,m_lifecyclePointHeight);
                    // Same public native resize callback used by Cocoa delegates.
                    // Next normal frame messagePump/beforeFrame performs target resize.
                    m_window->windowMovedOrResized();windowResized(m_window);return;
                }
                if(stage==4 || stage==7 || stage==10) {
                    lifecycleLiveReady();
                    lifecycleRequire(clearActiveWorld(true),"Normal save/closeAllWorlds/stop-loader join failed.");
                    m_lifecycleCloseReturned=true;
                    m_applicationFlow.returnToMainMenu();lifecycleContext();lifecycleEmptyReady(stage==4);
                    probe.commit(unsigned(m_frameCount),lifecycleSnapshot());return;
                }
                if(stage==5 || stage==8) {
                    const std::string directory=stage==5?std::getenv("HELLOMINE3D_LIFECYCLE_SAVE_B"):std::getenv("HELLOMINE3D_SAVE_DIR");
                    lifecycleRequire(m_applicationFlow.beginLoading(stage==5?"lifecycle-directory-b":"lifecycle-directory-a"),"Normal loading transition rejected.");
                    buildTerrain(true,directory);
                    lifecycleRequire(configureDirectionalShadows(m_config.directionalShadowQuality),"Normal shadow rebuild failed.");
                    syncActorVisuals();m_userInterface->setWorldContext(m_worldPlayer,m_world);
                    lifecycleRequire(m_applicationFlow.completeLoading(true),"Normal loading completion rejected.");
                    lifecycleContext();probe.commit(unsigned(m_frameCount),lifecycleSnapshot());return;
                }
            }
            // Wait for actual normal frames and actual reflection scene draws;
            // no mock/native-field rewrite or direct HDR definition mutation.
            if(unsigned(m_frameCount)-probe.stageFrame()<12)return;
            if(!m_waterReflection->statistics().active || m_waterReflection->statistics().privateMaterials==0)return;
            lifecycleLiveReady();
            if(stage>=1 && stage<=3) {
                const auto h=m_hdrPipeline->lifecycleFacts();
                lifecycleRequire(m_window->getWidth()==m_lifecycleExpectedWidth && m_window->getHeight()==m_lifecycleExpectedHeight &&
                    unsigned(m_camera->getViewport()->getActualWidth())==m_lifecycleExpectedWidth && unsigned(m_camera->getViewport()->getActualHeight())==m_lifecycleExpectedHeight &&
                    h.width==m_lifecycleExpectedWidth && h.height==m_lifecycleExpectedHeight && h.generation==m_lifecycleExpectedHdrGeneration,"Real window/HDR resize or generation did not change exactly once.");
            }
            probe.commit(unsigned(m_frameCount),lifecycleSnapshot());
        }

        bool shoreSectionSelected(glm::ivec3 section) const
        {
            const glm::ivec3 targetSection(
                int(std::floor(m_shoreTarget.x / double(CHUNK_SIZE))), 4,
                int(std::floor(m_shoreTarget.z / double(CHUNK_SIZE))));
            return section.x == targetSection.x && section.z == targetSection.z &&
                (section.y == 3 || section.y == 4);
        }

        void retainShoreUploadInput(SectionVisual& visual, const ChunkMesh& mesh,
                                   const std::string& material, glm::ivec3 section)
        {
            if (!m_shoreEditCapture || !shoreSectionSelected(section) ||
                (material != "HelloMine3D/Water" && material != "HelloMine3D/Terrain")) return;
            const auto& cpu = mesh.getClientMesh();
            const auto bytes = cpu.vertexPositions.size() / 3 * sizeof(TerrainRenderVertex) +
                cpu.indices.size() * sizeof(std::uint32_t);
            if (bytes > 16u * 1024u * 1024u)
                throw std::runtime_error("Shore direct original CPU upload input exceeds bound.");
            (material == "HelloMine3D/Water" ? visual.shoreDiagnosticWater :
                visual.shoreDiagnosticSolid) = std::make_unique<ChunkMesh>(mesh);
        }

        std::vector<ShoreEditCapture::Binding> collectShoreEditBindings()
        {
            const auto snapshot = m_world->collectSectionMeshSnapshot(false);
            std::vector<ShoreEditCapture::Binding> result;
            // The snapshot offers at most eight ready meshes. A deferred mesh
            // must not be mistaken for clean merely because it was not offered.
            if (snapshot.cpuReadyTotal != 0) return {};
            auto stateFor = [&](glm::ivec3 location, ShoreEditCapture::Part& part) {
                const auto live = std::find_if(snapshot.liveSectionVersions.begin(),
                    snapshot.liveSectionVersions.end(), [&](const auto& value) { return value.location == location; });
                const auto key = sectionKey(location);
                const auto uploaded = m_materialIdentityMeshRevisions.find(key);
                const auto state = m_sectionRenderStates.find(key);
                if (live == snapshot.liveSectionVersions.end() || uploaded == m_materialIdentityMeshRevisions.end() ||
                    state == m_sectionRenderStates.end()) return false;
                const bool ready = std::any_of(snapshot.cpuReadySections.begin(), snapshot.cpuReadySections.end(),
                    [&](const auto& value) { return value.location == location; });
                part = {location, uploaded->second, live->blockRevision, true,
                    state->second == ChunkRenderState::GpuResident, ready};
                return part.gpuResident && !part.stillCpuReady && part.uploadRevision == part.liveRevision;
            };
            auto append = [&](SectionVisual& visual, const std::string& key, bool batched) {
                for (auto& object : visual.renderables)
                {
                    const auto material = object->getMaterial()->getName();
                    if (material != "HelloMine3D/Water" && material != "HelloMine3D/Terrain") continue;
                    if (batched && material != "HelloMine3D/Terrain") continue;
                    ShoreEditCapture::Binding binding;
                    binding.renderable = object.get(); binding.origin = visual.location;
                    binding.ownerKey = key; binding.layer = material == "HelloMine3D/Water" ? "water" : "solid";
                    binding.uploadSerial = visual.shoreUploadSerial;
                    std::vector<TerrainRenderBatchPart> parts;
                    if (batched)
                    {
                        for (int y = 0; y < TerrainRenderBatchSections; ++y)
                        {
                            const glm::ivec3 location(visual.location.x, visual.location.y + y, visual.location.z);
                            const auto source = m_sectionVisuals.find(sectionKey(location));
                            if (source == m_sectionVisuals.end() || !source->second.batchMeshes) continue;
                            const auto& mesh = source->second.batchMeshes->solidMesh;
                            if (mesh.getClientMesh().indices.empty()) continue;
                            ShoreEditCapture::Part part;
                            if (!stateFor(location, part)) return false;
                            binding.parts.push_back(part); parts.push_back({location, &mesh});
                        }
                    }
                    else
                    {
                        const auto& mesh = material == "HelloMine3D/Water" ? visual.shoreDiagnosticWater : visual.shoreDiagnosticSolid;
                        if (!mesh) return false;
                        ShoreEditCapture::Part part;
                        if (!stateFor(visual.location, part)) return false;
                        binding.parts.push_back(part); parts.push_back({visual.location, mesh.get()});
                    }
                    std::size_t bytes = 0;
                    for (const auto& part : parts)
                        bytes += part.mesh->getClientMesh().vertexPositions.size() / 3 * sizeof(TerrainRenderVertex) +
                            part.mesh->getClientMesh().indices.size() * sizeof(std::uint32_t);
                    if (parts.empty() || bytes > 16u * 1024u * 1024u)
                        throw std::runtime_error("Shore original operation CPU pack exceeds bound or has no parts.");
                    binding.cpu = packTerrainRenderBatch(parts, binding.origin);
                    result.push_back(std::move(binding));
                }
                return true;
            };
            // Capture both sides of the vertical section boundary. Complete
            // solid batches keep the exact original ascending constructor order.
            const glm::ivec3 top(int(std::floor(m_shoreTarget.x / double(CHUNK_SIZE))), 4,
                int(std::floor(m_shoreTarget.z / double(CHUNK_SIZE))));
            for (int y : {3, 4})
            {
                const glm::ivec3 section(top.x, y, top.z);
                ShoreEditCapture::Part state;
                if (!stateFor(section, state)) return {};
                const auto found = m_sectionVisuals.find(sectionKey(section));
                if (found != m_sectionVisuals.end() && !append(found->second, found->first, false)) return {};
                const auto batch = m_terrainBatchVisuals.find(sectionKey(terrainRenderBatchOrigin(section)));
                if (batch != m_terrainBatchVisuals.end() && !append(batch->second, batch->first, true)) return {};
            }
            if (result.empty() || result.size() > 8 ||
                std::none_of(result.begin(), result.end(), [](const auto& b) { return b.layer == "water"; })) return {};
            return result;
        }

        OgreSurfaceMapDiagnosticSample shoreExpectedSurface() const
        {
            return {true, m_shorePhase == 4 ? 63 : 64,
                int(m_shorePhase == 3 ? BlockId::Sand : BlockId::Water)};
        }

        static bool shoreSampleMatches(const OgreSurfaceMapDiagnosticSample& a,
                                       const OgreSurfaceMapDiagnosticSample& b)
        { return a.known == b.known && a.height == b.height && a.blockId == b.blockId; }

        void prepareShoreEditCapture()
        {
            try
            {
            if (!m_shoreEditCapture || m_shoreComplete || !m_world || !m_worldPlayer || !m_userInterface) return;
            const auto now = std::chrono::steady_clock::now();
            if (m_shoreStarted == std::chrono::steady_clock::time_point{})
            { m_shoreStarted = now; m_shorePhaseStarted = now; }
            if (now - m_shoreStarted > std::chrono::seconds(60) ||
                now - m_shorePhaseStarted > std::chrono::seconds(10))
            {
                m_shoreEditCapture->retainNativeFailure("bounded shore phase/session wait expired without all original same-frame draw/UI endpoints");
                throw std::runtime_error("Shore edit diagnostic could not join actual World/UI/upload revisions within its bounded phase.");
            }
            if (!m_shoreInitialized)
            {
                const auto p = m_shoreTarget;
                m_shoreOriginalTop = m_world->getBlock(p.x, p.y, p.z);
                m_shoreOriginalLower = m_world->getBlock(p.x, p.y - 1, p.z);
                if (m_shoreOriginalTop.id != Block_t(BlockId::Water) ||
                    m_shoreOriginalLower.id != Block_t(BlockId::Water) ||
                    m_world->getBlock(p.x, p.y + 1, p.z).id != Block_t(BlockId::Air))
                    throw std::runtime_error("Shore target disagrees with the preserved current natural World locator.");
                PlayerSaveState inventory = m_worldPlayer->getSaveState();
                inventory.inventory.assign(5, {}); inventory.inventory[0] = {Material::Sand, 2, 0};
                inventory.heldItem = 0; m_worldPlayer->applySaveState(inventory);
                m_userInterface->configureSurfaceMapDiagnostic(p.x, p.z);
                if (!m_userInterface->setSurfaceMapDiagnosticView(SurfaceMapDiagnosticView::Flat))
                    throw std::runtime_error("Shore baseline cannot open the actual Flat page.");
                m_shoreInitialized = true; m_shoreActionApplied = true;
            }
            if (!m_shoreActionApplied)
            {
                const auto p = m_shoreTarget;
                m_shoreEditUiFrame = m_userInterface->surfaceMapDiagnosticFacts().frameId;
                if (!m_userInterface->setSurfaceMapDiagnosticView(SurfaceMapDiagnosticView::Hud))
                    throw std::runtime_error("Shore edit cannot switch to actual HUD256 sampling.");
                if (m_shorePhase == 1 || m_shorePhase == 3)
                    m_world->addCommand<PlayerBlockInteractionCommand>(PlayerBlockInteractionAction::Place,
                        glm::vec3(p.x, m_shorePhase == 1 ? p.y - 1 : p.y, p.z), *m_worldPlayer);
                else if (m_shorePhase == 4)
                    m_world->addCommand<PlayerBlockInteractionCommand>(PlayerBlockInteractionAction::Break,
                        glm::vec3(p), *m_worldPlayer);
                else
                {
                    const int y = m_shorePhase == 2 ? p.y - 1 : p.y;
                    const auto original = m_shorePhase == 2 ? m_shoreOriginalLower : m_shoreOriginalTop;
                    m_world->setBlock(p.x, y, p.z, original);
                }
                m_shoreActionApplied = true; m_shoreWaitHud = true; m_shorePhaseStarted = now;
            }
            if (m_shoreWaitHud)
            {
                const auto facts = m_userInterface->surfaceMapDiagnosticFacts();
                const auto expected = shoreExpectedSurface();
                if (facts.targetObservedFrame > m_shoreEditUiFrame &&
                    facts.targetObservedView == SurfaceMapDiagnosticView::Hud && facts.targetObservedStep == 4 &&
                    shoreSampleMatches(facts.lastObservedTarget, expected) && facts.fineTarget.available &&
                    shoreSampleMatches(facts.fineTarget.surface, expected))
                {
                    // Freeze the actual HUD reply before Flat can query the
                    // target. Flat's own refresh cannot conceal a stale HUD.
                    m_shoreHudBeforeFlat = facts;
                    if (!m_userInterface->setSurfaceMapDiagnosticView(SurfaceMapDiagnosticView::Flat))
                        throw std::runtime_error("Shore edit cannot reopen Flat after actual HUD observation.");
                    m_shoreWaitHud = false;
                }
            }
            }
            catch (const std::exception& error)
            {
                if (m_shoreEditCapture) m_shoreEditCapture->retainNativeFailure(error.what());
                throw;
            }
        }

        void writeShoreWorldMapFacts(const OgreSurfaceMapDiagnosticFacts& facts)
        {
            const auto path = std::filesystem::path(m_shoreEditCapture->phaseJsonPath());
            std::ofstream out(path.parent_path() / (path.stem().string() + "-world-map.json"));
            out << std::boolalpha;
            auto sample = [&](const auto& s) { out << "{\"known\":" << s.known << ",\"height\":" << s.height << ",\"block_id\":" << s.blockId << '}'; };
            auto cell = [&](const auto& c) {
                out << "{\"available\":" << c.available << ",\"world_x\":" << c.worldX << ",\"world_z\":" << c.worldZ << ",\"step\":" << c.step << ",\"surface\":";
                sample(c.surface); out << '}';
            };
            auto draw = [&](const auto& d) {
                out << "{\"submitted\":" << d.submitted << ",\"layer\":" << int(d.layer)
                    << ",\"step\":" << d.step << ",\"colour\":" << d.colour
                    << ",\"rect\":[" << d.left << ',' << d.top << ',' << d.right << ',' << d.bottom
                    << "],\"vertices\":[" << d.vertexBegin << ',' << d.vertexEnd
                    << "],\"indices\":[" << d.indexBegin << ',' << d.indexEnd << "],\"surface\":";
                sample(d.surface); out << '}';
            };
            const auto p = m_shoreTarget;
            out << "{\"schema\":\"hellomine3d-shore-edit-world-map-v1\",\"normal_input\":false,\"phase\":" << m_shorePhase
                << ",\"target\":[" << p.x << ',' << p.y << ',' << p.z << "],\"ui_frame\":" << facts.frameId
                << ",\"edit_ui_frame\":" << m_shoreEditUiFrame << ",\"inventory_sand\":" << m_worldPlayer->getInventoryCount(Material::Sand)
                << ",\"held_material\":" << int(m_worldPlayer->getHeldItems().getMaterial().id)
                << ",\"held_amount\":" << m_worldPlayer->getHeldItems().getNumInStack() << ",\"column\":[";
            // Independently readable real resident Water depth, including the
            // four corner-source columns used by water top vertices.
            for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx != -1 || dz != -1) out << ',';
                out << "{\"x\":" << p.x + dx << ",\"z\":" << p.z + dz << ",\"blocks\":[";
                for (int y = 56; y <= 65; ++y)
                {
                    if (y != 56) out << ',';
                    const auto b = m_world->getBlock(p.x + dx, y, p.z + dz);
                    out << '[' << y << ',' << int(b.id) << ',' << int(b.metadata) << ']';
                }
                out << "]}";
            }
            out << "],\"fine_target\":"; cell(facts.fineTarget);
            out << ",\"fine_west\":"; cell(facts.fineWest);
            out << ",\"fine_north\":"; cell(facts.fineNorth);
            out << ",\"flat_live_target\":"; cell(facts.flatLiveTarget);
            out << ",\"production_resolution\":"; cell(facts.diagnosticResolution);
            out << ",\"draw_target\":"; draw(facts.drawTarget);
            out << ",\"draw_west\":"; draw(facts.drawWest);
            out << ",\"draw_north\":"; draw(facts.drawNorth);
            out << ",\"backend_submitted\":" << facts.backendSubmitted
                << ",\"backend_vertex_count\":" << facts.backendVertexCount << ",\"backend_index_count\":" << facts.backendIndexCount
                << ",\"framebuffer_scale\":[" << facts.framebufferScaleX << ',' << facts.framebufferScaleY
                << "],\"flat_target_queried_this_frame\":" << (facts.targetQueried && facts.queryView == SurfaceMapDiagnosticView::Flat)
                << ",\"hud_before_flat\":{\"frame\":" << m_shoreHudBeforeFlat.frameId
                << ",\"target_observed_frame\":" << m_shoreHudBeforeFlat.targetObservedFrame
                << ",\"view\":" << int(m_shoreHudBeforeFlat.targetObservedView)
                << ",\"step\":" << m_shoreHudBeforeFlat.targetObservedStep << ",\"sample\":";
            sample(m_shoreHudBeforeFlat.lastObservedTarget);
            out << ",\"fine_target\":"; cell(m_shoreHudBeforeFlat.fineTarget);
            out << "},\"scope_open\":[\"ordinary_input\",\"inner_vao_fetch\",\"incarnation_ABA\",\"actual_mouse_selection\"]}\n";
            if (!out) throw std::runtime_error("Shore World/UI evidence write failed.");
        }

        void finishShoreEditFrame()
        {
            try
            {
            if (!m_shoreEditCapture || !m_shoreInitialized || m_shoreWaitHud || m_shoreComplete) return;
            const bool nativeReady = !m_shoreNativeDraw ||
                m_shoreEditCapture->finishNativeFrame(collectShoreEditBindings());
            const auto facts = m_userInterface->surfaceMapDiagnosticFacts();
            const auto expected = shoreExpectedSurface();
            if (facts.activeView != SurfaceMapDiagnosticView::Flat || !facts.backendSubmitted ||
                !facts.fineTarget.available || !shoreSampleMatches(facts.fineTarget.surface, expected) ||
                !facts.drawTarget.submitted || facts.drawTarget.layer != SurfaceMapDiagnosticLayer::FineHistory ||
                !shoreSampleMatches(facts.drawTarget.surface, expected) ||
                !facts.diagnosticResolution.available || !shoreSampleMatches(facts.diagnosticResolution.surface, expected) ||
                !facts.fineWest.available || !facts.fineNorth.available ||
                !facts.drawWest.submitted || !facts.drawNorth.submitted) return;
            const auto p = m_shoreTarget;
            const auto top = m_world->getBlock(p.x, p.y, p.z);
            const auto lower = m_world->getBlock(p.x, p.y - 1, p.z);
            const int topId = int(m_shorePhase == 3 ? BlockId::Sand : m_shorePhase == 4 ? BlockId::Air : BlockId::Water);
            if (top.id != topId || lower.id != Block_t(m_shorePhase == 1 ? BlockId::Sand : BlockId::Water))
                throw std::runtime_error("Shore production command did not produce the expected actual resident block.");
            constexpr int inventory[]{2,1,1,0,1,1};
            if (m_worldPlayer->getInventoryCount(Material::Sand) != inventory[m_shorePhase])
                throw std::runtime_error("Shore production inventory consumption/drop disagrees with command.");
            auto bindings = collectShoreEditBindings();
            if (bindings.empty()) return;
            if (!nativeReady) return;
            constexpr const char* phases[]{"baseline_flat", "submerged_sand_flat", "restored_depth_flat",
                "top_sand_flat", "lowered_water_flat", "restored_flat"};
            m_shoreEditCapture->capturePhase(phases[m_shorePhase],
                runtimeTerrainMaterialProfile().usesTextureArray() ? "standard" : "compatibility", facts.frameId, bindings);
            writeShoreWorldMapFacts(facts);
            m_window->writeContentsToFile(m_shoreEditCapture->phasePngPath());
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("Shore actual framebuffer PNG reported GL error.");
            std::cout << "[SHORE_EDIT_PHASE] phase=" << m_shorePhase << " ui_frame=" << facts.frameId
                << " original_objects=" << bindings.size() << " sand=" << inventory[m_shorePhase] << '\n';
            ++m_shorePhase; m_shoreActionApplied = false;
            m_shorePhaseStarted = std::chrono::steady_clock::now();
            if (m_shorePhase == 6)
            {
                if (!m_world->save()) throw std::runtime_error("Shore restored actual world save failed.");
                std::ofstream index(std::filesystem::path(m_shoreEditOutput) / "index.json");
                index << "{\"schema\":\"hellomine3d-shore-edit-capture-v1\",\"status\":\"CAPTURED\",\"normal_input\":false,\"restored_save_succeeded\":true,\"frames\":[";
                for (int i = 0; i < 6; ++i) { if (i) index << ','; index << "\"phase-00" << i << ".png\""; }
                index << "],\"scope_open\":[\"ordinary_input\",\"reopen_validation\",";
                if (!m_shoreNativeDraw) index << "\"inner_vao_fetch\",";
                index << "\"incarnation_ABA\"]}" << '\n';
                if (!index) throw std::runtime_error("Shore index write failed.");
                m_shoreComplete = true;
            }
            }
            catch (const std::exception& error)
            {
                if (m_shoreEditCapture) m_shoreEditCapture->retainNativeFailure(error.what());
                throw;
            }
        }

        void observeShoreNativeDraw()
        {
            if (!m_shoreNativeDraw || !m_shoreEditCapture || !m_shoreInitialized ||
                m_shoreWaitHud || m_shoreComplete) return;
            // updateSandbox has completed the actual production uploader before
            // this arm. No object's result is carried from an earlier frame.
            auto bindings = collectShoreEditBindings();
            if (!bindings.empty()) m_shoreEditCapture->beginNativeFrame(m_frameCount, std::move(bindings));
        }

        // Same ordinary World.update/mesh uploader as the client; the only
        // fixture is a fixed camera in a fresh natural World, never setBlock.
        std::vector<FloraWindCapture::Binding> collectFloraWindBindings()
        {
            if (!m_floraWindCapture || !m_world) return {};
            const std::array<glm::ivec3, 2> sources{{{966, 80, -19}, {966, 80, -18}}};
            const auto snapshot = m_world->collectSectionMeshSnapshot(false);
            std::vector<FloraWindCapture::Binding> result;
            auto stateFor = [&](glm::ivec3 location, FloraWindCapture::PartState& state) {
                const auto current = std::find_if(snapshot.liveSectionVersions.begin(),
                    snapshot.liveSectionVersions.end(), [&](const auto& x) { return x.location == location; });
                const auto key = sectionKey(location);
                const auto uploaded = m_materialIdentityMeshRevisions.find(key);
                const auto gpu = m_sectionRenderStates.find(key);
                if (current == snapshot.liveSectionVersions.end() ||
                    uploaded == m_materialIdentityMeshRevisions.end() ||
                    uploaded->second != current->blockRevision || gpu == m_sectionRenderStates.end() ||
                    gpu->second != ChunkRenderState::GpuResident) return false;
                // This existing diagnostic does not retain incarnation; preserve
                // this gap instead of reading the loader's unlocked Chunk map.
                state.section = location; state.incarnationKnown = false;
                state.liveRevision = current->blockRevision; state.uploadRevision = uploaded->second;
                state.gpuResident = true;
                return true;
            };
            auto append = [&](SectionVisual& visual, bool batched, const std::string& ownerKey) {
                for (auto& object : visual.renderables)
                {
                    if (object->getMaterial()->getName() != "HelloMine3D/Flora") continue;
                    std::vector<FloraWindCapture::SourceBlock> selected;
                    for (auto p : sources)
                    {
                        const glm::ivec3 section(int(std::floor(p.x/double(CHUNK_SIZE))),
                            int(std::floor(p.y/double(CHUNK_SIZE))),int(std::floor(p.z/double(CHUNK_SIZE))));
                        if (sectionKey(batched?terrainRenderBatchOrigin(section):section)==ownerKey)
                            selected.push_back({p});
                    }
                    if (selected.empty()) continue;
                    FloraWindCapture::Binding binding; binding.renderable = object.get();
                    // Startup's direct visual does not store its section location;
                    // derive the actual constructor origin from its existing node.
                    Ogre::Matrix4 actualWorld; object->getWorldTransforms(&actualWorld);
                    const auto translation=actualWorld.getTrans();
                    auto sectionAxis=[](float x) {
                        const double a=double(x)/CHUNK_SIZE;
                        if (!std::isfinite(a) || a<std::numeric_limits<int>::min() ||
                            a>std::numeric_limits<int>::max() || std::floor(a)!=a)
                            throw std::runtime_error("Natural Flora object has an invalid section translation.");
                        return static_cast<int>(a);
                    };
                    binding.origin={sectionAxis(translation.x),sectionAxis(translation.y),sectionAxis(translation.z)};
                    if (sectionKey(binding.origin)!=ownerKey)
                        throw std::runtime_error("Natural Flora node origin disagrees with its actual renderer owner key.");
                    binding.sources = std::move(selected);
                    std::vector<TerrainRenderBatchPart> parts;
                    if (batched)
                    {
                        for (int y=0; y<TerrainRenderBatchSections; ++y)
                        {
                            const glm::ivec3 location(binding.origin.x,binding.origin.y+y,binding.origin.z);
                            const auto existing = m_sectionVisuals.find(sectionKey(location));
                            if (existing == m_sectionVisuals.end() || !existing->second.batchMeshes) continue;
                            const auto& mesh = existing->second.batchMeshes->floraMesh;
                            if (mesh.getClientMesh().indices.empty()) continue;
                            FloraWindCapture::PartState state;
                            if (!stateFor(location,state)) return false;
                            binding.parts.push_back(state); parts.push_back({location,&mesh});
                        }
                    }
                    else
                    {
                        FloraWindCapture::PartState state;
                        if (!stateFor(binding.origin,state)) return false;
                        // Startup copied before the loader began, or copied
                        // from the actual uploader's locked snapshot input.
                        // Never dereference World CPU vectors concurrently here.
                        if (!visual.fernDiagnosticFlora) return false;
                        binding.parts.push_back(state);
                        parts.push_back({binding.origin,visual.fernDiagnosticFlora.get()});
                    }
                    std::size_t vertexBytes=0,indexBytes=0;
                    for (const auto& part : parts)
                    {
                        vertexBytes += part.mesh->getClientMesh().vertexPositions.size()/3*sizeof(TerrainRenderVertex);
                        indexBytes += part.mesh->getClientMesh().indices.size()*sizeof(std::uint32_t);
                    }
                    if (vertexBytes>16u*1024u*1024u || indexBytes>16u*1024u*1024u-vertexBytes)
                        throw std::runtime_error("Natural Flora operation exceeds the bounded original buffer observer.");
                    binding.cpu = packTerrainRenderBatch(parts,binding.origin);
                    result.push_back(std::move(binding));
                }
                return true;
            };
            for (auto& pair : m_sectionVisuals) if (!append(pair.second,false,pair.first)) return {};
            for (auto& pair : m_terrainBatchVisuals) if (!append(pair.second,true,pair.first)) return {};
            std::size_t found=0;
            for (auto p:sources)
            {
                unsigned matches=0;
                for (const auto& binding:result) for (const auto& source:binding.sources) matches += source.position==p;
                if (matches>1) throw std::runtime_error("Ambiguous existing natural Fern GPU object.");
                found += matches;
            }
            return found==sources.size() ? result : std::vector<FloraWindCapture::Binding>{};
        }

        void prepareFloraWindCapture(float deltaSeconds)
        {
            if (!m_floraWindCapture || !m_world || m_floraWindCapture->isComplete()) return;
            const auto now=std::chrono::steady_clock::now();
            if (m_fernWindStarted==std::chrono::steady_clock::time_point{}) m_fernWindStarted=now;
            if (now-m_fernWindStarted>std::chrono::seconds(45))
                throw std::runtime_error("Natural Fern capture exceeded 45 seconds waiting for actual resident draws.");
            m_fernWindPhaseSeconds += std::isfinite(deltaSeconds) ? std::clamp(deltaSeconds,0.f,.25f) : 0.f;
            if (!m_fernWindPhaseApplied)
            {
                const auto requested=m_floraWindCapture->frameCount()<2 ? DirectionalShadowQuality::Off : DirectionalShadowQuality::High;
                if (!configureDirectionalShadows(requested))
                    throw std::runtime_error("Fern capture could not select its actual requested shadow receiver route.");
                m_config.directionalShadowQuality=requested;
                m_fernWindPhaseApplied=true;
                m_fernWindPhaseSeconds=0.f;
            }
        }

        void observeFloraWindCapture()
        {
            if (!m_floraWindCapture || m_floraWindCapture->isComplete() ||
                m_floraWindCapture->isFrameOpen() || !m_fernWindPhaseApplied || m_fernWindPhaseSeconds<.75f) return;
            if (m_world->getChunkManager().getTerrainSeed()!=20260807 ||
                m_world->getChunkManager().getTerrainGenerationVersion()!=30)
                throw std::runtime_error("Fern natural source requires its real seed20260807 terrain30 World.");
            auto bindings=collectFloraWindBindings();
            if (bindings.empty()) return;
            constexpr const char* phases[]{"Off0","Off1","High0","High1"};
            m_floraWindCapture->beginFrame(phases[m_floraWindCapture->frameCount()],
                runtimeTerrainMaterialProfile().usesTextureArray()?"standard":"compatibility",
                static_cast<std::uint64_t>(m_frameCount), *m_world, std::move(bindings));
        }

        void prepareCameraDiagnostics(float deltaSeconds)
        {
            if (!m_cameraDiagnostics || !m_world || !m_worldPlayer ||
                !m_logicCamera || m_cameraDiagnosticPhase >= 6) return;
            const auto now = std::chrono::steady_clock::now();
            if (m_cameraDiagnosticStarted == std::chrono::steady_clock::time_point{})
                m_cameraDiagnosticStarted = now;
            if (now - m_cameraDiagnosticStarted > std::chrono::seconds(45))
                throw std::runtime_error("Camera diagnostics exceeded the bounded resident run.");
            m_cameraDiagnosticPhaseSeconds += std::isfinite(deltaSeconds)
                ? std::clamp(deltaSeconds, 0.f, .25f) : 0.f;
            if (m_cameraDiagnosticPhasePlaced) return;

            // This fixture is confined to the new diagnostic world. Require the
            // existing resident columns before writing; getBlock never loads.
            const auto resident = m_world->collectSectionMeshSnapshot(false);
            for (int x : {-1, 0}) for (int z : {-1, 0})
                if (std::none_of(resident.liveSectionVersions.begin(),
                        resident.liveSectionVersions.end(), [&](const auto& version)
                        { return version.location.x == x && version.location.z == z; })) return;
            int existingMaximumY = -1;
            for (const auto& version : resident.liveSectionVersions)
                existingMaximumY = std::max(existingMaximumY, version.location.y);
            std::cout << "[CAMERA_DIAGNOSTICS] placing_phase=" << m_cameraDiagnosticPhase
                << " existing_max_section_y=" << existingMaximumY << '\n';
            for (int x = -7; x <= 7; ++x)
                for (int z = -7; z <= 7; ++z)
                    for (int y = 198; y <= 204; ++y)
                        m_world->setBlock(x, y, z, BlockId::Air);
            for (int x = -5; x <= 5; ++x)
                for (int z = -5; z <= 5; ++z)
                    m_world->setBlock(x, 198, z, BlockId::Stone);
            auto sideWall = [&](int x)
            {
                for (int z = -1; z <= 4; ++z)
                    for (int y = 199; y <= 204; ++y)
                        m_world->setBlock(x, y, z, BlockId::Stone);
            };
            PlayerSaveState player = m_worldPlayer->getSaveState();
            player.position = {.5f, 200.f, .5f};
            player.rotation = {0.f, 0.f, 0.f};
            if (m_cameraDiagnosticPhase == 0)
            {
                player.inventory.assign(5, {});
                player.inventory[0] = {Material::OakPlank, 7, 0};
                player.heldItem = 0;
            }
            if (m_cameraDiagnosticPhase == 1)
            {
                player.position.x = 1.05f;
                sideWall(2);
            }
            else if (m_cameraDiagnosticPhase == 2)
            {
                player.rotation.x = 45.f;
                for (int x = -3; x <= 4; ++x)
                    for (int z = 1; z <= 6; ++z)
                        m_world->setBlock(x, 202, z, BlockId::Stone);
            }
            else if (m_cameraDiagnosticPhase == 3 || m_cameraDiagnosticPhase == 4)
            {
                sideWall(1);
                if (m_cameraDiagnosticPhase == 4)
                    for (int x = -3; x <= 4; ++x)
                        for (int y = 199; y <= 204; ++y)
                            m_world->setBlock(x, y, 1, BlockId::Stone);
            }
            m_worldPlayer->applySaveState(player);
            m_worldPlayer->box.update(m_worldPlayer->position);
            m_logicCamera->update();
            m_config.cameraPerspective = m_cameraDiagnosticPhase == 3
                ? CameraPerspective::FirstPerson : CameraPerspective::ThirdPerson;
            if (m_cameraDiagnosticPhase < 5) m_thirdPersonCameraState = {};
            m_cameraDiagnosticPhasePlaced = true;
            m_cameraDiagnosticPhaseSeconds = 0.f;
        }

        void observeCameraDiagnostics()
        {
            if (!m_cameraDiagnostics || !m_cameraDiagnosticPhasePlaced ||
                m_cameraDiagnosticFramePending || m_cameraDiagnosticPhaseSeconds < 1.f)
                return;
            const auto resident = m_world->collectSectionMeshSnapshot(false);
            for (int x : {-1, 0}) for (int z : {-1, 0})
            {
                const glm::ivec3 location(x, 12, z);
                const auto current = std::find_if(resident.liveSectionVersions.begin(),
                    resident.liveSectionVersions.end(), [&](const auto& version)
                    { return version.location == location; });
                const auto key = sectionKey(location);
                const auto uploaded = m_materialIdentityMeshRevisions.find(key);
                const auto gpu = m_sectionRenderStates.find(key);
                if (current == resident.liveSectionVersions.end() ||
                    uploaded == m_materialIdentityMeshRevisions.end() ||
                    uploaded->second != current->blockRevision ||
                    gpu == m_sectionRenderStates.end() || gpu->second != ChunkRenderState::GpuResident)
                    return;
            }
            constexpr const char* phases[]{"clear_rear", "wide_sidewall",
                "low_ceiling", "explicit_first", "corner_fallback", "release_clear"};
            m_cameraDiagnostics->beginFrame(phases[m_cameraDiagnosticPhase],
                *m_world, *m_worldPlayer, *m_logicCamera, *m_camera,
                m_effectiveCameraMode, m_config.cameraPerspective,
                static_cast<std::uint64_t>(m_frameCount),
                m_nominalCameraNearClipDistance,
                m_lastRenderCameraPose.nearClipQueries,
                m_lastRenderCameraPose.nearClipSafetyUnresolved,
                static_cast<int>(m_lastRenderCameraPose.nearClipStatus));
            m_cameraDiagnosticFramePending = true;
        }

        void prepareMaterialIdentityCapture(float deltaSeconds)
        {
            if (!m_materialIdentityCapture || !m_world || !m_worldPlayer ||
                !m_userInterface || m_materialIdentityPhase >= 9) return;
            const float delta = std::isfinite(deltaSeconds)
                ? std::clamp(deltaSeconds, 0.f, .25f) : 0.f;
            const auto now = std::chrono::steady_clock::now();
            if (m_materialIdentityStarted == std::chrono::steady_clock::time_point{})
                m_materialIdentityStarted = now;
            m_materialIdentityPhaseSeconds += delta;
            if (now - m_materialIdentityStarted > std::chrono::seconds(45))
                throw std::runtime_error("Material identity capture did not obtain all resident consumers within its bounded run.");
            if (!m_materialIdentityFixturesPlaced)
            {
                if (!m_userInterface->setMaterialIdentityMap3dVisible(true))
                    throw std::runtime_error("Cannot open the actual 3D map for material identity queries.");
                const auto& samples = m_materialIdentityCapture->lastMapSamples();
                std::vector<MaterialIdentityCapture::MapSample> columns;
                for (const auto& sample : samples)
                {
                    if (!sample.sampleAvailable || !sample.known ||
                        sample.height < 0 || sample.height > 250) continue;
                    const float dx = sample.worldX + .5f - m_worldPlayer->position.x;
                    const float dz = sample.worldZ + .5f - m_worldPlayer->position.z;
                    if (dx * dx + dz * dz < 36.f || dx * dx + dz * dz > 144.f) continue;
                    const bool duplicate = std::any_of(columns.begin(), columns.end(),
                        [&](const auto& prior) { return prior.worldX == sample.worldX &&
                            prior.worldZ == sample.worldZ; });
                    if (!duplicate) columns.push_back(sample);
                    if (columns.size() == m_materialIdentityFixtures.size()) break;
                }
                if (columns.size() != m_materialIdentityFixtures.size()) return;
                int stageHeight = 0;
                for (const auto& sample : samples)
                    if (sample.sampleAvailable && sample.known && sample.height >= 0 && sample.height <= 246)
                        stageHeight = std::max(stageHeight, sample.height + 4);
                if (stageHeight == 0) return;
                PlayerSaveState inventory = m_worldPlayer->getSaveState();
                inventory.inventory.assign(5, {});
                constexpr int amounts[]{7,11,13,17};
                for (std::size_t index = 0; index < m_materialIdentityFixtures.size(); ++index)
                {
                    auto& fixture = m_materialIdentityFixtures[index];
                    const auto& source = columns[index];
                    fixture.position = {source.worldX, stageHeight, source.worldZ};
                    fixture.mapCell = source.cell;
                    m_world->setBlock(fixture.position.x, fixture.position.y,
                                      fixture.position.z, fixture.block);
                    if (m_world->getBlock(fixture.position.x, fixture.position.y,
                                          fixture.position.z).id != static_cast<Block_t>(fixture.block))
                        throw std::runtime_error("Material identity fixture was not accepted by its resident World column.");
                    if (index < 4)
                    {
                        inventory.inventory[index] = {fixture.material, amounts[index], 0};
                        fixture.actor = m_world->spawnItemEntity(fixture.material, 1,
                            m_worldPlayer->position + glm::vec3(8.f + 2.f * index, .5f, 0.f));
                        if (fixture.actor == 0)
                            throw std::runtime_error("Material identity World rejected its bounded dropped item.");
                    }
                    std::cout << "[MATERIAL_IDENTITY_FIXTURE] material=" << int(fixture.material)
                        << " block=" << int(fixture.block) << " position=" << fixture.position.x
                        << ',' << fixture.position.y << ',' << fixture.position.z
                        << " map_cell=" << fixture.mapCell << " actor=" << fixture.actor << '\n';
                }
                inventory.heldItem = 0;
                m_worldPlayer->applySaveState(inventory);
                m_materialIdentityFixturesPlaced = true;
                m_materialIdentityPhaseSeconds = 0.f;
                return;
            }
            if (!m_materialIdentityMapReady)
            {
                for (const auto& sample : m_materialIdentityCapture->lastMapSamples())
                    for (auto& fixture : m_materialIdentityFixtures)
                        if (sample.sampleAvailable && sample.known &&
                            sample.worldX == fixture.position.x &&
                            sample.worldZ == fixture.position.z &&
                            sample.height == fixture.position.y &&
                            sample.blockId == int(fixture.block)) fixture.observed = true;
                const bool samplesReady = std::all_of(
                    m_materialIdentityFixtures.begin(), m_materialIdentityFixtures.end(),
                    [](const auto& fixture) { return fixture.observed; });
                if (!samplesReady || m_materialIdentityPhaseSeconds < 2.f) return;
                m_materialIdentityMapReady = true;
                m_materialIdentityPhaseSeconds = 0.f;
            }
            const bool map = m_materialIdentityPhase == 8;
            if (!m_userInterface->setMaterialIdentityMap3dVisible(map))
                throw std::runtime_error("Cannot switch the material identity HUD phase.");
            const int selected = m_materialIdentityPhase % 4;
            PlayerInputState input;
            input.hotbarSlot = selected;
            m_worldPlayer->applyInput(input);
            m_config.cameraPerspective = m_materialIdentityPhase >= 4 && !map
                ? CameraPerspective::ThirdPerson : CameraPerspective::FirstPerson;
            const float settle = m_materialIdentityPhase == 0 ? 1.f : .35f;
            if (m_materialIdentityPhaseSeconds < settle) return;
            const std::string phase = map ? "map_3d" :
                std::string(m_materialIdentityPhase < 4 ? "first_slot_" : "third_slot_") +
                std::to_string(selected);
            m_materialIdentityCapture->beginFrame(phase,
                runtimeTerrainMaterialProfile().usesTextureArray() ? "standard" : "compatibility",
                static_cast<std::uint64_t>(m_frameCount));
            m_materialIdentityFramePending = true;
        }

        void observeMaterialIdentityGeometry()
        {
            if (!m_materialIdentityCapture || !m_materialIdentityCapture->isFrameOpen() ||
                m_materialIdentityPhase < 4 || m_materialIdentityPhase >= 8) return;
            const int slot = m_materialIdentityPhase % 4;
            auto iterator = m_sceneManager->getMovableObjectIterator("ManualObject");
            Ogre::ManualObject* held = nullptr;
            while (iterator.hasMoreElements())
            {
                auto* object = static_cast<Ogre::ManualObject*>(iterator.getNext());
                const std::string& name = object->getName();
                const std::string suffix = "_HeldItemMesh";
                if (name.size() >= suffix.size() &&
                    name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
                {
                    if (held != nullptr) throw std::runtime_error("Ambiguous actual player-held object.");
                    held = object;
                }
            }
            if (!held || held->getNumSections() != 1 || !m_playerAvatarWasVisible)
                throw std::runtime_error("Actual third-person held material is unavailable.");
            auto facts = [&](const MaterialIdentityFixture& fixture, const char* consumer)
            {
                MaterialIdentityCapture::Facts result;
                result.consumer = consumer;
                result.materialId = int(fixture.material);
                result.materialName = Material::toStringId(fixture.material);
                result.blockId = int(fixture.block);
                result.worldX = fixture.position.x; result.worldY = fixture.position.y;
                result.worldZ = fixture.position.z; result.mapCell = int(fixture.mapCell);
                return result;
            };
            auto observe = [&](MaterialIdentityCapture::Facts source, Ogre::Renderable& renderable)
            {
                auto material = renderable.getMaterial();
                material->load();
                auto* technique = material->getBestTechnique();
                if (!technique || technique->getNumPasses() != 1)
                    throw std::runtime_error("Material identity requires the actual single production pass.");
                m_materialIdentityCapture->observeRenderable(source, renderable,
                    *technique->getPass(0), *m_camera);
            };
            const auto actualPlayer = m_worldPlayer->getSaveState();
            const auto& actualHeld = m_worldPlayer->getHeldItems();
            if (actualPlayer.heldItem != slot || actualHeld.isEmpty() ||
                actualHeld.getMaterial().id != m_materialIdentityFixtures[slot].material)
                throw std::runtime_error("Actual Player held source differs from the selected diagnostic phase.");
            auto heldFacts = facts(m_materialIdentityFixtures[slot], "held_third");
            heldFacts.materialId = int(actualHeld.getMaterial().id);
            heldFacts.materialName = Material::toStringId(actualHeld.getMaterial().id);
            heldFacts.slot = actualPlayer.heldItem;
            heldFacts.amount = m_worldPlayer->getInventorySlot(slot).getNumInStack();
            observe(heldFacts, *held->getSection(0));
            if (slot != 0) return;
            const auto resident = m_world->collectSectionMeshSnapshot(false);
            for (const auto& fixture : m_materialIdentityFixtures)
            {
                if (m_world->getBlock(fixture.position.x, fixture.position.y,
                    fixture.position.z).id != static_cast<Block_t>(fixture.block))
                    throw std::runtime_error("Actual World source changed before material capture.");
                Ogre::Renderable* world = nullptr;
                const Ogre::Vector3 centre(fixture.position.x + .5f,
                    fixture.position.y + .5f, fixture.position.z + .5f);
                auto find = [&](const auto& visuals)
                {
                    for (const auto& entry : visuals)
                        for (const auto& renderable : entry.second.renderables)
                            if (renderable->getMaterial()->getName() == "HelloMine3D/Terrain" &&
                                renderable->getWorldBoundingBox(true).contains(centre))
                            {
                                if (world != nullptr) throw std::runtime_error("Ambiguous actual World material buffer.");
                                world = renderable.get();
                            }
                };
                find(m_sectionVisuals); find(m_terrainBatchVisuals);
                if (!world) throw std::runtime_error("Resident material fixture has not reached its actual World GPU buffer.");
                const glm::ivec3 location(
                    int(std::floor(float(fixture.position.x) / CHUNK_SIZE)),
                    int(std::floor(float(fixture.position.y) / CHUNK_SIZE)),
                    int(std::floor(float(fixture.position.z) / CHUNK_SIZE)));
                const auto key = sectionKey(location);
                const auto uploaded = m_materialIdentityMeshRevisions.find(key);
                const auto gpu = m_sectionRenderStates.find(key);
                const auto current = std::find_if(resident.liveSectionVersions.begin(),
                    resident.liveSectionVersions.end(),
                    [&](const auto& version) { return version.location == location; });
                if (uploaded == m_materialIdentityMeshRevisions.end() ||
                    current == resident.liveSectionVersions.end() ||
                    uploaded->second != current->blockRevision ||
                    gpu == m_sectionRenderStates.end() || gpu->second != ChunkRenderState::GpuResident)
                    throw std::runtime_error("Material fixture mesh revision has not reached its current resident GPU buffer.");
                auto worldFacts = facts(fixture, "world");
                worldFacts.revision = uploaded->second;
                observe(worldFacts, *world);
                if (fixture.actor == 0) continue;
                const auto snapshot = std::find_if(m_frameActorSnapshots.begin(), m_frameActorSnapshots.end(),
                    [&](const auto& actor) { return actor.id == fixture.actor; });
                if (snapshot == m_frameActorSnapshots.end() || snapshot->itemMaterialId != int(fixture.material))
                    throw std::runtime_error("Actual World dropped-item snapshot is missing or has the wrong material.");
                const std::string name = "Actor_" + std::to_string(fixture.actor) + "_Mesh";
                if (!m_sceneManager->hasManualObject(name))
                    throw std::runtime_error("Actual dropped-item renderer is missing.");
                auto* object = m_sceneManager->getManualObject(name);
                if (object->getNumSections() != 1)
                    throw std::runtime_error("Actual dropped-item operation is ambiguous.");
                auto dropFacts = facts(fixture, "drop");
                dropFacts.actorId = fixture.actor; dropFacts.amount = snapshot->itemAmount;
                observe(dropFacts, *object->getSection(0));
            }
        }

        bool updateSandbox(float deltaSeconds)
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::updateSandbox");
            if (m_sandbox == nullptr)
            {
                return false;
            }
            if (m_materialIdentityCapture || m_cameraDiagnostics || m_floraWindCapture || m_shoreEditCapture || m_referenceEditProbe)
            {
                // This bounded diagnostic freezes simulation only in its isolated
                // world. Normal World residency/mesh upload and renderer sync run.
                m_sandbox->update({}, 0.f, false);
                clearTransientInput();
                syncRenderCamera(deltaSeconds);
                syncPlayerPresentation(0.f);
                syncSectionMeshes();
                syncActorVisuals(0.f);
                return true;
            }
            updateRcPerformanceScenario(deltaSeconds);
            if(m_referenceResidencyProbe)m_referenceResidencyDelta=deltaSeconds;
            if (!m_applicationFlow.acceptsWorldSimulation() ||
                (m_userInterface != nullptr && m_userInterface->wantsHudPointer()))
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
                if (m_blockFeedback != nullptr) m_blockFeedback->hideSelection();
                clearTransientInput();
                return false;
            }

            const bool worldInputActive = acceptsWorldInput();

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
                input.player.jumpPressed = m_jumpPressed;
                input.player.descend = movementModes.sneak;
                input.player.toggleFlying = m_toggleFlying;
                input.player.hotbarDelta = m_hotbarDelta;
                input.player.hotbarSlot = m_hotbarSlot;
                input.resetMeshes = m_resetMeshes;
                input.useHeldFood = m_useHeldFood;
            }
            const OIS::MouseState &mouseState = m_mouse->getMouseState();
            GameplayMouseFrameInput::Buttons heldMouseButtons{};
            for (std::size_t buttonIndex = 0;
                 buttonIndex < GameplayMouseButtonCount; ++buttonIndex)
            {
                heldMouseButtons[buttonIndex] =
                    mouseState.buttonDown(toOisMouseButton(
                        static_cast<GameplayMouseButton>(buttonIndex)));
            }
            const auto mouseButtons = m_mouseFrameInput.consume(
                heldMouseButtons, worldInputActive, m_focusGate);
            if (worldInputActive &&
                m_focusGate.acceptsLookSample())
            {
                const GameplayLookDelta look = calculateGameplayLookDelta(
                    m_pendingLookDelta.x, m_pendingLookDelta.y,
                    m_config.mouseSensitivity, m_config.invertMouseY);
                input.player.lookDelta.x = look.yaw;
                input.player.lookDelta.y = look.pitch;
            }
            if (worldInputActive)
            {
                const GameplayMouseBindings &mouseBindings =
                    m_config.mouseBindings;
                const auto down = [&](GameplayWorldAction action) {
                    const auto index = static_cast<std::size_t>(
                        mouseBindings.get(action));
                    return index < mouseButtons.size() && mouseButtons[index];
                };
                input.breakAttack = down(GameplayWorldAction::BreakAttack);
                input.useBlock = down(GameplayWorldAction::Use);
                input.placeBlock = down(GameplayWorldAction::Place);
                input.guardCombat = down(GameplayWorldAction::Guard);
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
            if (m_blockFeedbackCapture && diagnosticsActive)
            {
                m_blockFeedbackCaptureSeconds += std::clamp(deltaSeconds, 0.f, 0.25f);
                const float seconds = m_blockFeedbackCaptureSeconds;
                const auto &target = m_blockFeedbackCaptureTarget;
                input = {};
                input.breakAttack = ((seconds >= 2.f && seconds < 2.55f) ||
                                     (seconds >= 3.5f && seconds < 5.1f)) &&
                    m_world->getBlock(target.x,target.y,target.z).id ==
                        static_cast<Block_t>(m_blockFeedbackCaptureId);
            }
            m_sandbox->update(input,
                              freezeValidationCapture ? 0.0f : deltaSeconds,
                              m_blockFeedbackCapture || (!diagnosticsActive && worldInputActive));
            if(m_referenceRestartObservation)
                m_referenceRestartObservation->normalUpdate(freezeValidationCapture?0.f:deltaSeconds);
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
            // The Ogre camera must reflect the current authoritative logic
            // camera before any camera-facing actors or block feedback update.
            syncRenderCamera(deltaSeconds);
            syncPlayerPresentation(deltaSeconds);
            syncSectionMeshes();
            syncActorVisuals(deltaSeconds);
            if (m_blockFeedback != nullptr)
            {
                const auto& selection = m_sandbox->getBlockSelection();
                const MiningProgressSnapshot &progress =
                    m_sandbox->getMiningProgress();
                m_blockFeedback->update(*m_world,
                    selection && (worldInputActive || diagnosticsActive) ? &*selection : nullptr,
                    progress, m_sandbox->getActionFeedback(), *m_camera);
                if (m_blockFeedbackCapture && progress.crackStage() != m_blockFeedbackCaptureLastStage)
                {
                    m_blockFeedbackCaptureLastStage = progress.crackStage();
                    std::cout << "[BLOCK_FEEDBACK_CAPTURE] seconds=" << m_blockFeedbackCaptureSeconds
                              << " stage=" << progress.crackStage()
                              << " particles=" << m_sandbox->getActionFeedback().particles.size()
                              << " selected=" << (selection ? static_cast<int>(selection->blockId) : -1) << '\n';
                }
            }
            return true;
        }

        void resetAdventureAudioPresentation(bool beginWorld) noexcept
        {
            m_adventureAudioState = {};
            m_adventureAudioEnvironment = {};
            m_adventureAudioEnvironmentCell = {
                std::numeric_limits<int>::min(),
                std::numeric_limits<int>::min()};
            m_adventureAudioEnvironmentRefreshSeconds = 0.f;
            m_adventureAmbientReplayCountdown.fill(0.f);
            m_adventureAmbientCaptionAnnounced.fill(false);
            m_adventureAmbientReplaySuspended = true;
            m_frameActorSnapshots.clear();
            m_adventureAudioPlayerInterpolationEpoch =
                beginWorld && m_worldPlayer != nullptr
                    ? m_worldPlayer->getInterpolationEpoch()
                    : 0;
            if (beginWorld)
            {
                advanceAdventureAudioEpoch();
            }
        }

        void advanceAdventureAudioEpoch() noexcept
        {
            if (m_audio != nullptr)
            {
                // Teleports, respawns and world transitions must not carry
                // queued voices or replay timing from the previous place.
                m_audio->stopAllPlayback();
            }
            m_adventureAmbientReplayCountdown.fill(0.f);
            m_adventureAmbientCaptionAnnounced.fill(false);
            m_adventureAmbientReplaySuspended = true;
            ++m_adventureAudioWorldEpoch;
            if (m_adventureAudioWorldEpoch == 0)
                m_adventureAudioWorldEpoch = 1;
        }

        AdventureAudioPresentation::SurfaceKind
        playerSupportSurface() const
        {
            namespace Adapter = AdventureAudioWorldAdapter;
            using Surface = AdventureAudioPresentation::SurfaceKind;
            if (m_world == nullptr || m_worldPlayer == nullptr ||
                !m_worldPlayer->isOnGround() || m_worldPlayer->isFlying())
            {
                return Surface::Unknown;
            }

            const glm::vec3 position = m_worldPlayer->position;
            const glm::vec3 half = m_worldPlayer->box.dimensions;
            const int supportY = World::toBlockCoord(
                position.y - half.y - .04f);
            const float insetX = std::max(.05f, half.x * .72f);
            const float insetZ = std::max(.05f, half.z * .72f);
            const std::array<glm::vec2, 5> probes{{
                {0.f, 0.f},
                {-insetX, -insetZ},
                {insetX, -insetZ},
                {-insetX, insetZ},
                {insetX, insetZ}}};
            for (int verticalOffset = 0; verticalOffset >= -1;
                 --verticalOffset)
            {
                for (const glm::vec2 &probe : probes)
                {
                    const BlockId block = static_cast<BlockId>(
                        m_world->getBlock(
                            World::toBlockCoord(position.x + probe.x),
                            supportY + verticalOffset,
                            World::toBlockCoord(position.z + probe.y)).id);
                    const Surface surface = Adapter::surfaceKind(block);
                    if (surface != Surface::Unknown)
                        return surface;
                }
            }
            return Surface::Unknown;
        }

        void refreshAdventureAudioEnvironment(float deltaSeconds)
        {
            namespace Audio = AdventureAudioPresentation;
            namespace Adapter = AdventureAudioWorldAdapter;
            if (m_world == nullptr || m_worldPlayer == nullptr)
            {
                m_adventureAudioEnvironment = {};
                return;
            }

            m_adventureAudioEnvironmentRefreshSeconds = std::max(
                0.f, m_adventureAudioEnvironmentRefreshSeconds -
                         std::clamp(deltaSeconds, 0.f, .25f));
            const int centerX = World::toBlockCoord(
                m_worldPlayer->position.x);
            const int centerZ = World::toBlockCoord(
                m_worldPlayer->position.z);
            const glm::ivec2 cell{World::floorDiv(centerX, 8),
                                  World::floorDiv(centerZ, 8)};
            if (cell == m_adventureAudioEnvironmentCell &&
                m_adventureAudioEnvironmentRefreshSeconds > 0.f)
            {
                return;
            }

            constexpr int GridRadius = 2;
            constexpr int ProbeSpacing = 8;
            constexpr std::size_t ProbeCount =
                (GridRadius * 2 + 1) * (GridRadius * 2 + 1);
            std::array<VectorXZ, ProbeCount> coordinates{};
            std::vector<VectorXZ> query;
            query.reserve(ProbeCount);
            std::size_t output = 0;
            for (int dz = -GridRadius; dz <= GridRadius; ++dz)
            {
                for (int dx = -GridRadius; dx <= GridRadius; ++dx)
                {
                    coordinates[output++] = {
                        centerX + dx * ProbeSpacing,
                        centerZ + dz * ProbeSpacing};
                }
            }
            query.assign(coordinates.begin(), coordinates.end());
            const auto surfaces = m_world->getChunkManager()
                                      .collectSurfaceMapSamples(query);
            const bool residentBatchReady =
                surfaces.size() == coordinates.size();
            if (!residentBatchReady &&
                m_adventureAudioEnvironmentCell.x !=
                    std::numeric_limits<int>::min())
            {
                m_adventureAudioEnvironmentRefreshSeconds = .1f;
                return;
            }

            const TerrainGenerator &generator =
                m_world->getChunkManager().getTerrainGenerator();
            Adapter::EnvironmentAccumulator accumulator;
            bool playerBelowSurface = false;
            for (std::size_t index = 0; index < coordinates.size(); ++index)
            {
                const VectorXZ coordinate = coordinates[index];
                const int dx = static_cast<int>(index % 5) - GridRadius;
                const int dz = static_cast<int>(index / 5) - GridRadius;
                const float distance = std::sqrt(
                    static_cast<float>(dx * dx + dz * dz));
                const float weight = (dx == 0 && dz == 0)
                                         ? 3.f
                                         : 1.f / (1.f + distance * .38f);
                const bool known = residentBatchReady &&
                                   surfaces[index].known;
                const int height = known
                    ? surfaces[index].height
                    : generator.getSurfaceHeightAtWorld(
                          coordinate.x, coordinate.z);
                const BlockId material = known
                    ? surfaces[index].material
                    : BlockId::Air;
                accumulator.add({
                    generator.getBiomeAtWorld(coordinate.x, coordinate.z),
                    known, height, material,
                    {static_cast<float>(coordinate.x),
                     static_cast<float>(height + 1),
                     static_cast<float>(coordinate.z)},
                    weight});
                if (dx == 0 && dz == 0)
                {
                    playerBelowSurface =
                        m_worldPlayer->position.y <
                        static_cast<float>(height - 4);
                }
            }
            m_adventureAudioEnvironment = playerBelowSurface
                ? std::array<Audio::EnvironmentTarget,
                             Audio::AmbientKindCount>{}
                : accumulator.targets();
            m_adventureAudioEnvironmentCell = cell;
            m_adventureAudioEnvironmentRefreshSeconds = .5f;
        }

        AdventureAudioPresentation::Input adventureAudioInput(
            float deltaSeconds, bool presentationPaused,
            bool uiBlocked) const
        {
            namespace Audio = AdventureAudioPresentation;
            namespace Adapter = AdventureAudioWorldAdapter;
            Audio::Input input;
            input.deltaSeconds = deltaSeconds;
            input.worldEpoch = m_adventureAudioWorldEpoch;
            input.worldActive = m_world != nullptr &&
                                m_worldPlayer != nullptr;
            input.paused = presentationPaused;
            input.uiBlocked = uiBlocked;
            if (!input.worldActive)
                return input;

            input.playerPositionValid = true;
            input.playerPosition = {
                m_worldPlayer->position.x,
                m_worldPlayer->position.y,
                m_worldPlayer->position.z};
            input.grounded = m_worldPlayer->isOnGround();
            input.flying = m_worldPlayer->isFlying();
            input.supportSurface = playerSupportSurface();
            input.daylight = WorldEnvironment::evaluate(
                m_world->getWorldTime()).daylight;
            input.environment = m_adventureAudioEnvironment;

            for (const ActorSnapshot &snapshot : m_frameActorSnapshots)
            {
                const Audio::AnimalSpecies species =
                    Adapter::animalSpecies(snapshot.type);
                if (species == Audio::AnimalSpecies::Unknown ||
                    input.animalCount >= input.animals.size())
                {
                    continue;
                }
                input.animals[input.animalCount++] = {
                    snapshot.id, species,
                    Adapter::animalActivity(snapshot.wildlifeActivity),
                    {snapshot.position.x, snapshot.position.y,
                     snapshot.position.z}};
            }
            return input;
        }

        void submitAdventureAudioFrame(
            const AdventureAudioPresentation::Frame &frame)
        {
            if (m_audio == nullptr)
                return;
            for (std::size_t index = 0; index < frame.cueCount; ++index)
            {
                const AdventureAudioPresentation::Cue &cue =
                    frame.cues[index];
                switch (cue.kind)
                {
                case AdventureAudioPresentation::CueKind::AmbientOpenLand:
                case AdventureAudioPresentation::CueKind::AmbientForest:
                case AdventureAudioPresentation::CueKind::AmbientInlandWater:
                case AdventureAudioPresentation::CueKind::AmbientCoast:
                    // Fixed bounded ambient replay below owns these samples;
                    // the scheduler's sparse cue only advances its cadence.
                    continue;
                default:
                    break;
                }
                const char *cueId = AdventureAudioPresentation::cueId(cue);
                if (cueId[0] == '\0')
                    continue;
                const bool caption =
                    cue.kind ==
                        AdventureAudioPresentation::CueKind::AnimalSheep ||
                    cue.kind ==
                        AdventureAudioPresentation::CueKind::AnimalRabbit ||
                    cue.kind == AdventureAudioPresentation::CueKind::
                                    AnimalWetlandBird;
                m_audio->submit({
                    cueId, cue.spatial,
                    {cue.position.x, cue.position.y, cue.position.z},
                    cue.gain, caption});
            }
        }

        void updateAdventureAmbientLoops(
            const AdventureAudioPresentation::Frame &frame,
            float deltaSeconds, float daylight, bool reset,
            bool suspended)
        {
            namespace Audio = AdventureAudioPresentation;
            if (m_audio == nullptr)
                return;
            if (reset)
            {
                m_adventureAmbientReplayCountdown.fill(0.f);
                m_adventureAmbientReplaySuspended = true;
                return;
            }
            if (suspended)
            {
                // The backend retains and pauses its current buffers while
                // unfocused. Preserve the replay countdown so focus recovery
                // cannot stack a fresh copy over the resumed sample.
                m_adventureAmbientReplaySuspended = true;
                return;
            }

            std::array<float, Audio::AmbientKindCount> weights{};
            std::array<bool, Audio::AmbientKindCount> positioned{};
            std::array<Audio::Vec3, Audio::AmbientKindCount> positions{};
            for (std::size_t index = 0;
                 index < frame.ambientLayerCount; ++index)
            {
                const Audio::AmbientLayer &layer =
                    frame.ambientLayers[index];
                const std::size_t kind =
                    static_cast<std::size_t>(layer.kind);
                if (kind >= weights.size())
                    continue;
                weights[kind] = layer.weight;
                positioned[kind] = layer.hasPosition;
                positions[kind] = layer.position;
            }

            const float delta = std::clamp(deltaSeconds, 0.f, .25f);
            for (std::size_t kind = 0; kind < weights.size(); ++kind)
            {
                if (weights[kind] <= Audio::MinimumAudibleLayerWeight)
                {
                    m_adventureAmbientReplayCountdown[kind] = 0.f;
                    m_adventureAmbientCaptionAnnounced[kind] = false;
                    continue;
                }
                float &countdown =
                    m_adventureAmbientReplayCountdown[kind];
                if (!m_adventureAmbientReplaySuspended)
                    countdown = std::max(0.f, countdown - delta);
                if (countdown > 0.f)
                    continue;

                const Audio::AmbientKind ambientKind =
                    static_cast<Audio::AmbientKind>(kind);
                const char *cueId = Audio::cueId(
                    Audio::ambientCue(ambientKind));
                if (cueId[0] == '\0')
                    continue;
                const bool announceCaption =
                    !m_adventureAmbientCaptionAnnounced[kind] &&
                    m_audio->captionsEnabled();
                m_audio->submit({
                    cueId, positioned[kind],
                    {positions[kind].x, positions[kind].y,
                     positions[kind].z},
                    Audio::ambientLayerGain(weights[kind], daylight),
                    announceCaption});
                if (announceCaption)
                    m_adventureAmbientCaptionAnnounced[kind] = true;
                countdown = Audio::ambientReplayInterval(
                    ambientKind, daylight);
            }
            m_adventureAmbientReplaySuspended = false;
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
                if (m_worldPlayer != nullptr &&
                    m_adventureAudioPlayerInterpolationEpoch !=
                        m_worldPlayer->getInterpolationEpoch())
                {
                    m_adventureAudioPlayerInterpolationEpoch =
                        m_worldPlayer->getInterpolationEpoch();
                    // Player::resetInterpolation marks authoritative position
                    // discontinuities such as teleport and void recovery. Make
                    // them a scheduler epoch even when the displacement is
                    // below the generic four-metre teleport guard.
                    advanceAdventureAudioEpoch();
                }
                const bool focused = m_focusGate.isFocused();
                const bool uiBlocked = m_worldPlayer != nullptr &&
                    (m_worldPlayer->hasOpenContainer() ||
                     m_worldPlayer->hasOpenCrafting() ||
                     (m_userInterface != nullptr &&
                      (m_userInterface->wantsKeyboardInput() ||
                       m_userInterface->wantsMouseInput())));
                const bool presentationPaused =
                    worldPaused || !focused || uiBlocked;
                const bool ambientSuspended = !focused;
                m_audio->setWorldPaused(worldPaused);
                // Pause menus retain UI click feedback. Only loss of process
                // focus suspends the device queue itself.
                m_audio->setSuspended(!focused);
                // The adventure scheduler below owns environment timing. Keep
                // AudioRuntime updating the listener/backend while disabling
                // its legacy unconditional ambient.wind timer.
                m_audio->update(deltaSeconds,
                                false,
                                listener);
                refreshAdventureAudioEnvironment(deltaSeconds);
                const AdventureAudioPresentation::Input audioInput =
                    adventureAudioInput(deltaSeconds,
                                        presentationPaused,
                                        uiBlocked);
                const AdventureAudioPresentation::Frame frame =
                    AdventureAudioPresentation::update(
                        m_adventureAudioState,
                        audioInput);
                updateAdventureAmbientLoops(
                    frame, deltaSeconds, audioInput.daylight,
                    worldPaused || frame.resetThisFrame,
                    ambientSuspended);
                submitAdventureAudioFrame(frame);
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
            if (m_caveBoundaryRenderer != nullptr)
            {
                m_caveBoundaryRenderer->sync(snapshot.boundaryMasks);
                const auto &stats = m_caveBoundaryRenderer->stats();
                m_boundaryMaskPeakFacesScanned = std::max(
                    m_boundaryMaskPeakFacesScanned,
                    snapshot.boundaryMaskFacesScanned);
                m_boundaryMaskPeakUpdates = std::max(
                    m_boundaryMaskPeakUpdates, stats.updatesThisSync);
                ++m_boundaryMaskSyncCount;
                if (m_renderCapture != nullptr &&
                    m_boundaryMaskSyncCount % 60 == 1)
                {
                    std::cout << "[CAVE_BOUNDARY] candidates="
                              << snapshot.boundaryMaskCandidates
                              << " cache=" << snapshot.boundaryMaskCacheEntries
                              << " cpu_deferred=" << snapshot.boundaryMaskDeferred
                              << " scanned_faces=" << snapshot.boundaryMaskFacesScanned
                              << " scanned_cells=" << snapshot.boundaryMaskCellsScanned
                              << " live=" << stats.liveFaces
                              << " gpu_deferred=" << stats.deferredFaces
                              << " updates=" << stats.updatesThisSync
                              << " hidden=" << stats.hiddenFacesThisSync
                              << " texture_patch_bytes=" << stats.texturePatchBytesThisSync
                              << " vertex_patch_bytes=" << stats.vertexPatchBytesThisSync
                              << " gpu_bytes=" << stats.gpuBytes
                              << " peak_scanned_faces=" << m_boundaryMaskPeakFacesScanned
                              << " peak_updates=" << m_boundaryMaskPeakUpdates << '\n';
                }
            }
            // World height increases the number of live sections. Rebuilding
            // their string index every idle frame does not change residency.
            if (snapshot.liveSections != m_lastLiveSections)
            {
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
                    if(m_referenceResidencyProbe && visual!=m_sectionVisuals.end() && m_referenceResidencyProbe->skipRetirement(visual->second.location)) { ++it; continue; }
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
                    m_emptySectionUploadIdentities.erase(it->first);
                    it = m_sectionRenderStates.erase(it);
                }
                m_lastLiveSections = snapshot.liveSections;
            }

            // Clean CPU output may outlive its former Near GPU representation.
            // Request only missing, current Clean versions; never rebuild or
            // dirty World data just to restore an Ogre-owned representation.
            std::vector<WorldSectionMeshVersion> missingClean;
            std::vector<glm::ivec3> missingLocations;
            for(const auto& v:snapshot.liveSectionVersions) {
                if(v.meshState!=ChunkMeshState::Clean || m_sectionVisuals.count(sectionKey(v.location)))continue;
                const auto empty=m_emptySectionUploadIdentities.find(sectionKey(v.location));
                if(empty!=m_emptySectionUploadIdentities.end() && empty->second.blockRevision==v.blockRevision && empty->second.incarnation==v.incarnation)continue;
                missingLocations.push_back(v.location);
            }
            const auto center=World::getChunkXZ(World::toBlockCoord(m_worldPlayer->position.x),World::toBlockCoord(m_worldPlayer->position.z));
            for(const auto& location:ChunkRuntime::planSectionMeshUploads(missingLocations,center,ChunkRuntime::MaxSectionUploadsPerFrame)) {
                const auto v=std::find_if(snapshot.liveSectionVersions.begin(),snapshot.liveSectionVersions.end(),[&](const auto& candidate){return candidate.location==location;});
                missingClean.push_back(*v);
            }
            WorldRetainedMeshSnapshot replay;
            if(!missingClean.empty() && snapshot.cpuReadySections.size()<ChunkRuntime::MaxSectionUploadsPerFrame)
                replay=m_world->observeRetainedSectionMeshes(missingClean,snapshot.cpuReadySections.size());
            if(snapshot.cpuReadySections.size()+replay.sections.size()>ChunkRuntime::MaxSectionUploadsPerFrame)
                throw std::runtime_error("Combined normal/replay section upload budget exceeded");
            if(m_referenceResidencyProbe) {
                using namespace ReferenceResidency;const auto records=[](const auto& parts){std::vector<std::string> facts;for(const auto& v:parts)facts.push_back(object({{"location",xyz(v.location)},{"revision",number(v.blockRevision)},{"incarnation",number(v.incarnation)},{"mesh_state",quote(chunkMeshStateName(v.meshState))}}));return array(facts);};
                m_referenceResidencyProbe->recordUploadFrameFacts(object({{"frame",number(m_frameCount)},{"cpu_ready_offered",records(snapshot.cpuReadySections)},{"retained_clean_requested",records(missingClean)},{"retained_clean_copied",records(replay.sections)},{"normal_count",number(snapshot.cpuReadySections.size())},{"replay_count",number(replay.sections.size())},{"combined_count",number(snapshot.cpuReadySections.size()+replay.sections.size())},{"maximum_count",number(ChunkRuntime::MaxSectionUploadsPerFrame)}}));
            }
            std::vector<WorldSectionMeshVersion> uploaded;
            std::vector<WorldSectionMeshSnapshot*> uploadParts;
            for(auto& section:snapshot.cpuReadySections)uploadParts.push_back(&section);
            for(auto& section:replay.sections)uploadParts.push_back(&section);
            uploaded.reserve(uploadParts.size());
            for (auto* part : uploadParts)
            {
                auto& section=*part;
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
                uploaded.push_back(
                    {section.location, section.blockRevision, section.incarnation, section.meshState});
            }
            flushTerrainBatches();
            for (const auto& section : uploaded)
            {
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
            }
            // Residency cleanup above always runs. With no uploads there is
            // nothing to acknowledge or validate against a second snapshot;
            // new worker output will be offered on the next frame as usual.
            if (uploaded.empty())
            {
                return;
            }
            m_world->acknowledgeSectionMeshUploads(uploaded);
            const WorldMeshSnapshot acknowledged =
                m_world->collectSectionMeshSnapshot(false);
            if (m_caveBoundaryRenderer != nullptr)
            {
                m_caveBoundaryRenderer->sync(acknowledged.boundaryMasks, false);
            }
            std::vector<std::string> acceptedUploadFacts;
            for (const WorldSectionMeshVersion& version : uploaded)
            {
                const std::string key = sectionKey(version.location);
                // At most eight uploads need validation. Compare numeric
                // locations directly instead of indexing every live section.
                const auto current = std::find_if(
                    acknowledged.liveSectionVersions.begin(),
                    acknowledged.liveSectionVersions.end(),
                    [&version](const WorldSectionMeshVersion& candidate) {
                        return candidate.location == version.location;
                    });
                const bool stillCpuReady = std::any_of(
                    acknowledged.cpuReadySections.begin(),
                    acknowledged.cpuReadySections.end(),
                    [&version](const WorldSectionMeshSnapshot& candidate) {
                        return candidate.location == version.location;
                    });
                const bool acceptedCurrent =
                    current != acknowledged.liveSectionVersions.end() &&
                    current->blockRevision == version.blockRevision &&
                    current->incarnation == version.incarnation &&
                    current->meshState == ChunkMeshState::Clean &&
                    !stillCpuReady;
                const bool hasVisual =
                    m_sectionVisuals.find(key) != m_sectionVisuals.end();
                if(m_referenceResidencyProbe) {
                    using namespace ReferenceResidency;
                    acceptedUploadFacts.push_back(object({
                        {"location",xyz(version.location)},
                        {"revision",number(version.blockRevision)},
                        {"incarnation",number(version.incarnation)},
                        {"mesh_state",quote(chunkMeshStateName(version.meshState))},
                        {"accepted_current",boolean(acceptedCurrent)},
                        {"has_visual",boolean(hasVisual)},
                        {"current_revision",current!=acknowledged.liveSectionVersions.end()?number(current->blockRevision):"null"},
                        {"current_incarnation",current!=acknowledged.liveSectionVersions.end()?number(current->incarnation):"null"},
                        {"current_mesh_state",current!=acknowledged.liveSectionVersions.end()?quote(chunkMeshStateName(current->meshState)):"null"}}));
                }
                if (!acceptedCurrent && hasVisual)
                {
                    auto visual = m_sectionVisuals.find(key);
                    destroySectionVisual(visual->second);
                    m_sectionVisuals.erase(visual);
                }
                if(acceptedCurrent && !hasVisual)m_emptySectionUploadIdentities[key]=version;
                else m_emptySectionUploadIdentities.erase(key);
                transitionRenderState(
                    key, acceptedCurrent && hasVisual
                             ? ChunkRenderState::GpuResident
                             : ChunkRenderState::NotResident);
            }
            flushTerrainBatches();
            if(m_referenceResidencyProbe) {
                using namespace ReferenceResidency;
                const auto records=[](const auto& parts){std::vector<std::string> facts;for(const auto& v:parts)facts.push_back(object({{"location",xyz(v.location)},{"revision",number(v.blockRevision)},{"incarnation",number(v.incarnation)},{"mesh_state",quote(chunkMeshStateName(v.meshState))}}));return array(facts);};
                m_referenceResidencyProbe->completeUploadFrameFacts(object({
                    {"frame",number(m_frameCount)},
                    {"cpu_ready_offered",records(snapshot.cpuReadySections)},
                    {"retained_clean_requested",records(missingClean)},
                    {"retained_clean_copied",records(replay.sections)},
                    {"cpu_ready_uploaded",records(snapshot.cpuReadySections)},
                    {"retained_clean_uploaded",records(replay.sections)},
                    {"accepted_uploads",array(acceptedUploadFacts)},
                    {"normal_count",number(snapshot.cpuReadySections.size())},
                    {"replay_count",number(replay.sections.size())},
                    {"combined_count",number(uploaded.size())},
                    {"maximum_count",number(ChunkRuntime::MaxSectionUploadsPerFrame)}}));
            }
        }

        void syncActorVisuals(float deltaSeconds = 0.f)
        {
            HELLOMINE3D_PROFILE_SCOPE("Ogre::syncActorVisuals");
            if (m_world == nullptr || m_actorRenderer == nullptr ||
                m_logicCamera == nullptr)
            {
                return;
            }
            m_frameActorSnapshots = m_world->collectActorSnapshots();
            const bool wildlifeGallery = m_actorVisualCapture == "wildlife-cycle";
            const bool actorGallery =
                !m_actorVisualCapture.empty() &&
                m_actorVisualCapture != "projectiles" &&
                m_actorVisualCapture != "projectile-flight";
            std::vector<ActorSnapshot> gallerySnapshots;
            std::vector<ActorSnapshot>& snapshots =
                actorGallery ? gallerySnapshots : m_frameActorSnapshots;
            if (actorGallery && !wildlifeGallery)
            {
                // Fixed presentation gallery; no actors/items enter the World or save.
                const Ogre::Vector3 view = m_camera->getDirection();
                glm::vec3 forward(view.x, 0.f, view.z);
                if (glm::length(forward) < .001f) forward = {0,0,-1};
                forward = glm::normalize(forward);
                const glm::vec3 right(-forward.z, 0.f, forward.x);
                const Ogre::Vector3 cameraPosition = m_camera->getPosition();
                const glm::vec3 origin(cameraPosition.x, cameraPosition.y, cameraPosition.z);
                const char* types[]{"hellomine:stalker", "hellomine:brute", "hellomine:spitter",
                                    "hellomine:waystone_stalker"};
                for (int index=0; index<4; ++index) {
                    const auto* definition = runtimeEnemyRegistry().find(types[index]);
                    if (definition == nullptr)
                        throw std::runtime_error("Actor gallery enemy definition is missing");
                    ActorSnapshot sample = EnemyPresentation::gallerySnapshot(
                        *definition, m_actorVisualCapture, m_actorVisualCaptureSeconds);
                    sample.id = 900001 + index;
                    if (!m_actorVisualGalleryLogged)
                        std::cout << "[ACTOR_GALLERY_PROFILE] type=" << sample.type
                                  << " dimensions=" << sample.dimensions.x << ","
                                  << sample.dimensions.y << "," << sample.dimensions.z
                                  << " windup=" << definition->combat.windupTicks
                                  << " recover=" << definition->combat.recoverTicks << '\n';
                    sample.position = origin + forward * m_actorVisualDistance + right * ((index - 1.5f) * 1.5f);
                    sample.position.y -= .3f;
                    sample.rotation.y = glm::degrees(std::atan2(forward.x, forward.z));
                    if (m_actorVisualCapture == "walk") {
                        // Deliberately travel along x=-z: the old coordinate-
                        // sum gait froze on this path. These are render-only
                        // gallery snapshots, never actors in the world/save.
                        const float travel = .55f * std::sin(m_actorVisualCaptureSeconds * 2.f);
                        sample.position += glm::vec3(travel, 0.f, -travel);
                        sample.combatState = MobCombatState::Chase;
                        sample.rotation.y += 25.f;
                    }
                    else if (m_actorVisualCapture == "cycle") {
                        sample.rotation.y += 25.f;
                    }
                    snapshots.push_back(sample);
                }
                m_actorVisualGalleryLogged = true;
                const Material::ID materials[]{Material::Stone, Material::OakBark,
                    Material::StoneSword, Material::IronIngot, Material::Bread};
                for (int index=0; index<5; ++index) {
                    ActorSnapshot sample;
                    sample.id = 900011 + index; sample.type = "item";
                    sample.position = origin + forward * 3.5f + right * ((index - 2.f) * .65f);
                    sample.position.y -= 1.3f;
                    sample.dimensions = glm::vec3(.18f);
                    sample.itemMaterialId = materials[index]; sample.itemAmount = 1;
                    sample.itemAgeSeconds = m_actorVisualCaptureSeconds;
                    snapshots.push_back(sample);
                }
            }
            if (wildlifeGallery)
            {
                // Render-only samples use production species dimensions and
                // pose code. They never join the World or claim natural AI.
                const Ogre::Vector3 view = m_camera->getDirection();
                glm::vec3 forward(view.x, 0.f, view.z);
                if (glm::length(forward) < .001f) forward = {0,0,-1};
                forward = glm::normalize(forward);
                const glm::vec3 right(-forward.z, 0.f, forward.x);
                const Ogre::Vector3 eye = m_camera->getPosition();
                const glm::vec3 origin(eye.x, eye.y - 1.2f, eye.z);
                const char* types[]{WildlifeSpecies::Sheep, WildlifeSpecies::Rabbit,
                                    WildlifeSpecies::MarshBird};
                const int phase = static_cast<int>(m_actorVisualCaptureSeconds / 4.f) % 4;
                for (int index = 0; index < 3; ++index) {
                    WildlifeActor model(900021 + index, types[index], origin);
                    ActorSnapshot sample = model.getSnapshot();
                    sample.position += forward * m_actorVisualDistance + right * ((index - 1.f) * 1.3f);
                    sample.rotation.y = glm::degrees(std::atan2(forward.x, forward.z)) + 55.f;
                    sample.wildlifeActivity = phase;
                    sample.wildlifeMotionSeconds = m_actorVisualCaptureSeconds;
                    if (phase >= static_cast<int>(WildlifeActivity::Wander))
                        sample.position += right * (.25f * std::sin(m_actorVisualCaptureSeconds * 3.f));
                    if (!m_actorVisualGalleryLogged)
                        std::cout << "[WILDLIFE_GALLERY] type=" << sample.type
                                  << " dimensions=" << sample.dimensions.x << ','
                                  << sample.dimensions.y << ',' << sample.dimensions.z
                                  << " state_period_seconds=4 world_actor=0\n";
                    snapshots.push_back(sample);
                }
                m_actorVisualGalleryLogged = true;
            }
            const Ogre::Vector3 renderEye = m_camera->getPosition();
            m_actorRenderer->sync(snapshots,
                glm::vec3(renderEye.x, renderEye.y, renderEye.z),
                m_config.feedbackIntensity == GameplayFeedbackIntensity::Off ? 0.f :
                m_config.feedbackIntensity == GameplayFeedbackIntensity::Reduced ? .35f : 1.f,
                deltaSeconds);
            auto projectiles = m_world->collectCombatProjectileSnapshots();
            if (m_actorVisualCapture == "projectiles" || m_actorVisualCapture == "projectile-flight") {
                // Diagnostic mirror only: cover near/middle/far and the World
                // hard cap through the production renderer, without combat or save writes.
                projectiles.clear();
                const Ogre::Vector3 view = m_camera->getDirection();
                const glm::vec3 forward = glm::normalize(glm::vec3(view.x, view.y, view.z));
                const glm::vec3 reference = std::abs(forward.y) < .95f
                    ? glm::vec3(0,1,0) : glm::vec3(0,0,1);
                const glm::vec3 right = glm::normalize(glm::cross(forward, reference));
                const glm::vec3 up = glm::cross(right, forward);
                const glm::vec3 origin(renderEye.x, renderEye.y, renderEye.z);
                const float tickTime = std::floor(m_actorVisualCaptureSeconds * 20.f) / 20.f;
                for (int index = 0; index < 32; ++index) {
                    CombatProjectileSnapshot sample;
                    sample.id = 910001 + index; sample.ownerId = 1;
                    const float distance = index == 0 ? .9f : index == 1 ? 2.f : 6.f + (index/8)*2.f;
                    const float across = index == 0 ? .4f : index == 1 ? -.8f : ((index%8)-3.5f)*.8f;
                    sample.position = origin + forward*distance + right*across + up*(index < 2 ? .18f : (index/8-1.5f)*.55f);
                    sample.velocity = glm::normalize(right + up*((index%3-1)*.45f) - forward*.35f)*10.f;
                    if (m_actorVisualCapture == "projectile-flight")
                        sample.position += right*(std::fmod(tickTime*10.f + index*.4f, 4.f)-2.f);
                    sample.radius = .15f; sample.ticksRemaining = 50;
                    sample.distanceTravelled = 3.f; sample.maximumDistance = 20.f;
                    projectiles.push_back(sample);
                }
            }
            m_actorRenderer->syncProjectiles(projectiles);
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
            const glm::vec3 destination{
                static_cast<float>(m_fastStreamingTarget.x * CHUNK_SIZE +
                                   CHUNK_SIZE / 2),
                m_worldPlayer->position.y,
                static_cast<float>(m_fastStreamingTarget.z * CHUNK_SIZE +
                                   CHUNK_SIZE / 2)};
            if (!m_sandbox->getWorldManager().teleportPlayer(
                    *m_worldPlayer, destination))
            {
                throw std::runtime_error(
                    "RC fast-streaming teleport was rejected");
            }
            m_logicCamera->update();
            m_fastStreamingStarted = std::chrono::steady_clock::now();
            m_fastStreamingPending = true;
        }

        bool canBatchMaterial(const char* name)
        {
            Ogre::Pass* pass = materialPass(name);
            return canBatchTerrainMaterial(
                pass->getParent()->getNumPasses(), pass->getDepthWriteEnabled(),
                pass->isTransparent(), pass->getVertexProgramName(),
                pass->getFragmentProgramName());
        }

        bool uploadSectionVisual(WorldSectionMeshSnapshot& section)
        {
            const std::string key = sectionKey(section.location);
            m_emptySectionUploadIdentities.erase(key);
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->uploaded(section.location,section.blockRevision);
            const auto existing = m_sectionVisuals.find(key);
            if (existing != m_sectionVisuals.end())
            {
                destroySectionVisual(existing->second);
                m_sectionVisuals.erase(existing);
            }
            SectionVisual visual;
            visual.location = section.location;
            if (m_shoreEditCapture) visual.shoreUploadSerial = ++m_shoreUploadSerial;
            const std::string name = "ChunkSection_" + key;
            auto upload = [&](const ChunkMesh& mesh, const char* suffix,
                              const char* material, std::uint8_t queue, bool shadows) {
                if (mesh.getClientMesh().indices.empty())
                {
                    validateTerrainRenderPart({section.location, &mesh});
                    return;
                }
                if (m_floraWindCapture && std::string(material)=="HelloMine3D/Flora" &&
                    section.location==glm::ivec3(60,5,-2))
                {
                    const auto vertexBytes=mesh.getClientMesh().vertexPositions.size()/3*sizeof(TerrainRenderVertex);
                    const auto indexBytes=mesh.getClientMesh().indices.size()*sizeof(std::uint32_t);
                    if(vertexBytes>16u*1024u*1024u || indexBytes>16u*1024u*1024u-vertexBytes)
                        throw std::runtime_error("Natural Flora diagnostic CPU copy exceeds raw bound.");
                    visual.fernDiagnosticFlora=std::make_unique<ChunkMesh>(mesh);
                }
                retainShoreUploadInput(visual, mesh, material, section.location);
                auto object = std::make_unique<ChunkSectionRenderable>(
                    name + suffix, mesh, section.location, material, queue);
                if(m_referenceResidencyProbe)m_referenceResidencyProbe->retain(*object,{{section.location,&mesh}},section.location);
                object->setCastShadows(shadows);
                if(m_referenceEditProbe)
                    m_referenceEditProbe->retainUpload(*object,mesh,section.location,section.blockRevision);
                if (!visual.node)
                    visual.node = m_sceneManager->getRootSceneNode()->createChildSceneNode(
                        name + "_Node", Ogre::Vector3(
                            static_cast<float>(section.location.x) * CHUNK_SIZE,
                            static_cast<float>(section.location.y) * CHUNK_SIZE,
                            static_cast<float>(section.location.z) * CHUNK_SIZE));
                visual.node->attachObject(object.get());
                visual.renderables.push_back(std::move(object));
            };
            // Blended layers retain their per-section objects and depth sorting.
            upload(section.meshes.transparentMesh, "_Transparent", "HelloMine3D/Transparent",
                   Ogre::RENDER_QUEUE_8, false);
            upload(section.meshes.waterMesh, "_Water", "HelloMine3D/Water",
                   Ogre::RENDER_QUEUE_8, false);
            auto retainOrUpload = [&](ChunkMesh& mesh, bool solid) {
                const char* material = solid ? "HelloMine3D/Terrain" : "HelloMine3D/Flora";
                if (!mesh.getClientMesh().indices.empty() && canBatchMaterial(material))
                {
                    if (!visual.batchMeshes)
                        visual.batchMeshes = std::make_unique<ChunkMeshCollection>();
                    (solid ? visual.batchMeshes->solidMesh : visual.batchMeshes->floraMesh)
                        .adoptClientData(mesh);
                }
                else
                {
                    upload(mesh, solid ? "_Solid" : "_Flora", material,
                           solid ? Ogre::RENDER_QUEUE_MAIN : Ogre::RENDER_QUEUE_6, solid);
                }
            };
            retainOrUpload(section.meshes.solidMesh, true);
            retainOrUpload(section.meshes.floraMesh, false);
            if (visual.batchMeshes)
            {
                const auto origin = terrainRenderBatchOrigin(section.location);
                m_dirtyTerrainBatches[sectionKey(origin)] = origin;
            }
            if (!visual.node && !visual.batchMeshes) return false;
            m_sectionVisuals.emplace(key, std::move(visual));
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->witnessUploaded(section.location,section.blockRevision,section.incarnation,section.meshState,section.retainedCleanReplay?"retained-clean-replay":"cpu-ready","frame");
            if (m_materialIdentityCapture || m_cameraDiagnostics || m_floraWindCapture || m_shoreEditCapture || m_referenceEditProbe)
                m_materialIdentityMeshRevisions[key] = section.blockRevision;
            return true;
        }

        void flushTerrainBatches()
        {
            for (const auto& dirty : m_dirtyTerrainBatches)
            {
                const auto old = m_terrainBatchVisuals.find(dirty.first);
                if (old != m_terrainBatchVisuals.end())
                {
                    destroySectionVisual(old->second);
                    m_terrainBatchVisuals.erase(old);
                }
                const auto origin = dirty.second;
                std::vector<TerrainRenderBatchPart> solids, flora;
                for (int y = 0; y < TerrainRenderBatchSections; ++y)
                {
                    const glm::ivec3 location{origin.x, origin.y + y, origin.z};
                    const auto slot = m_sectionVisuals.find(sectionKey(location));
                    if (slot == m_sectionVisuals.end() || !slot->second.batchMeshes) continue;
                    const auto& meshes = *slot->second.batchMeshes;
                    if (!meshes.solidMesh.getClientMesh().indices.empty())
                        solids.push_back({location, &meshes.solidMesh});
                    if (!meshes.floraMesh.getClientMesh().indices.empty())
                        flora.push_back({location, &meshes.floraMesh});
                }
                SectionVisual visual;
                visual.location = origin;
                if (m_shoreEditCapture) visual.shoreUploadSerial = ++m_shoreUploadSerial;
                const std::string name = "TerrainBatch_" + dirty.first;
                auto upload = [&](const auto& parts, const char* suffix, const char* material,
                                  std::uint8_t queue, bool shadows) {
                    if (parts.empty()) return;
                    auto object = std::make_unique<ChunkSectionRenderable>(name + suffix,
                        parts, origin, material, queue);
                    if(m_referenceResidencyProbe)m_referenceResidencyProbe->retain(*object,parts,origin);
                    object->setCastShadows(shadows);
                    if (!visual.node)
                        visual.node = m_sceneManager->getRootSceneNode()->createChildSceneNode(
                            name + "_Node", Ogre::Vector3(
                                static_cast<float>(origin.x) * CHUNK_SIZE,
                                static_cast<float>(origin.y) * CHUNK_SIZE,
                                static_cast<float>(origin.z) * CHUNK_SIZE));
                    visual.node->attachObject(object.get());
                    visual.renderables.push_back(std::move(object));
                };
                upload(solids, "_Solid", "HelloMine3D/Terrain", Ogre::RENDER_QUEUE_MAIN, true);
                upload(flora, "_Flora", "HelloMine3D/Flora", Ogre::RENDER_QUEUE_6, false);
                if (visual.node) m_terrainBatchVisuals.emplace(dirty.first, std::move(visual));
            }
            m_dirtyTerrainBatches.clear();
        }

        void clearTerrainBatches()
        {
            for (auto& entry : m_terrainBatchVisuals) destroySectionVisual(entry.second);
            m_terrainBatchVisuals.clear();
            m_dirtyTerrainBatches.clear();
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
            std::vector<ReferenceResidencyProbe::Retired> retired;
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->forgetWitness(visual.location);
            if(m_referenceResidencyProbe)for(auto& object:visual.renderables){const auto value=m_referenceResidencyProbe->detach(*object);if(value)retired.push_back(*value);}
            if (visual.batchMeshes)
            {
                const auto origin = terrainRenderBatchOrigin(visual.location);
                m_dirtyTerrainBatches[sectionKey(origin)] = origin;
                visual.batchMeshes.reset();
            }
            for (auto& renderable : visual.renderables)
            {
                if (m_floraWindCapture) m_floraWindCapture->detachRenderable(*renderable);
                if (m_shoreEditCapture) m_shoreEditCapture->detachRenderable(*renderable);
                if (m_referenceEditProbe) m_referenceEditProbe->detach(*renderable);
                if (renderable->isAttached())
                {
                    renderable->detachFromParent();
                }
            }
            visual.renderables.clear();
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->retired(retired);
            visual.fernDiagnosticFlora.reset();
            if (visual.node != nullptr && m_sceneManager != nullptr)
            {
                m_sceneManager->destroySceneNode(visual.node);
                visual.node = nullptr;
            }
        }

        void clearTransientInput()
        {
            m_mouseFrameInput.clear();
            m_pendingLookDelta = glm::vec2(0.0f);
            m_jumpPressed = false;
            m_toggleFlying = false;
            m_resetMeshes = false;
            m_useHeldFood = false;
            m_hotbarDelta = 0;
            m_hotbarSlot = -1;
        }

        void resetPlayerAvatarMotion() noexcept
        {
            m_playerAvatarPoseHistory = {};
            m_playerMovementSeconds = 0.f;
            m_playerLandingEnvelope = 0.f;
            m_previousPlayerGrounded = false;
            m_playerGroundStateInitialized = false;
            m_playerAvatarWasVisible = false;
        }

        void resetPlayerPresentation() noexcept
        {
            m_playerLighting = {};
            m_viewRangeSkyAvailability = 0.f;
            m_viewRangeSkyKnown = false;
            m_thirdPersonCameraState = {};
            m_effectiveCameraMode =
                ThirdPersonCameraPresentation::Mode::FirstPerson;
            resetPlayerAvatarMotion();
            m_playerInterpolationEpoch =
                m_worldPlayer != nullptr
                    ? m_worldPlayer->getInterpolationEpoch()
                    : 0;
            if (m_playerRenderer != nullptr)
            {
                m_playerRenderer->setVisible(false);
            }
            if (m_userInterface != nullptr)
            {
                m_userInterface->setFirstPersonPresentationVisible(true);
                m_userInterface->setThirdPersonAimIndicator(
                    false, 0.f, 0.f);
            }
        }

        PlayerAvatarPresentation::MotionStrength
        playerMotionStrength() const noexcept
        {
            switch (m_config.feedbackIntensity)
            {
                case GameplayFeedbackIntensity::Off:
                    return PlayerAvatarPresentation::MotionStrength::Off;
                case GameplayFeedbackIntensity::Reduced:
                    return PlayerAvatarPresentation::MotionStrength::Reduced;
                case GameplayFeedbackIntensity::Full:
                    return PlayerAvatarPresentation::MotionStrength::Full;
            }
            return PlayerAvatarPresentation::MotionStrength::Reduced;
        }

        void syncRenderCamera(float deltaSeconds)
        {
            if (m_logicCamera == nullptr || m_camera == nullptr)
            {
                if (m_camera != nullptr)
                    m_camera->setNearClipDistance(m_nominalCameraNearClipDistance);
                m_effectiveCameraMode =
                    ThirdPersonCameraPresentation::Mode::FirstPerson;
                if (m_userInterface != nullptr)
                {
                    m_userInterface->setThirdPersonAimIndicator(
                        false, 0.f, 0.f);
                }
                return;
            }

            glm::vec3 position = m_logicCamera->position;
            glm::vec3 rotation = m_logicCamera->rotation;
            if (m_visualCameraSweep.enabled)
            {
                if (!m_visualCameraSweepAnchored ||
                    m_visualCameraSweepElapsed <= VisualCameraSweep::WarmupSeconds)
                {
                    m_visualCameraSweepOrigin = position;
                    m_visualCameraSweepRotation = rotation;
                    m_visualCameraSweepAnchored = true;
                }
                const auto offset = m_visualCameraSweep.offset(m_visualCameraSweepElapsed);
                position = m_visualCameraSweepOrigin + glm::vec3(offset[0], offset[1], offset[2]);
                rotation = m_visualCameraSweepRotation + glm::vec3(offset[4], offset[3], 0.0);
                rotation.x = std::clamp(rotation.x, -89.f, 89.f);
                const int second = static_cast<int>(m_visualCameraSweepElapsed);
                if (second != m_visualCameraSweepLoggedSecond)
                {
                    m_visualCameraSweepLoggedSecond = second;
                    std::cout << "[VISUAL_CAMERA_SWEEP] seconds=" << m_visualCameraSweepElapsed
                              << " position=" << position.x << ',' << position.y << ',' << position.z
                              << " rotation=" << rotation.x << ',' << rotation.y
                              << " player=" << m_worldPlayer->position.x << ','
                              << m_worldPlayer->position.y << ',' << m_worldPlayer->position.z << '\n';
                }
                ThirdPersonCameraPresentation::advance(
                    m_thirdPersonCameraState,
                    ThirdPersonCameraPresentation::Mode::FirstPerson,
                    0.f, 0.f, deltaSeconds);
                m_effectiveCameraMode =
                    ThirdPersonCameraPresentation::Mode::FirstPerson;
                if (m_userInterface != nullptr)
                {
                    m_userInterface->setThirdPersonAimIndicator(
                        false, 0.f, 0.f);
                }
            }
            else
            {
                const glm::vec3* hitPoint = nullptr;
                if (m_sandbox != nullptr)
                {
                    // Selection remains authoritative on the logic-camera ray.
                    // The shoulder camera stays stable while the HUD projects
                    // its reticle onto this selected depth, preserving the
                    // authoritative ray at both near and far interaction ranges.
                    const auto& actorSelection =
                        m_sandbox->getActorSelection();
                    const auto& blockSelection =
                        m_sandbox->getBlockSelection();
                    hitPoint = actorSelection
                        ? &actorSelection->hitPoint
                        : (blockSelection ? &blockSelection->hitPoint
                                          : nullptr);
                }
                const auto pose = OgreThirdPersonCameraRig::updateCameraPose(
                    *m_logicCamera, *m_camera, m_nominalCameraNearClipDistance,
                    m_world, m_worldPlayer,
                    m_config.cameraPerspective, m_thirdPersonCameraState,
                    deltaSeconds, hitPoint);
                m_lastRenderCameraPose = pose;
                position = pose.position;
                rotation = pose.rotation;
                m_effectiveCameraMode = pose.effectiveMode;
                if (m_userInterface != nullptr)
                {
                    const bool thirdPerson =
                        pose.effectiveMode ==
                        ThirdPersonCameraPresentation::Mode::ThirdPersonRear;
                    m_userInterface->setThirdPersonAimIndicator(
                        thirdPerson && pose.aimIndicatorVisible,
                        thirdPerson ? pose.aimIndicatorNdc.x : 0.f,
                        thirdPerson ? pose.aimIndicatorNdc.y : 0.f);
                }
            }
            OgreThirdPersonCameraRig::applyCameraPose(
                *m_camera, position, rotation);
        }

        void syncPlayerPresentation(float deltaSeconds)
        {
            if (m_playerRenderer == nullptr || m_worldPlayer == nullptr ||
                m_logicCamera == nullptr || m_sandbox == nullptr)
            {
                if (m_playerRenderer != nullptr)
                {
                    m_playerRenderer->setVisible(false);
                }
                return;
            }

            const std::uint64_t interpolationEpoch =
                m_worldPlayer->getInterpolationEpoch();
            if (interpolationEpoch != m_playerInterpolationEpoch)
            {
                resetPlayerAvatarMotion();
                // Teleport and respawn must anchor to the new eye-light
                // snapshot instead of carrying outdoor fade into a cave.
                m_viewRangeSkyAvailability = 0.f;
                m_viewRangeSkyKnown = false;
                m_playerInterpolationEpoch = interpolationEpoch;
            }

            const float elapsed = std::clamp(
                std::isfinite(deltaSeconds) ? deltaSeconds : 0.f,
                0.f, .1f);
            const bool grounded = m_worldPlayer->isOnGround();
            m_playerLandingEnvelope = std::max(
                0.f, m_playerLandingEnvelope - elapsed / .22f);
            if (m_playerGroundStateInitialized && grounded &&
                !m_previousPlayerGrounded)
            {
                m_playerLandingEnvelope = 1.f;
            }
            m_previousPlayerGrounded = grounded;
            m_playerGroundStateInitialized = true;

            const float horizontalSpeed = std::sqrt(
                m_worldPlayer->velocity.x * m_worldPlayer->velocity.x +
                m_worldPlayer->velocity.z * m_worldPlayer->velocity.z);
            const float movementStrength = std::clamp(
                horizontalSpeed / 4.5f, 0.f, 1.f);
            const float gaitRate = std::clamp(
                horizontalSpeed / 4.5f, 0.f, 1.65f);
            if (gaitRate > .02f)
            {
                m_playerMovementSeconds += elapsed * gaitRate;
            }

            const ActionFeedbackSnapshot actionFeedback =
                m_sandbox->getActionFeedback();
            const MiningProgressSnapshot& mining =
                m_sandbox->getMiningProgress();
            PlayerHandPresentation::Action feedbackAction =
                PlayerHandPresentation::Action::None;
            switch (actionFeedback.kind)
            {
                case ActionFeedbackKind::BlockBreak:
                case ActionFeedbackKind::BlockPlace:
                case ActionFeedbackKind::AttackMiss:
                case ActionFeedbackKind::AttackHit:
                case ActionFeedbackKind::Guard:
                    feedbackAction =
                        PlayerHandPresentation::Action::Strike;
                    break;
                case ActionFeedbackKind::BlockUse:
                    feedbackAction =
                        PlayerHandPresentation::Action::Use;
                    break;
                case ActionFeedbackKind::FoodConsume:
                    feedbackAction =
                        PlayerHandPresentation::Action::Consume;
                    break;
                case ActionFeedbackKind::None:
                case ActionFeedbackKind::PlayerHurt:
                case ActionFeedbackKind::ItemPickup:
                    break;
            }
            const auto actionPhase = PlayerHandPresentation::actionPhase(
                mining.active, mining.elapsedSeconds,
                feedbackAction, actionFeedback.elapsedSeconds);
            PlayerHandPresentation::MotionInput actionMotionInput;
            actionMotionInput.action = actionPhase.action;
            actionMotionInput.actionSeconds = actionPhase.seconds;
            actionMotionInput.strength = 1.f;
            actionMotionInput.recoil = actionFeedback.recoil;
            if (actionPhase.acceptsFeedbackContact &&
                actionFeedbackHoldsContact(actionFeedback.kind))
            {
                const float recovery = std::clamp(
                    actionFeedback.secondsRemaining / .32f, 0.f, 1.f);
                actionMotionInput.contact =
                    actionFeedback.hitStopSeconds > 0.f
                        ? 1.f
                        : recovery * recovery;
            }
            const auto toolPose = ToolActionPresentation::derive(
                actionMotionInput.action, actionMotionInput.actionSeconds,
                actionMotionInput.strength, actionMotionInput.recoil,
                actionMotionInput.contact);

            PlayerAvatarPresentation::Snapshot snapshot;
            const glm::vec3 playerCentre(
                m_logicCamera->position.x,
                m_logicCamera->position.y - .6f,
                m_logicCamera->position.z);
            const glm::vec3 feet = playerCentre - glm::vec3(
                0.f, m_worldPlayer->box.dimensions.y, 0.f);
            snapshot.position = {feet.x, feet.y, feet.z};
            snapshot.rotationDegrees = {
                m_logicCamera->rotation.x, m_logicCamera->rotation.y,
                m_logicCamera->rotation.z};
            snapshot.velocity = {
                m_worldPlayer->velocity.x, m_worldPlayer->velocity.y,
                m_worldPlayer->velocity.z};
            snapshot.grounded = grounded;
            snapshot.feedback.landing = m_playerLandingEnvelope;
            snapshot.feedback.tool = toolPose;
            snapshot.feedback.hurt =
                actionFeedback.kind == ActionFeedbackKind::PlayerHurt &&
                        m_config.feedbackIntensity !=
                            GameplayFeedbackIntensity::Off
                    ? std::max(.6f, actionFeedback.recoil)
                    : 0.f;
            snapshot.movementSeconds = m_playerMovementSeconds;
            snapshot.movementStrength = movementStrength;

            if (!m_playerMotionCapture.empty()) {
                // Exercise the real avatar path with copied render facts only.
                // The diagnostic never writes Player velocity or input state.
                m_playerMotionCaptureSeconds += elapsed;
                const float yaw = PlayerAvatarPresentation::wrapDegrees(snapshot.rotationDegrees.y) *
                    3.14159265359f / 180.f;
                const bool lateral = m_playerMotionCapture == "left" || m_playerMotionCapture == "right";
                const float sign = m_playerMotionCapture == "backward" || m_playerMotionCapture == "left" ? -1.f : 1.f;
                snapshot.velocity = lateral
                    ? PlayerAvatarPresentation::Vec3{4.5f * sign * std::cos(yaw), 0.f, 4.5f * sign * std::sin(yaw)}
                    : PlayerAvatarPresentation::Vec3{4.5f * sign * std::sin(yaw), 0.f, -4.5f * sign * std::cos(yaw)};
                snapshot.movementStrength = 1.f;
                snapshot.movementSeconds = m_playerMotionCaptureSeconds;
            }

            PlayerAvatarPresentation::Pose pose =
                PlayerAvatarPresentation::derivePose(
                    snapshot, m_playerAvatarProfile,
                    playerMotionStrength());
            // derivePose applies the accessibility scale once to articulation.
            // Scale only the independent shader tint here, avoiding a second
            // reduction of the body motion.
            if (m_config.feedbackIntensity ==
                GameplayFeedbackIntensity::Reduced)
            {
                pose.weights.hurt *= .55f;
            }
            const auto visibility =
                OgreThirdPersonCameraRig::visibilityForCamera(
                    m_effectiveCameraMode, m_visualCameraSweep.enabled);
            const bool visible = visibility.avatarVisible;
            if (m_userInterface != nullptr)
            {
                m_userInterface->setFirstPersonPresentationVisible(
                    visibility.firstPersonHandVisible);
            }
            if ((visible && !m_playerAvatarWasVisible) ||
                m_applicationFlow.state() != GameApplicationState::Playing ||
                !m_focusGate.isFocused())
            {
                m_playerAvatarPoseHistory = {};
            }
            pose = PlayerAvatarPresentation::smoothPose(
                m_playerAvatarPoseHistory, pose,
                m_playerAvatarProfile, deltaSeconds);
            const ItemStack& heldItem = m_worldPlayer->getHeldItems();
            const Material::ID heldMaterial = heldItem.isEmpty()
                ? Material::Nothing
                : heldItem.getMaterial().id;
            OgreThirdPersonCameraRig::syncAvatarForCamera(
                *m_playerRenderer, m_playerAvatarProfile, pose,
                m_effectiveCameraMode, m_visualCameraSweep.enabled,
                heldMaterial);
            if (!visible)
            {
                m_playerAvatarPoseHistory = {};
            }
            m_playerAvatarWasVisible = visible;
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
                for (const auto& visualEntry : m_terrainBatchVisuals)
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

        void restoreActorMaterialTints()
        {
            struct ActorTint
            {
                const char* material;
                Ogre::Vector4 value;
            };
            const ActorTint tints[] = {
                {"HelloMine3D/ActorMob", {.32f, .48f, .26f, 1.f}},
                {"HelloMine3D/ActorPlayer", {.23f, .46f, .47f, 1.f}},
                {"HelloMine3D/ActorStalker", {.25f, .47f, .51f, 1.f}},
                {"HelloMine3D/ActorBrute", {.55f, .32f, .24f, 1.f}},
                {"HelloMine3D/ActorSpitter", {.39f, .33f, .50f, 1.f}},
                {"HelloMine3D/ActorSheep", {.72f, .68f, .57f, 1.f}},
                {"HelloMine3D/ActorRabbit", {.55f, .45f, .34f, 1.f}},
                {"HelloMine3D/ActorMarshBird", {.38f, .51f, .49f, 1.f}},
                {"HelloMine3D/ActorItem", {1.f, .67f, .12f, 1.f}},
                {"HelloMine3D/CombatProjectile", {.92f, .35f, 1.f, 1.f}}
            };
            for (const ActorTint& tint : tints)
            {
                materialPass(tint.material)
                    ->getFragmentProgramParameters()
                    ->setNamedConstant("actorTint", tint.value);
            }
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
                {OgrePlayerRenderer::HeldMaterialName, "HelloMine3D/TerrainVertex",
                 "HelloMine3D/TerrainShadowVertex",
                 "HelloMine3D/TerrainFragment",
                 "HelloMine3D/TerrainShadowFragment"},
                {OgrePlayerRenderer::HeldTransparentMaterialName, "HelloMine3D/TerrainVertex",
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
                {"HelloMine3D/ActorSheep", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/ActorRabbit", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/ActorMarshBird", "HelloMine3D/ActorVertex",
                 "HelloMine3D/ActorShadowVertex",
                 "HelloMine3D/ActorFragment",
                 "HelloMine3D/ActorShadowFragment"},
                {"HelloMine3D/ActorPlayer", "HelloMine3D/ActorVertex",
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
                const bool terrain = std::string(receiver.fragment) == "HelloMine3D/TerrainFragment";
                pass->setFragmentProgram(terrain && runtimeTerrainMaterialProfile().usesTextureArray()
                    ? (m_referenceSurfaceEnabled
                        ? (enabled ? "HelloMine3D/TerrainShadowSurfaceFragment" : "HelloMine3D/TerrainSurfaceFragment")
                        : (enabled ? "HelloMine3D/TerrainShadowArrayFragment" : "HelloMine3D/TerrainArrayFragment"))
                    : (enabled ? receiver.shadowFragment : receiver.fragment));
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
            // setFragmentProgram() replaces the program parameter set. Restore
            // the per-material palette that was originally declared in the
            // material script after every normal/shadow program transition.
            restoreActorMaterialTints();
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
                OgrePlayerRenderer::HeldMaterialName,
                OgrePlayerRenderer::HeldTransparentMaterialName,
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
                "HelloMine3D/ActorSheep", "HelloMine3D/ActorRabbit",
                "HelloMine3D/ActorMarshBird",
                "HelloMine3D/ActorPlayer",
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
            // CONTENT_SHADOW units own the last shadow TexturePtr. Drop our
            // receiver units before SceneManager destroys its shadow textures
            // and calls refcount-aware ShadowTextureManager::clearUnused().
            // Other scenes' references remain protected by Ogre's own policy.
            m_directionalShadowQuality = DirectionalShadowQuality::Off;
            m_directionalShadowStrength = 0.f;
            setDirectionalShadowReceiverPrograms(false);
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
            if (m_actorRenderer != nullptr)
            {
                m_actorRenderer->setCastShadows(false);
            }
            if (m_playerRenderer != nullptr)
            {
                m_playerRenderer->setCastShadows(false);
            }
        }

        bool configureDirectionalShadows(
            DirectionalShadowQuality requestedQuality)
        {
            if (m_sceneManager == nullptr)
            {
                return false;
            }

            if (m_directionalSunLight != nullptr)
            {
                m_directionalSunLight->setCastShadows(false);
            }
            m_directionalShadowQuality =
                DirectionalShadowQuality::Off;
            m_directionalShadowStrength = 0.f;
            setDirectionalShadowReceiverPrograms(false);
            // Match normal world-close ordering: base receiver references are
            // gone before Ogre's refcount-aware shadow cache cleanup.
            m_sceneManager->setShadowTechnique(Ogre::SHADOWTYPE_NONE);
            if (m_actorRenderer != nullptr)
            {
                m_actorRenderer->setCastShadows(false);
            }
            if (m_playerRenderer != nullptr)
            {
                m_playerRenderer->setCastShadows(false);
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
                    m_directionalSunLight->setCustomShadowCameraSetup(
                        Ogre::ShadowCameraSetupPtr(OGRE_NEW StableSolarShadowCamera()));
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
                if (m_playerRenderer != nullptr)
                {
                    m_playerRenderer->setCastShadows(true);
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
                          << " pcf=quadratic-3x3 camera=stable-solar-v1 bias=" << profile.bias << '\n';
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

        void configureTerrainAppearance()
        {
            GLint maxLayers = 0, maxEdge = 0;
            glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxEdge);
            const bool capable = maxLayers >= 256 && maxEdge >= 64 &&
                m_root->getRenderSystem()->getCapabilities()->hasCapability(Ogre::RSC_TEXTURE_3D) &&
                !isTrueValue(std::getenv("HELLOMINE3D_FORCE_LEGACY_TERRAIN"));
            auto &profile = runtimeTerrainMaterialProfile();
            profile.freezeRenderingMode(m_config.visualDetail == VisualDetail::Standard, capable);
            if (profile.usesTextureArray())
            {
                m_terrainArrayLoader = std::make_unique<TerrainArrayLoader>(
                    TerrainTextureArray::load(runtimeResourcePackResolver().resolve(
                        profile.parameters().arrayTexture)));
                const auto &data = m_terrainArrayLoader->data;
                m_terrainArray = Ogre::TextureManager::getSingleton().createManual(
                    "HelloMine3D/TerrainArray64", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                    Ogre::TEX_TYPE_2D_ARRAY, data.edge, data.edge, data.layers,
                    static_cast<int>(data.mipCount - 1), Ogre::PF_BYTE_RGBA,
                    Ogre::TU_STATIC_WRITE_ONLY, m_terrainArrayLoader.get(), false);
                if (m_terrainArray.isNull())
                    throw std::runtime_error("Failed to allocate supported terrain texture array.");
                m_terrainArray->load();
                for (const char *name : {"HelloMine3D/TerrainArrayFragment",
                                        "HelloMine3D/TerrainShadowArrayFragment"})
                {
                    auto program = Ogre::HighLevelGpuProgramManager::getSingleton().getByName(name);
                    if (program.isNull())
                        throw std::runtime_error(std::string("Missing standard terrain shader: ") + name);
                    program->load();
                    if (!program->isSupported() || program->hasCompileError())
                        throw std::runtime_error(std::string("Invalid standard terrain shader: ") + name);
                }
                for (const char *name : {"HelloMine3D/Terrain", "HelloMine3D/Transparent",
                                        OgrePlayerRenderer::HeldMaterialName,
                                        OgrePlayerRenderer::HeldTransparentMaterialName,
                                        "HelloMine3D/Flora"})
                {
                    auto material = Ogre::MaterialManager::getSingleton().getByName(name);
                    if (material.isNull() || material->getNumTechniques() == 0)
                        throw std::runtime_error(std::string("Missing terrain material: ") + name);
                    auto *pass = material->getTechnique(0)->getPass(0);
                    pass->setFragmentProgram("HelloMine3D/TerrainArrayFragment");
                    auto *unit = pass->getTextureUnitState(0);
                    unit->setTexture(m_terrainArray);
                    unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_WRAP);
                    // Keep the authored pixel edges while blending only
                    // between independent mip levels in the distance.
                    unit->setTextureFiltering(Ogre::FT_MIN, Ogre::FO_POINT);
                    unit->setTextureFiltering(Ogre::FT_MAG, Ogre::FO_POINT);
                    unit->setTextureFiltering(Ogre::FT_MIP, Ogre::FO_LINEAR);
                }
                std::cout << "[TERRAIN_ARRAY] edge=" << data.edge
                          << " layers=" << data.layers << " mips=" << data.mipCount
                          << " rgba_bytes=" << data.rgba.size()
                          << " retained_cpu_bytes=" << data.rgba.size()
                          << " legacy_atlas_bytes=262144 reloadable=1\n";
            }
            syncReferenceSurfaceMode();
            std::cout << "[TERRAIN_APPEARANCE] standard=" << profile.usesTextureArray()
                      << " leaf_geometry=cube"
                      << " reason=" << profile.renderingModeReason()
                      << " max_array_layers=" << maxLayers << '\n';
        }

        void syncReferenceSurfaceMode()
        {
            bool surfaceShaderAvailable=false;
            if (m_hdrPipeline && m_hdrPipeline->active())
            {
                unsigned supported=0;
                for (const char* name:{"HelloMine3D/TerrainSurfaceFragment",
                                      "HelloMine3D/TerrainShadowSurfaceFragment"})
                {
                    auto program=Ogre::HighLevelGpuProgramManager::getSingleton().getByName(name);
                    if (program.isNull()) continue; // Complete old program set.
                    const auto parameters=program->getDefaultParameters();
                    unsigned fields=0;
                    for (const char* field:{"terrainNormalArray","terrainSurfaceArray",
                                           "localLightCount","localLightPositionRadius","localLightColourEnergy"})
                        fields+=parameters->_findNamedConstantDefinition(field,false)!=nullptr;
                    if (fields!=0 && fields!=5)
                        throw std::runtime_error(std::string("Incomplete reference surface shader interface: ")+name);
                    supported+=fields==5;
                }
                if (supported==1)
                    throw std::runtime_error("Reference surface shader interfaces disagree between receiver variants.");
                surfaceShaderAvailable=supported==2;
            }
            const bool enabled = m_hdrPipeline && m_hdrPipeline->active() &&
                runtimeTerrainMaterialProfile().usesTextureArray() && surfaceShaderAvailable &&
                runtimeReferenceSurfaceProfile().usableWithEffectiveTerrain();
            if (enabled == m_referenceSurfaceEnabled) return;
            if (enabled && m_referenceArrays[0].isNull())
            {
                const auto &profile = runtimeReferenceSurfaceProfile().parameters();
                std::size_t bytes = 0;
                for (unsigned channel=0;channel<3;++channel)
                {
                    m_referenceLoaders[channel] = std::make_unique<TerrainArrayLoader>(
                        TerrainTextureArray::load(runtimeResourcePackResolver().resolve(profile.textures[channel])));
                    const auto &data=m_referenceLoaders[channel]->data;
                    m_referenceArrays[channel]=Ogre::TextureManager::getSingleton().createManual(
                        "HelloMine3D/ReferenceSurface"+std::to_string(channel),
                        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                        Ogre::TEX_TYPE_2D_ARRAY,data.edge,data.edge,data.layers,
                        static_cast<int>(data.mipCount-1),Ogre::PF_BYTE_RGBA,
                        Ogre::TU_STATIC_WRITE_ONLY,m_referenceLoaders[channel].get(),channel==0);
                    if (m_referenceArrays[channel].isNull() ||
                        m_referenceArrays[channel]->isHardwareGammaEnabled()!=(channel==0))
                        throw std::runtime_error("Invalid reference surface channel gamma/storage.");
                    m_referenceArrays[channel]->load();
                    bytes+=data.rgba.size();
                }
                for (const char *name:{"HelloMine3D/TerrainSurfaceFragment",
                                      "HelloMine3D/TerrainShadowSurfaceFragment"})
                {
                    auto program=Ogre::HighLevelGpuProgramManager::getSingleton().getByName(name);
                    if (program.isNull()) throw std::runtime_error(std::string("Missing surface shader: ")+name);
                    program->load();
                    auto *shader=dynamic_cast<Ogre::GLSLShader*>(program.get());
                    if (!shader || !shader->compile(true) || program->hasCompileError())
                        throw std::runtime_error(std::string("Invalid surface shader: ")+name);
                }
                std::cout<<"[REFERENCE_SURFACE] channels=3 edge=64 layers=256 mips=7"
                         <<" colour_srgb=1 data_srgb=0 gpu_bytes="<<bytes
                         <<" reload_cpu_bytes="<<bytes<<" source_index_bytes_per_section=512\n";
            }
            m_referenceSurfaceEnabled=enabled;
            for (const char *name:{"HelloMine3D/Terrain","HelloMine3D/Transparent",
                                  OgrePlayerRenderer::HeldMaterialName,
                                  OgrePlayerRenderer::HeldTransparentMaterialName,"HelloMine3D/Flora"})
            {
                auto *pass=materialPass(name);
                for(int i=static_cast<int>(pass->getNumTextureUnitStates())-1;i>=1;--i)
                {
                    auto *unit=pass->getTextureUnitState(static_cast<unsigned short>(i));
                    if (unit->getName()=="referenceNormal" || unit->getName()=="referenceSurface" ||
                        unit->getContentType()==Ogre::TextureUnitState::CONTENT_SHADOW)
                        pass->removeTextureUnitState(static_cast<unsigned short>(i));
                }
                pass->getTextureUnitState(0)->setTexture(enabled?m_referenceArrays[0]:m_terrainArray);
                if (enabled)
                {
                    pass->getTextureUnitState(0)->setTextureFiltering(Ogre::TFO_TRILINEAR);
                    for(unsigned channel=1;channel<3;++channel)
                    {
                        auto *unit=pass->createTextureUnitState();
                        unit->setName(channel==1?"referenceNormal":"referenceSurface");
                        unit->setTexture(m_referenceArrays[channel]);
                        unit->setTextureAddressingMode(Ogre::TextureUnitState::TAM_WRAP);
                        unit->setTextureFiltering(Ogre::TFO_TRILINEAR);
                    }
                }
                else
                {
                    auto *unit=pass->getTextureUnitState(0);
                    unit->setTextureFiltering(Ogre::FT_MIN,Ogre::FO_POINT);
                    unit->setTextureFiltering(Ogre::FT_MAG,Ogre::FO_POINT);
                    unit->setTextureFiltering(Ogre::FT_MIP,Ogre::FO_LINEAR);
                }
            }
            setDirectionalShadowReceiverPrograms(m_directionalShadowQuality!=DirectionalShadowQuality::Off);
            std::cout<<"[REFERENCE_SURFACE_MODE] active="<<enabled
                     <<" resource_reason="<<runtimeReferenceSurfaceProfile().selectionReason()
                     <<" shader_available="<<surfaceShaderAvailable<<" colour_domain="
                     <<(enabled?"linear-srgb-sampled":"authored-legacy")<<'\n';
        }

        void bindLocalLightParameters(Ogre::GpuProgramParametersSharedPtr parameters)
        {
            if (!m_referenceSurfaceEnabled ||
                !parameters->_findNamedConstantDefinition("localLightCount",false)) return;
            std::array<float,32> positions{},radiance{};
            for(std::size_t i=0;i<m_localLights.count;++i)
            {
                const auto &light=m_localLights.sources[i];
                positions[i*4]=light.position.x; positions[i*4+1]=light.position.y;
                positions[i*4+2]=light.position.z; positions[i*4+3]=light.radius;
                radiance[i*4]=light.colour.r; radiance[i*4+1]=light.colour.g;
                radiance[i*4+2]=light.colour.b; radiance[i*4+3]=light.energy;
            }
            parameters->setNamedConstant("localLightCount",static_cast<int>(m_localLights.count));
            parameters->setNamedConstant("localLightPositionRadius",positions.data(),8,4);
            parameters->setNamedConstant("localLightColourEnergy",radiance.data(),8,4);
        }

        void syncTerrainMaterialParameters()
        {
            const TerrainMaterialParameters &profile =
                runtimeTerrainMaterialProfile().parameters();
            const char *terrainMaterials[] = {
                "HelloMine3D/Terrain", "HelloMine3D/Transparent",
                OgrePlayerRenderer::HeldMaterialName,
                OgrePlayerRenderer::HeldTransparentMaterialName,
                "HelloMine3D/Flora"};
            for (const char *materialName : terrainMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                if (!runtimeTerrainMaterialProfile().usesTextureArray())
                {
                    parameters->setNamedConstant("atlasPixels", static_cast<float>(profile.atlasPixels));
                    parameters->setNamedConstant("tilePixels", static_cast<float>(profile.tilePixels));
                }
                else
                {
                    const bool cutout = std::string(materialName) != "HelloMine3D/Transparent" &&
                        std::string(materialName) != OgrePlayerRenderer::HeldTransparentMaterialName;
                    parameters->setNamedConstant("alphaCutoff", cutout ? 0.5f : 0.01f);
                }
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
            // Program switches (including shadow On/Off) replace parameter
            // sets. Rebind the frozen active colour mode after every switch.
            if (m_hdrPipeline) m_hdrPipeline->applySceneParameters();
        }

        void syncEnvironment(const WorldEnvironmentState& air, float deltaSeconds)
        {
            float immersion = 0.f;
            WorldEnvironmentState regionalAir = air;
            // Water visibility is a basic camera medium, independent of the
            // optional cloud, surface-lighting and water-detail features.
            if (m_world != nullptr && m_camera != nullptr)
            {
                const Ogre::Vector3 eye = m_camera->getDerivedPosition();
                const int x = World::toBlockCoord(eye.x);
                const int y = World::toBlockCoord(eye.y);
                const int z = World::toBlockCoord(eye.z);
                if (m_v10cAtmosphereEnabled)
                {
                    const auto& generator = m_world->getChunkManager().getTerrainGenerator();
                    const auto region = m_regionalAtmosphere.at(eye.x, eye.z,
                        [&](int sx, int sz) { return generator.getBiomeAtWorld(sx, sz); },
                        [&](int sx, int sz) { return generator.getSurfaceHeightAtWorld(sx, sz); });
                    regionalAir = RegionalAtmosphere::apply(air, region);
                }
                // getBlock only observes resident chunks, never loads/generates.
                if (m_world->getBlock(x, y, z) == BlockId::Water)
                    immersion = m_world->getBlock(x, y + 1, z) == BlockId::Water
                        ? 1.f : WorldEnvironment::cameraWaterImmersion(y + 1.f - eye.y);
            }
            const WorldEnvironmentState state = WorldEnvironment::forCameraMedium(regionalAir, immersion);
            if (m_blockFeedback != nullptr) m_blockFeedback->setEnvironment(state);
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
            // A camera-centred square preserves diagonal terrain while fitting
            // inside demand even just before a chunk crossing. Leave a
            // two-metre animation guard and retire only the final eight metres.
            const float rangeEnd = std::max(1, m_config.renderDistance) * CHUNK_SIZE - 2.f;
            const Ogre::Vector2 viewRange(std::max(rangeEnd * .5f, rangeEnd - 8.f), rangeEnd);
            // Residency follows the logical camera, independently of shoulder
            // presentation and diagnostic camera sweeps.
            const Ogre::Vector2 viewRangeCentre(
                m_logicCamera != nullptr ? m_logicCamera->position.x : 0.f,
                m_logicCamera != nullptr ? m_logicCamera->position.z : 0.f);
            // Reuse the resident eye-light snapshot. Block light (including
            // torches) must never make an enclosed space look like open sky.
            if (m_frameWorldStats.playerLocalLightKnown)
            {
                const float target = std::clamp(
                    static_cast<float>(m_frameWorldStats.playerSunlight) / MAX_LIGHT_LEVEL,
                    0.f, 1.f);
                if (!m_viewRangeSkyKnown)
                {
                    m_viewRangeSkyAvailability = target;
                    m_viewRangeSkyKnown = true;
                }
                else
                {
                    const float amount = 1.f - std::exp(-4.f * std::max(0.f, deltaSeconds));
                    m_viewRangeSkyAvailability += (target - m_viewRangeSkyAvailability) * amount;
                }
            }
            const float skyBlend = std::clamp((m_viewRangeSkyAvailability - .05f) / .25f, 0.f, 1.f);
            const float viewRangeStrength = skyBlend * skyBlend * (3.f - 2.f * skyBlend);
            const bool shadowActive =
                m_directionalShadowQuality !=
                DirectionalShadowQuality::Off;
            const float shadowStrength = shadowActive
                ? std::max(0.f, std::min(0.34f,
                      state.sunIntensity * 0.34f))
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
            auto casterParameters = materialPass("HelloMine3D/DirectionalShadowCaster")
                ->getFragmentProgramParameters();
            casterParameters->setNamedConstant("viewRange", viewRange);
            casterParameters->setNamedConstant("viewRangeCentre", viewRangeCentre);
            casterParameters->setNamedConstant("viewRangeStrength", viewRangeStrength);

            m_sceneManager->setFog(Ogre::FOG_EXP2, fog,
                                   state.fogDensity);
            if (m_camera != nullptr && m_camera->getViewport() != nullptr)
            {
                m_camera->getViewport()->setBackgroundColour(fog);
            }

            const char* terrainMaterials[] = {
                "HelloMine3D/Terrain", "HelloMine3D/Transparent",
                OgrePlayerRenderer::HeldMaterialName,
                OgrePlayerRenderer::HeldTransparentMaterialName,
                "HelloMine3D/Flora"};
            for (const char* materialName : terrainMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                bindLocalLightParameters(parameters);
                parameters->setNamedConstant(
                    "environmentLight", state.daylight);
                parameters->setNamedConstant("fogColour", fogVector);
                parameters->setNamedConstant(
                    "fogSunwardColour", fogSunwardColour);
                parameters->setNamedConstant(
                    "sunDirection", sunDirection);
                parameters->setNamedConstant("sunColour", Ogre::Vector3(
                    state.sunColour.r, state.sunColour.g, state.sunColour.b));
                parameters->setNamedConstant("sunIntensity", state.sunIntensity);
                parameters->setNamedConstant("surfaceLightingStrength",
                    m_v10cAtmosphereEnabled ? 1.0f : 0.0f);
                parameters->setNamedConstant(
                    "fogDirectionalStrength", directionalStrength);
                parameters->setNamedConstant(
                    "fogDensity", state.fogDensity);
                parameters->setNamedConstant("viewRange", viewRange);
                parameters->setNamedConstant("viewRangeCentre", viewRangeCentre);
                parameters->setNamedConstant("viewRangeStrength", viewRangeStrength);
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
            waterParameters->setNamedConstant("viewRange", viewRange);
            waterParameters->setNamedConstant("viewRangeCentre", viewRangeCentre);
            waterParameters->setNamedConstant("viewRangeStrength", viewRangeStrength);
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
            waterParameters->setNamedConstant("waterDetailStrength",
                m_v10cAtmosphereEnabled ? 1.f : 0.f);
            materialPass("HelloMine3D/Water")->getVertexProgramParameters()
                ->setNamedConstant("waterDetailStrength",
                    m_v10cAtmosphereEnabled ? 1.f : 0.f);

            const char* actorMaterials[] = {
                "HelloMine3D/ActorMob", "HelloMine3D/ActorStalker",
                "HelloMine3D/ActorBrute", "HelloMine3D/ActorSpitter",
                "HelloMine3D/ActorSheep", "HelloMine3D/ActorRabbit",
                "HelloMine3D/ActorMarshBird",
                "HelloMine3D/ActorPlayer",
                "HelloMine3D/ActorItem",
                "HelloMine3D/CombatProjectile"};
            for (const char* materialName : actorMaterials)
            {
                Ogre::GpuProgramParametersSharedPtr parameters =
                    materialPass(materialName)
                        ->getFragmentProgramParameters();
                parameters->setNamedConstant("actorSurfaceStrength",
                    m_v10cAtmosphereEnabled ? 1.f : 0.f);
                bindLocalLightParameters(parameters);
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
                parameters->setNamedConstant("viewRange", viewRange);
                parameters->setNamedConstant("viewRangeCentre", viewRangeCentre);
                parameters->setNamedConstant("viewRangeStrength", viewRangeStrength);
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

        void toggleCameraPerspective()
        {
            Config candidate = m_config;
            candidate.cameraPerspective =
                m_config.cameraPerspective == CameraPerspective::FirstPerson
                    ? CameraPerspective::ThirdPerson
                    : CameraPerspective::FirstPerson;
            std::string error;
            if (!saveRuntimeConfig(ResourcePaths::bin("config.txt"),
                                   candidate, &error))
            {
                std::cerr << "[CAMERA_PERSPECTIVE] save-failed="
                          << error << '\n';
                if (m_userInterface != nullptr)
                {
                    m_userInterface->setStatusMessage(
                        LocalizedPresentation::text(
                            m_config.locale,
                            "camera.perspective_save_failed"));
                }
                return;
            }

            userSettings(m_config) = userSettings(candidate);
            if (m_sandbox != nullptr)
            {
                m_sandbox->applyUserSettings(userSettings(m_config));
            }
            if (m_userInterface != nullptr)
            {
                m_userInterface->reportSettingsApplied(
                    true, userSettings(m_config),
                    m_config.cameraPerspective ==
                            CameraPerspective::ThirdPerson
                        ? "camera.perspective.third"
                        : "camera.perspective.first");
            }
        }

        bool keyPressed(const OIS::KeyEvent& event) override
        {
            if(m_lifecycleProbe) ++m_lifecycleInputEvents;
            if(m_referenceEditProbe) ++m_referenceEditInputEvents;
            if(m_referenceResidencyProbe)++m_referenceResidencyInputs;
            if(m_referenceRestartObservation)m_referenceRestartObservation->input();
            const bool isJumpKey = event.key == toOisKey(
                m_config.inputBindings.get(GameplayAction::Jump));
            const bool firstJumpPress = isJumpKey && m_jumpHeldKey != event.key;
            // Track ownership even while a UI consumes the key, so OS repeat
            // cannot become a fresh world press when that UI closes.
            if (isJumpKey) m_jumpHeldKey = event.key;
            bool firstCursorTogglePress = false;
            const bool firstCameraTogglePress =
                event.key == OIS::KC_F5 && !m_f5KeyHeld;
            if (event.key == OIS::KC_F5)
            {
                m_f5KeyHeld = true;
            }
            if (event.key == OIS::KC_GRAVE || event.key == OIS::KC_L || event.key == OIS::KC_TAB)
            {
                bool &held = event.key == OIS::KC_GRAVE
                    ? m_graveKeyHeld : event.key == OIS::KC_L ? m_lKeyHeld : m_tabKeyHeld;
                firstCursorTogglePress = !held;
                held = true;
            }
            if (m_userInterface != nullptr)
            {
                m_userInterface->keyEvent(event, true, *m_keyboard);
            }
            if (!m_focusGate.isFocused() || m_focusTransitionFrame)
            {
                return true;
            }
#if defined(__APPLE__)
            // Cocoa can forward Command-key events to OIS. Do not let a
            // system shortcut also trigger gameplay actions such as hotbar 4.
            if (m_keyboard->isKeyDown(OIS::KC_LWIN) ||
                m_keyboard->isKeyDown(OIS::KC_RWIN))
            {
                return true;
            }
#endif
            if (event.key == OIS::KC_ESCAPE)
            {
                if (m_userInterface != nullptr &&
                    m_userInterface->hasBlockingModal())
                {
                    return true;
                }
                if (m_userInterface != nullptr &&
                    m_userInterface->dismissHudInteraction())
                {
                    updateNativeCursorCapture();
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
            // Tab is dedicated to the HUD pointer. Retain the old L/grave
            // aliases only when a user gameplay binding does not own that key.
            bool boundGameplayKey = false;
            for (std::size_t i = 0; i < GameplayActionCount; ++i)
                boundGameplayKey = boundGameplayKey || event.key == toOisKey(
                    m_config.inputBindings.get(static_cast<GameplayAction>(i)));
            if ((event.key == OIS::KC_TAB ||
                 ((event.key == OIS::KC_GRAVE || event.key == OIS::KC_L) && !boundGameplayKey)) &&
                firstCursorTogglePress && m_userInterface != nullptr &&
                m_userInterface->toggleHudPointer())
            {
                updateNativeCursorCapture();
                return true;
            }
            if (m_userInterface != nullptr &&
                m_userInterface->wantsKeyboardInput())
            {
                return true;
            }

            if (isJumpKey)
            {
                if (firstJumpPress && acceptsWorldInput()) m_jumpPressed = true;
                return true;
            }

            if (event.key == OIS::KC_F5 &&
                firstCameraTogglePress &&
                m_applicationFlow.state() == GameApplicationState::Playing)
            {
                toggleCameraPerspective();
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
            if(m_lifecycleProbe) ++m_lifecycleInputEvents;
            if(m_referenceEditProbe) ++m_referenceEditInputEvents;
            if(m_referenceResidencyProbe)++m_referenceResidencyInputs;
            if(m_referenceRestartObservation)m_referenceRestartObservation->input();
            if (m_jumpHeldKey == event.key) m_jumpHeldKey.reset();
            if (event.key == OIS::KC_GRAVE)
            {
                m_graveKeyHeld = false;
            }
            if (event.key == OIS::KC_L)
            {
                m_lKeyHeld = false;
            }
            if (event.key == OIS::KC_TAB) m_tabKeyHeld = false;
            if (event.key == OIS::KC_F5) m_f5KeyHeld = false;
            if (m_userInterface != nullptr)
            {
                m_userInterface->keyEvent(event, false, *m_keyboard);
            }
            return true;
        }

        bool mouseMoved(const OIS::MouseEvent& event) override
        {
            if(m_lifecycleProbe) ++m_lifecycleInputEvents;
            if(m_referenceEditProbe) ++m_referenceEditInputEvents;
            if(m_referenceResidencyProbe)++m_referenceResidencyInputs;
            if(m_referenceRestartObservation)m_referenceRestartObservation->input();
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
            if(m_lifecycleProbe) ++m_lifecycleInputEvents;
            if(m_referenceEditProbe) ++m_referenceEditInputEvents;
            if(m_referenceResidencyProbe)++m_referenceResidencyInputs;
            if(m_referenceRestartObservation)m_referenceRestartObservation->input();
            // Decide ownership before the UI can consume/close on this click.
            for (std::size_t i = 0; i < GameplayMouseButtonCount; ++i)
            {
                const auto mapped = static_cast<GameplayMouseButton>(i);
                if (toOisMouseButton(mapped) == button)
                {
                    m_mouseFrameInput.press(mapped, acceptsWorldInput());
                    break;
                }
            }
            if (m_userInterface != nullptr)
            {
                m_userInterface->mouseButton(event, button, true);
            }
            return true;
        }

        bool mouseReleased(const OIS::MouseEvent& event,
                           OIS::MouseButtonID button) override
        {
            if(m_lifecycleProbe) ++m_lifecycleInputEvents;
            if(m_referenceEditProbe) ++m_referenceEditInputEvents;
            if(m_referenceResidencyProbe)++m_referenceResidencyInputs;
            if(m_referenceRestartObservation)m_referenceRestartObservation->input();
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
#elif defined(__APPLE__)
            bool focused = false;
            m_window->getCustomAttribute("WINDOW_FOCUSED", &focused);
            return focused;
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
#if defined(__APPLE__)
            if (m_userInterface != nullptr)
            {
                m_userInterface->focusChanged(focused);
            }
            Ogre::LogManager::getSingleton().logMessage(
                std::string("[INPUT_FOCUS] focused=") + (focused ? "1" : "0") +
                " frame=" + std::to_string(m_frameCount));
#endif
            if (m_audio != nullptr)
            {
                m_audio->setSuspended(!focused);
            }
            if (m_music != nullptr)
            {
                m_music->setSuspended(!focused);
            }
            m_focusGate.setFocused(focused);
            m_focusTransitionFrame = true;
            m_graveKeyHeld = false;
            m_lKeyHeld = false;
            m_tabKeyHeld = false;
            m_f5KeyHeld = false;
            m_jumpHeldKey.reset();
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

        bool acceptsWorldInput() const
        {
            return m_worldPlayer != nullptr &&
                m_applicationFlow.state() == GameApplicationState::Playing &&
                m_focusGate.isFocused() && !m_focusTransitionFrame &&
                !m_worldPlayer->hasOpenContainer() &&
                !m_worldPlayer->hasOpenCrafting() &&
                (m_userInterface == nullptr ||
                 (!m_userInterface->wantsKeyboardInput() &&
                  !m_userInterface->wantsMouseInput() &&
                  !m_userInterface->wantsHudPointer() &&
                  !m_userInterface->hasBlockingModal()));
        }

        bool shouldCaptureNativeCursor() const
        {
            if (m_hiddenWindow || m_window == nullptr ||
                m_nativeWindowHandle == 0 || m_worldPlayer == nullptr ||
                m_applicationFlow.state() != GameApplicationState::Playing ||
                m_worldPlayer->hasOpenContainer() ||
                m_worldPlayer->hasOpenCrafting() ||
                (m_userInterface != nullptr &&
                 (m_userInterface->hasBlockingModal() || m_userInterface->wantsHudPointer())))
            {
                return false;
            }
            return runtimeWindowFocused();
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
            const bool worldOwnsInput = acceptsWorldInput();
            if (worldOwnsInput != m_previousWorldInput)
            {
                m_previousWorldInput = worldOwnsInput;
                m_focusGate.suppressUntilRelease();
                clearTransientInput();
            }
#if defined(__APPLE__)
            if (m_mouse != nullptr)
            {
                m_mouse->setCursorCaptured(shouldCaptureNativeCursor());
            }
#endif
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
#if defined(__APPLE__)
            if (m_mouse != nullptr)
            {
                m_mouse->setCursorCaptured(false);
            }
#endif
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
            m_referenceRestartObservation.reset();
            m_referenceEditProbe.reset();
            m_renderCapture.reset();
            m_materialIdentityCapture.reset();
            m_cameraDiagnostics.reset();
            m_floraWindCapture.reset();
            m_shoreEditCapture.reset();
            if (m_userInterface) m_userInterface->setPauseNotificationCapture(nullptr);
            m_pauseNotificationCapture.reset();
            m_userInterface.reset();
            destroyPostProcessingResources();
            if(m_lifecycleProbe && !m_lifecycleProbe->failed() && m_lifecycleProbe->stage()==11)
                m_lifecycleProbe->begin(unsigned(m_frameCount),lifecycleSnapshot());
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->detachDraws();
            m_waterReflection.reset();
            m_hdrPipeline.reset();
            if(m_referenceResidencyProbe){ReferenceResidency::require(m_root && m_sceneManager && !m_waterReflection && !m_hdrPipeline,"component ownership teardown order");m_referenceResidencyProbe->event("components-destroyed",lifecycleSnapshot());}
            if(m_lifecycleProbe && !m_lifecycleProbe->failed() && m_lifecycleProbe->stage()==11 && m_lifecycleProbe->begun()) {
                lifecycleRequire(m_root && m_sceneManager && !m_world && m_sectionVisuals.empty() && m_terrainBatchVisuals.empty(),"Components must release before live Scene/Root.");
                lifecycleRequire(lifecycleCameraCount()==1,"Reflection camera survived component destruction.");
                m_lifecycleProbe->commit(unsigned(m_frameCount),lifecycleSnapshot());
            }
            m_blockFeedback.reset();
            m_actorRenderer.reset();
            m_playerRenderer.reset();

            for (auto &entry : m_sectionVisuals)
            {
                destroySectionVisual(entry.second);
            }
            m_sectionVisuals.clear();
            clearTerrainBatches();
            m_caveBoundaryRenderer.reset();
            m_sectionRenderStates.clear();
            m_emptySectionUploadIdentities.clear();
            m_materialIdentityMeshRevisions.clear();
            m_lastLiveSections.clear();
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
            m_regionalAtmosphere.reset();
            m_worldPlayer = nullptr;
            m_sandbox.reset();
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->worldDestroyed();
            m_logicCamera.reset();
            m_music.reset();
            m_audio.reset();

            if(m_lifecycleProbe && !m_lifecycleProbe->failed() && m_lifecycleProbe->stage()==12)
                m_lifecycleProbe->begin(unsigned(m_frameCount),lifecycleSnapshot());
            m_camera = nullptr;
            m_sceneManager = nullptr;
            m_window = nullptr;
            m_terrainArray.setNull();
            for (auto &texture : m_referenceArrays) texture.setNull();
            m_root.reset();
            if(m_referenceResidencyProbe)m_referenceResidencyProbe->event("root-shutdown",lifecycleSnapshot());
            if(m_lifecycleProbe && !m_lifecycleProbe->failed() && m_lifecycleProbe->stage()==12 && m_lifecycleProbe->begun())
                m_lifecycleProbe->commit(unsigned(m_frameCount),lifecycleSnapshot());
            m_terrainArrayLoader.reset();
            for (auto &loader : m_referenceLoaders) loader.reset();
            m_gl3PlusPlugin.reset();
        }

        std::unique_ptr<Ogre::Root> m_root;
        std::unique_ptr<RenderLifecycleProbe> m_lifecycleProbe;
        std::uint64_t m_lifecycleWorldEpoch=0, m_lifecycleExpectedHdrGeneration=0;
        bool m_lifecycleCloseReturned=false;
        unsigned m_lifecycleInputEvents=0, m_lifecycleExpectedWidth=0, m_lifecycleExpectedHeight=0;
        unsigned m_lifecyclePointWidth=1280, m_lifecyclePointHeight=720;
        std::string m_lifecycleWorldDirectory;
        std::array<std::vector<std::string>,5> m_lifecycleEmptyManagerNames;
        std::unique_ptr<HdrPipeline> m_hdrPipeline;
        std::unique_ptr<PlanarWaterReflection> m_waterReflection;
        std::unique_ptr<ReferenceSettingsRestartObservation> m_referenceRestartObservation;
        std::string m_referenceRestartOutput;
        std::unique_ptr<ReferenceWorldEditProbe> m_referenceEditProbe;
        std::unique_ptr<ReferenceResidencyProbe> m_referenceResidencyProbe;
        unsigned m_referenceResidencyObservationFrame=0;
        bool m_referenceResidencyColumnsAvailable=false;
        bool m_referenceResidencyWitnessCellsAvailable=false;
        std::string m_referenceResidencyColumnFacts="[]";
        std::array<std::string,2> m_referenceResidencyWitnessCellFacts{{"{}","{}"}};
        std::string m_referenceResidencyOutput;
        std::unordered_map<std::string,WorldSectionMeshVersion> m_emptySectionUploadIdentities;
        unsigned m_referenceResidencyInputs=0;
        float m_referenceResidencyDelta=0;
        std::uint64_t m_referenceResidencyFrameRevision=0;
        std::string m_referenceEditOutput;
        unsigned m_referenceEditInputEvents=0;
        std::uint64_t m_referenceEditFrameSceneRevision=0;
        bool m_planarDiagnosticCaptured = false;
        std::array<std::unique_ptr<TerrainArrayLoader>,3> m_referenceLoaders;
        std::array<Ogre::TexturePtr,3> m_referenceArrays;
        bool m_referenceSurfaceEnabled = false;
        LocalLightSnapshot m_localLights;
        bool m_referenceVisualRequested = false;
        bool m_referenceVisualApplied = false;
        std::unique_ptr<TerrainArrayLoader> m_terrainArrayLoader;
        Ogre::TexturePtr m_terrainArray;
        Config m_config;
        std::unique_ptr<Ogre::GL3PlusPlugin> m_gl3PlusPlugin;
        Ogre::RenderWindow* m_window = nullptr;
        Ogre::SceneManager* m_sceneManager = nullptr;
        Ogre::Camera* m_camera = nullptr;
        float m_nominalCameraNearClipDistance = .1f;
        OgreThirdPersonCameraRig::CameraPose m_lastRenderCameraPose;
        Ogre::Light* m_directionalSunLight = nullptr;
        Ogre::SceneNode* m_directionalSunNode = nullptr;
        OIS::InputManager* m_inputManager = nullptr;
        OIS::Keyboard* m_keyboard = nullptr;
        OIS::Mouse* m_mouse = nullptr;
        std::uintptr_t m_nativeWindowHandle = 0;
        std::unique_ptr<OgreRenderCapture> m_renderCapture;
        VisualCameraSweep m_visualCameraSweep;
        double m_visualCameraSweepElapsed = 0.0;
        bool m_visualCameraSweepAnchored = false;
        int m_visualCameraSweepLoggedSecond = -1;
        glm::vec3 m_visualCameraSweepOrigin{0.f};
        glm::vec3 m_visualCameraSweepRotation{0.f};
        std::unique_ptr<OgreUserInterface> m_userInterface;
        struct MaterialIdentityFixture
        {
            Material::ID material;
            BlockId block;
            glm::ivec3 position{0};
            std::size_t mapCell = 0;
            ActorId actor = 0;
            bool observed = false;
        };
        std::array<MaterialIdentityFixture, 5> m_materialIdentityFixtures{{
            {Material::OakPlank, BlockId::OakPlank},
            {Material::Cobblestone, BlockId::Cobblestone},
            {Material::Chest, BlockId::Chest},
            {Material::Workbench, BlockId::Workbench},
            {Material::OakBark, BlockId::OakBark}}};
        std::unique_ptr<MaterialIdentityCapture> m_materialIdentityCapture;
        std::string m_materialIdentityOutput;
        int m_materialIdentityPhase = 0;
        std::chrono::steady_clock::time_point m_materialIdentityStarted;
        float m_materialIdentityPhaseSeconds = 0.f;
        bool m_materialIdentityFixturesPlaced = false;
        bool m_materialIdentityMapReady = false;
        bool m_materialIdentityFramePending = false;
        std::unordered_map<std::string, std::uint32_t> m_materialIdentityMeshRevisions;
        std::unique_ptr<PauseNotificationCapture> m_pauseNotificationCapture;
        std::string m_pauseNotificationOutput;
        std::unique_ptr<FloraWindCapture> m_floraWindCapture;
        std::unique_ptr<ShoreEditCapture> m_shoreEditCapture;
        std::string m_shoreEditOutput;
        glm::ivec3 m_shoreTarget{0};
        ChunkBlock m_shoreOriginalTop, m_shoreOriginalLower;
        std::uint64_t m_shoreUploadSerial = 0, m_shoreEditUiFrame = 0;
        int m_shorePhase = 0;
        bool m_shoreInitialized = false, m_shoreActionApplied = false;
        bool m_shoreWaitHud = false, m_shoreComplete = false;
        bool m_shoreNativeDraw = false;
        std::chrono::steady_clock::time_point m_shoreStarted{}, m_shorePhaseStarted{};
        OgreSurfaceMapDiagnosticFacts m_shoreHudBeforeFlat;
        std::string m_fernWindOutput;
        std::chrono::steady_clock::time_point m_fernWindStarted{};
        float m_fernWindPhaseSeconds=0.f;
        bool m_fernWindPhaseApplied=false;
        std::unique_ptr<OgreCameraDiagnostics> m_cameraDiagnostics;
        std::string m_cameraDiagnosticOutput;
        int m_cameraDiagnosticPhase = 0;
        std::chrono::steady_clock::time_point m_cameraDiagnosticStarted;
        float m_cameraDiagnosticPhaseSeconds = 0.f;
        bool m_cameraDiagnosticPhasePlaced = false;
        bool m_cameraDiagnosticFramePending = false;
        std::unique_ptr<AudioRuntime> m_audio;
        std::unique_ptr<MusicRuntime> m_music;
        AdventureAudioPresentation::State m_adventureAudioState;
        std::array<AdventureAudioPresentation::EnvironmentTarget,
                   AdventureAudioPresentation::AmbientKindCount>
            m_adventureAudioEnvironment{};
        glm::ivec2 m_adventureAudioEnvironmentCell{
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::min()};
        float m_adventureAudioEnvironmentRefreshSeconds = 0.f;
        std::array<float, AdventureAudioPresentation::AmbientKindCount>
            m_adventureAmbientReplayCountdown{};
        std::array<bool, AdventureAudioPresentation::AmbientKindCount>
            m_adventureAmbientCaptionAnnounced{};
        bool m_adventureAmbientReplaySuspended = true;
        std::uint64_t m_adventureAudioWorldEpoch = 0;
        std::uint64_t m_adventureAudioPlayerInterpolationEpoch = 0;
        std::vector<ActorSnapshot> m_frameActorSnapshots;
        std::vector<PendingCrashReport> m_pendingCrashReports;
        std::string m_audioDefinitionError;
        std::string m_musicDefinitionError;
        GameApplicationFlow m_applicationFlow;
        std::unique_ptr<WorldManagementService> m_worldManagement;
        std::unique_ptr<OgreBlockFeedback> m_blockFeedback;
        bool m_blockFeedbackCapture = false;
        glm::ivec3 m_blockFeedbackCaptureTarget{0};
        BlockId m_blockFeedbackCaptureId = BlockId::Air;
        float m_blockFeedbackCaptureSeconds = 0.f;
        std::string m_actorVisualCapture;
        std::string m_playerMotionCapture;
        float m_playerMotionCaptureSeconds = 0.f;
        float m_actorVisualCaptureSeconds = 0.f;
        bool m_actorVisualGalleryLogged = false;
        float m_actorVisualDistance = 4.6f;
        int m_blockFeedbackCaptureLastStage = -2;
        std::unique_ptr<OgreActorRenderer> m_actorRenderer;
        std::unique_ptr<OgrePlayerRenderer> m_playerRenderer;
        std::unique_ptr<OgreCaveBoundaryRenderer> m_caveBoundaryRenderer;
        std::size_t m_boundaryMaskSyncCount = 0;
        std::size_t m_boundaryMaskPeakFacesScanned = 0;
        std::size_t m_boundaryMaskPeakUpdates = 0;
        PlayerAvatarPresentation::Profile m_playerAvatarProfile =
            PlayerAvatarPresentation::defaultProfile();
        PlayerAvatarPresentation::PoseHistory m_playerAvatarPoseHistory;
        PlayerHandPresentation::LightingState m_playerLighting;
        float m_viewRangeSkyAvailability = 0.f;
        bool m_viewRangeSkyKnown = false;
        ThirdPersonCameraPresentation::State m_thirdPersonCameraState;
        ThirdPersonCameraPresentation::Mode m_effectiveCameraMode =
            ThirdPersonCameraPresentation::Mode::FirstPerson;
        float m_playerMovementSeconds = 0.f;
        float m_playerLandingEnvelope = 0.f;
        bool m_previousPlayerGrounded = false;
        bool m_playerGroundStateInitialized = false;
        bool m_playerAvatarWasVisible = false;
        std::uint64_t m_playerInterpolationEpoch = 0;
        Player* m_worldPlayer = nullptr;
        std::unique_ptr<::Camera> m_logicCamera;
        std::unique_ptr<SandboxRuntime> m_sandbox;
        World* m_world = nullptr;
        RegionalAtmosphere m_regionalAtmosphere;
        std::unordered_map<std::string, SectionVisual> m_sectionVisuals;
        std::unordered_map<std::string, SectionVisual> m_terrainBatchVisuals;
        std::unordered_map<std::string, glm::ivec3> m_dirtyTerrainBatches;
        std::unordered_map<std::string, ChunkRenderState>
            m_sectionRenderStates;
        std::vector<glm::ivec3> m_lastLiveSections;
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
        std::chrono::steady_clock::time_point m_sceneRenderEnd;
        std::chrono::steady_clock::time_point m_swapEnd;
        bool m_renderPhaseDiagnostics = false;
        glm::vec2 m_pendingLookDelta{0.0f};
        GameplayMovementModeTracker m_movementModeTracker;
        GameplayFocusGate m_focusGate;
        GameplayMouseFrameInput m_mouseFrameInput;
        bool m_focusTransitionFrame = false;
        bool m_jumpPressed = false;
        std::optional<OIS::KeyCode> m_jumpHeldKey;
        bool m_toggleFlying = false;
        bool m_resetMeshes = false;
        bool m_useHeldFood = false;
        bool m_previousWorldInput = false;
        bool m_tabKeyHeld = false;
        bool m_graveKeyHeld = false;
        bool m_lKeyHeld = false;
        bool m_f5KeyHeld = false;
        bool m_hiddenWindow = false;
        bool m_v10cAtmosphereEnabled = true;
        bool m_directionalShadowDiagnosticsEmitted = false;
        float m_directionalShadowStrength = 0.f;
        DirectionalShadowQuality m_directionalShadowQuality =
            DirectionalShadowQuality::Off;
        PostProcessingQuality m_postProcessingQuality =
            PostProcessingQuality::Off;
        bool m_postProcessingInstalled = false;
#if defined(_WIN32)
        bool m_nativeCursorCaptured = false;
        int m_cursorHideAdjustments = 0;
#endif
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
        std::vector<E2BatchPhase> m_e2BatchPhases;
        std::ofstream m_e2BatchEvents;
        std::size_t m_e2BatchIndex = 0;
        bool m_e2BatchEnabled = false;
        bool m_e2BatchEntryPending = false;
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
        ReferenceSettingsRestartObservation::validateEntrypoint(validateOnly);
        ReferenceResidencyProbe::validateEntrypoint(validateOnly);
        ReferenceWorldEditProbe::validateEntrypoint(validateOnly);
        RenderLifecycleProbe::validateEntrypoint(validateOnly);
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
        runtimeReferenceSurfaceProfile().freezeFromResourceView(runtimeResourcePackResolver());
        std::cout<<"[REFERENCE_SURFACE_PROFILE] available="<<runtimeReferenceSurfaceProfile().available()
                 <<" compatible="<<runtimeReferenceSurfaceProfile().usableWithEffectiveTerrain()
                 <<" reason="<<runtimeReferenceSurfaceProfile().selectionReason()<<'\n';
        validateAtmosphereShaderContract(
            runtimeResourcePackResolver());
        validateDirectionalShadowShaderContract(
            runtimeResourcePackResolver());
        validatePostProcessingShaderContract(
            runtimeResourcePackResolver());
        validateHdrShaderContract(runtimeResourcePackResolver());
        validateCaveBoundaryShaderContract(
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
