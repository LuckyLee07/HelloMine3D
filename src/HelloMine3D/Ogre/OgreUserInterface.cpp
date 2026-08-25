#include "OgreUserInterface.h"

#include <OIS.h>
#include <OgreCamera.h>
#include <OgreResourceGroupManager.h>
#include <OgreRenderWindow.h>
#include <OgreSceneManager.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "../Diagnostics/RuntimeDebugOptions.h"
#include "../Item/Material.h"
#include "../Item/CraftingSession.h"
#include "../Item/RecipeRegistry.h"
#include "../Item/ToolRegistry.h"
#include "../Player/Player.h"
#include "../Presentation/LocalizedTextRegistry.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../Util/ResourcePaths.h"
#include "../World/World.h"
#include "../World/Block/ChestContainer.h"
#include "../World/Block/FurnaceContainer.h"
#include "../Item/SmeltingRegistry.h"
#include "../World/Interaction/BlockMiningProgress.h"
#include "../World/Storage/WorldManagementService.h"

namespace
{
#if defined(__APPLE__)
    constexpr const char* ImGuiGlslVersion = "#version 150";
#else
    constexpr const char* ImGuiGlslVersion = "#version 130";
#endif

    ImGuiKey toImGuiKey(OIS::KeyCode key)
    {
        switch (key)
        {
            case OIS::KC_TAB: return ImGuiKey_Tab;
            case OIS::KC_LEFT: return ImGuiKey_LeftArrow;
            case OIS::KC_RIGHT: return ImGuiKey_RightArrow;
            case OIS::KC_UP: return ImGuiKey_UpArrow;
            case OIS::KC_DOWN: return ImGuiKey_DownArrow;
            case OIS::KC_PGUP: return ImGuiKey_PageUp;
            case OIS::KC_PGDOWN: return ImGuiKey_PageDown;
            case OIS::KC_HOME: return ImGuiKey_Home;
            case OIS::KC_END: return ImGuiKey_End;
            case OIS::KC_INSERT: return ImGuiKey_Insert;
            case OIS::KC_DELETE: return ImGuiKey_Delete;
            case OIS::KC_BACK: return ImGuiKey_Backspace;
            case OIS::KC_SPACE: return ImGuiKey_Space;
            case OIS::KC_RETURN: return ImGuiKey_Enter;
            case OIS::KC_ESCAPE: return ImGuiKey_Escape;
            case OIS::KC_APOSTROPHE: return ImGuiKey_Apostrophe;
            case OIS::KC_COMMA: return ImGuiKey_Comma;
            case OIS::KC_MINUS: return ImGuiKey_Minus;
            case OIS::KC_PERIOD: return ImGuiKey_Period;
            case OIS::KC_SLASH: return ImGuiKey_Slash;
            case OIS::KC_SEMICOLON: return ImGuiKey_Semicolon;
            case OIS::KC_EQUALS: return ImGuiKey_Equal;
            case OIS::KC_LBRACKET: return ImGuiKey_LeftBracket;
            case OIS::KC_BACKSLASH: return ImGuiKey_Backslash;
            case OIS::KC_RBRACKET: return ImGuiKey_RightBracket;
            case OIS::KC_GRAVE: return ImGuiKey_GraveAccent;
            case OIS::KC_CAPITAL: return ImGuiKey_CapsLock;
            case OIS::KC_SCROLL: return ImGuiKey_ScrollLock;
            case OIS::KC_NUMLOCK: return ImGuiKey_NumLock;
            case OIS::KC_F1: return ImGuiKey_F1;
            case OIS::KC_F2: return ImGuiKey_F2;
            case OIS::KC_F3: return ImGuiKey_F3;
            case OIS::KC_F4: return ImGuiKey_F4;
            case OIS::KC_F5: return ImGuiKey_F5;
            case OIS::KC_F6: return ImGuiKey_F6;
            case OIS::KC_F7: return ImGuiKey_F7;
            case OIS::KC_F8: return ImGuiKey_F8;
            case OIS::KC_F9: return ImGuiKey_F9;
            case OIS::KC_F10: return ImGuiKey_F10;
            case OIS::KC_F11: return ImGuiKey_F11;
            case OIS::KC_F12: return ImGuiKey_F12;
            case OIS::KC_0: return ImGuiKey_0;
            case OIS::KC_1: return ImGuiKey_1;
            case OIS::KC_2: return ImGuiKey_2;
            case OIS::KC_3: return ImGuiKey_3;
            case OIS::KC_4: return ImGuiKey_4;
            case OIS::KC_5: return ImGuiKey_5;
            case OIS::KC_6: return ImGuiKey_6;
            case OIS::KC_7: return ImGuiKey_7;
            case OIS::KC_8: return ImGuiKey_8;
            case OIS::KC_9: return ImGuiKey_9;
            case OIS::KC_A: return ImGuiKey_A;
            case OIS::KC_B: return ImGuiKey_B;
            case OIS::KC_C: return ImGuiKey_C;
            case OIS::KC_D: return ImGuiKey_D;
            case OIS::KC_E: return ImGuiKey_E;
            case OIS::KC_F: return ImGuiKey_F;
            case OIS::KC_G: return ImGuiKey_G;
            case OIS::KC_H: return ImGuiKey_H;
            case OIS::KC_I: return ImGuiKey_I;
            case OIS::KC_J: return ImGuiKey_J;
            case OIS::KC_K: return ImGuiKey_K;
            case OIS::KC_L: return ImGuiKey_L;
            case OIS::KC_M: return ImGuiKey_M;
            case OIS::KC_N: return ImGuiKey_N;
            case OIS::KC_O: return ImGuiKey_O;
            case OIS::KC_P: return ImGuiKey_P;
            case OIS::KC_Q: return ImGuiKey_Q;
            case OIS::KC_R: return ImGuiKey_R;
            case OIS::KC_S: return ImGuiKey_S;
            case OIS::KC_T: return ImGuiKey_T;
            case OIS::KC_U: return ImGuiKey_U;
            case OIS::KC_V: return ImGuiKey_V;
            case OIS::KC_W: return ImGuiKey_W;
            case OIS::KC_X: return ImGuiKey_X;
            case OIS::KC_Y: return ImGuiKey_Y;
            case OIS::KC_Z: return ImGuiKey_Z;
            case OIS::KC_LCONTROL: return ImGuiKey_LeftCtrl;
            case OIS::KC_LSHIFT: return ImGuiKey_LeftShift;
            case OIS::KC_LMENU: return ImGuiKey_LeftAlt;
            case OIS::KC_LWIN: return ImGuiKey_LeftSuper;
            case OIS::KC_RCONTROL: return ImGuiKey_RightCtrl;
            case OIS::KC_RSHIFT: return ImGuiKey_RightShift;
            case OIS::KC_RMENU: return ImGuiKey_RightAlt;
            case OIS::KC_RWIN: return ImGuiKey_RightSuper;
            default: return ImGuiKey_None;
        }
    }

    int toImGuiMouseButton(int button)
    {
        switch (button)
        {
            case OIS::MB_Left: return ImGuiMouseButton_Left;
            case OIS::MB_Right: return ImGuiMouseButton_Right;
            case OIS::MB_Middle: return ImGuiMouseButton_Middle;
            default: return -1;
        }
    }

    float normalizedWheelDelta(int relative)
    {
        if (relative == 0)
        {
            return 0.0f;
        }
        if (std::abs(relative) >= 120)
        {
            return static_cast<float>(relative) / 120.0f;
        }
        return relative > 0 ? 1.0f : -1.0f;
    }

    bool recipeFitsGrid(const RecipeDefinition &recipe, int gridSize)
    {
        if (recipe.type == RecipeType::Shaped)
        {
            return recipe.width > 0 && recipe.height > 0 &&
                   recipe.width <= gridSize && recipe.height <= gridSize;
        }
        int units = 0;
        for (const RecipeIngredient &ingredient : recipe.ingredients)
        {
            units += ingredient.count;
        }
        return units > 0 && units <= gridSize * gridSize;
    }

    std::string recipeIngredientSummary(const RecipeDefinition &recipe)
    {
        std::string summary;
        for (const RecipeIngredient &ingredient : recipe.ingredients)
        {
            if (!summary.empty())
            {
                summary += ", ";
            }
            summary += Material::toMaterial(ingredient.materialId).name +
                       " x" + std::to_string(ingredient.count);
        }
        return summary;
    }
}

class OgreUserInterface::Impl
{
  public:
    Impl(Ogre::RenderWindow &renderWindow,
         Ogre::SceneManager &renderSceneManager,
         Ogre::Camera &renderCamera, Player *worldPlayer, World *activeWorld,
         GameApplicationFlow &applicationFlow,
         WorldManagementService &worldManagement,
         const UserSettings &settings, std::function<void()> feedback,
         std::vector<PendingCrashReport> pendingCrashReports)
        : window(&renderWindow)
        , sceneManager(&renderSceneManager)
        , camera(&renderCamera)
        , player(worldPlayer)
        , world(activeWorld)
        , flow(&applicationFlow)
        , management(&worldManagement)
         , appliedSettings(settings)
         , uiFeedback(std::move(feedback))
         , crashReports(std::move(pendingCrashReports))
        , showDebugPanel(RuntimeDebugOptions::showDebugInfoAtStartup())
        , iniPath(ResourcePaths::bin("imgui-ogre.ini"))
    {
        std::snprintf(createName.data(), createName.size(), "%s",
                      "New World");
        createSeed = WorldManagementService::suggestWorldSeed();
    }

    void initialize(Ogre::RenderQueueListener *listener)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.BackendPlatformName = "HelloMine3D_OIS";
        io.IniFilename = iniPath.c_str();
        io.FontGlobalScale = appliedSettings.uiScale;
        ImGui::StyleColorsDark();

