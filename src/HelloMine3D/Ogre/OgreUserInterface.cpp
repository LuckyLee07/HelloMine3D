#include "OgreUserInterface.h"

#include <OIS.h>
#include <OgreCamera.h>
#include <OgreResourceGroupManager.h>
#include <OgreRenderWindow.h>
#include <OgreSceneManager.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>
#include <OgreViewport.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
#include "../Presentation/LocalizedPresentation.h"
#include "../Presentation/PresentationCaption.h"
#include "../Presentation/PresentationLayout.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../Util/ResourcePaths.h"
#include "../World/World.h"
#include "../World/Block/ChestContainer.h"
#include "../World/Block/FurnaceContainer.h"
#include "../World/Block/TerrainMaterialProfile.h"
#include "../Item/SmeltingRegistry.h"
#include "../World/Interaction/BlockMiningProgress.h"
#include "../Feedback/ActionFeedback.h"
#include "../World/Storage/WorldManagementService.h"

namespace
{
#if defined(__APPLE__)
    constexpr const char* ImGuiGlslVersion = "#version 150";
#else
    constexpr const char* ImGuiGlslVersion = "#version 130";
#endif

    bool environmentFlagEnabled(const char* name)
    {
        const char* value = std::getenv(name);
        return value != nullptr && std::string(value) == "1";
    }

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

    std::string recipeIngredientSummary(const RecipeDefinition &recipe,
                                        const std::string& locale)
    {
        std::string summary;
        for (const RecipeIngredient &ingredient : recipe.ingredients)
        {
            if (!summary.empty())
            {
                summary += ", ";
            }
            summary += LocalizedPresentation::materialName(
                           locale, ingredient.materialId) +
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
         const UserSettings &settings, std::string presentationFontPath,
         std::function<void()> feedback,
         std::vector<PendingCrashReport> pendingCrashReports)
        : window(&renderWindow)
        , sceneManager(&renderSceneManager)
        , camera(&renderCamera)
        , player(worldPlayer)
        , world(activeWorld)
        , flow(&applicationFlow)
        , management(&worldManagement)
        , appliedSettings(settings)
        , fontPath(std::move(presentationFontPath))
        , uiFeedback(std::move(feedback))
        , crashReports(std::move(pendingCrashReports))
        , showDebugPanel(RuntimeDebugOptions::showDebugInfoAtStartup())
        , settingsFixtureRequested(environmentFlagEnabled(
              "HELLOMINE3D_V10E_SETTINGS_FIXTURE"))
        , iniPath(ResourcePaths::bin("imgui-ogre.ini"))
    {
        std::snprintf(createName.data(), createName.size(), "%s",
                      LocalizedPresentation::text(
                          appliedSettings.locale,
                          "world.default_name", "New World").c_str());
        createSeed = WorldManagementService::suggestWorldSeed();
    }

    std::string tr(const std::string& key,
                   const std::string& fallback = {}) const
    {
        return LocalizedPresentation::text(
            appliedSettings.locale, key, fallback);
    }

    std::string label(const std::string& key, const char* stableId,
                      const std::string& fallback = {}) const
    {
        return tr(key, fallback) + stableId;
    }

    std::string materialName(Material::ID id) const
    {
        return LocalizedPresentation::materialName(
            appliedSettings.locale, id);
    }

    std::string difficultyName(WorldDifficulty difficulty) const
    {
        switch (difficulty)
        {
            case WorldDifficulty::Casual:
                return tr("difficulty.casual", "Casual");
            case WorldDifficulty::Normal:
                return tr("difficulty.normal", "Normal");
            case WorldDifficulty::Challenging:
                return tr("difficulty.challenging", "Challenging");
            case WorldDifficulty::Count:
                break;
        }
        return tr("difficulty.normal", "Normal");
    }

    std::string actionName(GameplayAction action) const
    {
        const std::string configKey = gameplayActionConfigKey(action);
        const std::string suffix = configKey.rfind("key_", 0) == 0
            ? configKey.substr(4)
            : configKey;
        return tr("action." + suffix, gameplayActionName(action));
    }

    std::string worldActionName(GameplayWorldAction action) const
    {
        const std::string configKey =
            gameplayWorldActionConfigKey(action);
        const std::string suffix = configKey.rfind("mouse_", 0) == 0
            ? configKey.substr(6)
            : configKey;
        return tr("action." + suffix, gameplayWorldActionName(action));
    }

    std::string objectiveText(const std::string& id, const char* field,
                              const std::string& fallback) const
    {
        return LocalizedPresentation::objectiveText(
            appliedSettings.locale, id, field, fallback);
    }

    std::string craftingPreviewMessage(CraftingPreviewStatus status) const
    {
        switch (status)
        {
            case CraftingPreviewStatus::NoMatch:
                return tr("crafting.preview.no_match");
            case CraftingPreviewStatus::MissingIngredients:
                return tr("crafting.preview.missing");
            case CraftingPreviewStatus::OutputFull:
                return tr("crafting.preview.output_full");
            case CraftingPreviewStatus::Ready:
                return tr("crafting.preview.ready");
        }
        return tr("crafting.preview.no_match");
    }

    std::string craftingCommitMessage(CraftingCommitStatus status) const
    {
        switch (status)
        {
            case CraftingCommitStatus::Success:
                return tr("crafting.commit.success");
            case CraftingCommitStatus::StaleSession:
            case CraftingCommitStatus::StaleInventory:
                return tr("crafting.commit.stale");
            case CraftingCommitStatus::NoMatch:
                return tr("crafting.commit.no_match");
            case CraftingCommitStatus::MissingIngredients:
                return tr("crafting.commit.missing");
            case CraftingCommitStatus::OutputFull:
                return tr("crafting.commit.output_full");
            case CraftingCommitStatus::InvalidRequest:
                return tr("crafting.commit.invalid");
        }
        return tr("crafting.commit.invalid");
    }

    void initialize(Ogre::RenderTargetListener *listener)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.BackendPlatformName = "HelloMine3D_OIS";
        io.IniFilename = iniPath.c_str();
        io.FontGlobalScale = appliedSettings.uiScale;
        const PresentationFontProbe fontProbe =
            probePresentationFont(fontPath);
        if (fontProbe.usable)
        {
            ImFontGlyphRangesBuilder glyphBuilder;
            glyphBuilder.AddRanges(
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
            const LocalizedTextRegistry& textRegistry =
                runtimeLocalizedTextRegistry();
            for (const std::string& key : textRegistry.keys("zh-CN"))
            {
                const std::string translated =
                    textRegistry.lookup("zh-CN", key);
                glyphBuilder.AddText(translated.c_str());
            }
            glyphBuilder.BuildRanges(&presentationGlyphRanges);
            if (io.Fonts->AddFontFromFileTTF(
                    fontPath.c_str(), 17.0f, nullptr,
                    presentationGlyphRanges.Data) ==
                nullptr)
            {
                fontDiagnostic = "Unable to parse presentation font: " +
                                 fontPath;
            }
        }
        else
        {
            fontDiagnostic = fontProbe.diagnostic;
        }
        if (io.Fonts->Fonts.empty())
        {
            io.Fonts->AddFontDefault();
        }
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

        window->addListener(listener);
        listenerInstalled = true;
        initialized = true;
    }

    void shutdown(Ogre::RenderTargetListener *listener)
    {
        if (listenerInstalled && window != nullptr)
        {
            window->removeListener(listener);
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
                    const MiningProgressSnapshot &progress,
                    const ActionFeedbackSnapshot &feedback)
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
        captionTimeline.update(deltaSeconds);
        interactionFeedbackSeconds = std::max(
            0.f, interactionFeedbackSeconds -
                     std::max(0.f, deltaSeconds));
        if (flow->state() == GameApplicationState::Playing &&
            previousPlayerHealth > 0.f && stats.playerHealth <= 0.f)
        {
            statusMessage = tr("death.respawn");
            statusMessageSeconds = 4.f;
        }
        if (stats.playerMaxHealth > 0.f)
        {
            previousPlayerHealth = stats.playerHealth;
        }
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
        actionFeedback = feedback;
        if (settingsFixtureRequested && !settingsFixtureOpened &&
            flow->state() == GameApplicationState::Playing && flow->pause())
        {
            settingsSession.begin(appliedSettings);
            settingsMessage.clear();
            settingsApplyPending = false;
            settingsFixtureOpened = true;
        }
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
                drawVictoryOverlay();
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
        drawCredits();
    }

    void drawCrashReportPrompt()
    {
        if (crashReports.empty())
        {
            return;
        }
        if (!crashPopupOpened)
        {
            const std::string popup =
                label("crash.title", "##PreviousCrashReport",
                      "Previous crash report");
            ImGui::OpenPopup(popup.c_str());
            crashPopupOpened = true;
        }
        ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);
        const std::string popup =
            label("crash.title", "##PreviousCrashReport",
                  "Previous crash report");
        if (!ImGui::BeginPopupModal(popup.c_str(), nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }

        const PendingCrashReport& report = crashReports.front();
        ImGui::TextWrapped("%s", tr("crash.body").c_str());
        ImGui::Separator();
        ImGui::Text("%s: %s", tr("crash.report").c_str(),
                    report.dumpFile.c_str());
        ImGui::Text("%s: %s", tr("crash.build").c_str(),
                    report.buildIdentity.c_str());
        ImGui::Text("%s: %s", tr("crash.exception").c_str(),
                    report.exceptionCode.c_str());
        if (crashReports.size() > 1)
        {
            ImGui::Text("%s: %llu", tr("crash.pending").c_str(),
                        static_cast<unsigned long long>(crashReports.size()));
        }
        if (!crashReportMessage.empty())
        {
            ImGui::TextWrapped("%s", crashReportMessage.c_str());
        }
        ImGui::Separator();
        if (ImGui::Button(label("crash.open_folder", "##CrashOpen").c_str(),
                          ImVec2(140.0f, 38.0f)))
        {
            std::string error;
            crashReportMessage = openCrashReportLocation(report, &error)
                                     ? tr("crash.opened")
                                     : tr("crash.open_failed") + ": " + error;
            playUiFeedback();
        }
        ImGui::SameLine();
        if (ImGui::Button(label("crash.copy_details", "##CrashCopy").c_str(),
                          ImVec2(140.0f, 38.0f)))
        {
            ImGui::SetClipboardText(report.clipboardText.c_str());
            crashReportMessage = tr("crash.copied");
            playUiFeedback();
        }
        ImGui::SameLine();
        if (ImGui::Button(label("crash.ignore", "##CrashIgnore").c_str(),
                          ImVec2(140.0f, 38.0f)))
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
                    tr("crash.ignore_failed") + ": " + error;
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
        ImGui::SetNextWindowSize(ImVec2(420.0f, 340.0f), ImGuiCond_Always);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##MainMenu", nullptr, flags))
        {
            ImGui::SetCursorPosY(38.0f);
            const std::string appTitle = tr("app.title", "HelloMine3D");
            const float titleWidth = ImGui::CalcTextSize(appTitle.c_str()).x;
            ImGui::SetCursorPosX((420.0f - titleWidth) * 0.5f);
            ImGui::TextUnformatted(appTitle.c_str());
            ImGui::SetCursorPos(ImVec2(90.0f, 105.0f));
            if (ImGui::Button(
                    label("main.single_player", "##SinglePlayer").c_str(),
                    ImVec2(240.0f, 48.0f)))
            {
                if (flow->showWorldList())
                {
                    worldsDirty = true;
                    playUiFeedback();
                }
            }
            ImGui::SetCursorPos(ImVec2(90.0f, 170.0f));
            if (ImGui::Button(label("main.credits", "##Credits").c_str(),
                              ImVec2(240.0f, 42.0f)))
            {
                showCredits = true;
                playUiFeedback();
            }
            ImGui::SetCursorPos(ImVec2(90.0f, 225.0f));
            if (ImGui::Button(label("common.quit", "##Quit").c_str(),
                              ImVec2(240.0f, 42.0f)))
            {
                pendingAction.type = OgreUserInterfaceActionType::Quit;
                playUiFeedback();
            }
        }
        ImGui::End();
    }

    void drawCredits()
    {
        if (!showCredits)
        {
            return;
        }
        const ImGuiIO& io = ImGui::GetIO();
        const PresentationWindowLayout layout = fitPresentationWindow(
            io.DisplaySize.x, io.DisplaySize.y, 620.0f, 440.0f,
            appliedSettings.uiScale);
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height),
                                 ImGuiCond_Always);
        bool open = true;
        const std::string title =
            label("credits.title", "##CreditsWindow");
        if (ImGui::Begin(title.c_str(), &open,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::BeginChild("##CreditsContent", ImVec2(0.0f, -52.0f),
                              false,
                              layout.scrollRequired
                                  ? ImGuiWindowFlags_AlwaysVerticalScrollbar
                                  : ImGuiWindowFlags_None);
            ImGui::TextWrapped("%s", tr("credits.intro").c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", tr("credits.project").c_str());
            ImGui::SeparatorText(tr("credits.font_heading").c_str());
            ImGui::TextWrapped("%s", tr("credits.font_name").c_str());
            ImGui::TextWrapped("%s", tr("credits.font_source").c_str());
            ImGui::TextWrapped("%s", tr("credits.font_license").c_str());
            ImGui::TextWrapped("%s", tr("credits.font_path").c_str());
            ImGui::SeparatorText(tr("credits.audio_heading").c_str());
            ImGui::TextWrapped("%s", tr("credits.audio_name").c_str());
            ImGui::TextWrapped("%s", tr("credits.audio_source").c_str());
            ImGui::TextWrapped("%s", tr("credits.audio_license").c_str());
            ImGui::TextWrapped("%s", tr("credits.audio_path").c_str());
            ImGui::SeparatorText(tr("credits.music_heading").c_str());
            ImGui::TextWrapped("%s", tr("credits.music_name").c_str());
            ImGui::TextWrapped("%s", tr("credits.music_source").c_str());
            ImGui::TextWrapped("%s", tr("credits.music_license").c_str());
            ImGui::TextWrapped("%s", tr("credits.music_path").c_str());
            if (!fontDiagnostic.empty())
            {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", fontDiagnostic.c_str());
            }
            ImGui::EndChild();
            if (ImGui::Button(label("common.close", "##CreditsClose").c_str(),
                              ImVec2(-1.0f, 38.0f)))
            {
                open = false;
                playUiFeedback();
            }
        }
        ImGui::End();
        showCredits = open;
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
        const std::string windowTitle = label("world.title", "##Worlds");
        if (ImGui::Begin(windowTitle.c_str(), nullptr, flags))
        {
            if (ImGui::Button(label("world.back_to_main", "##WorldBack").c_str()))
            {
                if (flow->returnToMainMenu())
                {
                    playUiFeedback();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(label("common.refresh", "##WorldRefresh").c_str()))
            {
                worldsDirty = true;
                playUiFeedback();
            }
            ImGui::Separator();

            ImGui::TextUnformatted(tr("world.create_title").c_str());
            ImGui::SetNextItemWidth(280.0f);
            ImGui::InputText(label("world.name", "##create-name").c_str(),
                             createName.data(),
                             createName.size());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt(label("world.seed", "##create-seed").c_str(),
                            &createSeed);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(135.0f);
            const std::string createDifficultyName = difficultyName(
                static_cast<WorldDifficulty>(createDifficulty));
            if (ImGui::BeginCombo(
                    label("world.difficulty", "##create-difficulty").c_str(),
                    createDifficultyName.c_str()))
            {
                for (int value = 0;
                     value < static_cast<int>(WorldDifficulty::Count);
                     ++value)
                {
                    const auto difficulty =
                        static_cast<WorldDifficulty>(value);
                    const bool selected = value == createDifficulty;
                    const std::string option = difficultyName(difficulty);
                    if (ImGui::Selectable(option.c_str(), selected))
                    {
                        createDifficulty = value;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            if (ImGui::Button(label("common.create", "##WorldCreate").c_str()))
            {
                const WorldManagementResult result =
                    management->createWorld(
                        createName.data(), createSeed,
                        static_cast<WorldDifficulty>(createDifficulty));
                reportResult(result);
                if (result.succeeded())
                {
                    createSeed = WorldManagementService::suggestWorldSeed();
                }
            }

            ImGui::Separator();
            ImGui::Text("%s (%llu)", tr("world.active").c_str(),
                        static_cast<unsigned long long>(worlds.size()));
            ImGui::BeginChild("WorldList", ImVec2(0.0f, 185.0f), true);
            const ImGuiTableFlags worldTableFlags =
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_NoSavedSettings;
            if (ImGui::BeginTable("ActiveWorlds", 4, worldTableFlags))
            {
                ImGui::TableSetupColumn(
                    tr("world.world").c_str(),
                    ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn(
                    tr("world.seed").c_str(),
                    ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn(
                    tr("world.difficulty").c_str(),
                    ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn(
                    tr("world.actions").c_str(),
                    ImGuiTableColumnFlags_WidthFixed, 150.0f);
                for (const WorldCatalogueEntry &entry : worlds)
                {
                    ImGui::PushID(entry.id.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    const bool selected = selectedWorldId == entry.id;
                    const std::string worldLabel = entry.completed
                        ? entry.displayName + "  [" +
                              runtimeLocalizedTextRegistry().lookup(
                                  appliedSettings.locale,
                                  "world.list.completed") + "]"
                        : entry.displayName;
                    if (ImGui::Selectable(worldLabel.c_str(), selected))
                    {
                        selectWorld(entry);
                    }
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s %d", tr("world.seed").c_str(),
                                entry.seed);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(
                        difficultyName(entry.difficulty).c_str());
                    if (entry.completedPostVictoryEvents > 0)
                    {
                        ImGui::TextDisabled(
                            "%s %d / %d", tr("world.echo_trials").c_str(),
                            entry.completedPostVictoryEvents,
                            PostVictoryEvents::MaximumEvents);
                    }
                    ImGui::TableSetColumnIndex(3);
                    if (ImGui::SmallButton(label("common.play", "##Play").c_str()))
                    {
                        pendingAction.type =
                            OgreUserInterfaceActionType::OpenWorld;
                        pendingAction.worldId = entry.id;
                        playUiFeedback();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(label("common.delete", "##Delete").c_str()))
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
                ImGui::InputText(label("world.display_name", "##rename").c_str(),
                                 renameName.data(),
                                 renameName.size());
                ImGui::SameLine();
                if (ImGui::Button(label("common.rename", "##Rename").c_str()))
                {
                    reportResult(management->renameWorld(
                        selectedWorldId, renameName.data()));
                }
                ImGui::SameLine();
                ImGui::Text("%s: %llu", tr("world.backups").c_str(),
                            static_cast<unsigned long long>(backups.size()));
                for (const WorldBackupInfo &backup : backups)
                {
                    ImGui::PushID(backup.id.c_str());
                    ImGui::Text("%s (%llu %s)", backup.id.c_str(),
                                static_cast<unsigned long long>(
                                    backup.fileCount),
                                tr("world.files").c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton(label("world.restore_backup", "##RestoreBackup").c_str()))
                    {
                        pendingBackupId = backup.id;
                        openBackupPopup = true;
                    }
                    ImGui::PopID();
                }
            }

            ImGui::Separator();
            ImGui::Text("%s (%llu)", tr("world.recoverable").c_str(),
                        static_cast<unsigned long long>(
                            deletedWorlds.size()));
            ImGui::BeginChild("DeletedWorldList", ImVec2(0.0f, 105.0f),
                              true);
            for (const DeletedWorldInfo &entry : deletedWorlds)
            {
                ImGui::PushID(entry.recoveryId.c_str());
                ImGui::TextUnformatted(entry.world.displayName.c_str());
                ImGui::SameLine(400.0f);
                if (ImGui::SmallButton(label("common.restore", "##RestoreWorld").c_str()))
                {
                    reportResult(management->restoreDeletedWorld(
                        entry.world.id));
                }
                ImGui::SameLine();
                if (ImGui::SmallButton(label("world.delete_permanently", "##DeletePermanent").c_str()))
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
                ImGui::OpenPopup(label("world.delete_recoverable_title", "##RecoverableDelete").c_str());
                openDeletePopup = false;
            }
            if (openPermanentDeletePopup)
            {
                ImGui::OpenPopup(label("world.delete_permanent_title", "##PermanentDelete").c_str());
                openPermanentDeletePopup = false;
            }
            if (openBackupPopup)
            {
                ImGui::OpenPopup(label("world.restore_backup_title", "##BackupRestore").c_str());
                openBackupPopup = false;
            }

            if (ImGui::BeginPopupModal(label("world.delete_recoverable_title", "##RecoverableDelete").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("%s", tr("world.delete_recoverable_body").c_str());
                if (ImGui::Button(label("common.delete", "##ConfirmDelete").c_str(), ImVec2(120.0f, 0.0f)))
                {
                    reportResult(management->deleteWorld(
                        pendingDeleteWorldId));
                    selectedWorldId.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(label("common.cancel", "##CancelDelete").c_str(), ImVec2(120.0f, 0.0f)))
                {
                    playUiFeedback();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal(label("world.delete_permanent_title", "##PermanentDelete").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("%s", tr("world.delete_permanent_body").c_str());
                if (ImGui::Button(label("world.delete_permanently", "##ConfirmPermanent").c_str(),
                                  ImVec2(170.0f, 0.0f)))
                {
                    reportResult(management->permanentlyDeleteWorld(
                        pendingPermanentDeleteWorldId));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(label("common.cancel", "##CancelPermanent").c_str(),
                                  ImVec2(120.0f, 0.0f)))
                {
                    playUiFeedback();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal(label("world.restore_backup_title", "##BackupRestore").c_str(), nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("%s", tr("world.restore_backup_body").c_str());
                if (ImGui::Button(label("common.restore", "##ConfirmBackup").c_str(), ImVec2(120.0f, 0.0f)))
                {
                    reportResult(management->restoreBackup(
                        selectedWorldId, pendingBackupId));
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(label("common.cancel", "##CancelBackup").c_str(),
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
            ImGui::Text("%s %s...", tr("loading.world").c_str(),
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
        const std::string pauseTitle = label("pause.title", "##PauseMenu");
        if (ImGui::Begin(pauseTitle.c_str(), nullptr,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            if (world != nullptr)
            {
                const ObjectiveSnapshot objective =
                    world->getObjectiveSnapshot();
                ImGui::Text("%s  %zu / %zu", tr("pause.journey").c_str(),
                            objective.completedObjectives,
                            objective.totalObjectives);
                const std::string objectiveTitle = objective.sessionComplete
                    ? tr("objective.complete.title")
                    : objectiveText(objective.currentId, "title",
                                    objective.title);
                const std::string objectiveInstruction =
                    objective.sessionComplete
                        ? tr("objective.complete.instruction")
                        : objectiveText(objective.currentId, "instruction",
                                        objective.instruction);
                ImGui::TextUnformatted(objectiveTitle.c_str());
                ImGui::TextWrapped("%s", objectiveInstruction.c_str());
                if (!objective.completedTitles.empty() &&
                    ImGui::CollapsingHeader(
                        tr("pause.completed_objectives").c_str()))
                {
                    ImGui::BeginChild("##ObjectiveHistory",
                                      ImVec2(0.0f, 105.0f), true);
                    for (std::size_t index = 0;
                         index < objective.completedTitles.size(); ++index)
                    {
                        const std::string id =
                            index < objective.completedIds.size()
                                ? objective.completedIds[index]
                                : std::string();
                        const std::string title = objectiveText(
                            id, "title", objective.completedTitles[index]);
                        ImGui::Text("[x] %s", title.c_str());
                    }
                    ImGui::EndChild();
                }
                ImGui::Separator();
                const DifficultyRuntimeSnapshot difficulty =
                    world->getDifficultySnapshot();
                if (!difficultyDraftInitialized)
                {
                    pauseDifficulty = static_cast<int>(
                        difficulty.changePending ? difficulty.pending
                                                 : difficulty.active);
                    difficultyDraftInitialized = true;
                }
                ImGui::Text("%s: %s", tr("world.difficulty").c_str(),
                            difficultyName(difficulty.active).c_str());
                if (difficulty.changePending)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s %s)",
                        tr("pause.pending").c_str(),
                        difficultyName(difficulty.pending).c_str());
                }
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo(
                        "##PauseDifficulty",
                        difficultyName(static_cast<WorldDifficulty>(
                            pauseDifficulty)).c_str()))
                {
                    for (int value = 0;
                         value < static_cast<int>(WorldDifficulty::Count);
                         ++value)
                    {
                        const auto candidate =
                            static_cast<WorldDifficulty>(value);
                        const bool selected = value == pauseDifficulty;
                        const std::string candidateName =
                            difficultyName(candidate);
                        if (ImGui::Selectable(
                                candidateName.c_str(), selected))
                        {
                            pauseDifficulty = value;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::Button(label("pause.apply_difficulty", "##ApplyDifficulty").c_str(),
                                  ImVec2(-1.0f, 32.0f)))
                {
                    pendingAction.type =
                        OgreUserInterfaceActionType::ApplyDifficulty;
                    pendingAction.difficulty =
                        static_cast<WorldDifficulty>(pauseDifficulty);
                    playUiFeedback();
                }
                ImGui::TextDisabled("%s",
                    tr("pause.difficulty_pending").c_str());
                ImGui::Separator();
            }
            if (ImGui::Button(label("pause.resume", "##Resume").c_str(), ImVec2(-1.0f, 38.0f)))
            {
                if (flow->resume())
                {
                    playUiFeedback();
                }
            }
            if (ImGui::Button(label("pause.settings", "##Settings").c_str(), ImVec2(-1.0f, 38.0f)))
            {
                settingsSession.begin(appliedSettings);
                settingsMessage.clear();
                settingsApplyPending = false;
                playUiFeedback();
            }
            if (ImGui::Button(label("pause.save_main", "##SaveMain").c_str(),
                              ImVec2(-1.0f, 38.0f)))
            {
                pendingAction.type =
                    OgreUserInterfaceActionType::ReturnToMainMenu;
                playUiFeedback();
            }
            if (ImGui::Button(label("pause.save_quit", "##SaveQuit").c_str(), ImVec2(-1.0f, 38.0f)))
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
        const std::string settingsTitle =
            label("settings.title", "##PausedSettings");
        if (ImGui::Begin(settingsTitle.c_str(), nullptr,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            UserSettings &draft = settingsSession.draft();
            ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, -58.0f),
                              false);
            int windowSize[2] = {draft.windowX, draft.windowY};
            if (ImGui::InputInt2(label("settings.window_size", "##WindowSize").c_str(), windowSize))
            {
                draft.windowX = windowSize[0];
                draft.windowY = windowSize[1];
            }
            ImGui::Checkbox(label("settings.fullscreen", "##Fullscreen").c_str(), &draft.isFullscreen);
            ImGui::SliderInt(label("settings.render_distance", "##RenderDistance").c_str(), &draft.renderDistance,
                             1, 32);
            const char* shadowPreviewKey =
                draft.directionalShadowQuality ==
                        DirectionalShadowQuality::High
                    ? "settings.shadow_high"
                    : draft.directionalShadowQuality ==
                              DirectionalShadowQuality::Medium
                          ? "settings.shadow_medium"
                          : "settings.shadow_off";
            if (ImGui::BeginCombo(
                    label("settings.shadow_quality",
                          "##DirectionalShadowQuality").c_str(),
                    tr(shadowPreviewKey).c_str()))
            {
                const DirectionalShadowQuality qualities[] = {
                    DirectionalShadowQuality::Off,
                    DirectionalShadowQuality::Medium,
                    DirectionalShadowQuality::High};
                const char* keys[] = {
                    "settings.shadow_off", "settings.shadow_medium",
                    "settings.shadow_high"};
                for (int index = 0; index < 3; ++index)
                {
                    const bool selected =
                        draft.directionalShadowQuality ==
                        qualities[index];
                    const std::string option = tr(keys[index]);
                    if (ImGui::Selectable(option.c_str(), selected))
                    {
                        draft.directionalShadowQuality =
                            qualities[index];
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            bool postProcessingEnabled =
                draft.postProcessingQuality == PostProcessingQuality::On;
            if (ImGui::Checkbox(
                    label("settings.post_processing",
                          "##PostProcessing").c_str(),
                    &postProcessingEnabled))
            {
                draft.postProcessingQuality = postProcessingEnabled
                    ? PostProcessingQuality::On
                    : PostProcessingQuality::Off;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s",
                    tr("settings.post_processing_help").c_str());
            }
            ImGui::SliderInt(label("settings.fov", "##Fov").c_str(), &draft.fov, 45, 120);
            ImGui::SliderFloat(label("settings.mouse_sensitivity", "##MouseSensitivity").c_str(),
                               &draft.mouseSensitivity, 0.005f, 1.0f,
                               "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::Checkbox(label("settings.invert_mouse_y", "##InvertMouseY").c_str(), &draft.invertMouseY);
            const auto drawHoldMode = [&](const char *translationKey,
                                          const char *widgetId,
                                          GameplayHoldMode &mode)
            {
                const char *previewKey =
                    mode == GameplayHoldMode::Toggle
                        ? "settings.mode_toggle"
                        : "settings.mode_hold";
                if (ImGui::BeginCombo(
                        label(translationKey, widgetId).c_str(),
                        tr(previewKey, gameplayHoldModeName(mode)).c_str()))
                {
                    for (GameplayHoldMode candidate : {
                             GameplayHoldMode::Hold,
                             GameplayHoldMode::Toggle})
                    {
                        const bool selected = candidate == mode;
                        const char *candidateKey =
                            candidate == GameplayHoldMode::Toggle
                                ? "settings.mode_toggle"
                                : "settings.mode_hold";
                        const std::string option = tr(
                            candidateKey, gameplayHoldModeName(candidate));
                        if (ImGui::Selectable(option.c_str(), selected))
                        {
                            mode = candidate;
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            };
            drawHoldMode("settings.sprint_mode", "##SprintMode",
                         draft.sprintMode);
            drawHoldMode("settings.sneak_mode", "##SneakMode",
                         draft.sneakMode);
            const std::string feedbackPreview = tr(
                draft.feedbackIntensity == GameplayFeedbackIntensity::Off
                    ? "settings.feedback_off"
                    : (draft.feedbackIntensity ==
                               GameplayFeedbackIntensity::Reduced
                           ? "settings.feedback_reduced"
                           : "settings.feedback_full"),
                gameplayFeedbackIntensityName(draft.feedbackIntensity));
            if (ImGui::BeginCombo(
                    label("settings.feedback_intensity",
                          "##FeedbackIntensity").c_str(),
                    feedbackPreview.c_str()))
            {
                for (GameplayFeedbackIntensity candidate : {
                         GameplayFeedbackIntensity::Off,
                         GameplayFeedbackIntensity::Reduced,
                         GameplayFeedbackIntensity::Full})
                {
                    const bool selected =
                        candidate == draft.feedbackIntensity;
                    const char *candidateKey =
                        candidate == GameplayFeedbackIntensity::Off
                            ? "settings.feedback_off"
                            : (candidate == GameplayFeedbackIntensity::Reduced
                                   ? "settings.feedback_reduced"
                                   : "settings.feedback_full");
                    const std::string option = tr(
                        candidateKey,
                        gameplayFeedbackIntensityName(candidate));
                    if (ImGui::Selectable(option.c_str(), selected))
                    {
                        draft.feedbackIntensity = candidate;
                    }
                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            const std::string languagePreview = draft.locale == "zh-CN"
                ? tr("language.zh-cn") : tr("language.en-us");
            if (ImGui::BeginCombo(label("settings.language", "##Language").c_str(),
                                  languagePreview.c_str()))
            {
                const char* locales[] = {"en-US", "zh-CN"};
                const char* keys[] = {"language.en-us", "language.zh-cn"};
                for (int index = 0; index < 2; ++index)
                {
                    const bool selected = draft.locale == locales[index];
                    const std::string option = tr(keys[index]);
                    if (ImGui::Selectable(option.c_str(), selected))
                    {
                        draft.locale = locales[index];
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::SliderFloat(label("settings.ui_scale", "##UiScale").c_str(), &draft.uiScale, 0.75f, 1.75f,
                               "%.2fx");
            ImGui::Checkbox(label("settings.show_action_hints", "##ActionHints").c_str(), &draft.showActionHints);
            ImGui::SeparatorText(tr("settings.audio").c_str());
            ImGui::SliderFloat(label("settings.master_volume", "##MasterVolume").c_str(), &draft.masterVolume, 0.0f, 1.0f);
            ImGui::SliderFloat(label("settings.ui_volume", "##UiVolume").c_str(), &draft.uiVolume, 0.0f, 1.0f);
            ImGui::SliderFloat(label("settings.effects_volume", "##EffectsVolume").c_str(), &draft.effectsVolume,
                               0.0f, 1.0f);
            ImGui::SliderFloat(label("settings.ambient_volume", "##AmbientVolume").c_str(), &draft.ambientVolume,
                               0.0f, 1.0f);
            ImGui::SliderFloat(label("settings.music_volume", "##MusicVolume").c_str(), &draft.musicVolume,
                               0.0f, 1.0f);
            ImGui::Checkbox(label("settings.audio_captions", "##AudioCaptions").c_str(), &draft.audioCaptions);
            ImGui::SeparatorText(tr("settings.controls").c_str());
            for (std::size_t actionIndex = 0;
                 actionIndex < GameplayActionCount; ++actionIndex)
            {
                const auto action =
                    static_cast<GameplayAction>(actionIndex);
                const GameplayKey current = draft.inputBindings.get(action);
                const std::string bindingLabel =
                    actionName(action) +
                    "##binding-" + std::to_string(actionIndex);
                if (ImGui::BeginCombo(bindingLabel.c_str(),
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
            for (std::size_t actionIndex = 0;
                 actionIndex < GameplayWorldActionCount; ++actionIndex)
            {
                const auto action =
                    static_cast<GameplayWorldAction>(actionIndex);
                const GameplayMouseButton current =
                    draft.mouseBindings.get(action);
                const std::string bindingLabel =
                    worldActionName(action) + "##mouse-binding-" +
                    std::to_string(actionIndex);
                if (ImGui::BeginCombo(
                        bindingLabel.c_str(),
                        gameplayMouseButtonName(current)))
                {
                    for (std::size_t buttonIndex = 0;
                         buttonIndex < GameplayMouseButtonCount;
                         ++buttonIndex)
                    {
                        const auto button =
                            static_cast<GameplayMouseButton>(buttonIndex);
                        const bool selected = button == current;
                        if (ImGui::Selectable(
                                gameplayMouseButtonName(button), selected))
                        {
                            draft.mouseBindings.set(action, button);
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            const std::string mouseSharing =
                describeGameplayMouseBindingSharing(draft.mouseBindings);
            if (!mouseSharing.empty())
            {
                ImGui::TextWrapped(
                    "%s: %s",
                    tr("settings.context_binding", "Context binding").c_str(),
                    mouseSharing.c_str());
            }
            ImGui::TextWrapped("%s", tr("settings.apply_note").c_str());
            if (!settingsMessage.empty())
            {
                ImGui::TextWrapped("%s", settingsMessage.c_str());
            }
            ImGui::EndChild();

            ImGui::BeginDisabled(settingsApplyPending);
            if (ImGui::Button(label("common.apply", "##ApplySettings").c_str(), ImVec2(140.0f, 38.0f)))
            {
                RuntimeSettingsApplyPlan plan;
                if (settingsSession.prepareApply(plan, settingsMessage))
                {
                    pendingAction.type =
                        OgreUserInterfaceActionType::ApplySettings;
                    pendingAction.settings = plan.settings;
                    settingsMessage = tr("settings.saving");
                    settingsApplyPending = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(label("common.cancel", "##CancelSettings").c_str(), ImVec2(140.0f, 38.0f)))
            {
                settingsSession.cancel();
                settingsMessage.clear();
                playUiFeedback();
            }
            ImGui::SameLine();
            if (ImGui::Button(label("common.defaults", "##DefaultSettings").c_str(), ImVec2(140.0f, 38.0f)))
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
        const bool restartRequired =
            appliedSettings.windowX != settings.windowX ||
            appliedSettings.windowY != settings.windowY ||
            appliedSettings.isFullscreen != settings.isFullscreen;
        appliedSettings = settings;
        settingsMessage = message.empty()
            ? tr(restartRequired
                     ? "settings.saved_restart"
                     : "settings.saved")
            : tr(message, message);
        ImGui::GetIO().FontGlobalScale = appliedSettings.uiScale;
        if (!appliedSettings.audioCaptions)
        {
            captionTimeline.clear();
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

    void setAudioCaption(std::string cueId, std::string caption)
    {
        if (cueId == "block.break")
        {
            interactionFeedbackColour = ImVec4(0.95f, 0.72f, 0.28f, 1.f);
            interactionFeedbackSeconds = 0.32f;
        }
        else if (cueId == "block.place")
        {
            interactionFeedbackColour = ImVec4(0.35f, 0.72f, 1.f, 1.f);
            interactionFeedbackSeconds = 0.28f;
        }
        else if (cueId == "item.pickup" || cueId == "craft.success")
        {
            interactionFeedbackColour = ImVec4(0.42f, 0.94f, 0.48f, 1.f);
            interactionFeedbackSeconds = 0.34f;
        }
        else if (cueId == "combat.hit")
        {
            interactionFeedbackColour = ImVec4(1.f, 0.34f, 0.28f, 1.f);
            interactionFeedbackSeconds = 0.28f;
        }
        if (!appliedSettings.audioCaptions)
        {
            return;
        }
        captionTimeline.submit(std::move(cueId), std::move(caption));
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
        const TerrainMaterialParameters& terrainMaterial =
            runtimeTerrainMaterialProfile().parameters();
        if (!coordinate.available() ||
            !terrainMaterial.containsTile(coordinate.x, coordinate.y))
        {
            return false;
        }
        const float atlasSize =
            static_cast<float>(terrainMaterial.atlasPixels);
        const float tileSize =
            static_cast<float>(terrainMaterial.tilePixels);
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

        const float actionFade = std::clamp(
            actionFeedback.secondsRemaining / 0.38f, 0.f, 1.f);
        // Keep the held-item contact pose for the short presentation-only
        // hit-stop window. Simulation and attack resolution have already
        // advanced before this snapshot reaches the renderer.
        const float feedbackPose = actionFeedback.hitStopSeconds > 0.f
            ? 1.f
            : actionFade;
        const float feedbackKick =
            (interactionFeedbackSeconds > 0.f
                 ? interactionFeedbackSeconds * 20.f
                 : 0.f) +
            actionFeedback.recoil * feedbackPose * 18.f;
        const float miningSwing = miningProgress.active
            ? std::sin(hudElapsedSeconds * 12.f) *
                  (0.08f + miningProgress.normalized() * 0.08f)
            : 0.f;
        const float bob = std::sin(hudElapsedSeconds * 2.1f) * 2.5f -
                          feedbackKick;
        const float angle = -0.12f +
                            std::sin(hudElapsedSeconds * 1.4f) * 0.025f +
                            miningSwing +
                            actionFeedback.recoil * feedbackPose * 0.10f;
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
            ImGui::Text("%s  %.2f ms", tr("hud.frame").c_str(),
                        displayedFrameMs);
#if defined(_DEBUG)
            ImGui::TextDisabled("Debug | %s %.2f ms",
                                tr("hud.debug_peak").c_str(),
                                displayedPeakFrameMs);
#else
            ImGui::TextDisabled("Release | %s %.2f ms",
                                tr("hud.debug_peak").c_str(),
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
            const ItemStack &heldItem = player->getHeldItems();
            int attackCooldownTotal = World::PlayerAttackCooldownTicks;
            if (!heldItem.isEmpty())
            {
                const ToolDefinition *tool =
                    runtimeToolRegistry().find(
                        heldItem.getMaterial().id);
                if (tool != nullptr)
                {
                    attackCooldownTotal = tool->attackCooldownTicks;
                }
            }
            if (worldStats.attackCooldownTicksRemaining > 0 &&
                attackCooldownTotal > 0)
            {
                constexpr float pi = 3.14159265358979323846f;
                const float readiness = 1.f - std::clamp(
                    static_cast<float>(
                        worldStats.attackCooldownTicksRemaining) /
                        static_cast<float>(attackCooldownTotal),
                    0.f, 1.f);
                foreground->AddCircle(center, 21.f,
                                      IM_COL32(18, 22, 28, 190), 36, 2.f);
                foreground->PathArcTo(
                    center, 21.f, -pi * 0.5f,
                    -pi * 0.5f + pi * 2.f * readiness, 36);
                foreground->PathStroke(IM_COL32(104, 215, 255, 245),
                                       0, 2.f);
            }
            for (const ActionFeedbackParticle &particle :
                 actionFeedback.particles)
            {
                const ImVec2 particleCenter(
                    center.x + particle.offsetX,
                    center.y + particle.offsetY);
                const float half = particle.size * 0.5f;
                const ImVec2 minimum(particleCenter.x - half,
                                     particleCenter.y - half);
                const ImVec2 maximum(particleCenter.x + half,
                                     particleCenter.y + half);
                const ImU32 tint = IM_COL32(
                    255, 255, 255,
                    static_cast<int>(std::clamp(
                        particle.alpha, 0.f, 1.f) * 255.f));
                if (!drawMaterialIcon(foreground, particle.materialId,
                                      minimum, maximum, tint))
                {
                    foreground->AddRectFilled(
                        minimum, maximum, tint, 1.f);
                }
            }
            if (actionFeedback.kind == ActionFeedbackKind::AttackMiss)
            {
                const float fade = std::clamp(
                    actionFeedback.secondsRemaining / 0.18f, 0.f, 1.f);
                foreground->AddCircle(
                    center, 18.f + (1.f - fade) * 5.f,
                    IM_COL32(190, 198, 210,
                             static_cast<int>(fade * 210.f)),
                    28, 2.f);
            }
            else if (actionFeedback.kind == ActionFeedbackKind::AttackHit ||
                     actionFeedback.kind == ActionFeedbackKind::Guard)
            {
                const bool guarded =
                    actionFeedback.kind == ActionFeedbackKind::Guard;
                const ImU32 markerColour = guarded
                    ? IM_COL32(80, 220, 235, 235)
                    : IM_COL32(255, 78, 66, 240);
                const float inner = 12.f;
                const float outer = 19.f;
                foreground->AddLine(
                    ImVec2(center.x - outer, center.y - outer),
                    ImVec2(center.x - inner, center.y - inner),
                    markerColour, 3.f);
                foreground->AddLine(
                    ImVec2(center.x + outer, center.y - outer),
                    ImVec2(center.x + inner, center.y - inner),
                    markerColour, 3.f);
                foreground->AddLine(
                    ImVec2(center.x + outer, center.y + outer),
                    ImVec2(center.x + inner, center.y + inner),
                    markerColour, 3.f);
                foreground->AddLine(
                    ImVec2(center.x - outer, center.y + outer),
                    ImVec2(center.x - inner, center.y + inner),
                    markerColour, 3.f);
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

        const float healthRatioForWarning =
            worldStats.playerMaxHealth > 0.f
                ? std::clamp(worldStats.playerHealth /
                                 worldStats.playerMaxHealth,
                             0.f, 1.f)
                : 0.f;
        if (healthRatioForWarning > 0.f &&
            healthRatioForWarning <= 0.30f &&
            appliedSettings.feedbackIntensity !=
                GameplayFeedbackIntensity::Off)
        {
            const float intensity =
                appliedSettings.feedbackIntensity ==
                        GameplayFeedbackIntensity::Reduced
                    ? 0.35f
                    : 0.65f;
            const float pulse = 0.75f +
                std::sin(hudElapsedSeconds * 5.f) * 0.25f;
            const ImU32 warning = IM_COL32(
                220, 35, 28,
                static_cast<int>(255.f * intensity * pulse));
            ImDrawList *foreground = ImGui::GetForegroundDrawList();
            foreground->AddRect(
                ImVec2(3.f, 3.f),
                ImVec2(io.DisplaySize.x - 3.f,
                       io.DisplaySize.y - 3.f),
                warning, 0.f, 0, 5.f);
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
                ImGui::Text("%s  %zu / %zu", tr("hud.journey").c_str(),
                            objective.completedObjectives,
                            objective.totalObjectives);
                ImGui::Separator();
                const std::string currentTitle = objective.sessionComplete
                    ? tr("objective.complete.title")
                    : objectiveText(objective.currentId, "title",
                                    objective.title);
                const std::string currentInstruction =
                    objective.sessionComplete
                        ? tr("objective.complete.instruction")
                        : objectiveText(objective.currentId, "instruction",
                                        objective.instruction);
                ImGui::TextUnformatted(currentTitle.c_str());
                ImGui::TextWrapped("%s", currentInstruction.c_str());
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
                if (objective.opportunities.size() > 1 &&
                    !objective.sessionComplete)
                {
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", tr("hud.opportunities").c_str());
                    for (std::size_t index = 1;
                         index < objective.opportunities.size(); ++index)
                    {
                        const ObjectiveOpportunitySnapshot &opportunity =
                            objective.opportunities[index];
                        const std::string track = tr(
                            "objective.track." + opportunity.track);
                        const std::string opportunityTitle = objectiveText(
                            opportunity.id, "title", opportunity.title);
                        const std::string opportunityInstruction =
                            objectiveText(opportunity.id, "instruction",
                                          opportunity.instruction);
                        ImGui::TextDisabled("[%s] %s", track.c_str(),
                                            opportunityTitle.c_str());
                        ImGui::TextWrapped("  %s",
                                           opportunityInstruction.c_str());
                    }
                }
                if (!objective.completionFeedback.empty())
                {
                    const std::string feedback = objectiveText(
                        objective.completionFeedbackId, "feedback",
                        objective.completionFeedback);
                    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.45f, 1.0f),
                                       "%s", feedback.c_str());
                }
                const ExplorationRewardSnapshot explorationReward =
                    world->getExplorationRewardSnapshot();
                if (explorationReward.ancientCompassHeld ||
                    explorationReward.raiderWardCarried)
                {
                    ImGui::Separator();
                    if (explorationReward.ancientCompassHeld)
                    {
                        const std::string direction =
                            explorationReward.homeDirection == "HERE"
                                ? tr("hud.compass_here")
                                : explorationReward.homeDirection;
                        ImGui::Text("%s: %s  %.0f m",
                                    tr("hud.compass_home").c_str(),
                                    direction.c_str(),
                                    explorationReward.homeDistance);
                    }
                    if (explorationReward.raiderWardCarried)
                    {
                        ImGui::TextDisabled(
                            "%s", tr("hud.raider_ward").c_str());
                    }
                }
                const WorldOutcomeSnapshot outcome =
                    world->getWorldOutcomeSnapshot();
                if (outcome.phase ==
                    WorldOutcomePhase::RewardClaimed)
                {
                    const PostVictoryEventSnapshot replay =
                        world->getPostVictoryEventSnapshot();
                    ImGui::Separator();
                    const std::string replayTitle =
                        tr("post_victory.hud.title");
                    ImGui::Text("%s  %d / %d", replayTitle.c_str(),
                                replay.completedEvents,
                                replay.totalEvents);
                    const char *instructionKey = replay.complete
                        ? "post_victory.hud.complete"
                        : (replay.rewardPending
                            ? "post_victory.hud.reward_pending"
                            : (replay.activeEvent > 0
                                ? "post_victory.hud.active"
                                : "post_victory.hud.available"));
                    const std::string replayInstruction = tr(instructionKey);
                    ImGui::TextWrapped("%s",
                                       replayInstruction.c_str());
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
        if (worldStats.combatFeedback.kind !=
                PlayerCombatFeedbackKind::None &&
            worldStats.combatFeedback.ticksRemaining > 0)
        {
            const ImVec2 feedbackCenter(io.DisplaySize.x * 0.5f,
                                        io.DisplaySize.y * 0.5f);
            const ImU32 colour = worldStats.combatFeedback.kind ==
                    PlayerCombatFeedbackKind::Guard
                ? IM_COL32(80, 220, 235, 225)
                : IM_COL32(245, 72, 64, 225);
            ImVec2 tip = feedbackCenter;
            ImVec2 left = feedbackCenter;
            ImVec2 right = feedbackCenter;
            switch (worldStats.combatFeedback.direction)
            {
                case CombatDirection::Front:
                    tip = ImVec2(feedbackCenter.x,
                                 feedbackCenter.y - 40.0f);
                    left = ImVec2(feedbackCenter.x - 9.0f,
                                  feedbackCenter.y - 58.0f);
                    right = ImVec2(feedbackCenter.x + 9.0f,
                                   feedbackCenter.y - 58.0f);
                    break;
                case CombatDirection::Right:
                    tip = ImVec2(feedbackCenter.x + 40.0f,
                                 feedbackCenter.y);
                    left = ImVec2(feedbackCenter.x + 58.0f,
                                  feedbackCenter.y - 9.0f);
                    right = ImVec2(feedbackCenter.x + 58.0f,
                                   feedbackCenter.y + 9.0f);
                    break;
                case CombatDirection::Back:
                    tip = ImVec2(feedbackCenter.x,
                                 feedbackCenter.y + 40.0f);
                    left = ImVec2(feedbackCenter.x - 9.0f,
                                  feedbackCenter.y + 58.0f);
                    right = ImVec2(feedbackCenter.x + 9.0f,
                                   feedbackCenter.y + 58.0f);
                    break;
                case CombatDirection::Left:
                    tip = ImVec2(feedbackCenter.x - 40.0f,
                                 feedbackCenter.y);
                    left = ImVec2(feedbackCenter.x - 58.0f,
                                  feedbackCenter.y - 9.0f);
                    right = ImVec2(feedbackCenter.x - 58.0f,
                                   feedbackCenter.y + 9.0f);
                    break;
                case CombatDirection::None:
                    tip = ImVec2(feedbackCenter.x,
                                 feedbackCenter.y - 40.0f);
                    left = ImVec2(feedbackCenter.x - 9.0f,
                                  feedbackCenter.y - 58.0f);
                    right = ImVec2(feedbackCenter.x + 9.0f,
                                   feedbackCenter.y - 58.0f);
                    break;
            }
            ImGui::GetForegroundDrawList()->AddTriangleFilled(
                tip, left, right, colour);
        }
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
                ImGui::Text("%s  %s",
                            gameplayKeyName(appliedSettings.inputBindings.get(
                                GameplayAction::OpenCrafting)),
                            tr("hint.crafting").c_str());
                ImGui::Text("%s  %s",
                            gameplayKeyName(appliedSettings.inputBindings.get(
                                 GameplayAction::ConsumeFood)),
                            tr("hint.eat").c_str());
                ImGui::Text(
                    "%s  %s",
                    gameplayMouseButtonName(
                        appliedSettings.mouseBindings.get(
                            GameplayWorldAction::Guard)),
                    tr("hint.guard").c_str());
                ImGui::Text("Esc  %s", tr("hint.pause").c_str());
            }
            ImGui::End();
        }
        const PresentationCaptionSnapshot caption =
            captionTimeline.snapshot();
        if (caption.visible())
        {
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f,
                       io.DisplaySize.y - 105.0f),
                ImGuiCond_Always, ImVec2(0.5f, 1.0f));
            ImGui::SetNextWindowBgAlpha(0.82f);
            if (ImGui::Begin("##AudioCaption", nullptr, overlayFlags))
            {
                const std::string localizedCaption =
                    LocalizedPresentation::audioCaption(
                        appliedSettings.locale, caption.cueId,
                        caption.fallback);
                ImGui::Text("[%s] %s", tr("caption.prefix").c_str(),
                            localizedCaption.c_str());
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
            ImGui::Text("%s %.0f / %.0f", tr("hud.health").c_str(),
                        std::ceil(worldStats.playerHealth),
                        std::ceil(worldStats.playerMaxHealth));
            ImGui::ProgressBar(healthRatio, ImVec2(-1.0f, 8.0f), "");
            if (worldStats.foodCooldownTicksRemaining > 0)
            {
                ImGui::Text("%s: %.1fs", tr("hud.food_cooldown").c_str(),
                            worldStats.foodCooldownTicksRemaining / 20.f);
            }
            if (worldStats.attackCooldownTicksRemaining > 0)
            {
                ImGui::Text("%s: %.1fs", tr("hud.attack_ready").c_str(),
                             worldStats.attackCooldownTicksRemaining / 20.f);
            }
            if (worldStats.combatFeedback.guarding)
            {
                ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.95f, 1.0f),
                                   "%s", tr("hud.guarding").c_str());
            }
            else if (worldStats.combatFeedback.guardRecoverTicksRemaining > 0)
            {
                ImGui::Text("%s: %.1fs", tr("hud.guard_ready").c_str(),
                            worldStats.combatFeedback.guardRecoverTicksRemaining /
                                20.f);
            }
            const bool heldItemValid = state.heldItem >= 0 &&
                state.heldItem < static_cast<int>(state.inventory.size());
            if (heldItemValid)
            {
                const InventorySlotState &held =
                    state.inventory[static_cast<std::size_t>(state.heldItem)];
                if (held.amount > 0)
                {
                    const std::string heldName = materialName(held.materialId);
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

    bool victoryOverlayVisible() const
    {
        if (world == nullptr || flow == nullptr ||
            flow->state() != GameApplicationState::Playing)
        {
            return false;
        }
        const WorldOutcomeSnapshot outcome =
            world->getWorldOutcomeSnapshot();
        return outcome.victory && outcome.rewardEpoch > 0 &&
               dismissedVictoryEpoch != outcome.rewardEpoch;
    }

    void drawVictoryOverlay()
    {
        if (!victoryOverlayVisible())
        {
            return;
        }
        const WorldOutcomeSnapshot outcome =
            world->getWorldOutcomeSnapshot();
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f,
                   io.DisplaySize.y * 0.42f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f),
                                 ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.96f);
        if (ImGui::Begin(
                "##VictoryOverlay", nullptr,
                ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_AlwaysAutoResize))
        {
            const std::string title = tr("victory.overlay.title");
            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextUnformatted(title.c_str());
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Separator();
            const std::string summary = tr("victory.overlay.summary");
            ImGui::TextWrapped("%s", summary.c_str());
            const std::string rewardMessage = tr(
                outcome.rewardAvailable ? "victory.reward.pending"
                                        : "victory.reward.claimed");
            ImGui::TextWrapped("%s", rewardMessage.c_str());
            if (outcome.rewardAvailable)
            {
                const std::string claim = tr("victory.reward.claim");
                if (ImGui::Button(claim.c_str(), ImVec2(220.0f, 42.0f)))
                {
                    pendingAction.type =
                        OgreUserInterfaceActionType::ClaimVictoryReward;
                    playUiFeedback();
                }
                ImGui::SameLine();
            }
            const std::string continueLabel =
                tr("victory.overlay.continue");
            if (ImGui::Button(continueLabel.c_str(),
                              ImVec2(220.0f, 42.0f)))
            {
                dismissedVictoryEpoch = outcome.rewardEpoch;
                playUiFeedback();
            }
        }
        ImGui::End();
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
                const std::string furnaceTitle =
                    label("furnace.title", "##Furnace");
                if (ImGui::Begin(furnaceTitle.c_str(), &open, flags))
                {
                    const FurnaceSlot furnaceSlots[] = {
                        FurnaceSlot::Input, FurnaceSlot::Fuel,
                        FurnaceSlot::Output};
                    const std::string slotNames[] = {
                        tr("furnace.input"), tr("furnace.fuel"),
                        tr("furnace.output")};
                    const InventorySlotState stacks[] = {
                        furnace->state.input, furnace->state.fuel,
                        furnace->state.output};
                    for (int index = 0; index < 3; ++index)
                    {
                        if (index > 0) ImGui::SameLine();
                        const Material &material =
                            Material::toMaterial(stacks[index].materialId);
                        const std::string label =
                            slotNames[index] + "\n" +
                            (stacks[index].amount > 0
                                 ? materialName(material.id)
                                 : tr("common.empty")) +
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
                    ImGui::TextUnformatted(tr("furnace.progress").c_str());
                    ImGui::ProgressBar(smeltProgress,
                                       ImVec2(-1.0f, 0.0f));
                    ImGui::TextUnformatted(
                        tr("furnace.fuel_remaining").c_str());
                    ImGui::ProgressBar(fuelProgress,
                                       ImVec2(-1.0f, 0.0f));
                    ImGui::Separator();
                    ImGui::TextWrapped("%s",
                        tr("furnace.inventory_hint").c_str());
                    for (int playerSlot = 0;
                         playerSlot < player->getInventorySlotCount();
                         ++playerSlot)
                    {
                        if (playerSlot > 0) ImGui::SameLine();
                        const ItemStack &stack =
                            player->getInventorySlot(playerSlot);
                        const std::string label =
                            (stack.isEmpty()
                                 ? tr("common.empty")
                                 : materialName(stack.getMaterial().id)) +
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
                    if (ImGui::Button(label("common.close", "##CloseFurnace").c_str(), ImVec2(100.0f, 32.0f)))
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
        const std::string chestTitle = label("chest.title", "##Chest");
        if (ImGui::Begin(chestTitle.c_str(), &open, flags))
        {
            ImGui::TextWrapped("%s", tr("chest.slots_hint").c_str());
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
                    (stack.amount > 0 ? materialName(material.id)
                                      : tr("common.empty")) + " x" +
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
            ImGui::TextWrapped("%s", tr("chest.hotbar_hint").c_str());
            for (int slot = 0; slot < player->getInventorySlotCount(); ++slot)
            {
                if (slot > 0)
                {
                    ImGui::SameLine();
                }
                const ItemStack &stack = player->getInventorySlot(slot);
                const std::string label =
                    (stack.isEmpty() ? tr("common.empty")
                                     : materialName(stack.getMaterial().id)) +
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
            ImGui::TextWrapped("%s", tr("container.close_hint").c_str());
            if (ImGui::Button(label("common.close", "##CloseChest").c_str(), ImVec2(100.0f, 32.0f)))
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
        const std::string title =
            gridSize == CraftingSession::WorkbenchGridSize
                ? label("crafting.workbench_title", "##Crafting")
                : label("crafting.player_title", "##Crafting");
        if (ImGui::Begin(title.c_str(), &open,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::TextWrapped("%s", tr("crafting.choose_hint").c_str());
            ImGui::TextWrapped("%s", tr("crafting.grid_hint").c_str());
            if (ImGui::CollapsingHeader(tr("crafting.recipe_book").c_str(),
                                        ImGuiTreeNodeFlags_DefaultOpen))
            {
                std::size_t eligibleRecipes = 0;
                std::size_t learnedRecipes = 0;
                for (const RecipeDefinition &recipe :
                     runtimeRecipeRegistry().recipes())
                {
                    if (recipeFitsGrid(recipe, gridSize))
                    {
                        ++eligibleRecipes;
                        if (world != nullptr &&
                            world->isRecipeDiscovered(recipe.id))
                        {
                            ++learnedRecipes;
                        }
                    }
                }
                ImGui::Text("%s: %zu / %zu",
                            tr("crafting.recipe_book_progress").c_str(),
                            learnedRecipes, eligibleRecipes);
                ImGui::BeginChild("##RecipeBook", ImVec2(0.0f, 135.0f),
                                  true);
                for (const RecipeDefinition &recipe :
                     runtimeRecipeRegistry().recipes())
                {
                    if (!recipeFitsGrid(recipe, gridSize) ||
                        world == nullptr ||
                        !world->isRecipeDiscovered(recipe.id))
                    {
                        continue;
                    }
                    const std::string button =
                        tr("crafting.load") + "##recipe-" + recipe.id;
                    if (ImGui::SmallButton(button.c_str()))
                    {
                        if (craftingSession->loadRecipe(recipe))
                        {
                            craftingMessage =
                                materialName(recipe.outputMaterialId) + ": " +
                                tr("crafting.loaded");
                            playUiFeedback();
                        }
                    }
                    ImGui::SameLine();
                    const std::string ingredients =
                        recipeIngredientSummary(recipe,
                                                appliedSettings.locale);
                    const std::string outputName =
                        materialName(recipe.outputMaterialId);
                    ImGui::Text("%s x%d  <-  %s", outputName.c_str(),
                                recipe.outputCount, ingredients.c_str());
                }
                if (learnedRecipes == 0)
                {
                    ImGui::TextWrapped(
                        "%s", tr("crafting.recipe_book_hint").c_str());
                }
                ImGui::EndChild();
            }
            ImGui::Separator();

            const PlayerSaveState state = player->getSaveState();
            ImGui::TextUnformatted(tr("crafting.inventory").c_str());
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
                    (slot.amount > 0 ? materialName(material.id)
                                     : tr("common.empty")) + " x" +
                    std::to_string(slot.amount) + "##craft-source-" +
                    std::to_string(index);
                if (ImGui::Button(label.c_str(), ImVec2(122.0f, 46.0f)) &&
                    slot.amount > 0)
                {
                    selectedCraftingMaterial = slot.materialId;
                }
            }
            ImGui::Text("%s: %s", tr("crafting.selected").c_str(),
                        materialName(selectedCraftingMaterial).c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton(label("crafting.clear_selection", "##ClearSelection").c_str()))
            {
                selectedCraftingMaterial = Material::ID::Nothing;
            }

            ImGui::Separator();
            ImGui::Text("%dx%d %s", gridSize, gridSize,
                        tr("crafting.input_grid").c_str());
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
                    (cell.amount > 0 ? materialName(material.id)
                                     : tr("common.empty")) +
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
            if (ImGui::Button(label("crafting.clear_grid", "##ClearGrid").c_str()))
            {
                craftingSession->clear();
            }

            const CraftingPreview preview = player->previewCrafting(
                *craftingSession, runtimeRecipeRegistry());
            ImGui::Separator();
            if (!preview.recipeId.empty())
            {
                ImGui::Text("%s: %s", tr("crafting.recipe").c_str(),
                            preview.recipeId.c_str());
                ImGui::Text("%s: %s x%d | %s: %d",
                            tr("crafting.output").c_str(),
                            materialName(preview.outputMaterialId).c_str(),
                            preview.outputCount,
                            tr("crafting.maximum_crafts").c_str(),
                            preview.maxCrafts);
            }
            ImGui::TextWrapped("%s",
                               craftingPreviewMessage(preview.status).c_str());
            ImGui::BeginDisabled(!preview.ready());
            if (ImGui::Button(label("crafting.craft_one", "##CraftOne").c_str(), ImVec2(150.0f, 38.0f)))
            {
                const CraftingCommitResult committed =
                    player->commitCrafting(
                        *craftingSession, runtimeRecipeRegistry(), preview,
                        1);
                craftingMessage = craftingCommitMessage(committed.status);
            }
            ImGui::SameLine();
            if (ImGui::Button(label("crafting.craft_maximum", "##CraftMaximum").c_str(), ImVec2(170.0f, 38.0f)))
            {
                const CraftingCommitResult committed =
                    player->commitCrafting(
                        *craftingSession, runtimeRecipeRegistry(), preview,
                        preview.maxCrafts);
                craftingMessage = craftingCommitMessage(committed.status);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button(label("common.close", "##CloseCrafting").c_str(), ImVec2(110.0f, 38.0f)))
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
        const std::string playerDiagnostics =
            label("diagnostics.player_title", "##PlayerDiagnostics");
        if (ImGui::Begin(playerDiagnostics.c_str()))
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
            ImGui::Text("Guard: %s recover=%d",
                        worldStats.combatFeedback.guarding ? "active" : "off",
                        worldStats.combatFeedback.guardRecoverTicksRemaining);
            ImGui::Text("Combatants I/C/W/R: %llu/%llu/%llu/%llu",
                        static_cast<unsigned long long>(
                            worldStats.combat.idleCount),
                        static_cast<unsigned long long>(
                            worldStats.combat.chaseCount),
                        static_cast<unsigned long long>(
                            worldStats.combat.windupCount),
                        static_cast<unsigned long long>(
                            worldStats.combat.recoverCount));
            ImGui::Text("Combat ray budget: %llu / %llu denied=%llu",
                        static_cast<unsigned long long>(
                            worldStats.combat.raycastsUsed),
                        static_cast<unsigned long long>(
                            worldStats.combat.raycastBudget),
                        static_cast<unsigned long long>(
                            worldStats.combat.raycastBudgetDenied));
            ImGui::Text("Combat chase budget: %llu / %llu denied=%llu",
                        static_cast<unsigned long long>(
                            worldStats.combat.chaseStepsUsed),
                        static_cast<unsigned long long>(
                            worldStats.combat.chaseStepBudget),
                        static_cast<unsigned long long>(
                            worldStats.combat.chaseStepBudgetDenied));
            ImGui::Text("Projectiles: %llu / %llu steps=%llu/%llu denied=%llu",
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileCount),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileWorldLimit),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileStepsUsed),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileStepBudget),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileStepBudgetDenied));
            ImGui::Text("Projectile L/C/H/G/B/E/O: %llu/%llu/%llu/%llu/%llu/%llu/%llu",
                        static_cast<unsigned long long>(
                            worldStats.combat.projectilesLaunched),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileCapacityDenied),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileHits),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileGuards),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileBlocks),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileExpirations),
                        static_cast<unsigned long long>(
                            worldStats.combat.projectileOwnerClears));
            ImGui::Text("Observed combat: actor=%llu target=%llu %s/%s %d",
                        static_cast<unsigned long long>(
                            worldStats.combat.observedActorId),
                        static_cast<unsigned long long>(
                            worldStats.combat.observedTargetId),
                        enemyCombatModeName(worldStats.combat.observedMode),
                        mobCombatStateName(worldStats.combat.observedState),
                        worldStats.combat.observedStateTicksRemaining);
            ImGui::Text("Transition: %s | projectile=%llu %s",
                        mobCombatTransitionReasonName(
                            worldStats.combat.observedReason),
                        static_cast<unsigned long long>(
                            worldStats.combat.observedProjectileId),
                        combatProjectileRemovalReasonName(
                            worldStats.combat.lastProjectileRemovalReason));
            ImGui::Text("Feedback=%s source=%llu epoch=%llu",
                        combatDirectionName(
                            worldStats.combatFeedback.direction),
                        static_cast<unsigned long long>(
                            worldStats.combatFeedback.sourceId),
                        static_cast<unsigned long long>(
                            worldStats.combatFeedback.epoch));
            ImGui::TextUnformatted("R: consume held food");
            ImGui::Text("Death inventory policy: %s",
                        World::PlayerDeathInventoryPolicy);
            ImGui::Text("F1: %s", tr("diagnostics.toggle").c_str());
        }
        ImGui::End();

        const std::string worldDiagnostics =
            label("diagnostics.sandbox_title", "##WorldDiagnostics");
        if (ImGui::Begin(worldDiagnostics.c_str()))
        {
            ImGui::Text("Seed: %d (terrain v%d)", worldStats.terrainSeed,
                        worldStats.terrainGenerationVersion);
            ImGui::Text("Difficulty: %s (profile v%d, epoch %llu)%s",
                        difficultyName(worldStats.difficulty).c_str(),
                        worldStats.difficultyProfileVersion,
                        worldStats.difficultyApplicationEpoch,
                        worldStats.difficultyChangePending
                            ? " [pending]" : "");
            ImGui::Text(
                "Echo trials: %d/%d (event %d, wave %d, remaining %d)",
                worldStats.completedPostVictoryEvents,
                PostVictoryEvents::MaximumEvents,
                worldStats.activePostVictoryEvent,
                worldStats.postVictoryEventWave,
                worldStats.postVictoryEventRemainingGuardians);
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
            ImGui::Text(
                "Simulation tick / dt / total ms: %d / %.3f / %.3f",
                worldStats.simulation.lastTick,
                worldStats.simulation.deltaSeconds,
                worldStats.simulation.tickElapsedMilliseconds);
            for (const WorldSimulationPhaseTiming &phase :
                 worldStats.simulation.phases) {
                const SimulationPhaseMetrics *metrics =
                    findSimulationPhaseMetrics(worldStats.simulation,
                                               phase.phase);
                if (metrics == nullptr) {
                    ImGui::Text("  %s: %.3f ms",
                                worldSimulationPhaseName(phase.phase),
                                phase.elapsedMilliseconds);
                }
                else if (metrics->budgetScope ==
                         SimulationPhaseBudgetScope::Unbudgeted) {
                    ImGui::Text(
                        "  %s: %.3f ms | processed / deferred: %llu / %llu | %s",
                        worldSimulationPhaseName(phase.phase),
                        metrics->elapsedMilliseconds,
                        static_cast<unsigned long long>(metrics->processed),
                        static_cast<unsigned long long>(metrics->deferred),
                        simulationPhaseBudgetStatusName(
                            metrics->budgetStatus()));
                }
                else {
                    ImGui::Text(
                        "  %s: %.3f ms | processed / deferred: %llu / %llu | budget: %llu %s (%s)",
                        worldSimulationPhaseName(phase.phase),
                        metrics->elapsedMilliseconds,
                        static_cast<unsigned long long>(metrics->processed),
                        static_cast<unsigned long long>(metrics->deferred),
                        static_cast<unsigned long long>(metrics->budget),
                        simulationPhaseBudgetScopeName(
                            metrics->budgetScope),
                        simulationPhaseBudgetStatusName(
                            metrics->budgetStatus()));
                }
            }
            ImGui::Separator();
            ImGui::Text("Sections: %llu",
                        static_cast<unsigned long long>(
                            worldStats.chunks.sections));
            ImGui::Text(
                "Demand epoch/revision/active/planned: %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.epoch),
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.revision),
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.activeDemands),
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.lastPlannedTargets));
            ImGui::Text(
                "Demand P/C/T/Pre expired: %llu / %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.playerDemands),
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.cameraDemands),
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.teleportDemands),
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.preloadDemands),
                static_cast<unsigned long long>(
                    worldStats.streamingDemand.expiredDemands));
            ImGui::Text(
                "Jobs pending/in-flight/results: %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.pendingJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.inFlightJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.completedResults));
            ImGui::Text(
                "Jobs submitted/started/completed: %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.submittedJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.startedJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.completedJobs));
            ImGui::Text(
                "Jobs load/mesh/work/none/reject: %llu / %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.chunkLoadOrGenerateJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.chunkMeshBuildJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.didWorkJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.noWorkJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.commitRejectedJobs));
            ImGui::Text(
                "Job ms queue/worker/commit: %.3f / %.3f / %.3f",
                worldStats.worldJobs.lastQueueLatencyMilliseconds,
                worldStats.worldJobs.lastWorkerMilliseconds,
                worldStats.worldJobs.lastCommitMilliseconds);
            ImGui::Text(
                "Data A/Rq/L/G/R/E/S: %llu / %llu / %llu / %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.chunks.dataAbsentChunks),
                static_cast<unsigned long long>(
                    worldStats.chunks.dataRequestedChunks),
                static_cast<unsigned long long>(
                    worldStats.chunks.dataLoadingChunks),
                static_cast<unsigned long long>(
                    worldStats.chunks.dataGeneratingChunks),
                static_cast<unsigned long long>(
                    worldStats.chunks.dataResidentChunks),
                static_cast<unsigned long long>(
                    worldStats.chunks.dataEvictRequestedChunks),
                static_cast<unsigned long long>(
                    worldStats.chunks.dataSavingChunks));
            ImGui::Text(
                "Mesh C/D/Q/B/CPU: %llu / %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.chunks.meshCleanSections),
                static_cast<unsigned long long>(
                    worldStats.chunks.meshDirtySections),
                static_cast<unsigned long long>(
                    worldStats.chunks.meshQueuedSections),
                static_cast<unsigned long long>(
                    worldStats.chunks.meshBuildingSections),
                static_cast<unsigned long long>(
                    worldStats.chunks.cpuReadySections));
            ImGui::Text(
                "Render N/U/G/S: %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.chunks.renderNotResidentSections),
                static_cast<unsigned long long>(
                    worldStats.chunks.renderUploadPendingSections),
                static_cast<unsigned long long>(
                    worldStats.chunks.gpuResidentSections),
                static_cast<unsigned long long>(
                    worldStats.chunks.renderStaleSections));
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
    PresentationCaptionTimeline captionTimeline;
    float previousPlayerHealth = -1.f;
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
    std::uint32_t dismissedVictoryEpoch = 0;
    std::array<char, 81> createName{};
    std::array<char, 81> renameName{};
    int createSeed = 0;
    int createDifficulty = static_cast<int>(WorldDifficulty::Normal);
    int pauseDifficulty = static_cast<int>(WorldDifficulty::Normal);
    bool difficultyDraftInitialized = false;
    bool worldsDirty = true;
    bool openDeletePopup = false;
    bool openPermanentDeletePopup = false;
    bool openBackupPopup = false;
    OgreUserInterfaceAction pendingAction;
    WorldDebugStats worldStats;
    MiningProgressSnapshot miningProgress;
    ActionFeedbackSnapshot actionFeedback;
    bool showDebugPanel = false;
    bool settingsFixtureRequested = false;
    bool settingsFixtureOpened = false;
    bool showCredits = false;
    bool initialized = false;
    bool listenerInstalled = false;
    bool framePending = false;
    Ogre::TexturePtr atlasTexture;
    ImTextureID atlasTextureId = ImTextureID_Invalid;
    std::string iniPath;
    std::string fontPath;
    std::string fontDiagnostic;
    ImVector<ImWchar> presentationGlyphRanges;
};

OgreUserInterface::OgreUserInterface(Ogre::RenderWindow &window,
                                     Ogre::SceneManager &sceneManager,
                                     Ogre::Camera &camera, Player *player,
                                     World *world,
                                     GameApplicationFlow &applicationFlow,
                                     WorldManagementService &worldManagement,
                                     const UserSettings &settings,
                                     std::string presentationFontPath,
                                     std::function<void()> uiFeedback,
                                     std::vector<PendingCrashReport> crashReports)
    : m_impl(std::make_unique<Impl>(window, sceneManager, camera, player,
                                    world, applicationFlow,
                                    worldManagement, settings,
                                    std::move(presentationFontPath),
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
    const MiningProgressSnapshot &miningProgress,
    const ActionFeedbackSnapshot &actionFeedback)
{
    m_impl->beginFrame(deltaSeconds, worldStats, miningProgress,
                       actionFeedback);
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
           m_impl->victoryOverlayVisible() ||
           ImGui::GetIO().WantCaptureKeyboard;
}

bool OgreUserInterface::wantsMouseInput() const
{
    return m_impl->flow->state() != GameApplicationState::Playing ||
           (m_impl->player != nullptr &&
            (m_impl->player->hasOpenContainer() ||
             m_impl->player->hasOpenCrafting())) ||
           m_impl->victoryOverlayVisible() ||
           ImGui::GetIO().WantCaptureMouse;
}

bool OgreUserInterface::hasBlockingModal() const noexcept
{
    return !m_impl->crashReports.empty() ||
           m_impl->victoryOverlayVisible();
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
    m_impl->dismissedVictoryEpoch = 0;
    m_impl->previousPlayerHealth = -1.f;
    m_impl->difficultyDraftInitialized = false;
    if (world != nullptr)
    {
        const DifficultyRuntimeSnapshot snapshot =
            world->getDifficultySnapshot();
        m_impl->pauseDifficulty = static_cast<int>(
            snapshot.changePending ? snapshot.pending : snapshot.active);
        m_impl->difficultyDraftInitialized = true;
    }
}

void OgreUserInterface::setStatusMessage(std::string message)
{
    m_impl->setStatusMessage(std::move(message));
}

void OgreUserInterface::setAudioCaption(std::string cueId,
                                        std::string caption)
{
    m_impl->setAudioCaption(std::move(cueId), std::move(caption));
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

void OgreUserInterface::postViewportUpdate(
    const Ogre::RenderTargetViewportEvent &event)
{
    if (!m_impl->framePending)
    {
        return;
    }
    if (event.source == nullptr ||
        event.source->getTarget() != m_impl->window)
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