        if (!ImGui_ImplOpenGL3_Init(ImGuiGlslVersion))
        {
            ImGui::DestroyContext();
            throw std::runtime_error(
                "Ogre ImGui failed to initialize the OpenGL backend.");
        }

        atlasTexture = Ogre::TextureManager::getSingleton().load(
            "DefaultPack.png",
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D, 0);
        unsigned int atlasGlId = 0;
        atlasTexture->getCustomAttribute("GLID", &atlasGlId);
        if (atlasGlId == 0)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui::DestroyContext();
            atlasTexture.setNull();
            throw std::runtime_error(
                "Ogre ImGui failed to resolve the gameplay atlas GL ID.");
        }
        atlasTextureId = static_cast<ImTextureID>(atlasGlId);

        sceneManager->addRenderQueueListener(listener);
        listenerInstalled = true;
        initialized = true;
    }

    void shutdown(Ogre::RenderQueueListener *listener)
    {
        if (listenerInstalled && sceneManager != nullptr)
        {
            sceneManager->removeRenderQueueListener(listener);
            listenerInstalled = false;
        }
        if (!initialized)
        {
            return;
        }
        if (framePending)
        {
            ImGui::EndFrame();
            framePending = false;
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
        atlasTextureId = ImTextureID_Invalid;
        atlasTexture.setNull();
        initialized = false;
    }

    void beginFrame(float deltaSeconds, const WorldDebugStats &stats,
                    const MiningProgressSnapshot &progress)
    {
        if (!initialized)
        {
            return;
        }
        if (framePending)
        {
            ImGui::EndFrame();
        }

        unsigned int width = 0;
        unsigned int height = 0;
        unsigned int colourDepth = 0;
        int left = 0;
        int top = 0;
        window->getMetrics(width, height, colourDepth, left, top);

        ImGuiIO &io = ImGui::GetIO();
        const float framebufferScale =
            std::max(1.0f, window->getViewPointToPixelScale());
        io.DisplaySize =
            ImVec2(static_cast<float>(width) / framebufferScale,
                   static_cast<float>(height) / framebufferScale);
        io.DisplayFramebufferScale =
            ImVec2(framebufferScale, framebufferScale);
        io.DeltaTime = std::max(deltaSeconds, 1.0f / 1000.0f);
        io.FontGlobalScale = appliedSettings.uiScale;
        statusMessageSeconds = std::max(
            0.f, statusMessageSeconds - std::max(0.f, deltaSeconds));
        audioCaptionSeconds = std::max(
            0.f, audioCaptionSeconds - std::max(0.f, deltaSeconds));
        interactionFeedbackSeconds = std::max(
            0.f, interactionFeedbackSeconds -
                     std::max(0.f, deltaSeconds));
        hudElapsedSeconds += std::max(0.f, deltaSeconds);
        const float frameSeconds = std::max(0.f, deltaSeconds);
        if (frameSeconds > 0.f)
        {
            performanceSampleSeconds += frameSeconds;
            performanceSamplePeakMs = std::max(
                performanceSamplePeakMs, frameSeconds * 1000.f);
            ++performanceSampleFrames;
            if (displayedFramesPerSecond <= 0.f)
            {
                displayedFramesPerSecond = 1.f / frameSeconds;
                displayedFrameMs = frameSeconds * 1000.f;
                displayedPeakFrameMs = displayedFrameMs;
            }
            if (performanceSampleSeconds >= 0.5f)
            {
                displayedFramesPerSecond =
                    static_cast<float>(performanceSampleFrames) /
                    performanceSampleSeconds;
                displayedFrameMs =
                    performanceSampleSeconds * 1000.f /
                    static_cast<float>(performanceSampleFrames);
                displayedPeakFrameMs = performanceSamplePeakMs;
                performanceSampleSeconds = 0.f;
                performanceSamplePeakMs = 0.f;
                performanceSampleFrames = 0;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
        framePending = true;
        worldStats = stats;
        miningProgress = progress;
        switch (flow->state())
        {
            case GameApplicationState::MainMenu:
                drawMainMenu();
                break;
            case GameApplicationState::WorldList:
                drawWorldList();
                break;
            case GameApplicationState::Loading:
                drawLoading();
                break;
            case GameApplicationState::Playing:
                drawHud();
                drawContainer();
                drawCrafting();
                if (showDebugPanel)
                {
                    drawDebugPanels();
                }
                break;
            case GameApplicationState::Paused:
                drawHud();
                if (settingsSession.isOpen())
                {
                    drawSettingsMenu();
                }
                else
                {
                    drawPauseMenu();
                }
                break;
        }
        drawCrashReportPrompt();
    }

    void drawCrashReportPrompt()
    {
        if (crashReports.empty())
        {
            return;
        }
        if (!crashPopupOpened)
        {
            ImGui::OpenPopup("Previous crash report");
            crashPopupOpened = true;
        }
        ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Previous crash report", nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }

        const PendingCrashReport& report = crashReports.front();
        ImGui::TextWrapped(
            "HelloMine3D detected a local crash report from the previous "
            "run. Nothing has been uploaded.");
        ImGui::Separator();
        ImGui::Text("Report: %s", report.dumpFile.c_str());
        ImGui::Text("Build: %s", report.buildIdentity.c_str());
        ImGui::Text("Exception: %s", report.exceptionCode.c_str());
        if (crashReports.size() > 1)
        {
            ImGui::Text("Pending reports: %llu",
                        static_cast<unsigned long long>(crashReports.size()));
        }
        if (!crashReportMessage.empty())
        {
            ImGui::TextWrapped("%s", crashReportMessage.c_str());
        }
        ImGui::Separator();
        if (ImGui::Button("Open folder", ImVec2(140.0f, 38.0f)))
        {
            std::string error;
            crashReportMessage = openCrashReportLocation(report, &error)
                                     ? "Opened the local report folder."
                                     : "Could not open the folder: " + error;
            playUiFeedback();
        }
        ImGui::SameLine();
        if (ImGui::Button("Copy details", ImVec2(140.0f, 38.0f)))
        {
            ImGui::SetClipboardText(report.clipboardText.c_str());
            crashReportMessage = "Copied sanitized local details.";
            playUiFeedback();
        }
        ImGui::SameLine();
        if (ImGui::Button("Ignore", ImVec2(140.0f, 38.0f)))
        {
            std::string error;
            if (acknowledgeCrashReport(report, &error))
            {
                crashReports.erase(crashReports.begin());
                crashReportMessage.clear();
                crashPopupOpened = false;
                ImGui::CloseCurrentPopup();
                playUiFeedback();
            }
            else
            {
                crashReportMessage =
                    "Could not ignore this report: " + error;
            }
        }
        ImGui::EndPopup();
    }

    void drawMainMenu()
    {
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.45f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 280.0f), ImGuiCond_Always);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##MainMenu", nullptr, flags))
        {
            ImGui::SetCursorPosY(38.0f);
            const float titleWidth = ImGui::CalcTextSize("HelloMine3D").x;
            ImGui::SetCursorPosX((420.0f - titleWidth) * 0.5f);
            ImGui::TextUnformatted("HelloMine3D");
            ImGui::SetCursorPos(ImVec2(90.0f, 105.0f));
            if (ImGui::Button("Single Player", ImVec2(240.0f, 48.0f)))
            {
                if (flow->showWorldList())
                {
                    worldsDirty = true;
                    playUiFeedback();
                }
            }
            ImGui::SetCursorPos(ImVec2(90.0f, 170.0f));
            if (ImGui::Button("Quit", ImVec2(240.0f, 42.0f)))
            {
                pendingAction.type = OgreUserInterfaceActionType::Quit;
                playUiFeedback();
            }
        }
        ImGui::End();
    }

    void refreshCatalogue()
    {
        worldsDirty = false;
        worlds.clear();
        deletedWorlds.clear();
        const WorldManagementListResult active = management->listWorlds();
        if (!active.succeeded())
        {
            statusMessage = active.message;
            return;
        }
        worlds = active.worlds;
        const DeletedWorldListResult deleted =
            management->listDeletedWorlds();
        if (!deleted.succeeded())
        {
            statusMessage = deleted.message;
            return;
        }
        deletedWorlds = deleted.worlds;
    }

    void selectWorld(const WorldCatalogueEntry &entry)
    {
        selectedWorldId = entry.id;
        renameName.fill('\0');
        std::snprintf(renameName.data(), renameName.size(), "%s",
                      entry.displayName.c_str());
        backups.clear();
        WorldManagementResult listed;
        if (!management->listBackups(entry.id, backups, &listed))
        {
            statusMessage = listed.message;
        }
    }

    void reportResult(const WorldManagementResult &result)
    {
        statusMessage = result.message;
        if (result.succeeded())
        {
            worldsDirty = true;
            playUiFeedback();
        }
    }

    void playUiFeedback() noexcept
    {
        if (!uiFeedback)
        {
            return;
        }
        try
        {
            uiFeedback();
        }
        catch (...)
        {
            // Audio feedback must never interrupt a UI command.
        }
    }

    void drawWorldList()
    {
        if (worldsDirty)
        {
            refreshCatalogue();
        }
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(820.0f, 620.0f), ImGuiCond_Always);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("Worlds", nullptr, flags))
        {
            if (ImGui::Button("Back to Main Menu"))
            {
                if (flow->returnToMainMenu())
                {
                    playUiFeedback();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh"))
            {
                worldsDirty = true;
                playUiFeedback();
            }
            ImGui::Separator();

            ImGui::TextUnformatted("Create world");
            ImGui::SetNextItemWidth(280.0f);
            ImGui::InputText("Name##create", createName.data(),
                             createName.size());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt("Seed##create", &createSeed);
            ImGui::SameLine();
            if (ImGui::Button("Create"))
            {
                const WorldManagementResult result =
                    management->createWorld(createName.data(), createSeed);
                reportResult(result);
                if (result.succeeded())
                {
                    createSeed = WorldManagementService::suggestWorldSeed();
                }
            }

            ImGui::Separator();
            ImGui::Text("Active worlds (%llu)",
                        static_cast<unsigned long long>(worlds.size()));
            ImGui::BeginChild("WorldList", ImVec2(0.0f, 185.0f), true);
            const ImGuiTableFlags worldTableFlags =
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_NoSavedSettings;
            if (ImGui::BeginTable("ActiveWorlds", 3, worldTableFlags))
            {
                ImGui::TableSetupColumn(
                    "World", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn(
                    "Seed", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn(
                    "Actions", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                for (const WorldCatalogueEntry &entry : worlds)
                {
                    ImGui::PushID(entry.id.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    const bool selected = selectedWorldId == entry.id;
                    const std::string worldLabel = entry.completed
                        ? entry.displayName + "  [" +
                              runtimeLocalizedTextRegistry().lookup(
                                  "en-US", "world.list.completed") + "]"
                        : entry.displayName;
                    if (ImGui::Selectable(worldLabel.c_str(), selected))
                    {
                        selectWorld(entry);
                    }
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("seed %d", entry.seed);
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::SmallButton("Play"))
                    {
                        pendingAction.type =
                            OgreUserInterfaceActionType::OpenWorld;
                        pendingAction.worldId = entry.id;
                        playUiFeedback();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete"))
                    {
                        pendingDeleteWorldId = entry.id;
                        openDeletePopup = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();

            if (!selectedWorldId.empty())
            {
                ImGui::SetNextItemWidth(300.0f);
                ImGui::InputText("Display name##rename", renameName.data(),
                                 renameName.size());
                ImGui::SameLine();
                if (ImGui::Button("Rename"))
                {
                    reportResult(management->renameWorld(
                        selectedWorldId, renameName.data()));
                }
                ImGui::SameLine();
                ImGui::Text("Backups: %llu",
                            static_cast<unsigned long long>(backups.size()));
                for (const WorldBackupInfo &backup : backups)
                {
                    ImGui::PushID(backup.id.c_str());
                    ImGui::Text("%s (%llu files)", backup.id.c_str(),
                                static_cast<unsigned long long>(
                                    backup.fileCount));
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Restore backup"))
                    {
                        pendingBackupId = backup.id;
                        openBackupPopup = true;
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Separator();
            ImGui::Text("Recoverable worlds (%llu)",
                        static_cast<unsigned long long>(
                            deletedWorlds.size()));
            ImGui::BeginChild("DeletedWorldList", ImVec2(0.0f, 105.0f),
                              true);
            for (const DeletedWorldInfo &entry : deletedWorlds)
            {
                ImGui::PushID(entry.recoveryId.c_str());
                ImGui::TextUnformatted(entry.world.displayName.c_str());
                ImGui::SameLine(400.0f);
                if (ImGui::SmallButton("Restore"))
                {
                    reportResult(management->restoreDeletedWorld(
                        entry.world.id));
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete permanently"))
                {
                    pendingPermanentDeleteWorldId = entry.world.id;
                    openPermanentDeletePopup = true;
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (!statusMessage.empty())
            {
                ImGui::TextWrapped("%s", statusMessage.c_str());
            }

            if (openDeletePopup)
            {
                ImGui::OpenPopup("Confirm recoverable delete");
                openDeletePopup = false;
            }
            if (openPermanentDeletePopup)
            {
                ImGui::OpenPopup("Confirm permanent delete");
                openPermanentDeletePopup = false;
            }
            if (openBackupPopup)
            {
                ImGui::OpenPopup("Confirm backup restore");
                openBackupPopup = false;
            }

            if (ImGui::BeginPopupModal("Confirm recoverable delete", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted(
                    "Move this world into bounded recovery storage?");
                if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
                {
                    reportResult(management->deleteWorld(
                        pendingDeleteWorldId));
                    selectedWorldId.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                {
                    playUiFeedback();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("Confirm permanent delete", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted(
                    "Permanently remove this recovered world?");
                if (ImGui::Button("Delete permanently",
                                  ImVec2(170.0f, 0.0f)))
                {
                    reportResult(management->permanentlyDeleteWorld(
                        pendingPermanentDeleteWorldId));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel##permanent",
                                  ImVec2(120.0f, 0.0f)))
                {
                    playUiFeedback();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("Confirm backup restore", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted(
                    "Replace the active world with this backup?");
                if (ImGui::Button("Restore", ImVec2(120.0f, 0.0f)))
                {
                    reportResult(management->restoreBackup(
                        selectedWorldId, pendingBackupId));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel##backup",
                                  ImVec2(120.0f, 0.0f)))
                {
                    playUiFeedback();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    void drawLoading()
    {
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(430.0f, 130.0f), ImGuiCond_Always);
        if (ImGui::Begin("##Loading", nullptr,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::SetCursorPos(ImVec2(32.0f, 40.0f));
            ImGui::Text("Loading world %s...",
                        flow->activeWorldId().c_str());
        }
        ImGui::End();
    }

    void drawPauseMenu()
    {
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.45f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(460.0f, 570.0f), ImGuiCond_Always);
        if (ImGui::Begin("Paused", nullptr,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            if (world != nullptr)
            {
                const ObjectiveSnapshot objective =
                    world->getObjectiveSnapshot();
                ImGui::Text("Journey  %zu / %zu",
                            objective.completedObjectives,
                            objective.totalObjectives);
                ImGui::TextUnformatted(objective.title.c_str());
                ImGui::TextWrapped("%s", objective.instruction.c_str());
                if (!objective.completedTitles.empty() &&
                    ImGui::CollapsingHeader("Completed objectives"))
                {
                    ImGui::BeginChild("##ObjectiveHistory",
                                      ImVec2(0.0f, 105.0f), true);
                    for (const std::string &title :
                         objective.completedTitles)
                    {
                        ImGui::Text("[x] %s", title.c_str());
                    }
                    ImGui::EndChild();
                }
                ImGui::Separator();
            }
            if (ImGui::Button("Resume", ImVec2(-1.0f, 38.0f)))
            {
                if (flow->resume())
                {
                    playUiFeedback();
                }
            }
            if (ImGui::Button("Settings", ImVec2(-1.0f, 38.0f)))
            {
                settingsSession.begin(appliedSettings);
                settingsMessage.clear();
                settingsApplyPending = false;
                playUiFeedback();
            }
            if (ImGui::Button("Save and Main Menu",
                              ImVec2(-1.0f, 38.0f)))
            {
                pendingAction.type =
                    OgreUserInterfaceActionType::ReturnToMainMenu;
                playUiFeedback();
            }
            if (ImGui::Button("Save and Quit", ImVec2(-1.0f, 38.0f)))
            {
                pendingAction.type = OgreUserInterfaceActionType::Quit;
                playUiFeedback();
            }
            if (!statusMessage.empty())
            {
                ImGui::TextWrapped("%s", statusMessage.c_str());
            }
        }
        ImGui::End();
    }

    void drawSettingsMenu()
    {
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.48f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        const float height = std::min(680.0f, io.DisplaySize.y - 30.0f);
        ImGui::SetNextWindowSize(ImVec2(620.0f, height), ImGuiCond_Always);
        if (ImGui::Begin("Paused Settings", nullptr,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            UserSettings &draft = settingsSession.draft();
            ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, -58.0f),
                              false);
            int windowSize[2] = {draft.windowX, draft.windowY};
            if (ImGui::InputInt2("Window size", windowSize))
            {
                draft.windowX = windowSize[0];
                draft.windowY = windowSize[1];
            }
            ImGui::Checkbox("Fullscreen", &draft.isFullscreen);
            ImGui::SliderInt("Render distance", &draft.renderDistance,
                             1, 32);
            ImGui::SliderInt("Field of view", &draft.fov, 45, 120);
            ImGui::SliderFloat("Mouse sensitivity",
                               &draft.mouseSensitivity, 0.005f, 1.0f,
                               "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::Checkbox("Invert mouse Y", &draft.invertMouseY);
            ImGui::SliderFloat("UI scale", &draft.uiScale, 0.75f, 1.75f,
                               "%.2fx");
            ImGui::Checkbox("Show action hints", &draft.showActionHints);
            ImGui::SeparatorText("Audio");
            ImGui::SliderFloat("Master", &draft.masterVolume, 0.0f, 1.0f);
            ImGui::SliderFloat("UI", &draft.uiVolume, 0.0f, 1.0f);
            ImGui::SliderFloat("Effects", &draft.effectsVolume,
                               0.0f, 1.0f);
            ImGui::SliderFloat("Ambient", &draft.ambientVolume,
                               0.0f, 1.0f);
            ImGui::Checkbox("Audio captions", &draft.audioCaptions);
            ImGui::SeparatorText("Controls");
            for (std::size_t actionIndex = 0;
                 actionIndex < GameplayActionCount; ++actionIndex)
            {
                const auto action =
                    static_cast<GameplayAction>(actionIndex);
                const GameplayKey current = draft.inputBindings.get(action);
                const std::string label =
                    std::string(gameplayActionName(action)) +
                    "##binding-" + std::to_string(actionIndex);
                if (ImGui::BeginCombo(label.c_str(),
                                      gameplayKeyName(current)))
                {
                    for (std::size_t keyIndex = 0;
                         keyIndex < GameplayKeyCount; ++keyIndex)
                    {
                        const auto key = static_cast<GameplayKey>(keyIndex);
                        const bool selected = key == current;
                        if (ImGui::Selectable(gameplayKeyName(key), selected))
                        {
                            draft.inputBindings.set(action, key);
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::TextWrapped(
                "FOV, controls, accessibility, render distance and volume "
                "apply immediately. "
                "Window size and fullscreen apply after restart.");
            if (!settingsMessage.empty())
            {
                ImGui::TextWrapped("%s", settingsMessage.c_str());
            }
            ImGui::EndChild();

            ImGui::BeginDisabled(settingsApplyPending);
            if (ImGui::Button("Apply", ImVec2(140.0f, 38.0f)))
            {
                RuntimeSettingsApplyPlan plan;
                if (settingsSession.prepareApply(plan, settingsMessage))
                {
                    pendingAction.type =
                        OgreUserInterfaceActionType::ApplySettings;
                    pendingAction.settings = plan.settings;
                    settingsMessage = "Saving settings...";
                    settingsApplyPending = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(140.0f, 38.0f)))
            {
                settingsSession.cancel();
                settingsMessage.clear();
                playUiFeedback();
            }
            ImGui::SameLine();
            if (ImGui::Button("Defaults", ImVec2(140.0f, 38.0f)))
            {
                settingsSession.restoreDefaults();
                settingsMessage.clear();
                playUiFeedback();
            }
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

    bool dismissSettings() noexcept
    {
        if (!settingsSession.isOpen())
        {
            return false;
        }
        if (settingsApplyPending)
        {
            return true;
        }
        settingsSession.cancel();
        settingsMessage.clear();
        return true;
    }

    void reportSettingsApplied(bool succeeded,
                               const UserSettings &settings,
                               std::string message)
    {
        settingsApplyPending = false;
        settingsMessage = std::move(message);
        if (!succeeded)
        {
            return;
        }
        appliedSettings = settings;
        ImGui::GetIO().FontGlobalScale = appliedSettings.uiScale;
        if (!appliedSettings.audioCaptions)
        {
            audioCaption.clear();
            audioCaptionSeconds = 0.f;
        }
        settingsSession.acceptApplied();
        statusMessage = settingsMessage;
        statusMessageSeconds = 4.f;
        playUiFeedback();
    }

    void setStatusMessage(std::string message)
    {
        statusMessage = std::move(message);
        statusMessageSeconds = 4.f;
        worldsDirty = true;
    }

    void setAudioCaption(std::string caption)
    {
        std::string lower = caption;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char value) {
                           return static_cast<char>(std::tolower(value));
                       });
        if (lower.find("broken") != std::string::npos)
        {
            interactionFeedbackColour = ImVec4(0.95f, 0.72f, 0.28f, 1.f);
            interactionFeedbackSeconds = 0.32f;
        }
        else if (lower.find("placed") != std::string::npos)
        {
            interactionFeedbackColour = ImVec4(0.35f, 0.72f, 1.f, 1.f);
            interactionFeedbackSeconds = 0.28f;
        }
        else if (lower.find("collected") != std::string::npos ||
                 lower.find("crafting") != std::string::npos)
        {
            interactionFeedbackColour = ImVec4(0.42f, 0.94f, 0.48f, 1.f);
            interactionFeedbackSeconds = 0.34f;
        }
        else if (lower.find("hit") != std::string::npos)
        {
            interactionFeedbackColour = ImVec4(1.f, 0.34f, 0.28f, 1.f);
            interactionFeedbackSeconds = 0.28f;
        }
        if (!appliedSettings.audioCaptions)
        {
            return;
        }
        audioCaption = std::move(caption);
        audioCaptionSeconds = 2.5f;
    }

    bool materialIconUv(Material::ID id, ImVec2 &uvMin,
                        ImVec2 &uvMax) const
    {
        if (atlasTextureId == ImTextureID_Invalid)
        {
            return false;
        }
        const Material::IconCoordinate coordinate =
            Material::iconCoordinate(id);
        if (!coordinate.available())
        {
            return false;
        }
        constexpr float atlasSize = 256.f;
        constexpr float tileSize = 16.f;
        constexpr float inset = 0.5f;
        uvMin = ImVec2((coordinate.x * tileSize + inset) / atlasSize,
                       (coordinate.y * tileSize + inset) / atlasSize);
        uvMax = ImVec2(((coordinate.x + 1) * tileSize - inset) /
                           atlasSize,
                       ((coordinate.y + 1) * tileSize - inset) /
                           atlasSize);
        return true;
    }

    bool drawMaterialIcon(ImDrawList *drawList, Material::ID id,
                          const ImVec2 &minimum, const ImVec2 &maximum,
                          ImU32 tint = IM_COL32_WHITE) const
    {
        ImVec2 uvMin;
        ImVec2 uvMax;
        if (drawList == nullptr || !materialIconUv(id, uvMin, uvMax))
        {
            return false;
        }
        drawList->AddImage(ImTextureRef(atlasTextureId), minimum, maximum,
                           uvMin, uvMax, tint);
        return true;
    }

    void drawHeldMaterial(const PlayerSaveState &state,
                          const ImGuiIO &io) const
    {
        if (flow->state() != GameApplicationState::Playing ||
            player->hasOpenContainer() || player->hasOpenCrafting() ||
            state.heldItem < 0 ||
            state.heldItem >= static_cast<int>(state.inventory.size()))
        {
            return;
        }
        const InventorySlotState &slot =
            state.inventory[static_cast<std::size_t>(state.heldItem)];
        ImVec2 uvMin;
        ImVec2 uvMax;
        if (slot.amount <= 0 || !materialIconUv(slot.materialId, uvMin, uvMax))
        {
            return;
        }

        const float feedbackKick = interactionFeedbackSeconds > 0.f
            ? interactionFeedbackSeconds * 20.f : 0.f;
        const float bob = std::sin(hudElapsedSeconds * 2.1f) * 2.5f -
                          feedbackKick;
        const float angle = -0.12f +
                            std::sin(hudElapsedSeconds * 1.4f) * 0.025f;
        const float half = 38.f;
        const ImVec2 center(io.DisplaySize.x - 77.f,
                            io.DisplaySize.y - 84.f + bob);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const auto rotate = [&](float x, float y) {
            return ImVec2(center.x + x * cosine - y * sine,
                          center.y + x * sine + y * cosine);
        };
        const ImVec2 p1 = rotate(-half, -half);
        const ImVec2 p2 = rotate(half, -half);
        const ImVec2 p3 = rotate(half, half);
        const ImVec2 p4 = rotate(-half, half);
        ImDrawList *foreground = ImGui::GetForegroundDrawList();
        const ImVec2 shadowOffset(5.f, 7.f);
        foreground->AddImageQuad(
            ImTextureRef(atlasTextureId),
            ImVec2(p1.x + shadowOffset.x, p1.y + shadowOffset.y),
            ImVec2(p2.x + shadowOffset.x, p2.y + shadowOffset.y),
            ImVec2(p3.x + shadowOffset.x, p3.y + shadowOffset.y),
            ImVec2(p4.x + shadowOffset.x, p4.y + shadowOffset.y),
            uvMin, ImVec2(uvMax.x, uvMin.y), uvMax,
            ImVec2(uvMin.x, uvMax.y), IM_COL32(0, 0, 0, 90));
        foreground->AddImageQuad(
            ImTextureRef(atlasTextureId), p1, p2, p3, p4, uvMin,
            ImVec2(uvMax.x, uvMin.y), uvMax,
            ImVec2(uvMin.x, uvMax.y), IM_COL32_WHITE);
    }

    void drawHotbarSlot(const InventorySlotState &slot,
                        std::size_t index, bool selected)
    {
        constexpr float slotSize = 56.f;
        ImGui::PushID(static_cast<int>(index));
        ImGui::InvisibleButton("##hotbar_slot", ImVec2(slotSize, slotSize));
        const ImVec2 minimum = ImGui::GetItemRectMin();
        const ImVec2 maximum = ImGui::GetItemRectMax();
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            minimum, maximum,
            selected ? IM_COL32(83, 68, 30, 238)
                     : IM_COL32(24, 29, 38, 228),
            3.f);
        drawList->AddRect(
            minimum, maximum,
            selected ? IM_COL32(241, 196, 54, 255)
                     : IM_COL32(96, 108, 126, 210),
            3.f, 0, selected ? 3.f : 1.f);

        const std::string key = std::to_string(index + 1);
        drawList->AddText(ImVec2(minimum.x + 4.f, minimum.y + 3.f),
                          IM_COL32(225, 230, 238, 230), key.c_str());
        if (slot.amount > 0)
        {
            drawMaterialIcon(drawList, slot.materialId,
                             ImVec2(minimum.x + 12.f, minimum.y + 8.f),
                             ImVec2(maximum.x - 8.f, maximum.y - 12.f));
            const ToolDefinition *tool =
                runtimeToolRegistry().find(slot.materialId);
            if (tool != nullptr && tool->maxDurability > 0)
            {
                const float durability = std::clamp(
                    static_cast<float>(slot.durability) /
                        static_cast<float>(tool->maxDurability),
                    0.f, 1.f);
                const ImVec2 barMin(minimum.x + 5.f, maximum.y - 7.f);
                const ImVec2 barMax(maximum.x - 5.f, maximum.y - 4.f);
                drawList->AddRectFilled(barMin, barMax,
                                        IM_COL32(10, 12, 16, 230));
                drawList->AddRectFilled(
                    barMin,
                    ImVec2(barMin.x + (barMax.x - barMin.x) * durability,
                           barMax.y),
                    durability > 0.35f
                        ? IM_COL32(72, 208, 88, 255)
                        : IM_COL32(230, 76, 56, 255));
            }
            else
            {
                const std::string amount = std::to_string(slot.amount);
                const ImVec2 amountSize = ImGui::CalcTextSize(amount.c_str());
                drawList->AddText(
                    ImVec2(maximum.x - amountSize.x - 4.f,
                           maximum.y - amountSize.y - 3.f),
                    IM_COL32(255, 255, 255, 255), amount.c_str());
            }
        }
        else
        {
            const char *emptyMark = "-";
            const ImVec2 markSize = ImGui::CalcTextSize(emptyMark);
            drawList->AddText(
                ImVec2((minimum.x + maximum.x - markSize.x) * 0.5f,
                       (minimum.y + maximum.y - markSize.y) * 0.5f),
                IM_COL32(120, 130, 145, 180), emptyMark);
        }
        ImGui::PopID();
    }

    void drawHud()
    {
        if (player == nullptr)
        {
            return;
        }
        const ImGuiIO &io = ImGui::GetIO();
        const ImVec2 center(io.DisplaySize.x * 0.5f,
                            io.DisplaySize.y * 0.5f);

        const ImGuiWindowFlags performanceFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.76f);
        if (ImGui::Begin("##FramePerformance", nullptr, performanceFlags))
        {
            const ImVec4 fpsColour = displayedFramesPerSecond >= 55.f
                ? ImVec4(0.42f, 0.92f, 0.46f, 1.f)
                : (displayedFramesPerSecond >= 30.f
                    ? ImVec4(1.f, 0.78f, 0.28f, 1.f)
                    : ImVec4(1.f, 0.35f, 0.30f, 1.f));
            ImGui::TextColored(fpsColour, "FPS  %.1f",
                               displayedFramesPerSecond);
            ImGui::Text("Frame  %.2f ms", displayedFrameMs);
#if defined(_DEBUG)
            ImGui::TextDisabled("Debug | 0.5s peak %.2f ms",
                                displayedPeakFrameMs);
#else
            ImGui::TextDisabled("Release | 0.5s peak %.2f ms",
                                displayedPeakFrameMs);
#endif
            ImGui::TextDisabled(
                "Stream Q %llu | Mesh dirty %llu",
                static_cast<unsigned long long>(
                    worldStats.queuedChunkUpdates),
                static_cast<unsigned long long>(
                    worldStats.chunks.meshDirtySections));
            performanceOverlayBottom =
                ImGui::GetWindowPos().y + ImGui::GetWindowSize().y;
        }
        ImGui::End();

        if (!player->hasOpenContainer() && !player->hasOpenCrafting())
        {
            ImDrawList *foreground = ImGui::GetForegroundDrawList();
            const ImU32 crosshairColour = IM_COL32(255, 255, 255, 230);
            foreground->AddLine(ImVec2(center.x - 8.0f, center.y),
                                ImVec2(center.x + 8.0f, center.y),
                                crosshairColour, 2.0f);
            foreground->AddLine(ImVec2(center.x, center.y - 8.0f),
                                ImVec2(center.x, center.y + 8.0f),
                                crosshairColour, 2.0f);
            if (miningProgress.active)
            {
                constexpr float pi = 3.14159265358979323846f;
                foreground->AddCircle(center, 15.f,
                                      IM_COL32(16, 20, 26, 180), 32, 3.f);
                foreground->PathArcTo(
                    center, 15.f, -pi * 0.5f,
                    -pi * 0.5f + pi * 2.f *
                        miningProgress.normalized(),
                    32);
                foreground->PathStroke(IM_COL32(245, 195, 58, 255),
                                       0, 3.f);
            }
            if (interactionFeedbackSeconds > 0.f)
            {
                const float fade = std::clamp(
                    interactionFeedbackSeconds / 0.34f, 0.f, 1.f);
                ImVec4 colour = interactionFeedbackColour;
                colour.w = fade;
                const ImU32 feedbackColour = ImGui::ColorConvertFloat4ToU32(
                    colour);
                const float outer = 18.f + (1.f - fade) * 8.f;
                const float inner = outer - 6.f;
                foreground->AddLine(
                    ImVec2(center.x - outer, center.y - outer),
                    ImVec2(center.x - inner, center.y - inner),
                    feedbackColour, 3.f);
                foreground->AddLine(
                    ImVec2(center.x + outer, center.y - outer),
                    ImVec2(center.x + inner, center.y - inner),
                    feedbackColour, 3.f);
                foreground->AddLine(
                    ImVec2(center.x + outer, center.y + outer),
                    ImVec2(center.x + inner, center.y + inner),
                    feedbackColour, 3.f);
                foreground->AddLine(
                    ImVec2(center.x - outer, center.y + outer),
                    ImVec2(center.x - inner, center.y + inner),
                    feedbackColour, 3.f);
            }
        }

        if (miningProgress.active &&
            !player->hasOpenContainer() && !player->hasOpenCrafting())
        {
            ImGui::SetNextWindowPos(
                ImVec2(center.x, center.y + 24.0f), ImGuiCond_Always,
                ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.68f);
            if (ImGui::Begin(
                    "##MiningProgress", nullptr,
                    ImGuiWindowFlags_NoDecoration |
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoInputs))
            {
                ImGui::ProgressBar(miningProgress.normalized(),
                                   ImVec2(180.0f, 10.0f), "");
            }
            ImGui::End();
        }

        if (world != nullptr)
        {
            const ObjectiveSnapshot objective =
                world->getObjectiveSnapshot();
            ImGui::SetNextWindowPos(ImVec2(18.0f,
                                          performanceOverlayBottom + 10.f),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.76f);
            if (ImGui::Begin(
                    "##Objectives", nullptr,
                    ImGuiWindowFlags_NoDecoration |
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoInputs |
                        ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav))
            {
                ImGui::Text("First Session  %zu / %zu",
                            objective.completedObjectives,
                            objective.totalObjectives);
                ImGui::Separator();
                ImGui::TextUnformatted(objective.title.c_str());
                ImGui::TextWrapped("%s", objective.instruction.c_str());
                if (objective.required > 1)
                {
                    const float ratio = std::clamp(
                        static_cast<float>(objective.progress) /
                            static_cast<float>(objective.required),
                        0.0f, 1.0f);
                    const std::string overlay =
                        std::to_string(std::min(objective.progress,
                                                objective.required)) +
                        " / " + std::to_string(objective.required);
                    ImGui::ProgressBar(ratio, ImVec2(260.0f, 10.0f),
                                       overlay.c_str());
                }
                if (!objective.nextTitle.empty() &&
                    !objective.sessionComplete)
                {
                    ImGui::TextDisabled("Next: %s",
                                        objective.nextTitle.c_str());
                }
                if (!objective.completionFeedback.empty())
                {
                    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.45f, 1.0f),
                                       "%s",
                                       objective.completionFeedback.c_str());
                }
            }
            ImGui::End();
        }

        const ImGuiWindowFlags overlayFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        if (appliedSettings.showActionHints &&
            flow->state() == GameApplicationState::Playing &&
            !player->hasOpenContainer() && !player->hasOpenCrafting())
        {
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x - 18.0f, 18.0f),
                ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.66f);
            if (ImGui::Begin("##ActionHints", nullptr, overlayFlags))
            {
                ImGui::Text("%s  Crafting",
                            gameplayKeyName(appliedSettings.inputBindings.get(
                                GameplayAction::OpenCrafting)));
                ImGui::Text("%s  Eat held food",
                            gameplayKeyName(appliedSettings.inputBindings.get(
                                GameplayAction::ConsumeFood)));
                ImGui::TextUnformatted("Esc  Pause");
            }
            ImGui::End();
        }
        if (audioCaptionSeconds > 0.f && !audioCaption.empty())
        {
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f,
                       io.DisplaySize.y - 105.0f),
                ImGuiCond_Always, ImVec2(0.5f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.82f);
            if (ImGui::Begin("##AudioCaption", nullptr, overlayFlags))
            {
                ImGui::Text("[Sound] %s", audioCaption.c_str());
            }
            ImGui::End();
        }
        if (statusMessageSeconds > 0.f && !statusMessage.empty() &&
            flow->state() == GameApplicationState::Playing)
        {
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f,
                       io.DisplaySize.y - 132.0f),
                ImGuiCond_Always, ImVec2(0.5f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.82f);
            if (ImGui::Begin("##StatusToast", nullptr, overlayFlags))
            {
                ImGui::TextUnformatted(statusMessage.c_str());
            }
            ImGui::End();
        }

        const PlayerSaveState state = player->getSaveState();
        drawHeldMaterial(state, io);
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 18.0f),
            ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::SetNextWindowBgAlpha(0.72f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 7.0f));
        if (ImGui::Begin("##OgrePlayerHud", nullptr, flags))
        {
            const float healthRatio =
                worldStats.playerMaxHealth > 0.f
                    ? std::clamp(worldStats.playerHealth /
                                     worldStats.playerMaxHealth,
                                 0.f, 1.f)
                    : 0.f;
            ImGui::Text("Health %.0f / %.0f",
                        std::ceil(worldStats.playerHealth),
                        std::ceil(worldStats.playerMaxHealth));
            ImGui::ProgressBar(healthRatio, ImVec2(-1.0f, 8.0f), "");
            if (worldStats.foodCooldownTicksRemaining > 0)
            {
                ImGui::Text("Food cooldown: %.1fs",
                            worldStats.foodCooldownTicksRemaining / 20.f);
            }
            if (worldStats.attackCooldownTicksRemaining > 0)
            {
                ImGui::Text("Attack ready in: %.1fs",
                            worldStats.attackCooldownTicksRemaining / 20.f);
            }
            const bool heldItemValid = state.heldItem >= 0 &&
                state.heldItem < static_cast<int>(state.inventory.size());
            if (heldItemValid)
            {
                const InventorySlotState &held =
                    state.inventory[static_cast<std::size_t>(state.heldItem)];
                if (held.amount > 0)
                {
                    const std::string heldName =
                        Material::toMaterial(held.materialId).name;
                    const float available = ImGui::GetContentRegionAvail().x;
                    const float width = ImGui::CalcTextSize(
                        heldName.c_str()).x;
                    ImGui::SetCursorPosX(
                        ImGui::GetCursorPosX() +
                        std::max(0.f, (available - width) * 0.5f));
                    ImGui::TextColored(ImVec4(0.96f, 0.82f, 0.34f, 1.f),
                                       "%s", heldName.c_str());
                }
            }
            for (std::size_t index = 0; index < state.inventory.size(); ++index)
            {
                if (index > 0)
                {
                    ImGui::SameLine();
                }

                drawHotbarSlot(
                    state.inventory[index], index,
                    static_cast<int>(index) == state.heldItem);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void drawContainer()
    {
        if (player == nullptr || !player->hasOpenContainer() ||
            player->hasOpenCrafting() || world == nullptr)
        {
            return;
        }

        if (runtimeSmeltingRegistry().isFrozen())
        {
            std::optional<FurnaceContainerView> furnace =
                FurnaceContainer::view(*world, *player,
                                       runtimeSmeltingRegistry());
            if (furnace)
            {
                const ImGuiIO &io = ImGui::GetIO();
                ImGui::SetNextWindowPos(
                    ImVec2(io.DisplaySize.x * 0.5f,
                           io.DisplaySize.y * 0.46f),
                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(
                    ImVec2(620.0f, 390.0f), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.96f);
                bool open = true;
                const ImGuiWindowFlags flags =
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoResize;
                if (ImGui::Begin("Furnace", &open, flags))
                {
                    const FurnaceSlot furnaceSlots[] = {
                        FurnaceSlot::Input, FurnaceSlot::Fuel,
                        FurnaceSlot::Output};
                    const char *slotNames[] = {"Input", "Fuel", "Output"};
                    const InventorySlotState stacks[] = {
                        furnace->state.input, furnace->state.fuel,
                        furnace->state.output};
                    for (int index = 0; index < 3; ++index)
                    {
                        if (index > 0) ImGui::SameLine();
                        const Material &material =
                            Material::toMaterial(stacks[index].materialId);
                        const std::string label =
                            std::string(slotNames[index]) + "\n" +
                            (stacks[index].amount > 0
                                 ? material.name
                                 : "Empty") +
                            " x" + std::to_string(stacks[index].amount) +
                            "##furnace" + std::to_string(index);
                        if (ImGui::Button(label.c_str(),
                                          ImVec2(190.0f, 58.0f)) &&
                            stacks[index].amount > 0 &&
                            FurnaceContainer::transferToPlayer(
                                *world, *player, furnaceSlots[index],
                                stacks[index].amount,
                                runtimeSmeltingRegistry()))
                        {
                            playUiFeedback();
                        }
                    }
                    const float smeltProgress =
                        furnace->recipeDurationTicks > 0
                            ? static_cast<float>(
                                  furnace->state.progressTicks) /
                                  static_cast<float>(
                                      furnace->recipeDurationTicks)
                            : 0.f;
                    const float fuelProgress =
                        furnace->state.burnTicksTotal > 0
                            ? static_cast<float>(
                                  furnace->state.burnTicksRemaining) /
                                  static_cast<float>(
                                      furnace->state.burnTicksTotal)
                            : 0.f;
                    ImGui::TextUnformatted("Smelting progress");
                    ImGui::ProgressBar(smeltProgress,
                                       ImVec2(-1.0f, 0.0f));
                    ImGui::TextUnformatted("Fuel remaining");
                    ImGui::ProgressBar(fuelProgress,
                                       ImVec2(-1.0f, 0.0f));
                    ImGui::Separator();
                    ImGui::TextUnformatted(
                        "Hotbar: smeltable items go to Input; fuel goes to Fuel");
                    for (int playerSlot = 0;
                         playerSlot < player->getInventorySlotCount();
                         ++playerSlot)
                    {
                        if (playerSlot > 0) ImGui::SameLine();
                        const ItemStack &stack =
                            player->getInventorySlot(playerSlot);
                        const std::string label =
                            (stack.isEmpty() ? "Empty"
                                             : stack.getMaterial().name) +
                            " x" +
                            std::to_string(stack.getNumInStack()) +
                            "##furnaceplayer" +
                            std::to_string(playerSlot);
                        if (ImGui::Button(label.c_str(),
                                          ImVec2(112.0f, 50.0f)) &&
                            !stack.isEmpty())
                        {
                            FurnaceSlot target = FurnaceSlot::Output;
                            if (runtimeSmeltingRegistry().findRecipe(
                                    stack.getMaterial().id) != nullptr)
                            {
                                target = FurnaceSlot::Input;
                            }
                            else if (runtimeSmeltingRegistry().findFuel(
                                         stack.getMaterial().id) != nullptr)
                            {
                                target = FurnaceSlot::Fuel;
                            }
                            if (target != FurnaceSlot::Output &&
                                FurnaceContainer::transferFromPlayer(
                                    *world, *player, target, playerSlot,
                                    stack.getNumInStack(),
                                    runtimeSmeltingRegistry()))
                            {
                                playUiFeedback();
                            }
                        }
                    }
                    if (ImGui::Button("Close", ImVec2(100.0f, 32.0f)))
                    {
                        open = false;
                        playUiFeedback();
                    }
                }
                ImGui::End();
                if (!open) player->closeContainer();
                return;
            }
        }

        std::optional<ChestContainerView> chest =
            ChestContainer::view(*world, *player);
        if (!chest)
        {
            player->closeContainer();
            return;
        }

        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.46f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(560.0f, 390.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        bool open = true;
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoResize;
        if (ImGui::Begin("Chest Container", &open, flags))
        {
            ImGui::TextUnformatted(
                "Chest slots (click to take into your hotbar)");
            for (int slot = 0; slot < chest->inventory.getSlotCount(); ++slot)
            {
                if (slot > 0 && slot % 3 != 0)
                {
                    ImGui::SameLine();
                }
                const InventorySlotState stack =
                    chest->inventory.getSlot(slot);
                const Material &material =
                    Material::toMaterial(stack.materialId);
                const std::string label =
                    (stack.amount > 0 ? material.name : "Empty") + " x" +
                    std::to_string(stack.amount) + "##chest" +
                    std::to_string(slot);
                if (ImGui::Button(label.c_str(), ImVec2(170.0f, 54.0f)) &&
                    stack.amount > 0)
                {
                    if (ChestContainer::transferToPlayer(
                            *world, *player, slot, stack.amount))
                    {
                        playUiFeedback();
                    }
                }
            }

            ImGui::Separator();
            ImGui::TextUnformatted(
                "Hotbar (click to store in the chest)");
            for (int slot = 0; slot < player->getInventorySlotCount(); ++slot)
            {
                if (slot > 0)
                {
                    ImGui::SameLine();
                }
                const ItemStack &stack = player->getInventorySlot(slot);
                const std::string label =
                    (stack.isEmpty() ? "Empty" : stack.getMaterial().name) +
                    " x" + std::to_string(stack.getNumInStack()) +
                    "##player" + std::to_string(slot);
                if (ImGui::Button(label.c_str(), ImVec2(102.0f, 50.0f)) &&
                    !stack.isEmpty())
                {
                    if (ChestContainer::transferFromPlayer(
                            *world, *player, slot,
                            stack.getNumInStack()))
                    {
                        playUiFeedback();
                    }
                }
            }
            ImGui::TextUnformatted(
                "Escape or Close returns to mouse-look.");
            if (ImGui::Button("Close", ImVec2(100.0f, 32.0f)))
            {
                open = false;
                playUiFeedback();
            }
        }
        ImGui::End();
        if (!open)
        {
            player->closeContainer();
        }
    }

    void drawCrafting()
    {
        if (player == nullptr || !player->hasOpenCrafting())
        {
            craftingSession.reset();
            return;
        }
        const int gridSize = player->getCraftingGridSize();
        if (craftingSession == nullptr ||
            craftingSession->gridSize() != gridSize)
        {
            craftingSession = std::make_unique<CraftingSession>(gridSize);
            selectedCraftingMaterial = Material::ID::Nothing;
            craftingMessage.clear();
        }

        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.48f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(820.0f, 660.0f), ImGuiCond_Always);
        bool open = true;
        const char *title =
            gridSize == CraftingSession::WorkbenchGridSize
                ? "Workbench Crafting"
                : "Player Crafting";
        if (ImGui::Begin(title, &open,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextUnformatted(
                "Choose an inventory material, fill the virtual grid, then craft.");
            ImGui::TextUnformatted(
                "Grid previews never remove items; right-click a cell to clear it.");
            if (ImGui::CollapsingHeader("Recipe Book",
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::BeginChild("##RecipeBook", ImVec2(0.0f, 135.0f),
                                  true);
                for (const RecipeDefinition &recipe :
                     runtimeRecipeRegistry().recipes())
                {
                    if (!recipeFitsGrid(recipe, gridSize))
                    {
                        continue;
                    }
                    const Material &output =
                        Material::toMaterial(recipe.outputMaterialId);
                    const std::string button =
                        "Load##recipe-" + recipe.id;
                    if (ImGui::SmallButton(button.c_str()))
                    {
                        if (craftingSession->loadRecipe(recipe))
                        {
                            craftingMessage = "Loaded " + output.name +
                                              " into the crafting grid.";
                            playUiFeedback();
                        }
                    }
                    ImGui::SameLine();
                    const std::string ingredients =
                        recipeIngredientSummary(recipe);
                    ImGui::Text("%s x%d  <-  %s", output.name.c_str(),
                                recipe.outputCount, ingredients.c_str());
                }
                ImGui::EndChild();
            }
            ImGui::Separator();

            const PlayerSaveState state = player->getSaveState();
            ImGui::TextUnformatted("Inventory materials");
            for (std::size_t index = 0; index < state.inventory.size();
                 ++index)
            {
                if (index > 0)
                {
                    ImGui::SameLine();
                }
                const InventorySlotState &slot = state.inventory[index];
                const Material &material =
                    Material::toMaterial(slot.materialId);
                const std::string label =
                    (slot.amount > 0 ? material.name : "Empty") + " x" +
                    std::to_string(slot.amount) + "##craft-source-" +
                    std::to_string(index);
                if (ImGui::Button(label.c_str(), ImVec2(122.0f, 46.0f)) &&
                    slot.amount > 0)
                {
                    selectedCraftingMaterial = slot.materialId;
                }
            }
            ImGui::Text("Selected: %s",
                        Material::toMaterial(selectedCraftingMaterial)
                            .name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Clear selection"))
            {
                selectedCraftingMaterial = Material::ID::Nothing;
            }

            ImGui::Separator();
            ImGui::Text("%dx%d input grid", gridSize, gridSize);
            for (int index = 0; index < craftingSession->cellCount();
                 ++index)
            {
                if (index % gridSize != 0)
                {
                    ImGui::SameLine();
                }
                const InventorySlotState &cell =
                    craftingSession->cell(index);
                const Material &material =
                    Material::toMaterial(cell.materialId);
                const std::string label =
                    (cell.amount > 0 ? material.name : "Empty") +
                    "##craft-cell-" + std::to_string(index);
                if (ImGui::Button(label.c_str(), ImVec2(145.0f, 52.0f)) &&
                    selectedCraftingMaterial != Material::ID::Nothing)
                {
                    craftingSession->setCell(
                        index, selectedCraftingMaterial);
                }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                {
                    craftingSession->clearCell(index);
                }
            }
            if (ImGui::Button("Clear grid"))
            {
                craftingSession->clear();
            }

            const CraftingPreview preview = player->previewCrafting(
                *craftingSession, runtimeRecipeRegistry());
            ImGui::Separator();
            if (!preview.recipeId.empty())
            {
                const Material &output =
                    Material::toMaterial(preview.outputMaterialId);
                ImGui::Text("Recipe: %s", preview.recipeId.c_str());
                ImGui::Text("Output: %s x%d | maximum crafts: %d",
                            output.name.c_str(), preview.outputCount,
                            preview.maxCrafts);
            }
            ImGui::TextWrapped("%s", preview.message.c_str());
            ImGui::BeginDisabled(!preview.ready());
            if (ImGui::Button("Craft one", ImVec2(150.0f, 38.0f)))
            {
                const CraftingCommitResult committed =
                    player->commitCrafting(
                        *craftingSession, runtimeRecipeRegistry(), preview,
                        1);
                craftingMessage = committed.message;
            }
            ImGui::SameLine();
            if (ImGui::Button("Craft maximum", ImVec2(170.0f, 38.0f)))
            {
                const CraftingCommitResult committed =
                    player->commitCrafting(
                        *craftingSession, runtimeRecipeRegistry(), preview,
                        preview.maxCrafts);
                craftingMessage = committed.message;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(110.0f, 38.0f)))
            {
                open = false;
                playUiFeedback();
            }
            if (!craftingMessage.empty())
            {
                ImGui::TextWrapped("%s", craftingMessage.c_str());
            }
        }
        ImGui::End();
        if (!open)
        {
            player->closeCrafting();
            craftingSession.reset();
        }
    }

    void drawDebugPanels()
    {
        if (player == nullptr || world == nullptr)
        {
            return;
        }
        if (ImGui::Begin("Player"))
        {
            const PlayerSaveState state = player->getSaveState();
            const Ogre::Vector3 cameraPosition = camera->getPosition();
            ImGui::Text("Player: %.2f, %.2f, %.2f", player->position.x,
                        player->position.y, player->position.z);
            ImGui::Text("Camera: %.2f, %.2f, %.2f", cameraPosition.x,
                        cameraPosition.y, cameraPosition.z);
            ImGui::Text("Selected slot: %d / %d", state.heldItem + 1,
                        static_cast<int>(state.inventory.size()));
            ImGui::Text("Health: %.1f / %.1f", worldStats.playerHealth,
                        worldStats.playerMaxHealth);
            ImGui::Text("Food cooldown ticks: %d",
                        worldStats.foodCooldownTicksRemaining);
            ImGui::Text("Attack cooldown ticks: %d",
                        worldStats.attackCooldownTicksRemaining);
            ImGui::TextUnformatted("R: consume held food");
            ImGui::Text("Death inventory policy: %s",
                        World::PlayerDeathInventoryPolicy);
            ImGui::TextUnformatted("F1: toggle debug panels");
        }
        ImGui::End();

        if (ImGui::Begin("Sandbox"))
        {
            ImGui::Text("Seed: %d (terrain v%d)", worldStats.terrainSeed,
                        worldStats.terrainGenerationVersion);
            ImGui::Text("World time: %.0f", worldStats.worldTime);
            ImGui::Text("Day cycle / light: %.3f / %.3f",
                        worldStats.environment.cycle,
                        worldStats.environment.daylight);
            ImGui::Text("Fog density: %.4f",
                        worldStats.environment.fogDensity);
            ImGui::Text("Cloud coverage: %.3f",
                        worldStats.environment.cloudCoverage);
            ImGui::Text(
                "Water shallow / deep: %.2f %.2f %.2f / %.2f %.2f %.2f",
                worldStats.environment.waterShallowColour.r,
                worldStats.environment.waterShallowColour.g,
                worldStats.environment.waterShallowColour.b,
                worldStats.environment.waterDeepColour.r,
                worldStats.environment.waterDeepColour.g,
                worldStats.environment.waterDeepColour.b);
            ImGui::Text("Actors: %llu",
                        static_cast<unsigned long long>(
                            worldStats.actorCount));
            ImGui::Text("Natural mobs: %llu / %llu (local cap %llu)",
                        static_cast<unsigned long long>(
                            worldStats.naturalMobCount),
                        static_cast<unsigned long long>(
                            worldStats.naturalMobWorldCap),
                        static_cast<unsigned long long>(
                            worldStats.naturalMobLocalCap));
            ImGui::Text("Natural spawn attempts / added / removed: %llu / %llu / %llu",
                        static_cast<unsigned long long>(
                            worldStats.naturalMobSpawnAttempts),
                        static_cast<unsigned long long>(
                            worldStats.naturalMobsSpawned),
                        static_cast<unsigned long long>(
                            worldStats.naturalMobsDespawned));
            ImGui::Separator();
            ImGui::Text("Chunks: %llu existing / %llu loaded",
                        static_cast<unsigned long long>(
                            worldStats.chunks.existingChunks),
                        static_cast<unsigned long long>(
                            worldStats.chunks.loadedChunks));
            ImGui::Text("Dirty chunks: %llu",
                        static_cast<unsigned long long>(
                            worldStats.chunks.saveDirtyChunks));
            const double averageSaveMs =
                worldStats.chunks.saveTransactions > 0
                    ? worldStats.chunks.saveTotalMs /
                          static_cast<double>(
                              worldStats.chunks.saveTransactions)
                    : 0.0;
            ImGui::Text(
                "Save transactions / ms total / avg / max: %llu / %.3f / %.3f / %.3f",
                static_cast<unsigned long long>(
                    worldStats.chunks.saveTransactions),
                worldStats.chunks.saveTotalMs, averageSaveMs,
                worldStats.chunks.saveMaxMs);
            ImGui::Text("Queued chunk updates: %llu",
                        static_cast<unsigned long long>(
                            worldStats.queuedChunkUpdates));
            ImGui::Text("Random ticks blocks / sections / last: %llu / %llu / %llu",
                        static_cast<unsigned long long>(
                            worldStats.randomTickBlocks),
                        static_cast<unsigned long long>(
                            worldStats.randomTickSections),
                        static_cast<unsigned long long>(
                            worldStats.randomTickSectionsProcessed));
            ImGui::Text("Random ticks dispatched: %llu",
                        static_cast<unsigned long long>(
                            worldStats.randomTicksDispatched));
            ImGui::Separator();
            ImGui::Text("Sections: %llu",
                        static_cast<unsigned long long>(
                            worldStats.chunks.sections));
            ImGui::Text("Mesh dirty / CPU / GPU: %llu / %llu / %llu",
                        static_cast<unsigned long long>(
                            worldStats.chunks.meshDirtySections),
                        static_cast<unsigned long long>(
                            worldStats.chunks.cpuReadySections),
                        static_cast<unsigned long long>(
                            worldStats.chunks.gpuBufferedSections));
            ImGui::Text("Mesh rebuilds: %llu",
                        static_cast<unsigned long long>(
                            worldStats.chunks.meshRebuilds));
            const double averageBuildMs =
                worldStats.chunks.meshRebuilds > 0
                    ? worldStats.chunks.meshBuildTotalMs /
                          static_cast<double>(worldStats.chunks.meshRebuilds)
                    : 0.0;
            ImGui::Text("Mesh build ms last / avg / max: %.3f / %.3f / %.3f",
                        worldStats.chunks.meshBuildLastMs, averageBuildMs,
                        worldStats.chunks.meshBuildMaxMs);
            ImGui::Text(
                "Mesh faces solid / glass / water / flora: %llu / %llu / %llu / %llu",
                        static_cast<unsigned long long>(
                            worldStats.chunks.solidFaces),
                        static_cast<unsigned long long>(
                            worldStats.chunks.transparentFaces),
                        static_cast<unsigned long long>(
                            worldStats.chunks.waterFaces),
                        static_cast<unsigned long long>(
                            worldStats.chunks.floraFaces));
            ImGui::Text(
                "Mesh vertices solid / glass / water / flora: %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.chunks.solidVertices),
                static_cast<unsigned long long>(
                    worldStats.chunks.transparentVertices),
                static_cast<unsigned long long>(
                    worldStats.chunks.waterVertices),
                static_cast<unsigned long long>(
                    worldStats.chunks.floraVertices));
            ImGui::Text("Terrain vertex / index stride: %llu / %llu B",
                        static_cast<unsigned long long>(
                            TerrainBufferMetrics::VertexStrideBytes),
                        static_cast<unsigned long long>(
                            TerrainBufferMetrics::IndexStrideBytes));
            ImGui::Text("Resident terrain vertices / indices: %llu / %llu",
                        static_cast<unsigned long long>(
                            worldStats.terrainBuffers.vertexCount),
                        static_cast<unsigned long long>(
                            worldStats.terrainBuffers.indexCount));
            ImGui::Text(
                "Resident terrain buffers: %.3f MiB (%llu vertex + %llu index bytes)",
                static_cast<double>(
                    worldStats.terrainBuffers.totalBytes()) /
                    (1024.0 * 1024.0),
                static_cast<unsigned long long>(
                    worldStats.terrainBuffers.vertexBytes()),
                static_cast<unsigned long long>(
                    worldStats.terrainBuffers.indexBytes()));
        }
        ImGui::End();
    }

    Ogre::RenderWindow *window = nullptr;
    Ogre::SceneManager *sceneManager = nullptr;
    Ogre::Camera *camera = nullptr;
    Player *player = nullptr;
    World *world = nullptr;
    GameApplicationFlow *flow = nullptr;
    WorldManagementService *management = nullptr;
    UserSettings appliedSettings;
    std::function<void()> uiFeedback;
    std::vector<PendingCrashReport> crashReports;
    std::string crashReportMessage;
    bool crashPopupOpened = false;
    RuntimeSettingsSession settingsSession;
    std::string settingsMessage;
    bool settingsApplyPending = false;
    std::unique_ptr<CraftingSession> craftingSession;
    Material::ID selectedCraftingMaterial = Material::ID::Nothing;
    std::string craftingMessage;
    std::vector<WorldCatalogueEntry> worlds;
    std::vector<DeletedWorldInfo> deletedWorlds;
    std::vector<WorldBackupInfo> backups;
    std::string selectedWorldId;
    std::string pendingDeleteWorldId;
    std::string pendingPermanentDeleteWorldId;
    std::string pendingBackupId;
    std::string statusMessage;
    float statusMessageSeconds = 0.f;
    std::string audioCaption;
    float audioCaptionSeconds = 0.f;
    float interactionFeedbackSeconds = 0.f;
    ImVec4 interactionFeedbackColour = ImVec4(1.f, 1.f, 1.f, 1.f);
    float hudElapsedSeconds = 0.f;
    float performanceSampleSeconds = 0.f;
    float performanceSamplePeakMs = 0.f;
    float displayedFramesPerSecond = 0.f;
    float displayedFrameMs = 0.f;
    float displayedPeakFrameMs = 0.f;
    float performanceOverlayBottom = 90.f;
    std::size_t performanceSampleFrames = 0;
    std::array<char, 81> createName{};
    std::array<char, 81> renameName{};
    int createSeed = 0;
    bool worldsDirty = true;
    bool openDeletePopup = false;
    bool openPermanentDeletePopup = false;
    bool openBackupPopup = false;
    OgreUserInterfaceAction pendingAction;
    WorldDebugStats worldStats;
    MiningProgressSnapshot miningProgress;
    bool showDebugPanel = false;
    bool initialized = false;
    bool listenerInstalled = false;
    bool framePending = false;
    Ogre::TexturePtr atlasTexture;
    ImTextureID atlasTextureId = ImTextureID_Invalid;
    std::string iniPath;
};

OgreUserInterface::OgreUserInterface(Ogre::RenderWindow &window,
                                     Ogre::SceneManager &sceneManager,
                                     Ogre::Camera &camera, Player *player,
                                     World *world,
                                     GameApplicationFlow &applicationFlow,
                                      WorldManagementService &worldManagement,
                                      const UserSettings &settings,
                                      std::function<void()> uiFeedback,
                                      std::vector<PendingCrashReport> crashReports)
    : m_impl(std::make_unique<Impl>(window, sceneManager, camera, player,
                                    world, applicationFlow,
                                     worldManagement, settings,
                                     std::move(uiFeedback),
                                     std::move(crashReports)))
{
    m_impl->initialize(this);
}

OgreUserInterface::~OgreUserInterface()
{
    m_impl->shutdown(this);
}

void OgreUserInterface::beginFrame(
    float deltaSeconds, const WorldDebugStats &worldStats,
    const MiningProgressSnapshot &miningProgress)
{
    m_impl->beginFrame(deltaSeconds, worldStats, miningProgress);
}

void OgreUserInterface::keyEvent(const OIS::KeyEvent &event, bool pressed,
                                 const OIS::Keyboard &keyboard)
{
    ImGuiIO &io = ImGui::GetIO();
    const ImGuiKey key = toImGuiKey(event.key);
    if (key != ImGuiKey_None)
    {
        io.AddKeyEvent(key, pressed);
    }
    io.AddKeyEvent(ImGuiMod_Ctrl,
                   keyboard.isKeyDown(OIS::KC_LCONTROL) ||
                       keyboard.isKeyDown(OIS::KC_RCONTROL));
    io.AddKeyEvent(ImGuiMod_Shift,
                   keyboard.isKeyDown(OIS::KC_LSHIFT) ||
                       keyboard.isKeyDown(OIS::KC_RSHIFT));
    io.AddKeyEvent(ImGuiMod_Alt,
                   keyboard.isKeyDown(OIS::KC_LMENU) ||
                       keyboard.isKeyDown(OIS::KC_RMENU));
    io.AddKeyEvent(ImGuiMod_Super,
                   keyboard.isKeyDown(OIS::KC_LWIN) ||
                       keyboard.isKeyDown(OIS::KC_RWIN));

    if (!pressed)
    {
        return;
    }
    if (event.text >= 32)
    {
        io.AddInputCharacter(event.text);
    }
    if (event.key == OIS::KC_F1)
    {
        m_impl->showDebugPanel = !m_impl->showDebugPanel;
        return;
    }
}

void OgreUserInterface::mouseMoved(const OIS::MouseEvent &event)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(event.state.X.abs),
                        static_cast<float>(event.state.Y.abs));
    const float wheel = normalizedWheelDelta(event.state.Z.rel);
    if (wheel != 0.0f)
    {
        io.AddMouseWheelEvent(0.0f, wheel);
    }
}

void OgreUserInterface::mouseButton(const OIS::MouseEvent &event,
                                    int button, bool pressed)
{
    ImGuiIO &io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(event.state.X.abs),
                        static_cast<float>(event.state.Y.abs));
    const int mappedButton = toImGuiMouseButton(button);
    if (mappedButton >= 0)
    {
        io.AddMouseButtonEvent(mappedButton, pressed);
    }
}

bool OgreUserInterface::wantsKeyboardInput() const
{
    return m_impl->flow->state() != GameApplicationState::Playing ||
           (m_impl->player != nullptr &&
            (m_impl->player->hasOpenContainer() ||
             m_impl->player->hasOpenCrafting())) ||
           ImGui::GetIO().WantCaptureKeyboard;
}

bool OgreUserInterface::wantsMouseInput() const
{
    return m_impl->flow->state() != GameApplicationState::Playing ||
           (m_impl->player != nullptr &&
            (m_impl->player->hasOpenContainer() ||
             m_impl->player->hasOpenCrafting())) ||
           ImGui::GetIO().WantCaptureMouse;
}

bool OgreUserInterface::hasBlockingModal() const noexcept
{
    return !m_impl->crashReports.empty();
}

bool OgreUserInterface::isDebugPanelVisible() const noexcept
{
    return m_impl->showDebugPanel;
}

void OgreUserInterface::setWorldContext(Player *player,
                                        World *world) noexcept
{
    m_impl->player = player;
    m_impl->world = world;
}

void OgreUserInterface::setStatusMessage(std::string message)
{
    m_impl->setStatusMessage(std::move(message));
}

void OgreUserInterface::setAudioCaption(std::string caption)
{
    m_impl->setAudioCaption(std::move(caption));
}

bool OgreUserInterface::dismissSettings() noexcept
{
    return m_impl->dismissSettings();
}

void OgreUserInterface::reportSettingsApplied(
    bool succeeded, const UserSettings &settings, std::string message)
{
    m_impl->reportSettingsApplied(succeeded, settings, std::move(message));
}

OgreUserInterfaceAction OgreUserInterface::consumeAction()
{
    OgreUserInterfaceAction action = std::move(m_impl->pendingAction);
    m_impl->pendingAction = {};
    return action;
}

void OgreUserInterface::postRenderQueues()
{
    if (!m_impl->framePending)
    {
        return;
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    m_impl->framePending = false;
}

OgreUserInterfaceValidation OgreUserInterface::validateConfiguration(
    const Player &player)
{
    const PlayerSaveState state = player.getSaveState();
    OgreUserInterfaceValidation validation;
    validation.debugPanelVisible =
        RuntimeDebugOptions::showDebugInfoAtStartup();
    validation.hotbarSlots = state.inventory.size();
    validation.selectedSlot = state.heldItem;
    validation.containerOpen = player.hasOpenContainer();
    if (state.inventory.empty())
    {
        validation.message = "player hotbar is empty";
        return validation;
    }
    if (state.heldItem < 0 ||
        state.heldItem >= static_cast<int>(state.inventory.size()))
    {
        validation.message = "selected hotbar slot is out of range";
        return validation;
    }
    validation.valid = true;
    validation.message = "ok";
    return validation;
}
