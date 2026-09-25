#include "OgreUserInterface.h"
#include "OgreItemGeometry.h"
#include "GameInterfaceWidgets.h"

#include <OIS.h>
#include <OgreCamera.h>
#include <OgreResourceGroupManager.h>
#include <OgreRenderWindow.h>
#include <OgreSceneManager.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>
#include <OgreViewport.h>

#include <algorithm>
#include <iostream>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
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
#include "../Item/FoodRegistry.h"
#include "../Player/Player.h"
#include "../Presentation/LocalizedTextRegistry.h"
#include "../Presentation/LocalizedPresentation.h"
#include "../Presentation/PresentationCaption.h"
#include "../Presentation/PresentationLayout.h"
#include "../Presentation/PresentationClock.h"
#include "../Presentation/PlayerHandPresentation.h"
#include "../Presentation/MinimapNavigation.h"
#include "../Presentation/HudInteraction.h"
#include "../Presentation/ExplorationMapInteraction.h"
#include "../Presentation/TerrainMapView.h"
#include "../Presentation/MapSurfaceRegion.h"
#include "../RuntimeConfig.h"
#include "../Sandbox/GameApplicationFlow.h"
#include "../Util/ResourcePaths.h"
#include "../World/World.h"
#include "../World/Exploration/ExplorationNavigation.h"
#include "../World/Block/BlockCapability.h"
#include "../World/Block/TerrainMaterialProfile.h"
#include "../Item/SmeltingRegistry.h"
#include "../World/Interaction/BlockMiningProgress.h"
#include "../World/WorldConstants.h"
#include "../Feedback/ActionFeedback.h"
#include "../World/Storage/WorldManagementService.h"

namespace
{
    const ImVec4 WarmText(0.957f, 0.933f, 0.863f, 1.f);
    const ImVec4 WarmMuted(0.68f, 0.74f, 0.77f, 1.f);
    const ImVec4 WarmAccent(0.871f, 0.714f, 0.431f, 1.f);

    constexpr int MinimapClipSegments = 96;

    void fillCircularMinimapCell(
        ImDrawList* draw, const ImVec2& cellMin, const ImVec2& cellMax,
        const ImVec2& center, float radius,
        const std::array<ImVec2, MinimapClipSegments>& circle,
        ImU32 colour)
    {
        const float nearestX = std::clamp(center.x, cellMin.x, cellMax.x);
        const float nearestY = std::clamp(center.y, cellMin.y, cellMax.y);
        const float nearestDx = nearestX - center.x;
        const float nearestDy = nearestY - center.y;
        if (nearestDx * nearestDx + nearestDy * nearestDy >=
            radius * radius)
            return;

        const float farthestDx = std::max(
            std::abs(cellMin.x - center.x), std::abs(cellMax.x - center.x));
        const float farthestDy = std::max(
            std::abs(cellMin.y - center.y), std::abs(cellMax.y - center.y));
        if (farthestDx * farthestDx + farthestDy * farthestDy <
            (radius - 0.1f) * (radius - 0.1f))
        {
            draw->AddRectFilled(cellMin, cellMax, colour);
            return;
        }

        // Only perimeter cells need geometry clipping. Keep the map's square
        // pixels intact in the middle while the circular silhouette gets an
        // anti-aliased convex boundary instead of cell-centre stair steps.
        std::array<ImVec2, MinimapClipSegments + 4> points{};
        std::array<ImVec2, MinimapClipSegments + 4> scratch{};
        points[0] = cellMin;
        points[1] = ImVec2(cellMax.x, cellMin.y);
        points[2] = cellMax;
        points[3] = ImVec2(cellMin.x, cellMax.y);
        ImVec2* input = points.data();
        ImVec2* output = scratch.data();
        int count = 4;
        for (int edgeIndex = 0;
             edgeIndex < MinimapClipSegments && count >= 3; ++edgeIndex)
        {
            const ImVec2& start = circle[edgeIndex];
            const ImVec2& end =
                circle[(edgeIndex + 1) % MinimapClipSegments];
            const float edgeX = end.x - start.x;
            const float edgeY = end.y - start.y;
            auto side = [&](const ImVec2& point) {
                return edgeX * (point.y - start.y) -
                       edgeY * (point.x - start.x);
            };
            int outputCount = 0;
            ImVec2 previous = input[count - 1];
            float previousSide = side(previous);
            for (int index = 0; index < count; ++index)
            {
                const ImVec2 current = input[index];
                const float currentSide = side(current);
                if ((previousSide >= 0.f) != (currentSide >= 0.f))
                {
                    const float t = previousSide /
                        (previousSide - currentSide);
                    output[outputCount++] = ImVec2(
                        previous.x + (current.x - previous.x) * t,
                        previous.y + (current.y - previous.y) * t);
                }
                if (currentSide >= 0.f)
                    output[outputCount++] = current;
                previous = current;
                previousSide = currentSide;
            }
            std::swap(input, output);
            count = outputCount;
        }
        if (count >= 3)
            draw->AddConvexPolyFilled(input, count, colour);
    }

    void applyWarmWildernessStyle()
    {
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(12.f, 10.f);
        style.FramePadding = ImVec2(8.f, 5.f);
        style.ItemSpacing = ImVec2(8.f, 6.f);
        style.WindowRounding = 6.f;
        style.ChildRounding = 5.f;
        style.FrameRounding = 4.f;
        style.PopupRounding = 6.f;
        style.ScrollbarRounding = 4.f;
        style.GrabRounding = 4.f;
        style.WindowBorderSize = 1.f;
        style.FrameBorderSize = 0.f;
        auto* colours = style.Colors;
        colours[ImGuiCol_Text] = WarmText;
        colours[ImGuiCol_TextDisabled] = WarmMuted;
        colours[ImGuiCol_WindowBg] = ImVec4(0.065f, 0.090f, 0.115f, 0.97f);
        colours[ImGuiCol_ChildBg] = ImVec4(0.080f, 0.110f, 0.140f, 0.94f);
        colours[ImGuiCol_PopupBg] = ImVec4(0.075f, 0.105f, 0.135f, 0.99f);
        colours[ImGuiCol_Border] = ImVec4(0.35f, 0.43f, 0.48f, 0.45f);
        colours[ImGuiCol_FrameBg] = ImVec4(0.135f, 0.180f, 0.215f, 1.f);
        colours[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.29f, 0.33f, 1.f);
        colours[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.35f, 0.39f, 1.f);
        colours[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.12f, 0.15f, 1.f);
        colours[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.18f, 0.22f, 1.f);
        colours[ImGuiCol_Button] = ImVec4(0.17f, 0.24f, 0.28f, 1.f);
        colours[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.34f, 0.39f, 1.f);
        colours[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.40f, 0.44f, 1.f);
        colours[ImGuiCol_Header] = ImVec4(0.18f, 0.27f, 0.30f, 1.f);
        colours[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.34f, 0.38f, 1.f);
        colours[ImGuiCol_HeaderActive] = ImVec4(0.29f, 0.40f, 0.43f, 1.f);
        colours[ImGuiCol_CheckMark] = WarmAccent;
        colours[ImGuiCol_SliderGrab] = WarmAccent;
        colours[ImGuiCol_SliderGrabActive] = ImVec4(0.97f, 0.83f, 0.56f, 1.f);
        colours[ImGuiCol_Separator] = ImVec4(0.34f, 0.44f, 0.50f, 0.45f);
        colours[ImGuiCol_SeparatorHovered] = WarmAccent;
        colours[ImGuiCol_SeparatorActive] = WarmAccent;
        colours[ImGuiCol_ResizeGrip] = ImVec4(0.68f, 0.73f, 0.56f, 0.25f);
        colours[ImGuiCol_ResizeGripHovered] = WarmAccent;
        colours[ImGuiCol_ResizeGripActive] = WarmAccent;
        colours[ImGuiCol_PlotHistogram] = ImVec4(0.60f, 0.72f, 0.44f, 1.f);
        colours[ImGuiCol_TextSelectedBg] = ImVec4(0.67f, 0.56f, 0.32f, 0.50f);
        colours[ImGuiCol_NavCursor] = WarmAccent;
    }

    void pushPrimaryButtonStyle()
    {
        ImGui::PushStyleColor(ImGuiCol_Button, WarmAccent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                             ImVec4(0.96f, 0.81f, 0.53f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                             ImVec4(0.76f, 0.60f, 0.34f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
                             ImVec4(0.12f, 0.17f, 0.13f, 1.f));
    }

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
        , uiFeedback(std::move(feedback))
        , crashReports(std::move(pendingCrashReports))
        , showDebugPanel(RuntimeDebugOptions::showDebugInfoAtStartup())
        , settingsFixtureRequested(environmentFlagEnabled(
              "HELLOMINE3D_V10E_SETTINGS_FIXTURE"))
        , iniPath(ResourcePaths::bin("imgui-ogre.ini"))
        , fontPath(std::move(presentationFontPath))
    {
        if (const char* page = std::getenv("HELLOMINE3D_HUD_PAGE_FIXTURE"))
        {
            hudPageFixture = page;
            if ((!environmentFlagEnabled("HELLO_RENDER_CAPTURE") && !environmentFlagEnabled("HELLO_PERF_CAPTURE")) ||
                (hudPageFixture != "map" && hudPageFixture != "journal" && hudPageFixture != "pointer"))
                throw std::runtime_error("HUD page fixture requires diagnostic capture and a valid page.");
        }
        if (const char* slot = std::getenv("HELLOMINE3D_HUD_INSPECT_SLOT"))
        {
            if (hudPageFixture != "pointer" || !environmentFlagEnabled("HELLOMINE3D_HUD_FIXTURE") ||
                slot[0] < '0' || slot[0] > '4' || slot[1] != '\0')
                throw std::runtime_error("HUD inspection fixture requires pointer capture, item fixture and slot 0..4.");
            inspectSlotFixture = slot[0] - '0';
        }
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

    std::string keyName(GameplayKey key) const
    {
        return tr("input.key." + std::string(gameplayKeyToken(key)),
                  gameplayKeyName(key));
    }

    std::string mouseButtonName(GameplayMouseButton button) const
    {
        return tr("input.mouse." +
                      std::string(gameplayMouseButtonToken(button)),
                  gameplayMouseButtonName(button));
    }

    std::string sharedMouseBinding(
        const GameplayMouseBindings& bindings) const
    {
        for (std::size_t buttonIndex = 0;
             buttonIndex < GameplayMouseButtonCount; ++buttonIndex)
        {
            const auto button =
                static_cast<GameplayMouseButton>(buttonIndex);
            std::string actions;
            int count = 0;
            for (GameplayWorldAction action : {
                     GameplayWorldAction::Use,
                     GameplayWorldAction::Place,
                     GameplayWorldAction::Guard})
            {
                if (bindings.get(action) != button)
                    continue;
                if (!actions.empty())
                    actions += tr("input.action_separator", ", ");
                actions += worldActionName(action);
                ++count;
            }
            if (count > 1)
                return mouseButtonName(button) + ": " + actions;
        }
        return {};
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

    std::string objectiveInstructionText(const std::string& id,
        const std::string& fallback, const std::string& guidanceKey) const
    {
        return LocalizedPresentation::objectiveInstruction(
            appliedSettings.locale, id, fallback, guidanceKey,
            keyName(appliedSettings.inputBindings.get(GameplayAction::ConsumeFood)));
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
            ImFontConfig fontConfig;
            fontConfig.RasterizerMultiply = 1.10f;
            if (io.Fonts->AddFontFromFileTTF(
                    fontPath.c_str(), 20.0f, &fontConfig,
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
        applyWarmWildernessStyle();

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

        try
        {
            menuTexture = Ogre::TextureManager::getSingleton().load(
                "WarmWildernessMenu.png",
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                Ogre::TEX_TYPE_2D, 0);
            unsigned int menuGlId = 0;
            menuTexture->getCustomAttribute("GLID", &menuGlId);
            if (menuGlId == 0)
            {
                throw std::runtime_error("Menu background has no GL texture ID.");
            }
            menuTextureId = static_cast<ImTextureID>(menuGlId);
            const auto loadHudTexture = [](const char* name, Ogre::TexturePtr& texture) {
                texture = Ogre::TextureManager::getSingleton().load(name,
                    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                    Ogre::TEX_TYPE_2D, 0);
                unsigned int glId = 0;
                texture->getCustomAttribute("GLID", &glId);
                if (glId == 0) throw std::runtime_error("HUD art has no GL texture ID.");
                return static_cast<ImTextureID>(glId);
            };
            hudPanelTextureId = loadHudTexture("SandboxPanel.png", hudPanelTexture);
            hudGlyphTextureId = loadHudTexture("SandboxGlyphs.png", hudGlyphTexture);
        }
        catch (...)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui::DestroyContext();
            atlasTexture.setNull();
            menuTexture.setNull();
            hudPanelTexture.setNull();
            hudGlyphTexture.setNull();
            throw;
        }

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
        menuTextureId = ImTextureID_Invalid;
        menuTexture.setNull();
        hudPanelTextureId = hudGlyphTextureId = ImTextureID_Invalid;
        hudPanelTexture.setNull();
        hudGlyphTexture.setNull();
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
        if (flow->state() == GameApplicationState::Playing)
        {
            objectiveHintSeconds = std::max(
                0.f, objectiveHintSeconds - std::max(0.f, deltaSeconds));
        }
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
        hudElapsedSeconds = advancePresentationClock(hudElapsedSeconds, deltaSeconds);
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
        const float targetMovement = player != nullptr &&
            flow->state() == GameApplicationState::Playing
                ? std::clamp(glm::length(glm::vec2(player->velocity.x, player->velocity.z)) / 5.f, 0.f, 1.f)
                : 0.f;
        heldMovement += (targetMovement - heldMovement) *
            (1.f - std::exp(-std::min(frameSeconds, .1f) * 12.f));
        if (flow->state() == GameApplicationState::MainMenu ||
            flow->state() == GameApplicationState::WorldList ||
            flow->state() == GameApplicationState::Loading)
        {
            drawMenuBackdrop();
        }
        if (settingsFixtureRequested && !settingsFixtureOpened &&
            flow->state() == GameApplicationState::Playing && flow->pause())
        {
            settingsSession.begin(appliedSettings);
            settingsMessage.clear();
            settingsApplyPending = false;
            settingsFixtureOpened = true;
        }
        if (flow->state() != GameApplicationState::Playing || victoryOverlayVisible() ||
            (player != nullptr && (player->hasOpenContainer() || player->hasOpenCrafting())))
            hudInteraction.dismiss();
        if (!hudPageFixture.empty() && !hudPageFixtureOpened && flow->state() == GameApplicationState::Playing)
        {
            hudPageFixtureSeconds += frameSeconds;
            if (hudPageFixtureSeconds >= 3.f && player && !player->hasOpenContainer() && !player->hasOpenCrafting())
            {
                hudInteraction.togglePointer();
                if (hudPageFixture != "pointer") hudInteraction.open(hudPageFixture == "map" ? HudInteraction::Page::Map : HudInteraction::Page::Journal);
                if (hudPageFixture == "map") mapFlatOverview = true;
                hudPageFixtureOpened = true;
                std::cout << "[HUD_PAGE_FIXTURE] page=" << hudPageFixture << "\n";
            }
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
                ImGui::GetBackgroundDrawList()->AddRectFilled(
                    ImVec2(0.f, 0.f), ImGui::GetIO().DisplaySize,
                    IM_COL32(8, 14, 11, 84));
                if (settingsSession.isOpen())
                {
                    drawSettingsMenu();
                }
                else
                {
                    drawPauseMenu();
                }
                drawHudNotifications(ImGui::GetIO().DisplaySize.y - 8.f);
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

    void drawMenuBackdrop()
    {
        if (menuTextureId == ImTextureID_Invalid || menuTexture.isNull())
        {
            return;
        }
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float viewAspect = display.x / std::max(1.f, display.y);
        const float imageAspect = static_cast<float>(menuTexture->getWidth()) /
                                  static_cast<float>(menuTexture->getHeight());
        ImVec2 uvMin(0.f, 0.f);
        ImVec2 uvMax(1.f, 1.f);
        if (viewAspect < imageAspect)
        {
            uvMin.x = (1.f - viewAspect / imageAspect) * 0.5f;
            uvMax.x = 1.f - uvMin.x;
        }
        else
        {
            uvMin.y = (1.f - imageAspect / viewAspect) * 0.5f;
            uvMax.y = 1.f - uvMin.y;
        }
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        background->AddImage(ImTextureRef(menuTextureId), ImVec2(0.f, 0.f),
                             display, uvMin, uvMax);
        background->AddRectFilledMultiColor(
            ImVec2(0.f, 0.f), display,
            IM_COL32(14, 25, 19, 222), IM_COL32(14, 25, 19, 28),
            IM_COL32(14, 25, 19, 65), IM_COL32(14, 25, 19, 242));
    }

    void drawMainMenu()
    {
        const ImGuiIO &io = ImGui::GetIO();
        const float scale = appliedSettings.uiScale;
        const PresentationWindowLayout layout = fitPresentationWindow(
            io.DisplaySize.x, io.DisplaySize.y, 350.f, 354.f, scale);
        const float left = std::min(io.DisplaySize.x * 0.08f,
                                   io.DisplaySize.x - layout.width - 15.f);
        ImGui::SetNextWindowPos(
            ImVec2(std::max(15.f, left), io.DisplaySize.y * 0.5f),
            ImGuiCond_Always, ImVec2(0.f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin("##MainMenu", nullptr, flags))
        {
            ImGui::Dummy(ImVec2(0.f, 12.f * scale));
            const std::string appTitle = tr("app.title", "HelloMine3D");
            const float titleScale = std::min(
                1.95f, ImGui::GetContentRegionAvail().x /
                           std::max(1.f, ImGui::CalcTextSize(appTitle.c_str()).x));
            ImGui::SetWindowFontScale(titleScale);
            ImGui::TextUnformatted(appTitle.c_str());
            ImGui::SetWindowFontScale(1.f);
            ImGui::PushStyleColor(ImGuiCol_Text, WarmMuted);
            ImGui::TextWrapped("%s", tr("main.tagline").c_str());
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0.f, 28.f * scale));
            pushPrimaryButtonStyle();
            if (ImGui::Button(
                    label("main.single_player", "##SinglePlayer").c_str(),
                    ImVec2(-1.f, 48.f * scale)))
            {
                if (flow->showWorldList())
                {
                    worldsDirty = true;
                    playUiFeedback();
                }
            }
            ImGui::PopStyleColor(4);
            ImGui::Dummy(ImVec2(0.f, 4.f * scale));
            if (ImGui::Button(label("main.credits", "##Credits").c_str(),
                              ImVec2(-1.f, 42.f * scale)))
            {
                showCredits = true;
                playUiFeedback();
            }
            if (ImGui::Button(label("common.quit", "##Quit").c_str(),
                              ImVec2(-1.f, 42.f * scale)))
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

    void reportResult(const WorldManagementResult &result,
                      const char* successKey)
    {
        statusMessage = result.succeeded()
            ? tr(successKey, result.message)
            : tr("world.operation_failed", "World operation failed") +
                  ": " + result.message;
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
        const float catalogueHeight = std::min(620.0f,
            (435.0f + (!selectedWorldId.empty()
                ? 42.0f + 24.0f * std::min<std::size_t>(backups.size(), 4)
                : 0.0f) + (!deletedWorlds.empty()
                ? 24.0f * std::min<std::size_t>(deletedWorlds.size(), 3)
                : 0.0f) + (!statusMessage.empty() ? 32.0f : 0.0f) +
                140.0f * std::max(0.0f, appliedSettings.uiScale - 1.0f)));
        const PresentationWindowLayout maximumLayout = fitPresentationWindow(
            io.DisplaySize.x, io.DisplaySize.y, 820.0f, 620.0f,
            appliedSettings.uiScale);
        const float top = std::max(18.0f,
            (io.DisplaySize.y - maximumLayout.height) * 0.5f);
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, top),
            ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        const PresentationWindowLayout layout = fitPresentationWindow(
            io.DisplaySize.x, io.DisplaySize.y, 820.0f,
            catalogueHeight, appliedSettings.uiScale);
        ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height), ImGuiCond_Always);
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
            ImGui::SameLine();
            ImGui::TextDisabled("%s v%d", tr("world.terrain").c_str(),
                                CurrentTerrainGenerationVersion);
            // Labels occupy their own lines so font scaling cannot push the
            // Create action outside the panel or consume the seed editor width.
            if (ImGui::BeginTable("WorldCreateForm", 2,
                    ImGuiTableFlags_SizingStretchProp |
                    ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
                ImGui::TableSetupColumn("Seed", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(tr("world.name").c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputText("##create-name", createName.data(), createName.size());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(tr("world.seed").c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputInt("##create-seed", &createSeed);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(tr("world.difficulty").c_str());
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                const std::string createDifficultyName = difficultyName(
                    static_cast<WorldDifficulty>(createDifficulty));
                if (ImGui::BeginCombo(
                        "##create-difficulty",
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
                ImGui::TableSetColumnIndex(1);
                ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
                if (ImGui::Button(label("common.create", "##WorldCreate").c_str()))
                {
                    const WorldManagementResult result =
                        management->createWorld(
                            createName.data(), createSeed,
                            static_cast<WorldDifficulty>(createDifficulty));
                    reportResult(result, "world.feedback.created");
                    if (result.succeeded())
                    {
                        createSeed = WorldManagementService::suggestWorldSeed();
                    }
                }
                ImGui::EndTable();
            }

            ImGui::Separator();
            ImGui::Text("%s (%llu)", tr("world.active").c_str(),
                        static_cast<unsigned long long>(worlds.size()));
            const float activeListHeight = std::min(185.0f,
                std::max(58.0f, 18.0f +
                    worlds.size() * ImGui::GetTextLineHeightWithSpacing()));
            ImGui::BeginChild("WorldList", ImVec2(0.0f, activeListHeight), true);
            if (worlds.empty())
                ImGui::TextDisabled("%s", tr("world.none").c_str());
            const ImGuiTableFlags worldTableFlags =
                ImGuiTableFlags_SizingStretchProp |
                ImGuiTableFlags_NoSavedSettings;
            if (ImGui::BeginTable("ActiveWorlds", 5, worldTableFlags))
            {
                ImGui::TableSetupColumn(
                    tr("world.world").c_str(),
                    ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn(
                    tr("world.seed").c_str(),
                    ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn(
                    tr("world.terrain").c_str(),
                    ImGuiTableColumnFlags_WidthFixed, 68.0f);
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
                    if (entry.terrainGenerationVersion > 0)
                        ImGui::Text("v%d",
                                    entry.terrainGenerationVersion);
                    else
                        ImGui::TextDisabled("%s",
                            tr("world.terrain_unknown").c_str());
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(
                        difficultyName(entry.difficulty).c_str());
                    if (entry.completedPostVictoryEvents > 0)
                    {
                        ImGui::TextDisabled(
                            "%s %d / %d", tr("world.echo_trials").c_str(),
                            entry.completedPostVictoryEvents,
                            PostVictoryEvents::MaximumEvents);
                    }
                    ImGui::TableSetColumnIndex(4);
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
                        selectedWorldId, renameName.data()),
                        "world.feedback.renamed");
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
            if (!deletedWorlds.empty())
            {
                const float deletedListHeight = std::min(105.0f,
                    18.0f + deletedWorlds.size() *
                    ImGui::GetTextLineHeightWithSpacing());
                ImGui::BeginChild("DeletedWorldList",
                                  ImVec2(0.0f, deletedListHeight), true);
                for (const DeletedWorldInfo &entry : deletedWorlds)
                {
                    ImGui::PushID(entry.recoveryId.c_str());
                    ImGui::TextUnformatted(entry.world.displayName.c_str());
                    ImGui::SameLine(400.0f);
                    if (ImGui::SmallButton(label("common.restore", "##RestoreWorld").c_str()))
                    {
                        reportResult(management->restoreDeletedWorld(
                            entry.world.id), "world.feedback.restored");
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
            }
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
                        pendingDeleteWorldId), "world.feedback.recoverable");
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
                        pendingPermanentDeleteWorldId),
                        "world.feedback.permanently_deleted");
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
                        selectedWorldId, pendingBackupId),
                        "world.feedback.backup_restored");
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

    Material::ID objectiveIcon(const std::string& id) const
    {
        const std::pair<const char*, Material::ID> icons[] = {
            {"wooden_pickaxe", Material::WoodenPickaxe}, {"stone_pickaxe", Material::StonePickaxe},
            {"iron_pickaxe", Material::IronPickaxe}, {"iron_sword", Material::IronSword},
            {"workbench", Material::Workbench}, {"gather_wood", Material::OakBark},
            {"gather_stone", Material::Cobblestone}, {"iron_ore", Material::IronOre},
            {"torches", Material::Torch}, {"coal", Material::CoalOre}, {"bread", Material::Bread},
            {"furnace", Material::Furnace}, {"smelt_iron", Material::IronIngot},
            {"planks", Material::OakPlank}, {"door", Material::OakDoor},
            {"waystone", Material::WaystoneCore}, {"ritual", Material::WaystoneCore},
            {"wheat", Material::Wheat}, {"reopen_world", Material::Chest}
        };
        for (const auto& icon : icons)
            if (id.find(icon.first) != std::string::npos) return icon.second;
        return Material::OakLeaf;
    }

    void adventureIcon(Material::ID icon, ImVec2 lo, float size)
    {
        if (icon == Material::Nothing) return;
        if (itemVisualUsesCube(icon))
            drawItemPortrait(icon, ImVec2(lo.x + size * .5f, lo.y + size * .5f), size * .77f);
        else drawMaterialIcon(ImGui::GetWindowDrawList(), icon, lo, ImVec2(lo.x + size, lo.y + size));
    }

    void adventureBackdrop()
    {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0,0), ImGui::GetIO().DisplaySize,
            IM_COL32(5, 12, 16, 110));
    }

    bool adventureHeader(const std::string& title, GameInterfaceWidgets::Glyph icon,
                         const std::string& subtitle = {}, bool stackedSubtitle = false)
    {
        const float scale = appliedSettings.uiScale;
        const auto lo = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
        auto* draw = ImGui::GetWindowDrawList();
        GameInterfaceWidgets::surface(draw, lo, ImVec2(lo.x + size.x, lo.y + size.y), false, scale);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x, closeSize = 28.f * scale;
        GameInterfaceWidgets::glyph(draw, icon, ImVec2(start.x, start.y + 3.f * scale), 24.f * scale);
        const float font = ImGui::GetFontSize() * 1.35f;
        const std::string heading = boundedHudText(title, font, width - 78.f * scale);
        draw->AddText(ImGui::GetFont(), font, ImVec2(start.x + 36.f * scale, start.y),
            ImGui::ColorConvertFloat4ToU32(WarmText), heading.c_str());
        const float headingWidth = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.f, heading.c_str()).x;
        if (!subtitle.empty())
        {
            const float x = stackedSubtitle ? start.x+36.f*scale : start.x+64.f*scale+headingWidth;
            if (!stackedSubtitle) draw->AddLine(ImVec2(x-14.f*scale,start.y+5.f*scale),
                ImVec2(x-14.f*scale,start.y+font-3.f*scale),IM_COL32(117,140,139,180));
            const auto detail = boundedHudText(subtitle, ImGui::GetFontSize() * .8f,
                start.x + width - closeSize - 12.f * scale - x);
            draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * .8f,
                ImVec2(x, start.y + (stackedSubtitle ? font+3.f*scale : 5.f*scale)), ImGui::ColorConvertFloat4ToU32(WarmMuted), detail.c_str());
        }
        ImGui::SetCursorScreenPos(ImVec2(start.x + width - closeSize, start.y));
        const bool close = ImGui::Button("##AdventureClose", ImVec2(closeSize, closeSize));
        const ImVec2 a(start.x + width - closeSize + 8.f * scale, start.y + 8.f * scale);
        const ImVec2 b(a.x + 12.f * scale, a.y + 12.f * scale);
        const auto colour = ImGui::GetColorU32(ImGui::IsItemHovered() ? WarmAccent : WarmText);
        draw->AddLine(a, b, colour, 1.8f);
        draw->AddLine(ImVec2(a.x,b.y), ImVec2(b.x,a.y), colour, 1.8f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s · Esc", tr("common.close").c_str());
        ImGui::SetCursorScreenPos(start);
        ImGui::Dummy(ImVec2(width, std::max(closeSize, font) + (stackedSubtitle ? 28.f : 3.f) * scale));
        ImGui::Separator();
        return close;
    }

    bool adventureButton(const char* key, float width = -1.f, bool primary = false,
        Material::ID material = Material::Nothing, GameInterfaceWidgets::Glyph glyph = GameInterfaceWidgets::Glyph::None,
        float height = 0.f)
    {
        const float scale = appliedSettings.uiScale;
        const bool icon = material != Material::Nothing || glyph != GameInterfaceWidgets::Glyph::None;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2((icon ? 40.f : 10.f) * scale, 8.f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(icon ? 0.f : .5f, .5f));
        if (primary) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(98, 86, 52, 155));
            ImGui::PushStyleColor(ImGuiCol_Border, WarmAccent);
        }
        const bool result = ImGui::Button(tr(key).c_str(), ImVec2(width, height));
        if (primary) ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        if (icon)
        {
            const auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
            const ImVec2 at(lo.x + 11.f * scale, (lo.y + hi.y) * .5f - 10.f * scale);
            if (material != Material::Nothing) adventureIcon(material, at, 20.f * scale);
            else GameInterfaceWidgets::glyph(ImGui::GetWindowDrawList(), glyph, at, 20.f * scale);
        }
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        return result;
    }

    bool adventureTab(const char* key, bool selected, float width = 0.f)
    {
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(111, 94, 50, 165));
            ImGui::PushStyleColor(ImGuiCol_Border, WarmAccent);
            ImGui::PushStyleColor(ImGuiCol_Text, WarmAccent);
        }
        const bool clicked = ImGui::Button(tr(key).c_str(), ImVec2(width, 0.f));
        if (selected) ImGui::PopStyleColor(3);
        return clicked;
    }

    void adventureEscape(const char* key, bool centered = false)
    {
        const float scale=appliedSettings.uiScale;
        ImGui::PushFont(nullptr,17.f);
        const auto label=tr(key);
        const float keyWidth=ImGui::CalcTextSize("Esc").x+12.f*scale;
        const float width=keyWidth+8.f*scale+ImGui::CalcTextSize(label.c_str()).x;
        if (centered) ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.f,(ImGui::GetContentRegionAvail().x-width)*.5f));
        const auto at=ImGui::GetCursorScreenPos();
        auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(at,ImVec2(at.x+keyWidth,at.y+23.f*scale),IM_COL32(35,54,59,255),3.f);
        draw->AddRect(at,ImVec2(at.x+keyWidth,at.y+23.f*scale),IM_COL32(115,140,139,210),3.f);
        draw->AddText(ImVec2(at.x+6.f*scale,at.y+3.f*scale),ImGui::GetColorU32(WarmText),"Esc");
        draw->AddText(ImVec2(at.x+keyWidth+8.f*scale,at.y+3.f*scale),ImGui::GetColorU32(WarmMuted),label.c_str());
        ImGui::Dummy(ImVec2(width,23.f*scale)); ImGui::PopFont();
    }

    const char* currentRegionKey() const
    {
        return minimapBiome == TerrainBiome::Desert ? "hud.region_desert" :
            minimapBiome == TerrainBiome::Wetland ? "hud.region_wetland" :
            minimapBiome == TerrainBiome::RockPlateau ? "hud.region_plateau" :
            minimapBiome == TerrainBiome::River ? "hud.region_river" :
            minimapBiome == TerrainBiome::Lake ? "hud.region_lake" :
            minimapBiome == TerrainBiome::Ocean ? "hud.region_ocean" :
            minimapBiome == TerrainBiome::Mountain ? "hud.region_mountain" :
            minimapBiome == TerrainBiome::TemperateForest ? "hud.region_forest" :
            minimapBiome == TerrainBiome::LightForest ? "hud.region_woodland" : "hud.region_meadow";
    }

    void drawPauseMenu()
    {
        const auto& io = ImGui::GetIO();
        const float scale = appliedSettings.uiScale;
        const bool compact=io.DisplaySize.y<520.f*scale;
        const float rowHeight=(compact ? 32.f : 52.f)*scale;
        const float worldHeight=(compact ? 52.f : 66.f)*scale;
        GameInterfaceWidgets::OverlayStyle theme(scale);
        adventureBackdrop();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, io.DisplaySize.y * .5f), ImGuiCond_Always, ImVec2(.5f,.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(380.f * scale,io.DisplaySize.x-32.f),
            std::min(485.f * scale,io.DisplaySize.y-32.f)), ImGuiCond_Always);
        if (ImGui::Begin("##PauseMenu",nullptr,ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground))
        {
            if (adventureHeader(tr("pause.title"),GameInterfaceWidgets::Glyph::Pause,tr(currentRegionKey()),!compact) && flow->resume()) playUiFeedback();
            const float footerHeight = ImGui::GetTextLineHeight() + 35.f * scale + rowHeight*2.f;
            ImGui::BeginChild("##PauseOptions",ImVec2(0,-footerHeight),false);
            if (adventureButton("pause.resume",-1.f,true,Material::Nothing,GameInterfaceWidgets::Glyph::Play,rowHeight) && flow->resume())
                playUiFeedback();
            if (adventureButton("pause.settings",-1.f,false,Material::Nothing,GameInterfaceWidgets::Glyph::Settings,rowHeight))
            {
                settingsSession.begin(appliedSettings);
                settingsMessage.clear();
                settingsApplyPending = false;
                playUiFeedback();
            }
            if (!compact) ImGui::Spacing();
            ImGui::Separator(); ImGui::Spacing();
            const auto worldAt = ImGui::GetCursorScreenPos();
            const float worldWidth = ImGui::GetContentRegionAvail().x;
            const auto foldId = ImGui::GetID("##PauseWorldExpanded");
            bool expanded = ImGui::GetStateStorage()->GetBool(foldId,false);
            if (ImGui::Button("##WorldJourney",ImVec2(worldWidth,worldHeight))) {
                expanded=!expanded; ImGui::GetStateStorage()->SetBool(foldId,expanded);
            }
            adventureIcon(Material::Grass,ImVec2(worldAt.x+10.f*scale,worldAt.y+worldHeight*.5f-15.f*scale),30.f*scale);
            auto* worldDraw = ImGui::GetWindowDrawList();
            const float titleX=worldAt.x+50.f*scale;
            const float copyWidth=std::max(1.f,worldWidth-75.f*scale);
            const auto worldTitle=boundedHudText(tr("pause.world_journey"),ImGui::GetFontSize(),copyWidth);
            worldDraw->AddText(ImVec2(titleX,worldAt.y+(compact ? 5.f : 8.f)*scale),ImGui::GetColorU32(WarmText),worldTitle.c_str());
            if (world != nullptr) {
                const auto objective=world->getObjectiveSnapshot();
                const std::string summary=difficultyName(world->getDifficultySnapshot().active)+" · "+tr("journal.main")+" "+
                    std::to_string(objective.completedObjectives)+" / "+std::to_string(objective.totalObjectives);
                const auto shortSummary=boundedHudText(summary,ImGui::GetFontSize()*.8f,copyWidth);
                worldDraw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.8f,
                    ImVec2(titleX,worldAt.y+(compact ? 30.f : 37.f)*scale),ImGui::GetColorU32(WarmMuted),shortSummary.c_str());
            }
            const ImVec2 chevron(worldAt.x+worldWidth-17.f*scale,worldAt.y+worldHeight*.5f);
            if (expanded) {
                worldDraw->AddLine(ImVec2(chevron.x-4.f*scale,chevron.y-2.f*scale),ImVec2(chevron.x,chevron.y+2.f*scale),ImGui::GetColorU32(WarmText),1.5f);
                worldDraw->AddLine(ImVec2(chevron.x,chevron.y+2.f*scale),ImVec2(chevron.x+4.f*scale,chevron.y-2.f*scale),ImGui::GetColorU32(WarmText),1.5f);
            } else {
                worldDraw->AddLine(ImVec2(chevron.x-2.f*scale,chevron.y-4.f*scale),ImVec2(chevron.x+2.f*scale,chevron.y),ImGui::GetColorU32(WarmText),1.5f);
                worldDraw->AddLine(ImVec2(chevron.x+2.f*scale,chevron.y),ImVec2(chevron.x-2.f*scale,chevron.y+4.f*scale),ImGui::GetColorU32(WarmText),1.5f);
            }
            if (expanded)
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
                        : objectiveInstructionText(objective.currentId,
                                        objective.instruction, objective.guidanceKey);
                ImGui::TextUnformatted(objectiveTitle.c_str());
                ImGui::TextWrapped("%s", objectiveInstruction.c_str());
                if (objective.opportunities.size() > 1 &&
                    ImGui::CollapsingHeader(tr("hud.opportunities").c_str()))
                {
                    for (std::size_t index = 1;
                         index < objective.opportunities.size(); ++index)
                    {
                        const auto& opportunity = objective.opportunities[index];
                        ImGui::TextWrapped("%s", objectiveText(
                            opportunity.id, "title", opportunity.title).c_str());
                        ImGui::TextWrapped("%s", objectiveInstructionText(
                            opportunity.id, opportunity.instruction, opportunity.guidanceKey).c_str());
                    }
                }
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

            }
            ImGui::EndChild();
            ImGui::Separator();
            if (adventureButton("pause.save_main",-1.f,false,Material::Chest,GameInterfaceWidgets::Glyph::None,rowHeight))
            {
                pendingAction.type = OgreUserInterfaceActionType::ReturnToMainMenu;
                playUiFeedback();
            }
            if (adventureButton("pause.save_quit",-1.f,false,Material::Nothing,GameInterfaceWidgets::Glyph::Exit,rowHeight))
            {
                pendingAction.type = OgreUserInterfaceActionType::Quit;
                playUiFeedback();
            }
            ImGui::Separator();
            adventureEscape("pause.resume",true);
            if (!statusMessage.empty() && statusMessageSeconds > 0.f) drawNotification(statusMessage,io.DisplaySize.y - 12.f);
        }
        ImGui::End();
    }

    void drawSettingsMenu()
    {
        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, (io.DisplaySize.y - 64.f) * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        const PresentationWindowLayout layout = fitPresentationWindow(
            io.DisplaySize.x, io.DisplaySize.y - 64.f, 620.0f, 680.0f, appliedSettings.uiScale);
        ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height), ImGuiCond_Always);
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
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * .42f);
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
            const char *visualPreview = draft.visualDetail == VisualDetail::Standard
                ? "settings.visual_standard" : "settings.visual_compatibility";
            ImGui::TextWrapped("%s", tr("settings.visual_detail").c_str());
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::BeginCombo("##VisualDetail", tr(visualPreview).c_str()))
            {
                for (const auto detail : {VisualDetail::Standard, VisualDetail::Compatibility})
                {
                    const char *key = detail == VisualDetail::Standard
                        ? "settings.visual_standard" : "settings.visual_compatibility";
                    if (ImGui::Selectable(tr(key).c_str(), draft.visualDetail == detail))
                        draft.visualDetail = detail;
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", tr("settings.visual_detail_help").c_str());
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
            if (ImGui::BeginCombo(label("settings.minimap_range", "##MinimapRange").c_str(),
                (std::to_string(draft.minimapRange) + " m").c_str()))
            {
                for (const int range : {64, 128, 256})
                    if (ImGui::Selectable((std::to_string(range) + " m").c_str(),
                        draft.minimapRange == range)) draft.minimapRange = range;
                ImGui::EndCombo();
            }
            ImGui::TextWrapped("%s", tr("settings.minimap_help").c_str());
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
                                      keyName(current).c_str()))
                {
                    for (std::size_t keyIndex = 0;
                         keyIndex < GameplayKeyCount; ++keyIndex)
                    {
                        const auto key = static_cast<GameplayKey>(keyIndex);
                        const bool selected = key == current;
                        const std::string option = keyName(key);
                        if (ImGui::Selectable(option.c_str(), selected))
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
                        mouseButtonName(current).c_str()))
                {
                    for (std::size_t buttonIndex = 0;
                         buttonIndex < GameplayMouseButtonCount;
                         ++buttonIndex)
                    {
                        const auto button =
                            static_cast<GameplayMouseButton>(buttonIndex);
                        const bool selected = button == current;
                        const std::string option = mouseButtonName(button);
                        if (ImGui::Selectable(option.c_str(), selected))
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
                sharedMouseBinding(draft.mouseBindings);
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
            ImGui::PopItemWidth();
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
        const auto& callbacks = ImGui::GetPlatformIO();
        if (callbacks.DrawCallback_SetSamplerNearest)
            drawList->AddCallback(callbacks.DrawCallback_SetSamplerNearest, nullptr);
        drawList->AddImage(ImTextureRef(atlasTextureId), minimum, maximum,
                           uvMin, uvMax, tint);
        if (callbacks.DrawCallback_SetSamplerLinear)
            drawList->AddCallback(callbacks.DrawCallback_SetSamplerLinear, nullptr);
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
        const bool hasItem = slot.amount > 0 && materialIconUv(slot.materialId, uvMin, uvMax);
        const auto grip = !hasItem ? PlayerHandPresentation::Grip::Empty :
            itemVisualUsesCube(slot.materialId) ? PlayerHandPresentation::Grip::Block :
            PlayerHandPresentation::Grip::Icon;
        const auto& hand = PlayerHandPresentation::mesh(grip);
        static const ItemVisualGeometry::Mesh emptyGeometry;
        const auto& geometry = hasItem ? itemVisualGeometry(slot.materialId) : emptyGeometry;
        const float intensity = appliedSettings.feedbackIntensity == GameplayFeedbackIntensity::Off
            ? 0.f : appliedSettings.feedbackIntensity == GameplayFeedbackIntensity::Reduced ? .35f : 1.f;
        const bool contactAction = actionFeedback.kind == ActionFeedbackKind::AttackHit ||
            actionFeedback.kind == ActionFeedbackKind::AttackMiss ||
            actionFeedback.kind == ActionFeedbackKind::BlockBreak ||
            actionFeedback.kind == ActionFeedbackKind::BlockPlace ||
            actionFeedback.kind == ActionFeedbackKind::Guard;
        const float recovery = contactAction ? std::clamp(
            actionFeedback.secondsRemaining / .32f, 0.f, 1.f) : 0.f;
        const float contact = actionFeedback.hitStopSeconds > 0.f ? 1.f : recovery * recovery;
        const auto pose = PlayerHandPresentation::motion(hudElapsedSeconds, heldMovement,
            intensity, miningProgress.active, contact);
        const float swing = pose.swing;
        const float hudRight = io.DisplaySize.x * .5f +
            (300.f * appliedSettings.uiScale) * .5f + 12.f;
        // Reserve the complete rotated silhouette and its contact travel in
        // the gutter. Large accessibility UI must not slice a block in half.
        const float size = std::min(std::clamp(io.DisplaySize.y * .24f, 100.f, 200.f),
            std::max(16.f, (io.DisplaySize.x - hudRight - 65.f) / 1.7f));
        const ImVec2 center(io.DisplaySize.x - size * .66f - 18.f - swing * 35.f,
            io.DisplaySize.y - size * .78f - 22.f + pose.bob - swing * 20.f);
        struct ProjectedFace {
            std::array<ImVec2, 4> points, uv;
            float depth;
            ImU32 tint;
            bool textured;
        };
        std::vector<ProjectedFace> faces;
        faces.reserve(geometry.size() + hand.size());
        const auto& atlas = runtimeTerrainMaterialProfile().parameters();
        const float exposure = .42f + .58f * std::clamp(worldStats.environment.daylight, 0.f, 1.f);
        const auto project = [&](const ItemVisualGeometry::Face& face, glm::vec3 colour, bool textured) {
            const glm::vec3 normal = pose.rotate(face.normal);
            glm::vec3 midpoint(0.f);
            for (const auto& vertex : face.positions) midpoint += pose.rotate(vertex) * .25f;
            if (glm::dot(normal, glm::vec3(0,0,3) - midpoint) <= 0.f) return;
            ProjectedFace projected{};
            projected.depth = midpoint.z;
            projected.textured = textured;
            const float light = exposure * (.60f + .40f * std::max(0.f,
                glm::dot(normal, glm::normalize(glm::vec3(-.35f, .65f, 1.f)))));
            projected.tint = IM_COL32(static_cast<int>(colour.r * light),
                static_cast<int>(colour.g * light), static_cast<int>(colour.b * light), 255);
            for (int corner = 0; corner < 4; ++corner) {
                const glm::vec3 point = pose.rotate(face.positions[corner]);
                const float perspective = 3.f / (3.f - point.z);
                projected.points[corner] = ImVec2(center.x + point.x * size * perspective,
                                                  center.y - point.y * size * perspective);
                const auto& uv = face.uv[corner];
                projected.uv[corner] = ImVec2(
                    (face.tile.x * atlas.tilePixels + .5f + uv.x * (atlas.tilePixels - 1.f)) / atlas.atlasPixels,
                    (face.tile.y * atlas.tilePixels + .5f + uv.y * (atlas.tilePixels - 1.f)) / atlas.atlasPixels);
            }
            faces.push_back(projected);
        };
        for (const auto& face : geometry) project(face, glm::vec3(255.f), true);
        for (const auto& face : hand) project(face.geometry, face.colour, false);
        std::sort(faces.begin(), faces.end(), [](const auto& a, const auto& b) { return a.depth < b.depth; });
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->PushClipRect(ImVec2(hudRight, io.DisplaySize.y * .5f), io.DisplaySize, true);
        const auto& callbacks = ImGui::GetPlatformIO();
        if (callbacks.DrawCallback_SetSamplerNearest)
            draw->AddCallback(callbacks.DrawCallback_SetSamplerNearest, nullptr);
        for (const auto& face : faces) {
            if (face.textured)
                draw->AddImageQuad(ImTextureRef(atlasTextureId), face.points[0], face.points[1],
                    face.points[2], face.points[3], face.uv[0], face.uv[1], face.uv[2], face.uv[3], face.tint);
            else
                draw->AddConvexPolyFilled(face.points.data(), 4, face.tint);
        }
        if (callbacks.DrawCallback_SetSamplerLinear)
            draw->AddCallback(callbacks.DrawCallback_SetSamplerLinear, nullptr);
        draw->PopClipRect();
    }

    bool drawInventoryCard(Material::ID materialId, int amount, const std::string& id,
                           ImVec2 size, bool selected = false, bool compact = false,
                           int shortcut = 0)
    {
        const float scale = appliedSettings.uiScale;
        if (!compact) size.y = std::max(size.y, 54.f * scale);
        ImGui::PushID(id.c_str());
        const bool clicked = ImGui::InvisibleButton("##item_card", size, ImGuiButtonFlags_EnableNav);
        ImGui::PopID();
        const bool focused = ImGui::IsItemFocused() && ImGui::GetIO().NavVisible;
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 minimum = ImGui::GetItemRectMin();
        const ImVec2 maximum = ImGui::GetItemRectMax();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        GameInterfaceWidgets::slotFrame(draw, minimum, maximum, selected || focused,
            hovered, ImGui::IsItemActive(), scale);
        draw->PushClipRect(minimum, maximum, true);
        if (amount > 0 && compact)
        {
            const float iconSize = std::min(size.y * .66f, size.x * .64f);
            const float inset = ImGui::IsItemActive() ? 1.f * scale : 0.f;
            const float iconX = minimum.x + (size.x - iconSize) * .5f + inset;
            const float iconY = minimum.y + (size.y - iconSize) * .42f + inset;
            drawMaterialIcon(draw, materialId, ImVec2(iconX, iconY),
                ImVec2(iconX + iconSize, iconY + iconSize));
            if (amount > 1)
            {
                const std::string quantity = std::to_string(amount);
                const ImVec2 textSize = ImGui::CalcTextSize(quantity.c_str());
                const ImVec2 textPos(maximum.x - textSize.x - 5.f * scale,
                                     maximum.y - textSize.y - 3.f * scale);
                draw->AddRectFilled(ImVec2(textPos.x - 3.f, textPos.y),
                    ImVec2(maximum.x - 2.f, maximum.y - 2.f), IM_COL32(12, 20, 24, 215), 2.f);
                draw->AddText(textPos, IM_COL32(242, 237, 222, 255), quantity.c_str());
            }
        }
        else if (amount > 0)
        {
            const float iconSize = std::min(32.f * scale, size.x * .32f);
            drawMaterialIcon(draw, materialId,
                ImVec2(minimum.x + 7.f * scale, minimum.y + (size.y - iconSize) * .5f),
                ImVec2(minimum.x + 7.f * scale + iconSize, minimum.y + (size.y + iconSize) * .5f));
            const std::string fullName = materialName(materialId);
            std::string name = fullName;
            const float textX = minimum.x + iconSize + 13.f * scale;
            const float available = maximum.x - textX - 6.f * scale;
            bool shortened = false;
            while (!name.empty() && ImGui::CalcTextSize((name + (shortened ? "…" : "")).c_str()).x > available)
            {
                std::size_t last = name.size() - 1;
                while (last > 0 && (static_cast<unsigned char>(name[last]) & 0xc0) == 0x80) --last;
                name.erase(last); shortened = true;
            }
            if (shortened) name += "…";
            draw->AddText(ImVec2(textX, minimum.y + 6.f * scale), IM_COL32(242, 237, 222, 255), name.c_str());
            const std::string quantity = "x" + std::to_string(amount);
            draw->AddText(ImVec2(textX, maximum.y - ImGui::GetTextLineHeight() - 5.f * scale),
                IM_COL32(164, 185, 182, 255), quantity.c_str());
        }
        else
        {
            const ImVec2 c((minimum.x + maximum.x) * .5f, (minimum.y + maximum.y) * .5f);
            draw->AddLine(ImVec2(c.x - 4.f, c.y), ImVec2(c.x + 4.f, c.y), IM_COL32(63, 80, 86, 160));
            draw->AddLine(ImVec2(c.x, c.y - 4.f), ImVec2(c.x, c.y + 4.f), IM_COL32(63, 80, 86, 160));
        }
        if (shortcut > 0)
        {
            const std::string key = std::to_string(shortcut);
            draw->AddText(ImVec2(minimum.x + 5.f * scale, minimum.y + 3.f * scale),
                IM_COL32(164, 180, 181, 230), key.c_str());
        }
        draw->PopClipRect();
        if ((hovered || focused) && amount > 0)
        {
            if (!hovered) ImGui::SetNextWindowPos(minimum, ImGuiCond_Always, ImVec2(0.f, 1.f));
            ImGui::SetTooltip("%s  x%d", materialName(materialId).c_str(), amount);
        }
        return clicked;
    }

    void drawInventoryHeading(const std::string& title, const std::string& detail)
    {
        ImGui::TextColored(WarmMuted, "%s", title.c_str());
        if (!detail.empty())
        {
            const float width = ImGui::CalcTextSize(detail.c_str()).x;
            const float right = ImGui::GetWindowContentRegionMax().x;
            if (ImGui::GetCursorPosX() + width < right)
            {
                ImGui::SameLine(std::max(ImGui::GetItemRectSize().x + 24.f,
                    right - width));
                ImGui::TextDisabled("%s", detail.c_str());
            }
        }
    }

    bool drawInventoryHeader(Material::ID icon, const std::string& title,
                             const std::string& subtitle)
    {
        const float scale = appliedSettings.uiScale;
        GameInterfaceWidgets::panelFrame(scale);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float emblem = 34.f * scale;
        const float height = 40.f * scale;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        GameInterfaceWidgets::slotFrame(draw, start, ImVec2(start.x + emblem, start.y + emblem),
            true, false, false, scale);
        drawMaterialIcon(draw, icon, ImVec2(start.x + 6.f * scale, start.y + 6.f * scale),
            ImVec2(start.x + emblem - 6.f * scale, start.y + emblem - 6.f * scale));
        const float textX = start.x + emblem + 12.f * scale;
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.18f,
            ImVec2(textX, start.y - 1.f), IM_COL32(242, 237, 222, 255), title.c_str());
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * .83f,
            ImVec2(textX, start.y + 22.f * scale), IM_COL32(156, 177, 177, 255), subtitle.c_str());
        const float closeSize = 25.f * scale;
        ImGui::SetCursorScreenPos(ImVec2(start.x + width - closeSize, start.y));
        const bool close = ImGui::InvisibleButton("##PanelClose", ImVec2(closeSize, closeSize), ImGuiButtonFlags_EnableNav);
        const ImVec2 lo = ImGui::GetItemRectMin();
        const ImU32 colour = ImGui::IsItemHovered() || ImGui::IsItemFocused()
            ? IM_COL32(242, 215, 157, 255) : IM_COL32(136, 159, 163, 255);
        const float a = 8.f * scale, b = 17.f * scale;
        draw->AddLine(ImVec2(lo.x + a, lo.y + a), ImVec2(lo.x + b, lo.y + b), colour, 1.5f);
        draw->AddLine(ImVec2(lo.x + a, lo.y + b), ImVec2(lo.x + b, lo.y + a), colour, 1.5f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s · Esc", tr("common.close").c_str());
        ImGui::SetCursorScreenPos(start);
        ImGui::Dummy(ImVec2(width, height));
        ImGui::Separator();
        return close;
    }

    void drawItemPortrait(Material::ID material, ImVec2 center, float size)
    {
        if (material == Material::ID::Nothing) return;
        const auto& geometry = itemVisualGeometry(material);
        const auto& atlas = runtimeTerrainMaterialProfile().parameters();
        struct Face { std::array<ImVec2, 4> points, uv; float depth; ImU32 tint; };
        std::vector<Face> faces; faces.reserve(geometry.size());
        const auto rotate = [](glm::vec3 p) {
            const glm::vec3 yaw(.82f * p.x + .57f * p.z, p.y, -.57f * p.x + .82f * p.z);
            return glm::vec3(yaw.x, .90f * yaw.y - .44f * yaw.z, .44f * yaw.y + .90f * yaw.z);
        };
        for (const auto& face : geometry)
        {
            const glm::vec3 normal = rotate(face.normal);
            if (normal.z <= 0.f) continue;
            Face projected{};
            const float light = .65f + .35f * std::max(0.f,
                glm::dot(normal, glm::normalize(glm::vec3(-.35f, .65f, 1.f))));
            const int value = static_cast<int>(255.f * light);
            projected.tint = IM_COL32(value, value, value, 255);
            for (int corner = 0; corner < 4; ++corner)
            {
                const glm::vec3 p = rotate(face.positions[corner]);
                projected.depth += p.z * .25f;
                projected.points[corner] = ImVec2(center.x + p.x * size, center.y - p.y * size);
                projected.uv[corner] = ImVec2(
                    (face.tile.x * atlas.tilePixels + .5f + face.uv[corner].x * (atlas.tilePixels - 1.f)) / atlas.atlasPixels,
                    (face.tile.y * atlas.tilePixels + .5f + face.uv[corner].y * (atlas.tilePixels - 1.f)) / atlas.atlasPixels);
            }
            faces.push_back(projected);
        }
        std::sort(faces.begin(), faces.end(), [](const auto& a, const auto& b) { return a.depth < b.depth; });
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const auto& callbacks = ImGui::GetPlatformIO();
        if (callbacks.DrawCallback_SetSamplerNearest) draw->AddCallback(callbacks.DrawCallback_SetSamplerNearest, nullptr);
        for (const auto& face : faces)
            draw->AddImageQuad(ImTextureRef(atlasTextureId), face.points[0], face.points[1], face.points[2], face.points[3],
                face.uv[0], face.uv[1], face.uv[2], face.uv[3], face.tint);
        if (callbacks.DrawCallback_SetSamplerLinear) draw->AddCallback(callbacks.DrawCallback_SetSamplerLinear, nullptr);
    }

    void drawFieldKitPanel(ImDrawList* draw, ImVec2 lo, ImVec2 hi, float corner) const
    {
        GameInterfaceWidgets::texturedPanel(draw, hudPanelTextureId, lo, hi, corner);
    }

    void drawFieldKitWindow() const
    {
        const ImVec2 lo = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        drawFieldKitPanel(ImGui::GetWindowDrawList(), lo,
            ImVec2(lo.x + size.x, lo.y + size.y), 14.f * appliedSettings.uiScale);
    }

    void drawHudGlyph(ImDrawList* draw, int glyph, ImVec2 lo, float size,
                      ImU32 tint = IM_COL32_WHITE) const
    {
        const auto& callbacks = ImGui::GetPlatformIO();
        if (callbacks.DrawCallback_SetSamplerNearest) draw->AddCallback(callbacks.DrawCallback_SetSamplerNearest, nullptr);
        draw->AddImage(ImTextureRef(hudGlyphTextureId), lo, ImVec2(lo.x + size, lo.y + size),
            ImVec2(glyph * .5f, 0.f), ImVec2((glyph + 1) * .5f, 1.f), tint);
        if (callbacks.DrawCallback_SetSamplerLinear) draw->AddCallback(callbacks.DrawCallback_SetSamplerLinear, nullptr);
    }

    std::string boundedHudText(std::string text, float fontSize, float width, int lines = 1) const
    {
        const auto fits = [&](const std::string& value) {
            const ImVec2 measured = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX,
                lines == 1 ? 0.f : width, value.c_str());
            return measured.x <= width && measured.y <= fontSize * lines + .5f;
        };
        if (fits(text)) return text;
        while (!text.empty())
        {
            std::size_t last = text.size() - 1;
            while (last > 0 && (static_cast<unsigned char>(text[last]) & 0xc0) == 0x80) --last;
            text.erase(last);
            if (fits(text + "…")) return text + "…";
        }
        return "";
    }

    float drawHudActionStrip(float bottom, Material::ID heldMaterial)
    {
        if (!appliedSettings.showActionHints || flow->state() != GameApplicationState::Playing) return bottom;
        const auto& io = ImGui::GetIO();
        const float scale = appliedSettings.uiScale;
        const float fontSize = ImGui::GetFontSize() * .78f;
        std::string hint = "Tab  " + tr(hudInteraction.ownsInput() ? "hud.pointer_resume" : "hud.pointer_show");
        if (hudInteraction.ownsInput())
            hint += "   ·   " + keyName(appliedSettings.inputBindings.get(GameplayAction::OpenCrafting)) + "  " + tr("hint.crafting");
        else if (runtimeFoodRegistry().find(heldMaterial) && worldStats.playerHealth < worldStats.playerMaxHealth)
            hint += "   ·   " + keyName(appliedSettings.inputBindings.get(GameplayAction::ConsumeFood)) + "  " + tr("hint.eat");
        else if (const auto* tool = runtimeToolRegistry().find(heldMaterial);
                 tool && tool->miningClass == MiningClass::Weapon)
            hint += "   ·   " + mouseButtonName(appliedSettings.mouseBindings.get(GameplayWorldAction::Guard)) + "  " + tr("action.guard");
        const float lane = (io.DisplaySize.x - 300.f * scale) * .5f - 32.f;
        const bool beside = lane >= 145.f * scale;
        const float available = beside ? lane : io.DisplaySize.x - 36.f;
        const auto text = boundedHudText(hint, fontSize, available, 2);
        const ImVec2 measured = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, available, text.c_str());
        const ImVec2 at(18.f, beside ? io.DisplaySize.y - 22.f - measured.y : bottom - measured.y);
        auto* draw = ImGui::GetForegroundDrawList();
        draw->AddText(ImGui::GetFont(), fontSize, ImVec2(at.x + 1.f, at.y + 1.f),
            IM_COL32(8, 16, 20, 230), text.c_str(), nullptr, available);
        draw->AddText(ImGui::GetFont(), fontSize, at, IM_COL32(236, 237, 223, 255),
            text.c_str(), nullptr, available);
        return beside ? bottom : at.y - 10.f * scale;
    }

    void drawItemDetails(const InventorySlotState& slot, int hotbarIndex)
    {
        const float scale = appliedSettings.uiScale;
        const float width = std::min(320.f * scale, ImGui::GetIO().DisplaySize.x - 24.f);
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.f), ImVec2(width, FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f * scale, 12.f * scale));
        if (ImGui::BeginTooltip())
        {
            const auto lo = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
            GameInterfaceWidgets::surface(ImGui::GetWindowDrawList(), lo,
                ImVec2(lo.x + size.x, lo.y + size.y), false, scale);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 28.f * scale);
            if (slot.amount <= 0 || slot.materialId == Material::Nothing)
            {
                ImGui::TextColored(WarmAccent, "%s %d", tr("item.empty_slot").c_str(), hotbarIndex + 1);
                ImGui::TextWrapped("%s", tr("item.empty_help").c_str());
            }
            else
            {
                const auto& material = Material::toMaterial(slot.materialId);
                const auto* tool = runtimeToolRegistry().find(slot.materialId);
                const auto* food = runtimeFoodRegistry().find(slot.materialId);
                ImGui::TextColored(WarmAccent, "%s", materialName(slot.materialId).c_str());
                ImGui::TextDisabled("%s  ·  %s %d / %d", tr(tool ? "item.kind_tool" : food ? "item.kind_food" :
                    material.isBlock ? "item.kind_block" : "item.kind_material").c_str(),
                    tr("item.stack").c_str(), slot.amount, material.maxStackSize);
                ImGui::Separator();
                std::string materialKey = Material::toStringId(slot.materialId);
                const auto separator = materialKey.find(':');
                if (separator != std::string::npos) materialKey.erase(0, separator + 1);
                const std::string helpKey = "item.help." + materialKey;
                const std::string fallback = tr(tool ? "item.tool_help" : food ? "item.food_help" :
                    material.isBlock ? "item.block_help" : "item.material_help");
                const auto& texts = runtimeLocalizedTextRegistry();
                ImGui::TextWrapped("%s", (texts.hasKey(appliedSettings.locale, helpKey) ||
                    texts.hasKey("en-US", helpKey) ? tr(helpKey) : fallback).c_str());
                if (tool != nullptr)
                {
                    ImGui::Spacing();
                    ImGui::Text("%s  %s", tr("item.specialty").c_str(),
                        tr(std::string("item.mining.") + ToolRegistry::miningClassName(tool->miningClass)).c_str());
                    ImGui::Text("%s  %d / %d", tr("item.durability").c_str(),
                        std::clamp(slot.durability, 0, tool->maxDurability), tool->maxDurability);
                    ImGui::ProgressBar(tool->maxDurability > 0 ? std::clamp(
                        static_cast<float>(slot.durability) / tool->maxDurability, 0.f, 1.f) : 0.f,
                        ImVec2(-1.f, 5.f * scale), "");
                    ImGui::Text("%s  %d", tr("item.tier").c_str(), tool->tier);
                    ImGui::Text("%s  %.1f×", tr("item.mining_speed").c_str(), tool->speedMultiplier);
                    ImGui::Text("%s  %.1f   ·   %s  %.1f m", tr("item.damage").c_str(), tool->attackDamage,
                        tr("item.reach").c_str(), tool->attackReach);
                    ImGui::Text("%s  %.1f s", tr("item.attack_cooldown").c_str(), tool->attackCooldownTicks / 20.f);
                }
                if (food != nullptr)
                {
                    ImGui::Spacing();
                    ImGui::Text("%s  +%.1f", tr("item.healing").c_str(), food->healthRestored);
                    ImGui::Text("%s  %.1f s", tr("item.food_cooldown").c_str(), food->cooldownTicks / 20.f);
                    ImGui::TextWrapped("%s · %s", keyName(appliedSettings.inputBindings.get(GameplayAction::ConsumeFood)).c_str(),
                        tr("item.eat_help").c_str());
                }
                ImGui::Spacing();
                ImGui::TextDisabled("%s %d  ·  %s", tr("item.slot").c_str(), hotbarIndex + 1,
                    tr("item.select_hint").c_str());
            }
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar();
    }

    void drawHotbarSlot(const InventorySlotState &slot,
                        std::size_t index, bool selected)
    {
        const float scale = appliedSettings.uiScale;
        const float slotSize = 52.f * scale;
        ImGui::PushID(static_cast<int>(index));
        const bool clicked = ImGui::InvisibleButton("##hotbar_slot", ImVec2(slotSize, slotSize));
        const bool fixture = hudPageFixtureOpened && inspectSlotFixture == static_cast<int>(index);
        const bool hovered = hudInteraction.ownsInput() && (ImGui::IsItemHovered() || fixture);
        if (clicked && hudInteraction.ownsInput())
        {
            pendingAction.type = OgreUserInterfaceActionType::SelectHotbar;
            pendingAction.hotbarSlot = static_cast<int>(index);
            if (uiFeedback) uiFeedback();
        }
        const ImVec2 minimum = ImGui::GetItemRectMin();
        const ImVec2 maximum = ImGui::GetItemRectMax();
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        GameInterfaceWidgets::slotFrame(drawList, minimum, maximum, selected, hovered, ImGui::IsItemActive(), scale);

        const std::string key = std::to_string(index + 1);
        drawList->AddText(ImVec2(minimum.x + 5.f * scale, minimum.y + 2.f * scale),
                          IM_COL32(202, 211, 209, 255), key.c_str());
        if (slot.amount > 0)
        {
            drawMaterialIcon(drawList, slot.materialId,
                             ImVec2(minimum.x + 12.f * scale, minimum.y + 8.f * scale),
                             ImVec2(maximum.x - 7.f * scale, maximum.y - 11.f * scale));
            const ToolDefinition *tool =
                runtimeToolRegistry().find(slot.materialId);
            if (tool != nullptr && tool->maxDurability > 0)
            {
                const float durability = std::clamp(
                    static_cast<float>(slot.durability) /
                        static_cast<float>(tool->maxDurability),
                    0.f, 1.f);
                const ImVec2 barMin(minimum.x + 5.f * scale, maximum.y - 7.f * scale);
                const ImVec2 barMax(maximum.x - 5.f * scale, maximum.y - 4.f * scale);
                drawList->AddRectFilled(barMin, barMax,
                                        IM_COL32(20, 30, 23, 230));
                drawList->AddRectFilled(
                    barMin,
                    ImVec2(barMin.x + (barMax.x - barMin.x) * durability,
                           barMax.y),
                    durability > 0.35f
                        ? IM_COL32(156, 184, 115, 255)
                        : IM_COL32(209, 122, 94, 255));
            }
            else
            {
                const std::string amount = std::to_string(slot.amount);
                const ImVec2 amountSize = ImGui::CalcTextSize(amount.c_str());
                drawList->AddText(
                    ImVec2(maximum.x - amountSize.x - 4.f,
                           maximum.y - amountSize.y - 3.f * scale),
                    IM_COL32(244, 238, 220, 255), amount.c_str());
            }
        }
        else
        {
            const char *emptyMark = "";
            const ImVec2 markSize = ImGui::CalcTextSize(emptyMark);
            drawList->AddText(
                ImVec2((minimum.x + maximum.x - markSize.x) * 0.5f,
                       (minimum.y + maximum.y - markSize.y) * 0.5f),
                IM_COL32(123, 141, 119, 150), emptyMark);
        }
        if (hovered)
        {
            hotbarDetailsVisible = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (fixture)
                ImGui::SetNextWindowPos(ImVec2(std::clamp(minimum.x, 12.f,
                    std::max(12.f, ImGui::GetIO().DisplaySize.x - 320.f * scale - 12.f)), minimum.y - 12.f),
                    ImGuiCond_Always, ImVec2(0.f,1.f));
            drawItemDetails(slot, static_cast<int>(index));
        }
        ImGui::PopID();
    }

    using MinimapCell = SurfaceMapSample;
    static constexpr int MinimapCellCount = 65;


    static ImU32 mapCellColour(const MinimapCell* cells, int count, int x, int z)
    {
        const auto& cell = cells[z * count + x];
        if (!cell.known) return IM_COL32(30, 39, 45, 255);
        ImVec4 colour;
        switch (cell.material)
        {
            case BlockId::Grass: colour = ImVec4(126, 150, 86, 255); break;
            case BlockId::OakLeaf: colour = ImVec4(49, 91, 63, 255); break;
            case BlockId::Water: colour = ImVec4(58, 133, 158, 255); break;
            case BlockId::Sand: colour = ImVec4(204, 186, 132, 255); break;
            case BlockId::Snow: colour = ImVec4(227, 235, 239, 255); break;
            case BlockId::Gravel: colour = ImVec4(147, 143, 135, 255); break;
            case BlockId::Clay: colour = ImVec4(168, 104, 75, 255); break;
            case BlockId::ForestFloor: colour = ImVec4(99, 76, 49, 255); break;
            case BlockId::MossStone: colour = ImVec4(109, 120, 82, 255); break;
            case BlockId::Silt: colour = ImVec4(101, 83, 65, 255); break;
            case BlockId::Dirt: colour = ImVec4(132, 106, 79, 255); break;
            case BlockId::OakBark: colour = ImVec4(104, 82, 58, 255); break;
            case BlockId::OakPlank:
            case BlockId::Chest:
            case BlockId::Workbench:
            case BlockId::OakDoorClosed:
            case BlockId::OakDoorOpen: colour = ImVec4(189, 147, 93, 255); break;
            case BlockId::WaystoneCore: colour = ImVec4(116, 195, 210, 255); break;
            case BlockId::Cactus: colour = ImVec4(77, 117, 75, 255); break;
            case BlockId::Air: colour = ImVec4(37, 43, 46, 255); break;
            default: colour = ImVec4(157, 157, 145, 255); break;
        }
        const auto& west = cells[z * count + std::max(0, x - 1)];
        const auto& north = cells[std::max(0, z - 1) * count + x];
        const int slope = (west.known ? west.height - cell.height : 0) +
                          (north.known ? north.height - cell.height : 0);
        const float shade = std::clamp(1.f - slope * 0.055f, 0.72f, 1.18f);
        return IM_COL32(static_cast<int>(std::clamp(colour.x * shade, 0.f, 255.f)),
                        static_cast<int>(std::clamp(colour.y * shade, 0.f, 255.f)),
                        static_cast<int>(std::clamp(colour.z * shade, 0.f, 255.f)), 255);
    }

    ImU32 minimapCellColour(int x, int z) const
    {
        return mapCellColour(minimapCells.data(), MinimapCellCount, x, z);
    }

    void refreshMinimap(const PlayerSaveState& state)
    {
        if (world == nullptr) { minimapValid = false; return; }
        const int requestedStep = MinimapNavigation::cellStep(appliedSettings.minimapRange);
        const bool scaleChanged = requestedStep != minimapStep;
        minimapStep = requestedStep;
        const auto encounter = world->getWaystoneEncounterSnapshot();
        if (encounter.anchorKnown)
            navigationMemory.observe(encounter.anchor, MinimapNavigation::Kind::Waystone);
        const int centerX = World::floorDiv(World::toBlockCoord(state.position.x),
                                           minimapStep) * minimapStep;
        const int centerZ = World::floorDiv(World::toBlockCoord(state.position.z),
                                           minimapStep) * minimapStep;
        const bool identityChanged = minimapSeed != worldStats.terrainSeed ||
            minimapGenerationVersion != worldStats.terrainGenerationVersion;
        if (!minimapValid || identityChanged || scaleChanged ||
            minimapCenterX != centerX || minimapCenterZ != centerZ)
        {
            const auto old = minimapCells;
            minimapCells.fill({});
            ++minimapRevision;
            selectedMapCell = -1;
            // Retain only samples at the exact same world coordinates.
            if (minimapValid && !identityChanged && !scaleChanged)
            {
                const int dx = (centerX - minimapCenterX) / minimapStep;
                const int dz = (centerZ - minimapCenterZ) / minimapStep;
                for (int z = 0; z < MinimapCellCount; ++z)
                for (int x = 0; x < MinimapCellCount; ++x)
                    if (x + dx >= 0 && x + dx < MinimapCellCount &&
                        z + dz >= 0 && z + dz < MinimapCellCount)
                        minimapCells[z * MinimapCellCount + x] =
                            old[(z + dz) * MinimapCellCount + x + dx];
            }
            minimapCenterX = centerX;
            minimapCenterZ = centerZ;
            minimapSeed = worldStats.terrainSeed;
            minimapGenerationVersion = worldStats.terrainGenerationVersion;
            minimapBiome = world->getChunkManager().getTerrainGenerator()
                .getBiomeAtWorld(centerX, centerZ);
            minimapValid = true;
        }
        if (hudElapsedSeconds < minimapNextRefresh) return;
        minimapNextRefresh = hudElapsedSeconds + 1.0 / 30.0;
        constexpr int rowsPerRefresh = 3;
        std::vector<VectorXZ> positions;
        positions.reserve(MinimapCellCount * rowsPerRefresh);
        for (int row = 0; row < rowsPerRefresh; ++row)
        for (int x = 0; x < MinimapCellCount; ++x)
            positions.push_back({centerX + (x - MinimapCellCount / 2) * minimapStep,
                centerZ + ((minimapRefreshRow + row) % MinimapCellCount -
                           MinimapCellCount / 2) * minimapStep});
        const auto samples = world->observeSurfaceMap(positions);
        if (samples.empty()) return; // Lock contention defers observation only.
        for (int row = 0; row < rowsPerRefresh; ++row)
        for (int x = 0; x < MinimapCellCount; ++x)
        {
            auto& cell = minimapCells[((minimapRefreshRow + row) % MinimapCellCount) * MinimapCellCount + x];
            const auto& sample = samples[row * MinimapCellCount + x];
            if (cell.known != sample.known || cell.height != sample.height || cell.material != sample.material)
                ++minimapRevision;
            cell = sample;
        }
        minimapRefreshRow = (minimapRefreshRow + rowsPerRefresh) % MinimapCellCount;
    }

    void drawMinimap(const PlayerSaveState& state, const ImGuiIO& io)
    {
        refreshMinimap(state);
        minimapOverlayWidth = 0.f;
        if (!minimapValid || camera == nullptr)
        {
            return;
        }

        const float scale = appliedSettings.uiScale;
        const auto trackedMarker = world->trackedExplorationMarker();
        const float mapDiameter = std::min(148.f * scale, io.DisplaySize.y * .25f);
        const char* regionKey = currentRegionKey();
        const float labelFont = ImGui::GetFontSize() * .85f;
        const std::string region = tr(regionKey);
        const float regionWidth = ImGui::GetFont()->CalcTextSizeA(labelFont, FLT_MAX, 0.f, region.c_str()).x;
        const float routeFont = ImGui::GetFontSize() * .65f;
        std::string trackingLine;
        if (trackedMarker)
        {
            constexpr const char* directions[] = {
                "map.direction_n", "map.direction_ne", "map.direction_e",
                "map.direction_se", "map.direction_s", "map.direction_sw",
                "map.direction_w", "map.direction_nw"};
            const auto bearing = ExplorationNavigation::toward(
                World::toBlockCoord(state.position.x),
                World::toBlockCoord(state.position.z),
                trackedMarker->worldX, trackedMarker->worldZ);
            trackingLine = tr("map.marker_tracking") + "  " +
                tr(directions[bearing.octant]) + "  " +
                std::to_string(bearing.metres) + " m";
        }
        const float trackingWidth = trackingLine.empty() ? 0.f :
            ImGui::GetFont()->CalcTextSizeA(routeFont, FLT_MAX, 0.f,
                trackingLine.c_str()).x;
        // Reserve the cardinal badges outside the terrain disc, including at
        // the smallest window size and largest text setting.
        const float bezelDiameter = mapDiameter + 24.f * scale;
        const float bezelInset = (bezelDiameter - mapDiameter) * .5f;
        const float windowWidth = std::max({bezelDiameter,
            regionWidth + 20.f * scale, trackingWidth + 16.f * scale});
        const ImVec2 windowSize(windowWidth,
            bezelDiameter + (trackedMarker ? 67.f : 49.f) * scale);
        minimapOverlayWidth = windowWidth;
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - 18.f, 18.f), ImGuiCond_Always,
            ImVec2(1.f, 0.f));
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.f);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings |
            (hudInteraction.ownsInput() ? 0 : ImGuiWindowFlags_NoInputs) |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            ImVec2(0.f, 0.f));
        if (ImGui::Begin("##Minimap", nullptr, flags))
        {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 mapMin(origin.x + (windowWidth - mapDiameter) * .5f, origin.y + bezelInset);
            const ImVec2 mapCenter(mapMin.x + mapDiameter * 0.5f,
                                   mapMin.y + mapDiameter * 0.5f);
            const float radius = mapDiameter * 0.5f - 4.f;
            const float cellSize = mapDiameter /
                static_cast<float>(MinimapCellCount);
            std::array<ImVec2, MinimapClipSegments> circle{};
            for (int index = 0; index < MinimapClipSegments; ++index)
            {
                constexpr float TwoPi = 6.28318530718f;
                const float angle = TwoPi * static_cast<float>(index) /
                    static_cast<float>(MinimapClipSegments);
                circle[index] = ImVec2(
                    mapCenter.x + radius * std::cos(angle),
                    mapCenter.y + radius * std::sin(angle));
            }
            draw->AddCircleFilled(mapCenter, radius,
                                  IM_COL32(17, 28, 25, 255),
                                  MinimapClipSegments);
            for (int mapZ = 0; mapZ < MinimapCellCount; ++mapZ)
            {
                for (int mapX = 0; mapX < MinimapCellCount; ++mapX)
                {
                    const float x0 = mapMin.x + mapX * cellSize;
                    const float y0 = mapMin.y + mapZ * cellSize;
                    fillCircularMinimapCell(draw,
                        ImVec2(x0, y0),
                        ImVec2(x0 + cellSize + 0.6f,
                               y0 + cellSize + 0.6f),
                        mapCenter, radius, circle,
                        minimapCellColour(mapX, mapZ));
                }
            }

            constexpr int half = MinimapCellCount / 2;
            for (int index = 0; index < MinimapCellCount; ++index)
            {
                const int worldX = minimapCenterX +
                    (index - half) * minimapStep;
                const int worldZ = minimapCenterZ +
                    (index - half) * minimapStep;
                if (showDebugPanel && worldX % CHUNK_SIZE == 0)
                {
                    const float x = mapMin.x +
                        (static_cast<float>(index) + 0.5f) * cellSize;
                    const float dx = x - mapCenter.x;
                    const float extent = std::sqrt(std::max(
                        0.f, radius * radius - dx * dx));
                    draw->AddLine(ImVec2(x, mapCenter.y - extent),
                                  ImVec2(x, mapCenter.y + extent),
                                  IM_COL32(236, 238, 220, 34), 1.f);
                }
                if (showDebugPanel && worldZ % CHUNK_SIZE == 0)
                {
                    const float y = mapMin.y +
                        (static_cast<float>(index) + 0.5f) * cellSize;
                    const float dy = y - mapCenter.y;
                    const float extent = std::sqrt(std::max(
                        0.f, radius * radius - dy * dy));
                    draw->AddLine(ImVec2(mapCenter.x - extent, y),
                                  ImVec2(mapCenter.x + extent, y),
                                  IM_COL32(236, 238, 220, 34), 1.f);
                }
            }
            draw->AddCircle(mapCenter, radius + 1.f, IM_COL32(24, 38, 40, 235), MinimapClipSegments, 5.f);
            draw->AddCircle(mapCenter, radius, IM_COL32(134, 155, 144, 245), MinimapClipSegments, 2.f);
            const char* cardinalKeys[] = {"hud.minimap_east", "hud.minimap_south",
                                          "hud.minimap_west", "hud.minimap_north"};
            const char* cardinalFallbacks[] = {"E", "S", "W", "N"};
            const float cardinalFont = ImGui::GetFontSize() * .78f;
            for (int direction = 0; direction < 4; ++direction)
            {
                const float angle = direction * glm::pi<float>() * .5f;
                const ImVec2 axis(std::cos(angle), std::sin(angle));
                draw->AddLine(ImVec2(mapCenter.x + axis.x * (radius - 11.f * scale), mapCenter.y + axis.y * (radius - 11.f * scale)),
                    ImVec2(mapCenter.x + axis.x * (radius + 3.f * scale), mapCenter.y + axis.y * (radius + 3.f * scale)),
                    IM_COL32(216, 224, 202, 245), 2.f);
                const std::string label = tr(cardinalKeys[direction], cardinalFallbacks[direction]);
                const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(cardinalFont, FLT_MAX, 0.f, label.c_str());
                const ImVec2 badgeCenter(mapCenter.x + axis.x * (radius + 4.f * scale),
                    mapCenter.y + axis.y * (radius + 4.f * scale));
                const float halfBadge = 9.f * scale;
                const ImVec2 badgeMin(badgeCenter.x - halfBadge, badgeCenter.y - halfBadge);
                const ImVec2 badgeMax(badgeCenter.x + halfBadge, badgeCenter.y + halfBadge);
                draw->AddRectFilled(badgeMin, badgeMax, IM_COL32(32, 51, 57, 238), 2.f * scale);
                draw->AddRect(badgeMin, badgeMax, IM_COL32(94, 120, 121, 160), 2.f * scale);
                draw->AddText(ImGui::GetFont(), cardinalFont,
                    ImVec2(badgeCenter.x - textSize.x * .5f, badgeCenter.y - textSize.y * .5f),
                    IM_COL32(237, 239, 222, 255), label.c_str());
            }

            const float playerDx =
                (state.position.x - static_cast<float>(minimapCenterX)) /
                static_cast<float>(minimapStep) * cellSize;
            const float playerDy =
                (state.position.z - static_cast<float>(minimapCenterZ)) /
                static_cast<float>(minimapStep) * cellSize;
            const ImVec2 marker(mapCenter.x + playerDx,
                                mapCenter.y + playerDy);
            const auto heading = ExplorationNavigation::heading(state.rotation.y);
            const float forwardX = heading.x;
            const float forwardY = heading.z;
            const float markerLength = 10.f;
            const float markerWidth = 5.f;
            const ImVec2 tip(marker.x + forwardX * markerLength,
                             marker.y + forwardY * markerLength);
            const ImVec2 left(
                marker.x - forwardX * 5.f - forwardY * markerWidth,
                marker.y - forwardY * 5.f + forwardX * markerWidth);
            const ImVec2 right(
                marker.x - forwardX * 5.f + forwardY * markerWidth,
                marker.y - forwardY * 5.f - forwardX * markerWidth);
            MinimapNavigation::MarkerLayout markerLayout;
            // Most recently encountered anchors win when markers overlap.
            const auto& landmarks = navigationMemory.landmarks();
            for (auto it = landmarks.rbegin(); it != landmarks.rend(); ++it)
            {
                const float dx = (it->position.x + .5f - minimapCenterX) /
                    minimapStep * cellSize;
                const float dy = (it->position.z + .5f - minimapCenterZ) /
                    minimapStep * cellSize;
                if (!markerLayout.reserve(dx, dy, radius)) continue;
                const ImVec2 point(mapCenter.x + dx, mapCenter.y + dy);
                draw->AddCircleFilled(point, 6.f, IM_COL32(15, 25, 31, 240), 16);
                if (it->kind == MinimapNavigation::Kind::Waystone)
                    draw->AddQuadFilled(ImVec2(point.x, point.y - 5.f),
                        ImVec2(point.x + 4.f, point.y), ImVec2(point.x, point.y + 5.f),
                        ImVec2(point.x - 4.f, point.y), IM_COL32(127, 218, 228, 255));
                else if (it->kind == MinimapNavigation::Kind::Workbench)
                {
                    draw->AddLine(ImVec2(point.x - 4, point.y - 2),
                        ImVec2(point.x + 4, point.y - 2), IM_COL32(242, 211, 151, 255), 2);
                    draw->AddLine(ImVec2(point.x - 3, point.y - 2),
                        ImVec2(point.x - 3, point.y + 4), IM_COL32(242, 211, 151, 255), 2);
                    draw->AddLine(ImVec2(point.x + 3, point.y - 2),
                        ImVec2(point.x + 3, point.y + 4), IM_COL32(242, 211, 151, 255), 2);
                }
                else draw->AddRectFilled(ImVec2(point.x - 3.5f, point.y - 3.f),
                    ImVec2(point.x + 3.5f, point.y + 3.f), IM_COL32(232, 186, 124, 255), 1.f);
            }
            if (trackedMarker)
            {
                const float dx = static_cast<float>(
                    static_cast<std::int64_t>(trackedMarker->worldX) -
                    World::toBlockCoord(state.position.x));
                const float dz = static_cast<float>(
                    static_cast<std::int64_t>(trackedMarker->worldZ) -
                    World::toBlockCoord(state.position.z));
                const float length = std::hypot(dx, dz);
                if (length > 0.5f)
                {
                    const float reach = std::min(length /
                        minimapStep * cellSize, radius - 10.f * scale);
                    const ImVec2 target(mapCenter.x + dx / length * reach,
                                        mapCenter.y + dz / length * reach);
                    draw->AddCircleFilled(target, 6.f * scale,
                        IM_COL32(17, 29, 35, 255));
                    draw->AddCircleFilled(target, 3.f * scale,
                        trackedMarker->kind == ExplorationMarkers::Kind::Home
                            ? IM_COL32(242, 211, 151, 255)
                            : IM_COL32(127, 218, 228, 255));
                }
            }
            GameInterfaceWidgets::playerArrow(draw, tip, left, right);

            // The open label stack follows the compass concept. A restrained
            // light keyline keeps its dark lettering readable over night scenes.
            const ImVec2 plaqueMin(origin.x, origin.y + bezelDiameter);
            const auto navigationText = [&](float font, ImVec2 at, ImU32 colour, const char* text) {
                for (const ImVec2 offset : {ImVec2(-1.f, 0.f), ImVec2(1.f, 0.f), ImVec2(0.f, -1.f), ImVec2(0.f, 1.f)})
                    draw->AddText(ImGui::GetFont(), font, ImVec2(at.x + offset.x * .75f, at.y + offset.y * .75f), IM_COL32(211, 224, 211, 185), text);
                draw->AddText(ImGui::GetFont(), font, at, colour, text);
            };
            navigationText(labelFont, ImVec2(mapCenter.x - regionWidth * .5f, plaqueMin.y + 5.f * scale),
                IM_COL32(20, 42, 47, 255), region.c_str());
            // End-to-end distance includes the centered label gap. Use half
            // the sampled width so both arms remain visible at large UI scales.
            const float scaleLength = mapDiameter * 32.f / MinimapCellCount;
            const std::string scaleLabel = std::to_string(32 * minimapStep) + " m";
            const float smallFont = ImGui::GetFontSize() * .65f;
            const float textWidth = ImGui::GetFont()->CalcTextSizeA(smallFont, FLT_MAX, 0.f, scaleLabel.c_str()).x;
            const ImVec2 scaleStart(mapCenter.x - scaleLength * .5f,
                plaqueMin.y + 30.f * scale);
            const float labelGap = textWidth * .5f + 5.f * scale;
            const auto scaleLine = [&](ImVec2 from, ImVec2 to) {
                draw->AddLine(from, to, IM_COL32(211, 224, 211, 185), 3.f);
                draw->AddLine(from, to, IM_COL32(20, 42, 47, 255), 1.5f);
            };
            scaleLine(scaleStart, ImVec2(mapCenter.x - labelGap, scaleStart.y));
            scaleLine(ImVec2(mapCenter.x + labelGap, scaleStart.y), ImVec2(scaleStart.x + scaleLength, scaleStart.y));
            for (const float x : {scaleStart.x, scaleStart.x + scaleLength})
                scaleLine(ImVec2(x, scaleStart.y - 3.f * scale), ImVec2(x, scaleStart.y + 3.f * scale));
            navigationText(smallFont, ImVec2(mapCenter.x - textWidth * .5f,
                scaleStart.y - smallFont * .5f), IM_COL32(20, 42, 47, 255), scaleLabel.c_str());
            if (!trackingLine.empty())
            {
                navigationText(routeFont,
                    ImVec2(mapCenter.x - trackingWidth * .5f,
                           plaqueMin.y + 48.f * scale),
                    IM_COL32(20, 42, 47, 255), trackingLine.c_str());
            }
            if (world->explorationMapStatus().needsAttention())
            {
                const ImVec2 badge(mapCenter.x + radius * .72f,
                                   mapCenter.y + radius * .72f);
                draw->AddCircleFilled(badge, 9.f * scale, IM_COL32(35,47,48,255));
                draw->AddCircle(badge, 9.f * scale, IM_COL32(235,192,112,255), 16, 1.5f);
                const auto textSize = ImGui::CalcTextSize("!");
                draw->AddText(ImVec2(badge.x - textSize.x * .5f,
                    badge.y - textSize.y * .5f), IM_COL32(255,218,140,255), "!");
            }
            if (hudInteraction.ownsInput())
            {
                ImGui::SetCursorScreenPos(origin);
                if (ImGui::InvisibleButton("##OpenTerrainMap", windowSize))
                {
                    mapView = {}; selectedMapCell = -1; mapGesture.button = -1;
                    overviewOffsetX = overviewOffsetZ = 0;
                    overviewPanRemainderX = overviewPanRemainderZ = 0.f;
                    selectedOverviewCell = -1;
                    mapMarkerPanel = false;
                    hudInteraction.open(HudInteraction::Page::Map);
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    draw->AddCircle(mapCenter, radius + 3.f, IM_COL32(244,208,132,255), MinimapClipSegments, 2.f);
                    ImGui::SetTooltip("%s", tr(world->explorationMapStatus().needsAttention()
                        ? "map.status_notice" : "map.open").c_str());
                }
            }
        }
        hudNoticeRightTop = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y + 10.f;
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void zoomOverview(float factor)
    {
        const int oldStep = overviewScale.step;
        overviewScale.change(factor);
        overviewAutoFit = false;
        overviewFollowsFit = false;
        if (oldStep != overviewScale.step) {
            overviewValid = false;
            selectedOverviewCell = -1;
            overviewPanRemainderX = overviewPanRemainderZ = 0.f;
        }
    }

    void mapMetric(const char* key, const std::string& value)
    {
        const auto label=tr(key);
        const float width=ImGui::GetContentRegionAvail().x;
        const float valueWidth=ImGui::CalcTextSize(value.c_str()).x;
        const bool sameRow=ImGui::CalcTextSize(label.c_str()).x+valueWidth+12.f*appliedSettings.uiScale<=width;
        ImGui::TextColored(WarmMuted,"%s",label.c_str());
        const float x=ImGui::GetWindowContentRegionMax().x-valueWidth;
        if (sameRow) ImGui::SameLine(x);
        else ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),x));
        ImGui::TextUnformatted(value.c_str());
    }

    void mapLegendRow(const char* key, ImU32 colour, int kind)
    {
        const float scale = appliedSettings.uiScale;
        const auto at = ImGui::GetCursorScreenPos();
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 c(at.x+11.f*scale,at.y+11.f*scale);
        if (kind==0) GameInterfaceWidgets::playerArrow(draw,ImVec2(c.x,c.y-8.f*scale),
            ImVec2(c.x-5.f*scale,c.y+5.f*scale),ImVec2(c.x+5.f*scale,c.y+5.f*scale));
        else if (kind==1) draw->AddQuadFilled(ImVec2(c.x,c.y-7.f*scale),ImVec2(c.x+7.f*scale,c.y),
            ImVec2(c.x,c.y+7.f*scale),ImVec2(c.x-7.f*scale,c.y),colour);
        else { draw->AddRectFilled(at,ImVec2(at.x+21.f*scale,at.y+21.f*scale),colour,2.f);
               draw->AddRect(at,ImVec2(at.x+21.f*scale,at.y+21.f*scale),IM_COL32(91,117,119,170),2.f); }
        ImGui::Dummy(ImVec2(24.f*scale,24.f*scale)); ImGui::SameLine();
        ImGui::TextUnformatted(tr(key).c_str());
    }

    void mapSurfaceCard(const MinimapCell* cell, int x, int z)
    {
        const float scale = appliedSettings.uiScale;
        const auto at = ImGui::GetCursorScreenPos();
        if (cell && cell->known) {
            adventureIcon(Material::toMaterial(cell->material).id,at,38.f*scale);
            ImGui::Indent(49.f*scale);
            ImGui::TextWrapped("%s",LocalizedPresentation::surfaceName(appliedSettings.locale,cell->material).c_str());
            ImGui::SetWindowFontScale(.70f);
            ImGui::Text("X %d  Y %d  Z %d",x,cell->height,z);
            ImGui::SetWindowFontScale(.82f);
            ImGui::Unindent(49.f*scale);
            ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),at.y-ImGui::GetWindowPos().y+49.f*scale));
        } else {
            ImGui::TextColored(WarmMuted,"%s",tr("map.unexplored").c_str());
            ImGui::SetWindowFontScale(.72f);
            ImGui::TextWrapped("%s",tr("map.select").c_str());
            ImGui::SetWindowFontScale(.82f);
        }
    }

    void drawExplorationOverview(const PlayerSaveState& state)
    {
        constexpr int count = OverviewCellCount;
        const float scale = appliedSettings.uiScale;
        const auto& io = ImGui::GetIO();
        const float width = ImGui::GetContentRegionAvail().x;
        const bool wide = width > 760.f * scale;
        const float legendWidth = wide ? 276.f * scale : 0.f;
        const float footerHeight = wide ? 0.f : 58.f * scale;
        const ImVec2 size(std::max(1.f, width - legendWidth - (wide ? 10.f : 0.f)),
            std::max(60.f, ImGui::GetContentRegionAvail().y - footerHeight));
        if (overviewFollowsFit && (overviewLastViewport.x!=size.x || overviewLastViewport.y!=size.y)) overviewAutoFit=true;
        overviewLastViewport=size;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##OverviewViewport", size,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool hovered = ImGui::IsItemHovered();
        if (hovered && io.MouseWheel != 0.f)
            zoomOverview(std::pow(1.25f,std::clamp(io.MouseWheel,-4.f,4.f)));
        const int step = overviewScale.step;
        float pixel = overviewScale.cellPixels(size.x,size.y);
        if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (io.MouseDelta.x!=0.f || io.MouseDelta.y!=0.f) overviewFollowsFit=false;
            overviewPanRemainderX += io.MouseDelta.x / pixel;
            overviewPanRemainderZ += io.MouseDelta.y / pixel;
            const int dx = static_cast<int>(overviewPanRemainderX);
            const int dz = static_cast<int>(overviewPanRemainderZ);
            overviewOffsetX -= dx * step;
            overviewOffsetZ -= dz * step;
            overviewPanRemainderX -= dx;
            overviewPanRemainderZ -= dz;
        }
        constexpr std::int64_t maxOffset = 1000000;
        overviewOffsetX = std::clamp(overviewOffsetX, -maxOffset, maxOffset);
        overviewOffsetZ = std::clamp(overviewOffsetZ, -maxOffset, maxOffset);
        const std::int64_t playerX = World::toBlockCoord(state.position.x);
        const std::int64_t playerZ = World::toBlockCoord(state.position.z);
        const auto align = [=](std::int64_t value) {
            return (value >= 0 ? value / step : (value - step + 1) / step) * step;
        };
        const std::int64_t centerX = align(playerX + overviewOffsetX);
        const std::int64_t centerZ = align(playerZ + overviewOffsetZ);
        if (!overviewValid || centerX != overviewCenterX ||
            centerZ != overviewCenterZ || hudElapsedSeconds >= overviewNextRefresh)
        {
            if (overviewValid && (centerX != overviewCenterX ||
                                  centerZ != overviewCenterZ))
                selectedOverviewCell = -1;
            overviewCenterX = centerX;
            overviewCenterZ = centerZ;
            overviewCells.fill({});
            overviewObservedPositions.fill({});
            const auto samples = world->exploredOverviewAt(
                centerX, centerZ, count, step);
            for (std::size_t index = 0; index < samples.size(); ++index)
            {
                if (!samples[index].known) continue;
                overviewCells[index] = {true, samples[index].surface.height,
                    samples[index].surface.material};
                overviewObservedPositions[index] =
                    {samples[index].worldX, samples[index].worldZ};
            }
            if (overviewFollowsFit && overviewSourceRevision != detailMap.revision)
                overviewAutoFit = true;
            overviewSourceRevision = detailMap.revision;
            overviewValid = true;
            overviewNextRefresh = hudElapsedSeconds + 1.0;
        }
        if (overviewAutoFit) {
            int radiusX=5, radiusZ=5;
            bool known=false;
            for (int z=0;z<count;++z) for (int x=0;x<count;++x)
                if (overviewCells[z*count+x].known) {
                    known=true; radiusX=std::max(radiusX,std::abs(x-count/2));
                    radiusZ=std::max(radiusZ,std::abs(z-count/2));
                }
            if (known) {
                const float fitted = .82f*std::min(size.x/(2*radiusX+5),size.y/(2*radiusZ+5));
                overviewScale.zoom=std::clamp(fitted*count/std::max(size.x,size.y),1.f,8.f);
                pixel=overviewScale.cellPixels(size.x,size.y);
                overviewAutoFit=false;
            }
        }
        const float side=pixel*count;
        const ImVec2 grid(origin.x+(size.x-side)*.5f,origin.y+(size.y-side)*.5f);
        const ImVec2 edge(origin.x+size.x,origin.y+size.y);
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin,edge,IM_COL32(20,35,42,255));
        draw->PushClipRect(origin,edge,true);
        for (int line = 0; line <= count; line += 4) {
            const float offset = line * pixel;
            draw->AddLine(ImVec2(grid.x + offset,grid.y),ImVec2(grid.x + offset,grid.y + side),IM_COL32(55,76,81,95));
            draw->AddLine(ImVec2(grid.x,grid.y + offset),ImVec2(grid.x + side,grid.y + offset),IM_COL32(55,76,81,95));
        }
        const auto oldFlags = draw->Flags;
        draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        for (int z = 0; z < count; ++z)
        for (int x = 0; x < count; ++x)
        {
            const auto& cell = overviewCells[z * count + x];
            if (!cell.known || grid.x+(x+1)*pixel<origin.x || grid.x+x*pixel>edge.x ||
                grid.y+(z+1)*pixel<origin.y || grid.y+z*pixel>edge.y) continue;
            draw->AddRectFilled(ImVec2(grid.x + x * pixel, grid.y + z * pixel),
                ImVec2(grid.x + (x + 1) * pixel, grid.y + (z + 1) * pixel),
                mapCellColour(overviewCells.data(), count, x, z));
        }
        draw->Flags = oldFlags;
        const int hoveredX = hovered ? static_cast<int>((io.MousePos.x - grid.x) / pixel) : -1;
        const int hoveredZ = hovered ? static_cast<int>((io.MousePos.y - grid.y) / pixel) : -1;
        const int hoverCell = hoveredX >= 0 && hoveredX < count &&
            hoveredZ >= 0 && hoveredZ < count ? hoveredZ * count + hoveredX : -1;
        if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            selectedOverviewCell = hoverCell;
        const int inspection = hoverCell >= 0 ? hoverCell : selectedOverviewCell;
        if (inspection >= 0 && overviewCells[inspection].known)
        {
            const int x = inspection % count, z = inspection / count;
            draw->AddRect(ImVec2(grid.x + x * pixel, grid.y + z * pixel),
                ImVec2(grid.x + (x + 1) * pixel, grid.y + (z + 1) * pixel),
                IM_COL32(255, 222, 137, 255), 0.f, 0, 2.f);
        }
        const auto markers = world->explorationMarkers();
        const auto tracked = world->trackedExplorationMarker();
        const auto objective = world->getObjectiveSnapshot(!trackedObjectiveId.empty());
        std::string taskId = objective.currentId;
        std::string taskTitle = objective.title;
        if (!trackedObjectiveId.empty())
        {
            const auto entry = std::find_if(objective.journal.begin(),
                objective.journal.end(), [&](const auto& value) {
                    return value.id == trackedObjectiveId && value.available;
                });
            if (entry != objective.journal.end())
            {
                taskId = entry->id;
                taskTitle = entry->title;
            }
        }
        const auto taskSite = world->knownWaystoneTaskSite();
        const bool taskLocated = ExplorationNavigation::knownWaystoneTask(
            taskId, taskSite.has_value());
        for (const auto& marker : markers)
        {
            const float mx = grid.x + side * .5f +
                static_cast<float>(static_cast<std::int64_t>(marker.worldX) - centerX) /
                step * pixel;
            const float mz = grid.y + side * .5f +
                static_cast<float>(static_cast<std::int64_t>(marker.worldZ) - centerZ) /
                step * pixel;
            if (mx < origin.x + 5.f || mx > edge.x - 5.f ||
                mz < origin.y + 5.f || mz > edge.y - 5.f) continue;
            const ImVec2 at(mx, mz);
            const auto colour = marker.kind == ExplorationMarkers::Kind::Home
                ? IM_COL32(242, 211, 151, 255)
                : IM_COL32(127, 218, 228, 255);
            draw->AddCircleFilled(at, 6.f * scale,
                IM_COL32(17, 29, 35, 255));
            if (marker.kind == ExplorationMarkers::Kind::Home)
                GameInterfaceWidgets::glyph(draw,GameInterfaceWidgets::Glyph::Home,
                    ImVec2(mx-9.f*scale,mz-9.f*scale),18.f*scale,colour);
            else
                draw->AddCircleFilled(at, 3.f * scale, colour);
            if (tracked && tracked->id == marker.id)
                draw->AddCircle(at, 11.f * scale, colour, 16, 1.5f);
            if ((tracked && tracked->id==marker.id) || mapMarkerEditor.id==marker.id || marker.kind==ExplorationMarkers::Kind::Home) {
                const float font=ImGui::GetFontSize()*.72f;
                const auto label=boundedHudText(marker.name,font,std::min(150.f*scale,edge.x-mx-16.f*scale));
                const ImVec2 pos(mx+13.f*scale,mz-font*.5f);
                draw->AddText(ImGui::GetFont(),font,ImVec2(pos.x+1.f,pos.y+1.f),IM_COL32(8,20,24,255),label.c_str());
                draw->AddText(ImGui::GetFont(),font,pos,colour,label.c_str());
            }
            if (hovered && std::abs(io.MousePos.x - mx) < 7.f * scale &&
                std::abs(io.MousePos.y - mz) < 7.f * scale)
            {
                ImGui::SetTooltip("%s · X %d  Z %d", marker.name.c_str(),
                    marker.worldX, marker.worldZ);
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    mapMarkerEditor.select(marker.id, marker.name);
                    mapMarkerFeedbackKey.clear();
                }
            }
        }
        if (taskLocated)
        {
            const float mx = grid.x + side * .5f +
                static_cast<float>(static_cast<std::int64_t>(taskSite->worldX) - centerX) /
                step * pixel;
            const float mz = grid.y + side * .5f +
                static_cast<float>(static_cast<std::int64_t>(taskSite->worldZ) - centerZ) /
                step * pixel;
            if (mx >= origin.x + 5.f && mx <= edge.x - 5.f &&
                mz >= origin.y + 5.f && mz <= edge.y - 5.f)
            {
                draw->AddCircleFilled(ImVec2(mx, mz), 7.f * scale,
                    IM_COL32(17, 29, 35, 255));
                draw->AddQuadFilled(ImVec2(mx, mz - 5.f * scale),
                    ImVec2(mx + 5.f * scale, mz),
                    ImVec2(mx, mz + 5.f * scale),
                    ImVec2(mx - 5.f * scale, mz),
                    IM_COL32(232, 188, 115, 255));
                if (hovered && std::abs(io.MousePos.x - mx) < 8.f * scale &&
                    std::abs(io.MousePos.y - mz) < 8.f * scale)
                    ImGui::SetTooltip("%s · X %d  Z %d", taskTitle.c_str(),
                        taskSite->worldX, taskSite->worldZ);
            }
        }
        const float px = grid.x + side * .5f +
            static_cast<float>(playerX - centerX) / step * pixel;
        const float pz = grid.y + side * .5f +
            static_cast<float>(playerZ - centerZ) / step * pixel;
        const auto heading = ExplorationNavigation::heading(state.rotation.y);
        const auto arrowPoint = [&](float forward, float right) {
            return ImVec2(px + (heading.x * forward - heading.z * right) * scale,
                          pz + (heading.z * forward + heading.x * right) * scale);
        };
        GameInterfaceWidgets::playerArrow(draw, arrowPoint(7.f, 0.f),
            arrowPoint(-4.f, -5.f), arrowPoint(-4.f, 5.f));
        GameInterfaceWidgets::compass(draw,ImVec2(origin.x+35.f*scale,origin.y+57.f*scale),0,-1,scale,tr("hud.minimap_north").c_str());
        GameInterfaceWidgets::mapRuler(draw,ImVec2(edge.x-18.f*scale,edge.y-9.f*scale),pixel/step,scale);
        std::string surfaceHint=tr("map.unexplored");
        if (inspection>=0 && overviewCells[inspection].known) {
            const auto& cell=overviewCells[inspection];
            surfaceHint=LocalizedPresentation::surfaceName(appliedSettings.locale,cell.material)+" · X "+
                std::to_string(overviewObservedPositions[inspection].first)+" Y "+std::to_string(cell.height)+" Z "+
                std::to_string(overviewObservedPositions[inspection].second);
        }
        const float hintFont=ImGui::GetFontSize()*.7f;
        const auto hint=boundedHudText(surfaceHint,hintFont,std::max(1.f,size.x-125.f*scale));
        const ImVec2 hintAt(origin.x+16.f*scale,edge.y-29.f*scale);
        draw->AddRectFilled(ImVec2(hintAt.x-4.f,hintAt.y-2.f),
            ImVec2(hintAt.x+ImGui::GetFont()->CalcTextSizeA(hintFont,FLT_MAX,0,hint.c_str()).x+4.f,hintAt.y+hintFont+2.f),
            IM_COL32(20,35,42,210),2.f);
        draw->AddText(ImGui::GetFont(),hintFont,hintAt,IM_COL32(176,195,189,240),hint.c_str());
        draw->PopClipRect();
        draw->AddRect(origin,edge,IM_COL32(89,116,119,175),2.f);
        if (wide) ImGui::SameLine();
        ImGui::BeginChild("##OverviewLegend", ImVec2(0, 0), wide);
        ImGui::SetWindowFontScale(wide ? .82f : .72f);
        if (wide) {
            drawExplorationMarkerPanel(state);
            ImGui::Spacing(); ImGui::Separator();
            const std::string span=std::to_string(int(size.x/pixel*step))+" × "+std::to_string(int(size.y/pixel*step))+" m";
            mapMetric("map.overview",span);
        }
        if (!wide) {
            ImGui::TextWrapped("%s",surfaceHint.c_str());
        }
        if (taskLocated)
        {
            constexpr const char* directions[] = {
                "map.direction_n", "map.direction_ne", "map.direction_e",
                "map.direction_se", "map.direction_s", "map.direction_sw",
                "map.direction_w", "map.direction_nw"};
            const auto bearing = ExplorationNavigation::toward(
                World::toBlockCoord(state.position.x),
                World::toBlockCoord(state.position.z),
                taskSite->worldX, taskSite->worldZ);
            ImGui::TextWrapped("%s: %s · %s  %llu m",
                tr("map.objective_destination").c_str(), taskTitle.c_str(),
                tr(directions[bearing.octant]).c_str(),
                static_cast<unsigned long long>(bearing.metres));
        }
        ImGui::SetWindowFontScale(1.f);
        ImGui::EndChild();
    }

    void drawExplorationMarkerPanel(const PlayerSaveState& state)
    {
        using Result = ExplorationMarkers::Result;
        using Kind = ExplorationMarkers::Kind;
        const float scale = appliedSettings.uiScale;
        const auto feedback = [&](Result result) {
            const char* key = "map.marker_invalid";
            switch (result) {
                case Result::Created: key = "map.marker_created"; break;
                case Result::Changed: key = "map.marker_saved"; break;
                case Result::Removed: key = "map.marker_removed"; break;
                case Result::Unchanged: key = "map.marker_unchanged"; break;
                case Result::Full: key = "map.marker_full"; break;
                case Result::Missing: key = "map.marker_missing"; break;
                default: break;
            }
            mapMarkerFeedbackKey = key;
            playUiFeedback();
        };
        std::optional<std::pair<int,int>> selectedPosition;
        if (overviewValid && selectedOverviewCell >= 0 && selectedOverviewCell < int(overviewCells.size()) &&
            overviewCells[selectedOverviewCell].known)
            selectedPosition = overviewObservedPositions[selectedOverviewCell];
        const auto markers = world->explorationMarkers();
        const auto tracked = world->trackedExplorationMarker();
        ImGui::Text("%s",tr("map.markers").c_str());
        const auto count=std::to_string(markers.size())+" / "+std::to_string(ExplorationMarkers::Capacity);
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-ImGui::CalcTextSize(count.c_str()).x));
        ImGui::TextDisabled("%s",count.c_str());
        if (!mapMarkerEditor.id && !markers.empty()) {
            const auto first=tracked ? *tracked : markers.front();
            mapMarkerEditor.select(first.id,first.name);
        }
        ImGui::Separator();
        const float listHeight = std::clamp(60.f*scale*std::max(std::size_t(1),markers.size()),
            62.f*scale,std::max(62.f*scale,std::min(145.f*scale,ImGui::GetContentRegionAvail().y*.28f)));
        ImGui::BeginChild("##MarkerRows",ImVec2(0,listHeight),false);
        if (markers.empty()) ImGui::TextWrapped("%s",tr("map.marker_empty").c_str());
        for (const auto& marker : markers)
        {
            ImGui::PushID(static_cast<int>(marker.id));
            const float width=ImGui::GetContentRegionAvail().x;
            const float rowHeight=52.f*scale;
            const auto at=ImGui::GetCursorScreenPos();
            if (ImGui::Selectable("##Marker",mapMarkerEditor.id==marker.id,0,ImVec2(0,rowHeight))) {
                mapMarkerEditor.select(marker.id,marker.name); mapMarkerFeedbackKey.clear();
            }
            auto* draw=ImGui::GetWindowDrawList();
            const ImU32 colour=ImGui::GetColorU32(marker.kind==Kind::Home ? WarmAccent : WarmText);
            GameInterfaceWidgets::glyph(draw,marker.kind==Kind::Home ? GameInterfaceWidgets::Glyph::Home : GameInterfaceWidgets::Glyph::Pin,
                ImVec2(at.x+5.f*scale,at.y+12.f*scale),24.f*scale,colour);
            const auto title=boundedHudText(marker.name,ImGui::GetFontSize(),width-42.f*scale);
            draw->AddText(ImVec2(at.x+38.f*scale,at.y+3.f*scale),colour,title.c_str());
            std::string badge=marker.kind==Kind::Home ? tr("map.marker_home") : tr("map.markers");
            if (tracked && tracked->id==marker.id) badge+=" · "+tr("map.marker_tracking");
            draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*.75f,ImVec2(at.x+38.f*scale,at.y+29.f*scale),
                ImGui::GetColorU32(WarmMuted),badge.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",marker.name.c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
        if (adventureButton("map.marker_add")) {
            mapMarkerFeedbackKey.clear();
            ImGui::OpenPopup("##NewMapMarker");
        }
        if (ImGui::BeginPopup("##NewMapMarker"))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min(280.f * scale,ImGui::GetIO().DisplaySize.x-80.f));
            if (selectedPosition) ImGui::Text("X %d  Z %d",selectedPosition->first,selectedPosition->second);
            else ImGui::TextWrapped("%s",tr("map.marker_choose_cell").c_str());
            ImGui::TextUnformatted(tr("map.marker_new_name").c_str());
            ImGui::SetNextItemWidth(std::min(260.f*scale,ImGui::GetIO().DisplaySize.x-96.f));
            ImGui::InputText("##NewName",newMapMarkerName.data(),newMapMarkerName.size());
            ImGui::BeginDisabled(!selectedPosition.has_value());
            const auto create = [&](Kind kind) {
                std::uint32_t id = 0;
                const auto result = world->createExplorationMarker(selectedPosition->first,selectedPosition->second,
                    newMapMarkerName.data(),kind,&id);
                feedback(result);
                if (result == Result::Created) {
                    mapMarkerEditor.select(id,newMapMarkerName.data()); newMapMarkerName.fill(0);
                    ImGui::CloseCurrentPopup();
                }
            };
            if (adventureButton("map.marker_add",-1.f,true)) create(Kind::Note);
            if (adventureButton("map.marker_add_home")) create(Kind::Home);
            ImGui::EndDisabled();
            if (!mapMarkerFeedbackKey.empty())
                ImGui::TextWrapped("%s",tr(mapMarkerFeedbackKey).c_str());
            if (adventureButton("common.cancel")) ImGui::CloseCurrentPopup();
            ImGui::PopTextWrapPos(); ImGui::EndPopup();
        }
        ImGui::Separator();
        const auto selected = std::find_if(markers.begin(),markers.end(),[&](const auto& m){return m.id==mapMarkerEditor.id;});
        if (selected != markers.end())
        {
            constexpr const char* directions[] = {"map.direction_n","map.direction_ne","map.direction_e","map.direction_se",
                "map.direction_s","map.direction_sw","map.direction_w","map.direction_nw"};
            const auto bearing = ExplorationNavigation::toward(World::toBlockCoord(state.position.x),World::toBlockCoord(state.position.z),
                selected->worldX,selected->worldZ);
            ImGui::Text("%s · %llu m",tr(directions[bearing.octant]).c_str(),static_cast<unsigned long long>(bearing.metres));
            ImGui::TextDisabled("X %d  Z %d",selected->worldX,selected->worldZ);
            ImGui::TextUnformatted(tr("map.marker_edit_name").c_str());
            const float editWidth = ImGui::GetContentRegionAvail().x;
            const float saveWidth = ImGui::CalcTextSize(tr("map.marker_rename").c_str()).x + 20.f*scale;
            ImGui::SetNextItemWidth(std::max(60.f*scale,editWidth-saveWidth-ImGui::GetStyle().ItemSpacing.x));
            ImGui::InputText("##EditMapMarkerName",mapMarkerEditor.name.data(),mapMarkerEditor.name.size());
            ImGui::SameLine();
            if (adventureButton("map.marker_rename",saveWidth)) feedback(world->renameExplorationMarker(selected->id,mapMarkerEditor.name.data()));
            const bool tracking = tracked && tracked->id == selected->id;
            const float moreWidth = ImGui::CalcTextSize(tr("map.marker_more").c_str()).x + 24.f*scale;
            if (adventureButton(tracking ? "map.marker_untrack" : "map.marker_track",editWidth-moreWidth-ImGui::GetStyle().ItemSpacing.x,true))
                feedback(world->trackExplorationMarker(tracking ? 0 : selected->id));
            ImGui::SameLine();
            if (adventureButton("map.marker_more",moreWidth)) ImGui::OpenPopup("##MarkerMore");
            bool deleteRequested = false;
            if (ImGui::BeginPopup("##MarkerMore"))
            {
                if (selected->kind != Kind::Home && ImGui::MenuItem(tr("map.marker_set_home").c_str()))
                    feedback(world->setHomeExplorationMarker(selected->id));
                if (ImGui::MenuItem(tr("map.marker_move_here").c_str(),nullptr,false,selectedPosition.has_value()))
                    feedback(world->moveExplorationMarker(selected->id,selectedPosition->first,selectedPosition->second));
                ImGui::Separator();
                if (ImGui::MenuItem(tr("map.marker_delete").c_str())) deleteRequested = true;
                ImGui::EndPopup();
            }
            if (deleteRequested) ImGui::OpenPopup("##ConfirmMapMarkerDelete");
            if (ImGui::BeginPopupModal("##ConfirmMapMarkerDelete",nullptr,ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextWrapped("%s",tr("map.marker_delete_confirm").c_str());
                if (adventureButton("map.marker_delete",0.f)) {
                    feedback(world->eraseExplorationMarker(selected->id)); mapMarkerEditor.clear(); ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (adventureButton("common.cancel",0.f)) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        if (!mapMarkerFeedbackKey.empty()) {
            ImGui::Spacing(); ImGui::PushFont(nullptr,16.f);
            const auto message=tr(mapMarkerFeedbackKey);
            const auto shortMessage=boundedHudText(message,ImGui::GetFontSize(),ImGui::GetContentRegionAvail().x);
            ImGui::TextColored(WarmMuted,"%s",shortMessage.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",message.c_str());
            ImGui::PopFont();
        }
        if (markers.empty()) {
            ImGui::Spacing(); ImGui::PushStyleColor(ImGuiCol_Text,WarmMuted);
            ImGui::TextWrapped("%s",tr("map.marker_choose_cell").c_str());
            ImGui::PopStyleColor();
        }
    }

    void drawExplorationMapStatus()
    {
        const auto health = world->explorationMapStatus();
        if (health.needsAttention() && ImGui::CollapsingHeader(
            tr("map.status_notice").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (const char* key = health.recoveryMessageKey())
                ImGui::TextWrapped("%s", tr(key).c_str());
            if (health.full)
                ImGui::TextWrapped("%s", tr("map.status_full").c_str());
            if (ImGui::Button(tr("map.backup_open").c_str()))
                ImGui::OpenPopup("##MapBackupHelp");
        }
        if (ImGui::BeginPopupModal("##MapBackupHelp", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                std::min(360.f * appliedSettings.uiScale,
                         ImGui::GetIO().DisplaySize.x - 96.f));
            ImGui::TextWrapped("%s", tr("map.backup_body").c_str());
            ImGui::PopTextWrapPos();
            if (ImGui::Button(tr("map.backup_continue").c_str()))
            {
                pendingAction.type = OgreUserInterfaceActionType::OpenWorldBackups;
                pendingAction.worldId = flow->activeWorldId();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(tr("common.cancel").c_str()))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        if (!statusMessage.empty() && statusMessageSeconds > 0.f)
            ImGui::TextWrapped("%s", statusMessage.c_str());
    }

    void refreshDetailMap(const PlayerSaveState& state)
    {
        const bool reset = detailMap.configure(World::toBlockCoord(state.position.x),
            World::toBlockCoord(state.position.z), world->getRenderDistance());
        if (reset) {
            selectedMapCell = -1;
            detailMapNextRefresh = 0;
            if (overviewFollowsFit) {
                overviewScale = {};
                while (OverviewCellCount * overviewScale.step < detailMap.count * detailMap.step)
                    overviewScale.step *= 2;
                overviewAutoFit = true;
            }
            overviewValid = false;
        }
        if (hudElapsedSeconds < detailMapNextRefresh) return;
        detailMapNextRefresh = hudElapsedSeconds + 1.0 / 30.0;
        const auto batch = detailMap.nextBatch();
        std::vector<VectorXZ> positions;
        positions.reserve(batch.size());
        for (const auto& query : batch) positions.push_back({query.x, query.z});
        detailMap.accept(batch, world->observeSurfaceMap(positions));
    }

    void drawTerrainMap(const PlayerSaveState& state)
    {
        if (world == nullptr) return;
        refreshDetailMap(state);
        const auto& io = ImGui::GetIO();
        const float scale = appliedSettings.uiScale;
        GameInterfaceWidgets::OverlayStyle theme(scale);
        adventureBackdrop();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(1120.f*scale,io.DisplaySize.x-32.f),
            std::min(630.f*scale,io.DisplaySize.y-32.f)),ImGuiCond_Always);
        if (ImGui::Begin("##TerrainMap", nullptr, ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground))
        {
            if (adventureHeader(tr("map.title"),GameInterfaceWidgets::Glyph::Map)) hudInteraction.dismiss();
            const auto footer = [&] {
                ImGui::Separator();
                const auto hint = boundedHudText(tr(mapFlatOverview ? "map.overview_controls" : "map.controls"),
                    ImGui::GetFontSize()*.72f,std::max(1.f,ImGui::GetContentRegionAvail().x-135.f*scale));
                ImGui::SetWindowFontScale(.72f);
                ImGui::TextDisabled("%s",hint.c_str());
                ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowWidth()-140.f*scale));
                ImGui::TextDisabled("Esc  %s",tr("ui.return_game").c_str());
                ImGui::SetWindowFontScale(1.f);
            };
            ImGui::BeginChild("##MapContents",ImVec2(0,-29.f*scale),false);
            drawExplorationMapStatus();
            const float toolbarWidth=ImGui::GetContentRegionAvail().x;
            ImGui::PushFont(nullptr,20.f);
            const float tabWidth=std::max(104.f*scale,ImGui::CalcTextSize(tr("map.view_flat").c_str()).x+22.f*scale);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,8.f*scale));
            if (adventureTab("map.view_flat",mapFlatOverview,tabWidth)) {
                mapFlatOverview=true; mapMarkerPanel=false; selectedOverviewCell=selectedMapCell=-1;
            }
            ImGui::SameLine();
            if (adventureTab("map.view_3d",!mapFlatOverview,tabWidth)) {
                mapFlatOverview=false; mapMarkerPanel=false; selectedOverviewCell=selectedMapCell=-1;
            }
            ImGui::PopStyleVar();
            const float gap=ImGui::GetStyle().ItemSpacing.x;
            const float centerWidth=ImGui::CalcTextSize(tr("map.center").c_str()).x+20.f*scale;
            const float northWidth=ImGui::CalcTextSize(tr("map.north").c_str()).x+20.f*scale;
            const float markerWidth=ImGui::CalcTextSize(tr(mapMarkerPanel ? "map.marker_back" : "map.markers").c_str()).x+40.f*scale;
            const float toolsWidth=centerWidth+(mapFlatOverview ? 0.f : northWidth+gap)+
                3.f*32.f*scale+markerWidth+4.f*gap;
            const bool singleToolbar=toolbarWidth>tabWidth*2.f+toolsWidth+20.f*scale;
            const auto markerControls = [&] {
                if (adventureButton(mapMarkerPanel ? "map.marker_back" : "map.markers",markerWidth,false,
                    Material::Nothing,GameInterfaceWidgets::Glyph::Pin)) {
                    mapFlatOverview=true; mapMarkerPanel=!mapMarkerPanel;
                }
                ImGui::SameLine();
                if (ImGui::Button("?##MapHelp",ImVec2(32.f*scale,0))) ImGui::OpenPopup("##MapPan");
            };
            if (singleToolbar) ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-toolsWidth);
            else {
                const float remaining=markerWidth+32.f*scale+gap;
                if (toolbarWidth>tabWidth*2.f+remaining+gap)
                    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x-remaining);
                markerControls();
            }
            if (ImGui::Button(tr("map.center").c_str(),ImVec2(centerWidth,0))) {
                if (mapFlatOverview) {
                    overviewOffsetX=overviewOffsetZ=0; overviewScale={}; overviewAutoFit=overviewFollowsFit=true;
                    overviewValid=false; overviewPanRemainderX=overviewPanRemainderZ=0.f;
                    selectedOverviewCell=-1;
                } else { mapView={}; selectedMapCell=-1; }
            }
            if (!mapFlatOverview) {
                ImGui::SameLine(); if (ImGui::Button(tr("map.north").c_str(),ImVec2(northWidth,0))) mapView.yaw=0;
            }
            ImGui::SameLine();
            if (ImGui::Button("-##MapZoom",ImVec2(32.f*scale,0))) {
                if (mapFlatOverview) zoomOverview(.8f); else mapView.zoom/=1.25f;
            }
            ImGui::SameLine();
            if (ImGui::Button("+##MapZoom",ImVec2(32.f*scale,0))) {
                if (mapFlatOverview) zoomOverview(1.25f); else mapView.zoom*=1.25f;
            }
            if (singleToolbar) { ImGui::SameLine(); markerControls(); }
            if (ImGui::BeginPopup("##MapPan")) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+std::min(300.f*scale,io.DisplaySize.x-96.f));
                ImGui::TextWrapped("%s",tr(mapFlatOverview ? "map.overview_note" : "map.session").c_str());
                ImGui::TextWrapped("%s",tr("map.unknown").c_str());
                ImGui::Separator();
                const auto pan = [&](int dx,int dz) {
                    if (mapFlatOverview) { overviewOffsetX+=dx*8*overviewScale.step; overviewOffsetZ+=dz*8*overviewScale.step; }
                    else { mapView.panX+=dx*.12f; mapView.panY+=dz*.12f; }
                };
                if (ImGui::Button("<")) pan(-1,0);
                ImGui::SameLine(); if (ImGui::Button(">")) pan(1,0);
                ImGui::SameLine(); if (ImGui::Button("^")) pan(0,-1);
                ImGui::SameLine(); if (ImGui::Button("v")) pan(0,1);
                ImGui::PopTextWrapPos(); ImGui::EndPopup();
            }
            ImGui::PopFont();
            if (mapMarkerPanel && toolbarWidth <= 760.f*scale)
            {
                ImGui::BeginChild("##CompactMarkers",ImVec2(0,0),false);
                drawExplorationMarkerPanel(state);
                ImGui::EndChild(); ImGui::EndChild(); footer(); ImGui::End(); return;
            }
            if (mapFlatOverview)
            {
                drawExplorationOverview(state);
                ImGui::EndChild(); footer(); ImGui::End(); return;
            }
            const auto& mapCells = detailMap.cells;
            const int mapCount = detailMap.count, mapStep = detailMap.step;
            const int mapCenterX = detailMap.centerX, mapCenterZ = detailMap.centerZ;
            const float width = ImGui::GetContentRegionAvail().x;
            const bool wide = width > 760.f * scale;
            const float legendWidth = wide ? 228.f * scale : 0.f;
            const float footerHeight = wide ? 0.f : 58.f * scale;
            const ImVec2 size(std::max(1.f, width - legendWidth - (wide ? 10.f : 0.f)),
                std::max(60.f, ImGui::GetContentRegionAvail().y - footerHeight));
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##MapViewport", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
            const bool hovered = ImGui::IsItemHovered();
            if (hovered && io.MouseWheel != 0) mapView.zoom *= std::pow(1.15f, std::clamp(io.MouseWheel, -4.f, 4.f));
            for (int button = 0; button < 2; ++button)
                if (hovered && ImGui::IsMouseClicked(button))
                    mapGesture.begin(mapView, io.MousePos.x, io.MousePos.y, button);
            if (mapGesture.button >= 0)
                mapGesture.update(mapView, io.MousePos.x, io.MousePos.y, size.x, size.y,
                    ImGui::IsMouseDown(mapGesture.button));
            mapView.constrain();
            const auto playerSurface = detailMap.surfaceHeight(state.position.x, state.position.z);
            const float baseHeight = playerSurface.value_or(0.f);
            if (mapBuiltRevision != detailMap.revision || mapBuiltYaw != mapView.yaw ||
                mapBuiltPitch != mapView.pitch || mapBuiltBase != baseHeight)
            {
                TerrainMapView::build(mapCells, mapCount, mapStep, baseHeight, mapView, mapFaces);
                mapBounds={};
                for (const auto& face : mapFaces) for (const auto& p : face.points) mapBounds.include(p);
                mapFloorHeight=baseHeight;
                for (const auto& cell : mapCells) if (cell.known) mapFloorHeight=std::min(mapFloorHeight,float(cell.height));
                mapBuiltRevision = detailMap.revision; mapBuiltYaw = mapView.yaw;
                mapBuiltPitch = mapView.pitch; mapBuiltBase = baseHeight;
            }
            const float pixels = mapBounds.fittedScale(size.x,size.y,mapCount*mapStep*.35f)*mapView.zoom;
            const ImVec2 center(origin.x+size.x*(.5f+mapView.panX)-(mapBounds.minX+mapBounds.maxX)*.5f*pixels,
                origin.y+size.y*(.5f+mapView.panY)-(mapBounds.minY+mapBounds.maxY)*.5f*pixels);
            const auto screen = [&](const TerrainMapView::Point& point) { return ImVec2(center.x + point.x * pixels, center.y + point.y * pixels); };
            auto* draw = ImGui::GetWindowDrawList();
            const ImVec2 edge(origin.x+size.x,origin.y+size.y);
            draw->AddRectFilled(origin,edge,IM_COL32(20,35,42,255));
            draw->PushClipRect(origin,edge,true);
            const TerrainMapView::Projection project(mapView);
            const float extent=mapCount*mapStep*2.f;
            const float floor=mapFloorHeight-baseHeight-1.f;
            for (float line=-extent;line<=extent;line+=8.f*mapStep) {
                draw->AddLine(screen(project(line,floor,-extent)),screen(project(line,floor,extent)),IM_COL32(87,111,116,45));
                draw->AddLine(screen(project(-extent,floor,line)),screen(project(extent,floor,line)),IM_COL32(87,111,116,45));
            }
            const int hoverCell = hovered ? TerrainMapView::pick(mapFaces,
                (io.MousePos.x - center.x) / pixels, (io.MousePos.y - center.y) / pixels) : -1;
            if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !mapGesture.dragged)
                selectedMapCell = hoverCell;
            const auto oldFlags = draw->Flags;
            draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            const auto& callbacks=ImGui::GetPlatformIO();
            if (callbacks.DrawCallback_SetSamplerNearest) draw->AddCallback(callbacks.DrawCallback_SetSamplerNearest,nullptr);
            const auto& atlas=runtimeTerrainMaterialProfile().parameters();
            for (const auto& face : mapFaces)
            {
                const auto a = screen(face.points[0]), b = screen(face.points[1]), c = screen(face.points[2]), d = screen(face.points[3]);
                if (std::max({a.x,b.x,c.x,d.x})<origin.x || std::min({a.x,b.x,c.x,d.x})>edge.x ||
                    std::max({a.y,b.y,c.y,d.y})<origin.y || std::min({a.y,b.y,c.y,d.y})>edge.y) continue;
                const auto& cell=mapCells[face.cell];
                const auto& render=BlockDatabase::get().getDefinition(cell.material).render;
                const auto tile=face.top ? render.texTopCoord : render.texSideCoord;
                if (atlasTextureId!=ImTextureID_Invalid && atlas.containsTile(int(tile.x),int(tile.y)) && cell.material!=BlockId::Air) {
                    const ImVec2 lo((tile.x*atlas.tilePixels+.5f)/atlas.atlasPixels,(tile.y*atlas.tilePixels+.5f)/atlas.atlasPixels);
                    const ImVec2 hi(((tile.x+1)*atlas.tilePixels-.5f)/atlas.atlasPixels,((tile.y+1)*atlas.tilePixels-.5f)/atlas.atlasPixels);
                    const int shade=int(245.f*face.shade);
                    draw->AddImageQuad(ImTextureRef(atlasTextureId),a,b,c,d,lo,ImVec2(hi.x,lo.y),hi,ImVec2(lo.x,hi.y),
                        IM_COL32(shade,shade,shade,255));
                } else {
                    auto colour=ImGui::ColorConvertU32ToFloat4(mapCellColour(mapCells.data(),mapCount,face.cell%mapCount,face.cell/mapCount));
                    colour.x*=face.shade; colour.y*=face.shade; colour.z*=face.shade;
                    draw->AddQuadFilled(a,b,c,d,ImGui::ColorConvertFloat4ToU32(colour));
                }
            }
            if (callbacks.DrawCallback_SetSamplerLinear) draw->AddCallback(callbacks.DrawCallback_SetSamplerLinear,nullptr);
            draw->Flags = oldFlags;
            // Selection is a separate pass, keeping texture submission batched.
            for (const auto& face : mapFaces)
                if (face.top && (face.cell==selectedMapCell || face.cell==hoverCell))
                    draw->AddQuad(screen(face.points[0]),screen(face.points[1]),screen(face.points[2]),screen(face.points[3]),IM_COL32(255,222,137,255),2.f);
            const auto pointAt = [&](float x, float y, float z) {
                return screen(project(x - mapCenterX, y - baseHeight, z - mapCenterZ));
            };
            for (const auto& landmark : navigationMemory.landmarks())
            {
                const float dx = landmark.position.x - mapCenterX, dz = landmark.position.z - mapCenterZ;
                if (std::abs(dx) > (mapCount / 2) * mapStep || std::abs(dz) > (mapCount / 2) * mapStep) continue;
                const auto at = pointAt(landmark.position.x + .5f, landmark.position.y + 1.f, landmark.position.z + .5f);
                const auto colour = landmark.kind == MinimapNavigation::Kind::Waystone ? IM_COL32(117,221,237,255) : IM_COL32(235,178,112,255);
                draw->AddCircleFilled(at, 7.f * scale, IM_COL32(17,29,35,255));
                draw->AddQuadFilled(ImVec2(at.x,at.y-5.f*scale),ImVec2(at.x+4.f*scale,at.y),ImVec2(at.x,at.y+5.f*scale),ImVec2(at.x-4.f*scale,at.y),colour);
                if (hovered && std::abs(io.MousePos.x-at.x) < 10.f*scale && std::abs(io.MousePos.y-at.y) < 10.f*scale)
                    ImGui::SetTooltip("%s · %d, %d, %d", tr(landmark.kind == MinimapNavigation::Kind::Waystone ? "map.waystone" :
                        landmark.kind == MinimapNavigation::Kind::Workbench ? "map.workbench" : "map.storage").c_str(),
                        landmark.position.x,landmark.position.y,landmark.position.z);
            }
            // Map navigation uses the observed surface below the player. Flying
            // or jumping must not move the horizontal position off the terrain.
            if (playerSurface) {
                const auto at = pointAt(state.position.x, *playerSurface + 1.f, state.position.z);
                const auto heading = ExplorationNavigation::heading(state.rotation.y);
                const auto facing = project(heading.x,0.f,heading.z);
                const float facingLength = std::max(.001f,std::hypot(facing.x,facing.y));
                const float dx = facing.x/facingLength, dy = facing.y/facingLength;
                const auto arrow = [&](float forward,float right) {
                    return ImVec2(at.x+(dx*forward-dy*right)*scale,at.y+(dy*forward+dx*right)*scale);
                };
                GameInterfaceWidgets::playerArrow(draw,arrow(8.f,0.f),arrow(-4.f,-5.f),arrow(-4.f,5.f));
                draw->AddText(ImVec2(at.x+10.f*scale,at.y-8.f*scale),IM_COL32(255,222,137,255),tr("map.player").c_str());
            }
            const auto north=project(0,0,-1), east=project(1,0,0);
            GameInterfaceWidgets::compass(draw,ImVec2(origin.x+35.f*scale,origin.y+57.f*scale),north.x,north.y,scale,tr("hud.minimap_north").c_str());
            GameInterfaceWidgets::mapRuler(draw,ImVec2(edge.x-18.f*scale,edge.y-9.f*scale),pixels*std::hypot(east.x,east.y),scale);
            draw->PopClipRect();
            draw->AddRect(origin,edge,IM_COL32(89,116,119,175),2.f);
            if (wide) ImGui::SameLine();
            ImGui::BeginChild("##MapLegend", ImVec2(0,0), wide);
            ImGui::SetWindowFontScale(wide ? .82f : .72f);
            int inspection=hoverCell>=0 ? hoverCell : selectedMapCell;
            if (inspection<0 && mapCells[(mapCount/2)*mapCount+mapCount/2].known) inspection=(mapCount/2)*mapCount+mapCount/2;
            if (wide) {
                ImGui::TextUnformatted(tr("map.surface").c_str());
                ImGui::Separator(); ImGui::Spacing();
                mapSurfaceCard(inspection>=0 ? &mapCells[inspection] : nullptr,
                    mapCenterX+(inspection%mapCount-mapCount/2)*mapStep,
                    mapCenterZ+(inspection/mapCount-mapCount/2)*mapStep);
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                mapMetric("map.region",std::to_string(mapCount*mapStep)+" m");
                char zoom[24]; std::snprintf(zoom,sizeof(zoom),"%.1f ×",double(mapView.zoom));
                mapMetric("map.zoom",zoom);
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                mapLegendRow("map.player",IM_COL32(247,235,195,255),0);
                mapLegendRow("map.waystone",IM_COL32(117,221,237,255),1);
                mapLegendRow("map.workbench",IM_COL32(235,178,112,255),1);
                mapLegendRow("map.storage",IM_COL32(235,178,112,255),1);
                mapLegendRow("map.unexplored",IM_COL32(20,35,42,255),2);
            } else if (inspection>=0 && mapCells[inspection].known) {
                const auto& cell=mapCells[inspection];
                ImGui::Text("%s · %d m · %.1fx",LocalizedPresentation::surfaceName(appliedSettings.locale,cell.material).c_str(),
                    mapCount*mapStep,mapView.zoom);
                ImGui::TextDisabled("X %d  Y %d  Z %d",mapCenterX+(inspection%mapCount-mapCount/2)*mapStep,
                    cell.height,mapCenterZ+(inspection/mapCount-mapCount/2)*mapStep);
            } else ImGui::TextWrapped("%s",tr("map.select").c_str());
            ImGui::SetWindowFontScale(1.f);
            ImGui::EndChild();
            ImGui::EndChild();
            footer();
        }
        ImGui::End();
    }

    void drawQuestJournal()
    {
        if (world == nullptr) return;
        const auto snapshot = world->getObjectiveSnapshot(true);
        const auto& io = ImGui::GetIO();
        const float scale = appliedSettings.uiScale;
        GameInterfaceWidgets::OverlayStyle theme(scale);
        adventureBackdrop();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, io.DisplaySize.y * .5f), ImGuiCond_Always, ImVec2(.5f,.5f));
        ImGui::SetNextWindowSize(ImVec2(std::min(800.f * scale, io.DisplaySize.x - 32.f),
            std::min(520.f * scale, io.DisplaySize.y - 32.f)), ImGuiCond_Always);
        if (ImGui::Begin("##QuestJournal", nullptr, ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground))
        {
            const std::string summary = tr("journal.main") + " " + std::to_string(snapshot.completedObjectives) +
                " / " + std::to_string(snapshot.totalObjectives);
            if (adventureHeader(tr("journal.title"), GameInterfaceWidgets::Glyph::Journal, summary))
                hudInteraction.dismiss();
            const char* filters[] = {"journal.all", "journal.available", "journal.completed"};
            const float tabWidth = std::min(126.f * scale, (ImGui::GetContentRegionAvail().x - 16.f * scale) / 3.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(0,8.f*scale));
            for (int i = 0; i < 3; ++i) {
                if (i) ImGui::SameLine();
                if (adventureTab(filters[i], journalFilter == i, tabWidth)) journalFilter = i;
            }
            ImGui::PopStyleVar();
            ImGui::Separator();
            std::vector<const ObjectiveJournalEntry*> entries;
            for (const auto& entry : snapshot.journal)
                if (journalFilter == 0 || (journalFilter == 1 && entry.available) ||
                    (journalFilter == 2 && entry.completed)) entries.push_back(&entry);
            if (std::none_of(entries.begin(), entries.end(), [&](const auto* e) { return e->id == selectedJournalId; }))
                selectedJournalId = entries.empty() ? std::string{} : entries.front()->id;
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = std::max(1.f, ImGui::GetContentRegionAvail().y - 29.f * scale);
            const bool wide = width >= 610.f * scale;
            const float listWidth = width * .36f;
            const float listHeight = height;
            float detailHeight = height;
            if (!wide)
            {
                const auto selected = std::find_if(entries.begin(),entries.end(),[&](const auto* e){return e->id==selectedJournalId;});
                const auto title = selected == entries.end() ? tr("journal.empty") : objectiveText((*selected)->id,"title",(*selected)->title);
                const auto preview = boundedHudText(title,ImGui::GetFontSize(),width-60.f*scale);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::BeginCombo("##CompactQuestSelection",preview.c_str(),ImGuiComboFlags_HeightLarge)) {
                    for (const auto* entry : entries) {
                        const auto option = objectiveText(entry->id,"title",entry->title);
                        ImGui::PushID(entry->id.c_str());
                        if (ImGui::Selectable(option.c_str(),entry->id==selectedJournalId)) selectedJournalId=entry->id;
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                detailHeight = std::max(1.f,ImGui::GetContentRegionAvail().y-29.f*scale);
            }
            else
            {
            ImGui::BeginChild("##QuestList", ImVec2(listWidth, listHeight), false);
            if (entries.empty()) ImGui::TextWrapped("%s", tr("journal.empty").c_str());
            for (const auto* entry : entries)
            {
                ImGui::PushID(entry->id.c_str());
                const auto title = objectiveText(entry->id, "title", entry->title);
                const bool selected = selectedJournalId == entry->id;
                const float icon = 34.f * scale;
                const float rowWidth = ImGui::GetContentRegionAvail().x;
                const float textWidth = std::max(1.f, rowWidth - icon - 36.f * scale);
                const float titleHeight = ImGui::CalcTextSize(title.c_str(), nullptr, false, textWidth).y;
                const float rowHeight = std::max(66.f * scale, titleHeight + ImGui::GetTextLineHeight() + 16.f * scale);
                const ImVec2 at = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##QuestRow", selected, 0, ImVec2(0,rowHeight))) selectedJournalId = entry->id;
                if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsItemVisible())
                {
                    auto* draw = ImGui::GetWindowDrawList();
                    draw->AddRect(at, ImVec2(at.x + rowWidth, at.y + rowHeight),
                        selected ? IM_COL32(184,153,90,225) : IM_COL32(71,96,101,145), 3.f);
                    if (selected) draw->AddLine(ImVec2(at.x+1.f,at.y+4.f),ImVec2(at.x+1.f,at.y+rowHeight-4.f),
                        ImGui::ColorConvertFloat4ToU32(WarmAccent), 3.f * scale);
                    adventureIcon(objectiveIcon(entry->id), ImVec2(at.x + 9.f * scale, at.y + 13.f * scale), icon);
                    const ImVec2 arrow(at.x+rowWidth-12.f*scale,at.y+rowHeight*.5f);
                    draw->AddLine(ImVec2(arrow.x-3.f*scale,arrow.y-5.f*scale),arrow,ImGui::GetColorU32(WarmMuted),1.5f);
                    draw->AddLine(arrow,ImVec2(arrow.x-3.f*scale,arrow.y+5.f*scale),ImGui::GetColorU32(WarmMuted),1.5f);
                    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                        ImVec2(at.x + icon + 17.f * scale, at.y + 7.f * scale),
                        ImGui::ColorConvertFloat4ToU32(entry->available || entry->completed ? WarmText : WarmMuted),
                        title.c_str(), nullptr, textWidth);
                    const auto status = tr(entry->completed ? "journal.completed" : entry->available ? "journal.available" : "journal.locked");
                    const auto secondary = boundedHudText(tr(entry->optional ? "journal.optional" : "journal.main") + " · " + status +
                        (entry->id == trackedObjectiveId ? " · " + tr("journal.tracking") : ""),
                        ImGui::GetFontSize() * .78f, textWidth);
                    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * .78f,
                        ImVec2(at.x + icon + 17.f * scale, at.y + rowHeight - ImGui::GetTextLineHeight() - 4.f * scale),
                        ImGui::ColorConvertFloat4ToU32(selected ? WarmAccent : WarmMuted), secondary.c_str());
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::SameLine();
            }
            if (wide) {
                const auto at=ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddLine(ImVec2(at.x-4.f*scale,at.y),
                    ImVec2(at.x-4.f*scale,at.y+detailHeight),IM_COL32(105,132,131,175));
            }
            ImGui::PushStyleColor(ImGuiCol_ChildBg,IM_COL32(0,0,0,0));
            ImGui::BeginChild("##QuestDetail", ImVec2(0, detailHeight),false);
            if (wide) ImGui::Indent(16.f*scale);
            const auto detail = std::find_if(entries.begin(), entries.end(), [&](const auto* e) { return e->id == selectedJournalId; });
            if (detail != entries.end())
            {
                const auto& entry = **detail;
                const float actionHeight = entry.available ? (wide ? 83.f*scale : ImGui::GetTextLineHeight()+30.f*scale) : 0.f;
                ImGui::BeginChild("##QuestReading", ImVec2(0, -std::max(1.f,actionHeight)), false);
                const auto progress = std::to_string(std::min(entry.progress,entry.required)) + " / " + std::to_string(entry.required);
                const auto status = tr(entry.completed ? "journal.completed" : entry.available ? "journal.available" : "journal.locked");
                if (wide)
                {
                    const ImVec2 at = ImGui::GetCursorScreenPos();
                    adventureIcon(objectiveIcon(entry.id), at,45.f*scale);
                    ImGui::Indent(58.f*scale);
                    ImGui::SetWindowFontScale(1.30f);
                    ImGui::TextWrapped("%s",objectiveText(entry.id,"title",entry.title).c_str());
                    ImGui::SetWindowFontScale(1.f);
                    ImGui::TextColored(WarmAccent,"%s · %s",tr(entry.optional ? "journal.optional" : "journal.main").c_str(),status.c_str());
                    ImGui::Unindent(58.f*scale);
                    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(),at.y-ImGui::GetWindowPos().y+53.f*scale));
                    ImGui::Separator(); ImGui::Spacing();
                }
                else
                {
                    // The selection above already names the task. Keep its
                    // state and progress together, leaving room to read the goal.
                    ImGui::SetWindowFontScale(.75f);
                    ImGui::TextColored(WarmAccent,"%s · %s · %s",tr(entry.optional ? "journal.optional" : "journal.main").c_str(),status.c_str(),progress.c_str());
                    ImGui::SetWindowFontScale(1.f);
                }
                ImGui::TextWrapped("%s", objectiveInstructionText(entry.id,entry.instruction,entry.guidanceKey).c_str());
                ImGui::Spacing();
                const auto bar=ImGui::GetCursorScreenPos();
                const float progressWidth=ImGui::GetContentRegionAvail().x;
                const float progressHeight=wide ? 65.f*scale : 7.f*scale;
                auto* draw=ImGui::GetWindowDrawList();
                if (wide) {
                    draw->AddRectFilled(bar,ImVec2(bar.x+progressWidth,bar.y+progressHeight),IM_COL32(19,36,42,90),3.f);
                    draw->AddRect(bar,ImVec2(bar.x+progressWidth,bar.y+progressHeight),IM_COL32(91,117,119,150),3.f);
                    const auto icon=objectiveIcon(entry.id);
                    const bool materialTarget=entry.id=="alpha.gather_wood" || entry.id=="alpha.gather_stone" ||
                        entry.id=="alpha.gather_iron_ore" || entry.id=="alpha.gather_coal";
                    if (materialTarget) adventureIcon(icon,ImVec2(bar.x+10.f*scale,bar.y+8.f*scale),23.f*scale);
                    const auto label=materialTarget ? materialName(icon) : tr("journal.progress");
                    draw->AddText(ImVec2(bar.x+(materialTarget ? 40.f : 12.f)*scale,bar.y+8.f*scale),ImGui::GetColorU32(WarmText),label.c_str());
                    draw->AddText(ImVec2(bar.x+progressWidth-12.f*scale-ImGui::CalcTextSize(progress.c_str()).x,bar.y+8.f*scale),
                        ImGui::GetColorU32(WarmAccent),progress.c_str());
                }
                const ImVec2 barStart(bar.x+(wide ? 12.f*scale : 0.f),bar.y+(wide ? 44.f*scale : 0.f));
                GameInterfaceWidgets::progress(draw,barStart,
                    ImVec2(bar.x+progressWidth-(wide ? 12.f*scale : 0.f),barStart.y+7.f*scale),
                    entry.required>0 ? float(entry.progress)/entry.required : 0.f);
                ImGui::Dummy(ImVec2(progressWidth,progressHeight));
                if (!entry.prerequisiteId.empty())
                    ImGui::TextWrapped("%s: %s",tr("journal.prerequisite").c_str(),
                        objectiveText(entry.prerequisiteId,"title",entry.prerequisiteTitle).c_str());
                ImGui::EndChild();
                if (entry.available)
                {
                    const bool tracked = trackedObjectiveId == entry.id;
                    const char* action=tracked ? "journal.untrack" : "journal.track";
                    const float buttonWidth=wide ? std::min(ImGui::GetContentRegionAvail().x,
                        std::max(188.f*scale,ImGui::CalcTextSize(tr(action).c_str()).x+28.f*scale)) : ImGui::GetContentRegionAvail().x;
                    if (wide) ImGui::SetCursorPosX(ImGui::GetCursorPosX()+ImGui::GetContentRegionAvail().x-buttonWidth);
                    if (adventureButton(action,buttonWidth,true,Material::Nothing,GameInterfaceWidgets::Glyph::None,wide ? 46.f*scale : 0.f))
                    {
                        trackedObjectiveId = tracked ? std::string{} : entry.id;
                        objectiveHintSeconds = 12.f;
                        playUiFeedback();
                    }
                    if (wide) {
                        ImGui::PushFont(nullptr,17.f);
                        const auto hint=tr("journal.track_hint");
                        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-ImGui::CalcTextSize(hint.c_str()).x));
                        ImGui::TextColored(WarmMuted,"%s",hint.c_str());
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tr("journal.track_help").c_str());
                        ImGui::PopFont();
                    }
                }
            }
            if (wide) ImGui::Unindent(16.f*scale);
            ImGui::EndChild(); ImGui::PopStyleColor();
            adventureEscape("ui.return_game");
        }
        ImGui::End();
    }

    void drawHud()
    {
        hotbarDetailsVisible = false;
        if (player == nullptr)
        {
            return;
        }
        const ImGuiIO &io = ImGui::GetIO();
        // These interfaces hide the map, so observe their actual open anchors
        // before returning. Observing only in drawMinimap loses every visit.
        if (player->getOpenContainer())
            navigationMemory.observe(*player->getOpenContainer(), MinimapNavigation::Kind::Container);
        if (player->getOpenWorkbench())
            navigationMemory.observe(*player->getOpenWorkbench(), MinimapNavigation::Kind::Workbench);
        if (player->hasOpenContainer() || player->hasOpenCrafting())
        {
            drawHudNotifications(io.DisplaySize.y - 8.f);
            return;
        }
        if (hudInteraction.page() == HudInteraction::Page::Journal)
        {
            drawQuestJournal();
            return;
        }
        const PlayerSaveState state = player->getSaveState();
        if (hudInteraction.page() == HudInteraction::Page::Map)
        {
            drawTerrainMap(state);
            return;
        }
        hudNoticeLeftTop = 18.f;
        hudNoticeRightTop = 18.f;
        drawMinimap(state, io);
        const bool hasHeldStack = state.heldItem >= 0 &&
            state.heldItem < static_cast<int>(state.inventory.size()) &&
            state.inventory[static_cast<std::size_t>(state.heldItem)].amount > 0;
        const Material::ID heldMaterial = hasHeldStack
            ? state.inventory[static_cast<std::size_t>(state.heldItem)].materialId
            : Material::ID::Nothing;
        const ImVec2 center(io.DisplaySize.x * 0.5f,
                            io.DisplaySize.y * 0.5f);

        const ImGuiWindowFlags performanceFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        // The compact player HUD and F1 diagnostics share the same explicit
        // visibility state. A hidden panel must not reserve objective space.
        performanceOverlayBottom = 8.f;
        if (showDebugPanel)
        {
            ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.92f);
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
        }

        if (!hudInteraction.ownsInput() && !player->hasOpenContainer() && !player->hasOpenCrafting())
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
                if (particle.worldSpace)
                {
                    continue;
                }
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
            const float visibility = 0.65f;
            const ImU32 warning = IM_COL32(
                220, 35, 28,
                static_cast<int>(255.f * intensity * visibility));
            ImDrawList *foreground = ImGui::GetForegroundDrawList();
            for (const float x : {4.f, io.DisplaySize.x - 4.f})
            {
                const float dx = x < io.DisplaySize.x * .5f ? 22.f : -22.f;
                for (const float y : {4.f, io.DisplaySize.y - 4.f})
                {
                    const float dy = y < io.DisplaySize.y * .5f ? 22.f : -22.f;
                    foreground->AddLine(ImVec2(x, y), ImVec2(x + dx, y), warning, 3.f);
                    foreground->AddLine(ImVec2(x, y), ImVec2(x, y + dy), warning, 3.f);
                }
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
            ObjectiveSnapshot objective = world->getObjectiveSnapshot(!trackedObjectiveId.empty());
            if (!trackedObjectiveId.empty())
            {
                const auto entry = std::find_if(objective.journal.begin(), objective.journal.end(),
                    [&](const auto& value) { return value.id == trackedObjectiveId && value.available; });
                if (entry == objective.journal.end()) trackedObjectiveId.clear();
                else
                {
                    objective.currentId = entry->id;
                    objective.title = entry->title;
                    objective.instruction = entry->instruction;
                    objective.guidanceKey = entry->guidanceKey;
                    objective.progress = entry->progress;
                    objective.required = entry->required;
                    objective.sessionComplete = false;
                }
            }
            if (displayedObjectiveId != objective.currentId ||
                displayedObjectiveGuidanceKey != objective.guidanceKey)
            {
                displayedObjectiveId = objective.currentId;
                displayedObjectiveGuidanceKey = objective.guidanceKey;
                objectiveHintSeconds = 12.f;
            }
            const float objectiveWidth = std::min({210.f * appliedSettings.uiScale,
                io.DisplaySize.x * .52f, io.DisplaySize.x - minimapOverlayWidth - 54.f});
            ImGui::SetNextWindowPos(ImVec2(18.0f,
                                          performanceOverlayBottom + 10.f),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(objectiveWidth, 0.f), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f * appliedSettings.uiScale, 8.f * appliedSettings.uiScale));
            if (ImGui::Begin(
                    "##Objectives", nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        (hudInteraction.ownsInput() ? 0 : ImGuiWindowFlags_NoInputs) |
                        ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav))
            {
                const float scale = appliedSettings.uiScale;
                auto* draw = ImGui::GetWindowDrawList();
                const ImVec2 lo = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
                GameInterfaceWidgets::surface(draw, lo, ImVec2(lo.x + size.x, lo.y + size.y), true, scale);
                const ImVec2 header = ImGui::GetCursorScreenPos();
                const float width = ImGui::GetContentRegionAvail().x;
                const std::string currentTitle = objective.sessionComplete
                    ? tr("objective.complete.title")
                    : objectiveText(objective.currentId, "title", objective.title);
                const std::string instruction = objective.sessionComplete
                    ? tr("objective.complete.instruction")
                    : objectiveInstructionText(objective.currentId, objective.instruction, objective.guidanceKey);
                GameInterfaceWidgets::glyph(draw,GameInterfaceWidgets::Glyph::Feather,
                    ImVec2(header.x,header.y),20.f*scale);
                const auto title = boundedHudText(currentTitle, ImGui::GetFontSize(), width - 43.f * scale);
                draw->AddText(ImVec2(header.x + 25.f * scale, header.y),
                    ImGui::ColorConvertFloat4ToU32(WarmText), title.c_str());
                const float arrowX = header.x + width - 5.f * scale;
                draw->AddLine(ImVec2(arrowX - 3.f * scale, header.y + 4.f * scale),
                    ImVec2(arrowX + scale, header.y + 8.f * scale), IM_COL32(218, 222, 205, 255), 1.5f);
                draw->AddLine(ImVec2(arrowX + scale, header.y + 8.f * scale),
                    ImVec2(arrowX - 3.f * scale, header.y + 12.f * scale), IM_COL32(218, 222, 205, 255), 1.5f);
                ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeight() + 2.f * scale));
                const ImVec2 row = ImGui::GetCursorScreenPos();
                const float font = ImGui::GetFontSize() * .85f;
                const std::string count = std::to_string(std::min(objective.progress, objective.required)) +
                    " / " + std::to_string(objective.required);
                const float countWidth = objective.required > 0
                    ? ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.f, count.c_str()).x : 0.f;
                const std::string summary = boundedHudText(objective.currentId == "alpha.gather_wood"
                    ? tr("hud.collect_wood") : instruction, font, width - countWidth - 36.f * scale);
                adventureIcon(objectiveIcon(objective.currentId),ImVec2(row.x,row.y-2.f*scale),18.f*scale);
                draw->AddText(ImGui::GetFont(),font,ImVec2(row.x+25.f*scale,row.y),ImGui::ColorConvertFloat4ToU32(WarmText),summary.c_str());
                if (objective.required > 0)
                    draw->AddText(ImGui::GetFont(), font, ImVec2(row.x + width - countWidth, row.y),
                        ImGui::ColorConvertFloat4ToU32(WarmAccent), count.c_str());
                ImGui::Dummy(ImVec2(width, font));
                if (objective.required > 0)
                {
                    const ImVec2 bar = ImGui::GetCursorScreenPos();
                    GameInterfaceWidgets::progress(draw, bar, ImVec2(bar.x + width, bar.y + 3.f * scale),
                        float(objective.progress) / objective.required);
                    ImGui::Dummy(ImVec2(width, 3.f * scale));
                }
                if (hudInteraction.ownsInput() && ImGui::IsWindowHovered())
                    ImGui::SetTooltip("%s\n%s", instruction.c_str(), tr("journal.open").c_str());
                if (objective.opportunities.size() > 1 &&
                    !objective.sessionComplete && showDebugPanel)
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
                            objectiveInstructionText(opportunity.id,
                                          opportunity.instruction, opportunity.guidanceKey);
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
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.53f, .8f, .68f, 1.f));
                    ImGui::TextWrapped("%s", feedback.c_str());
                    ImGui::PopStyleColor();
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
                        ImGui::TextWrapped("%s: %s  %.0f m",
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
                if (hudInteraction.ownsInput() && ImGui::IsWindowHovered())
                {
                    const auto origin = ImGui::GetWindowPos();
                    const auto size = ImGui::GetWindowSize();
                    // Window hover captures the whole card without adding a
                    // layout item whose size feeds back into auto-resizing.
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        selectedJournalId = objective.currentId;
                        journalFilter = 0;
                        hudInteraction.open(HudInteraction::Page::Journal);
                    }
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::GetWindowDrawList()->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                        ImGui::ColorConvertFloat4ToU32(WarmAccent), 2.f, 0, 2.f);
                }
            }
            hudNoticeLeftTop = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y + 10.f;
            ImGui::End();
            ImGui::PopStyleVar();
        }

        if (!hudInteraction.ownsInput() && worldStats.combatFeedback.kind !=
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
        if (!hudInteraction.ownsInput()) drawHeldMaterial(state, io);
        const float scale = appliedSettings.uiScale;
        const float dockWidth = 300.f * scale;
        const float dockHeight = 94.f * scale;
        const ImVec2 dockMin((io.DisplaySize.x - dockWidth) * .5f, io.DisplaySize.y - 16.f - dockHeight);
        ImGui::SetNextWindowPos(dockMin, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(dockWidth, dockHeight), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f * scale, 8.f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f * scale, 4.f * scale));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings | (hudInteraction.ownsInput() ? 0 : ImGuiWindowFlags_NoInputs) | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
        if (ImGui::Begin("##OgrePlayerHud", nullptr, flags))
        {
            auto* draw = ImGui::GetWindowDrawList();
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const float contentWidth = dockWidth - 16.f * scale;
            const float healthRatio = worldStats.playerMaxHealth > 0.f
                ? std::clamp(worldStats.playerHealth / worldStats.playerMaxHealth, 0.f, 1.f) : 0.f;
            const ImU32 healthColour = healthRatio > .3f ? IM_COL32(146, 199, 143, 255) : IM_COL32(245, 139, 111, 255);
            const float font = ImGui::GetFontSize() * .8f;
            // A fixed health glyph and numeric value are readable without
            // relying on colour or a flashing full-screen warning.
            drawHudGlyph(draw, 1, ImVec2(origin.x - 5.f * scale, origin.y - 3.f * scale), 24.f * scale, healthColour);
            char healthValue[48];
            std::snprintf(healthValue, sizeof(healthValue), "%.0f / %.0f", std::ceil(worldStats.playerHealth), std::ceil(worldStats.playerMaxHealth));
            const float numberWidth = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0.f, healthValue).x;
            draw->AddText(ImGui::GetFont(), font,
                ImVec2(origin.x + contentWidth - numberWidth, origin.y),
                ImGui::ColorConvertFloat4ToU32(WarmText), healthValue);
            const ImVec2 barStart(origin.x + 22.f * scale, origin.y + 5.f * scale);
            const ImVec2 barEnd(origin.x + contentWidth - numberWidth - 6.f * scale, barStart.y + 7.f * scale);
            GameInterfaceWidgets::progress(draw, barStart, barEnd, healthRatio, healthColour);
            ImGui::Dummy(ImVec2(contentWidth, 21.f * scale));
            for (std::size_t index = 0; index < state.inventory.size(); ++index)
            {
                if (index > 0) ImGui::SameLine();
                drawHotbarSlot(state.inventory[index], index, static_cast<int>(index) == state.heldItem);
            }
            const ImVec2 statusOrigin = ImGui::GetCursorScreenPos();
            const float columnWidth = contentWidth / 3.f;
            const float statusFont = ImGui::GetFontSize() * .7f;
            const auto cooldown = [&](int column, const char* key, int ticks, bool guarding = false) {
                if (ticks <= 0 && !guarding) return;
                char value[32]; std::snprintf(value, sizeof(value), " %.1fs", ticks / 20.f);
                const std::string text = boundedHudText(guarding ? tr("hud.guarding") : tr(key) + value, statusFont, columnWidth - 5.f * scale);
                draw->AddText(ImGui::GetFont(), statusFont, ImVec2(statusOrigin.x + column * columnWidth, statusOrigin.y),
                    guarding ? IM_COL32(146, 216, 218, 255) : IM_COL32(188, 204, 198, 255), text.c_str());
            };
            cooldown(0, "hud.status_food", worldStats.foodCooldownTicksRemaining);
            cooldown(1, "hud.status_attack", worldStats.attackCooldownTicksRemaining);
            cooldown(2, "hud.status_guard", worldStats.combatFeedback.guardRecoverTicksRemaining, worldStats.combatFeedback.guarding);
            ImGui::Dummy(ImVec2(contentWidth, 14.f * scale));
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
        if (!hotbarDetailsVisible)
        {
            const float notificationBottom = drawHudActionStrip(dockMin.y - 9.f * scale, heldMaterial);
            drawHudNotifications(notificationBottom);
        }
    }

    float drawNotification(const std::string& text, float bottom, int side = 0)
    {
        const ImGuiIO& io = ImGui::GetIO();
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 padding(10.f * appliedSettings.uiScale, 6.f * appliedSettings.uiScale);
        const float fontSize = ImGui::GetFontSize() * .82f;
        const float maximumWidth = std::max(1.f,
            side == 0 ? std::min(520.f * appliedSettings.uiScale, io.DisplaySize.x - 32.f)
                      : std::min(300.f * appliedSettings.uiScale, io.DisplaySize.x * .5f - 58.f));
        const float wrapWidth = std::max(1.f, maximumWidth - 2.f * padding.x);
        const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, wrapWidth, text.c_str());
        const float width = std::min(maximumWidth, textSize.x + 2.f * padding.x);
        const float height = std::min(textSize.y + 2.f * padding.y,
            std::max(1.f, io.DisplaySize.y - 16.f));
        if (side != 0) bottom += height;
        bottom = std::clamp(bottom, height + 8.f, std::max(height + 8.f, io.DisplaySize.y - 8.f));
        const float left = side < 0 ? 18.f : side > 0 ? io.DisplaySize.x - 18.f - width
                                                            : (io.DisplaySize.x - width) * .5f;
        const ImVec2 topLeft(left, bottom - height);
        const ImVec2 bottomRight(topLeft.x + width, bottom);

        // Warnings must remain above crafting/container windows without taking
        // focus or intercepting their controls. Both text and background belong
        // to the foreground layer; a no-input window can still be covered.
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImVec4 background = style.Colors[ImGuiCol_WindowBg];
        background.w = .96f;
        draw->AddRectFilled(topLeft, bottomRight, ImGui::GetColorU32(background),
            style.WindowRounding);
        draw->AddRect(topLeft, bottomRight, ImGui::GetColorU32(ImGuiCol_Border),
            style.WindowRounding);
        draw->PushClipRect(topLeft, bottomRight, true);
        draw->AddLine(ImVec2(topLeft.x + 1.f, topLeft.y + 4.f), ImVec2(topLeft.x + 1.f, bottomRight.y - 4.f), IM_COL32(153, 183, 174, 255), 2.f);
        draw->AddText(ImGui::GetFont(), fontSize,
            ImVec2(topLeft.x + padding.x, topLeft.y + padding.y),
            ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr, wrapWidth);
        draw->PopClipRect();
        return topLeft.y - 6.f;
    }

    void drawHudNotifications(float notificationBottom)
    {
        const bool sideLanes = player != nullptr && !player->hasOpenContainer() && !player->hasOpenCrafting() &&
            flow->state() == GameApplicationState::Playing && ImGui::GetIO().DisplaySize.y < 600.f * appliedSettings.uiScale;
        const PresentationCaptionSnapshot caption =
            captionTimeline.snapshot();
        if (caption.visible())
        {
            const std::string localizedCaption =
                LocalizedPresentation::audioCaption(
                    appliedSettings.locale, caption.cueId,
                    caption.fallback);
            notificationBottom = drawNotification(
                "[" + tr("caption.prefix") + "] " + localizedCaption,
                sideLanes ? hudNoticeRightTop : notificationBottom, sideLanes ? 1 : 0);
        }
        if (statusMessageSeconds > 0.f && !statusMessage.empty() &&
            flow->state() == GameApplicationState::Playing)
        {
            drawNotification(statusMessage, sideLanes ? hudNoticeLeftTop : notificationBottom, sideLanes ? -1 : 0);
        }

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

        const BlockCapabilities capabilities = BlockCapabilityAccess::query(
            *world, *player->getOpenContainer());
        if (!capabilities.inventoryProvider)
        {
            player->closeContainer();
            return;
        }
        const InventoryProvider &provider =
            *capabilities.inventoryProvider;
        std::optional<InventoryProviderView> inventory =
            provider.view(*world, runtimeSmeltingRegistry());
        if (!inventory)
        {
            player->closeContainer();
            return;
        }

        std::optional<MachineProcessorView> processor;
        if (capabilities.machineProcessor &&
            runtimeSmeltingRegistry().isFrozen())
        {
            processor = capabilities.machineProcessor->view(
                *world, runtimeSmeltingRegistry());
        }
        if (processor)
        {
            const bool isCrusher = capabilities.machineProcessor->kind() ==
                MachineProcessorKind::Crusher;
            std::optional<MechanicalNodeSnapshot> mechanicalNode;
            if (isCrusher && capabilities.mechanicalPort)
            {
                mechanicalNode = capabilities.mechanicalPort->view(*world);
            }
            const std::string machineKey = isCrusher
                ? "crusher"
                : "furnace";
            const std::string widgetKey = isCrusher
                ? "crusher"
                : "furnace";
            const ImGuiIO &io = ImGui::GetIO();
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f,
                       (io.DisplaySize.y - 80.f) * 0.5f),
                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            const PresentationWindowLayout layout = fitPresentationWindow(
                io.DisplaySize.x, io.DisplaySize.y - 80.f, 700.f,
                isCrusher ? 540.f : 440.f, appliedSettings.uiScale);
            ImGui::SetNextWindowSize(
                ImVec2(layout.width, layout.height), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.96f);
            bool open = true;
            const ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoResize;
            const std::string machineTitle =
                label(machineKey + ".title", "##Machine");
            if (ImGui::Begin(machineTitle.c_str(), &open, flags))
            {
                const float slotWidth = (ImGui::GetContentRegionAvail().x -
                    (inventory->slotCount - 1) * ImGui::GetStyle().ItemSpacing.x) /
                    std::max(1, inventory->slotCount);
                for (int index = 0; index < inventory->slotCount; ++index)
                {
                    if (index > 0) ImGui::SameLine();
                    ImGui::BeginGroup();
                    const InventorySlotState &stack =
                        inventory->slots[index].state;
                    const InventorySlotRole role =
                        inventory->slots[index].role;
                    const std::string slotName = role == InventorySlotRole::Fuel
                        ? tr("furnace.fuel")
                        : tr(machineKey +
                             (role == InventorySlotRole::Output
                                  ? ".output"
                                  : ".input"));
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + slotWidth);
                    ImGui::TextDisabled("%s", slotName.c_str());
                    ImGui::PopTextWrapPos();
                    if (drawInventoryCard(stack.materialId, stack.amount,
                            widgetKey + std::to_string(index),
                            ImVec2(slotWidth, 62.f * appliedSettings.uiScale)) &&
                        stack.amount > 0 &&
                        provider.transferToPlayer(
                            *world, *player, index, stack.amount,
                            runtimeSmeltingRegistry()))
                    {
                        playUiFeedback();
                    }
                    ImGui::EndGroup();
                }
                const float machineProgress =
                    processor->recipeDurationTicks > 0
                        ? static_cast<float>(processor->progressTicks) /
                              static_cast<float>(
                                  processor->recipeDurationTicks)
                        : 0.f;
                const float powerProgress =
                    processor->powerTicksTotal > 0
                        ? static_cast<float>(
                              processor->powerTicksRemaining) /
                              static_cast<float>(processor->powerTicksTotal)
                        : 0.f;
                ImGui::TextUnformatted(
                    tr(isCrusher ? "crusher.progress"
                                 : "furnace.progress").c_str());
                ImGui::ProgressBar(machineProgress, ImVec2(-1.0f, 0.0f));
                ImGui::TextUnformatted(
                    tr(isCrusher ? "crusher.power_remaining"
                                 : "furnace.fuel_remaining").c_str());
                ImGui::ProgressBar(powerProgress, ImVec2(-1.0f, 0.0f));
                const std::string statusText =
                    tr(std::string("machine.status.") +
                       machineStatusName(processor->status));
                ImGui::Text("%s: %s", tr("machine.status").c_str(),
                            statusText.c_str());
                if (mechanicalNode && showDebugPanel)
                {
                    ImGui::Text("%s: %s",
                        tr("crusher.network_id").c_str(),
                        mechanicalNetworkIdString(
                            mechanicalNode->networkId).c_str());
                    ImGui::Text("%s: %llu",
                        tr("crusher.network_nodes").c_str(),
                        static_cast<unsigned long long>(
                            mechanicalNode->nodeCount));
                    ImGui::Text("%s: %llu",
                        tr("crusher.network_connections").c_str(),
                        static_cast<unsigned long long>(
                            mechanicalNode->connectionCount));
                }
                if (processor->manualPowerSupported &&
                    ImGui::Button(
                        label("crusher.crank", "##CrusherCrank").c_str(),
                        ImVec2(-1.f, 36.f * appliedSettings.uiScale)))
                {
                    if (capabilities.machineProcessor->supplyManualPower(
                            *world, *player))
                    {
                        playUiFeedback();
                    }
                }
                ImGui::Separator();
                ImGui::TextWrapped("%s",
                    tr(machineKey + ".inventory_hint").c_str());
                const int playerSlots = player->getInventorySlotCount();
                const float playerSlotWidth = (ImGui::GetContentRegionAvail().x -
                    (playerSlots - 1) * ImGui::GetStyle().ItemSpacing.x) /
                    std::max(1, playerSlots);
                for (int playerSlot = 0;
                     playerSlot < playerSlots;
                     ++playerSlot)
                {
                    if (playerSlot > 0) ImGui::SameLine();
                    const ItemStack &stack =
                        player->getInventorySlot(playerSlot);
                    if (drawInventoryCard(stack.getMaterial().id,
                            stack.getNumInStack(), widgetKey + "player" +
                                std::to_string(playerSlot),
                            ImVec2(playerSlotWidth, 54.f * appliedSettings.uiScale)) &&
                        !stack.isEmpty())
                    {
                        int target = -1;
                        if (isCrusher && stack.getMaterial().id ==
                                             Material::ID::Cobblestone)
                        {
                            target = 0;
                        }
                        else if (!isCrusher &&
                            runtimeSmeltingRegistry().findRecipe(
                                stack.getMaterial().id) != nullptr)
                        {
                            target = 0;
                        }
                        else if (!isCrusher &&
                            runtimeSmeltingRegistry().findFuel(
                                     stack.getMaterial().id) != nullptr)
                        {
                            target = 1;
                        }
                        if (target >= 0 && provider.transferFromPlayer(
                                *world, *player, target, playerSlot,
                                stack.getNumInStack(),
                                runtimeSmeltingRegistry()))
                        {
                            playUiFeedback();
                        }
                    }
                }
                if (ImGui::Button(
                        label("common.close", isCrusher
                            ? "##CloseCrusher"
                            : "##CloseFurnace").c_str(),
                        ImVec2(100.0f, 32.0f)))
                {
                    open = false;
                    playUiFeedback();
                }
            }
            ImGui::End();
            if (!open) player->closeContainer();
            return;
        }

        const ImGuiIO &io = ImGui::GetIO();
        const float scale = appliedSettings.uiScale;
        const float captionSpace = 64.f;
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), io.DisplaySize, IM_COL32(5, 12, 16, 85));
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * .5f, (io.DisplaySize.y - captionSpace) * .5f),
            ImGuiCond_Always, ImVec2(.5f, .5f));
        const PresentationWindowLayout layout = fitPresentationWindow(
            io.DisplaySize.x, io.DisplaySize.y - captionSpace, 700.f, 520.f * scale, scale);
        ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height), ImGuiCond_Always);
        bool open = true;
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground;
        if (ImGui::Begin("##Chest", nullptr, flags))
        {
            if (drawInventoryHeader(Material::ID::Chest, tr("chest.title"), tr("inventory.storage_subtitle"))) open = false;
            const ImGuiStyle &style = ImGui::GetStyle();
            const float width = ImGui::GetContentRegionAvail().x;
            const float slotGap = 8.f * scale;
            const int playerSlots = player->getInventorySlotCount();
            const float hotbarCell = std::min(58.f * scale,
                (width - (playerSlots - 1) * slotGap) / std::max(1, playerSlots));
            const float footer = ImGui::GetTextLineHeight() + hotbarCell +
                ImGui::GetFrameHeight() + 6.f * style.ItemSpacing.y + 1.f;
            drawInventoryHeading(tr("inventory.storage"), std::to_string(inventory->slotCount) + " " + tr("inventory.slots"));
            const float bodyHeight = std::max(1.f, ImGui::GetContentRegionAvail().y - footer);
            const int columns = bodyHeight < 210.f * scale ? 5 : 3;
            const int rows = (inventory->slotCount + columns - 1) / columns;
            const float cell = std::min(76.f * scale, std::max(24.f,
                (bodyHeight - (rows - 1) * slotGap - 4.f) / rows));
            const float gridWidth = columns * cell + (columns - 1) * slotGap;
            InventorySlotState inspected{};
            for (int slot = 0; slot < inventory->slotCount; ++slot)
                if (inventory->slots[slot].state.amount > 0) { inspected = inventory->slots[slot].state; break; }
            ImGui::BeginGroup();
            ImGui::BeginChild("##ChestContents", ImVec2(gridWidth, bodyHeight), false);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(slotGap, slotGap));
            for (int slot = 0; slot < inventory->slotCount; ++slot)
            {
                if (slot % columns != 0) ImGui::SameLine();
                const InventorySlotState stack = inventory->slots[slot].state;
                const bool clicked = drawInventoryCard(stack.materialId, stack.amount, "chest" + std::to_string(slot),
                    ImVec2(cell, cell), false, true);
                if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && io.NavVisible)) inspected = stack;
                if (clicked && stack.amount > 0 && provider.transferToPlayer(
                    *world, *player, slot, stack.amount, runtimeSmeltingRegistry())) playUiFeedback();
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::SameLine(0.f, 22.f * scale);
            ImGui::BeginChild("##ItemInspection", ImVec2(std::max(1.f, width - gridWidth - 22.f * scale), bodyHeight), false);
            const ImVec2 previewStart = ImGui::GetCursorScreenPos();
            const float previewWidth = ImGui::GetContentRegionAvail().x;
            const float portraitHeight = bodyHeight < 155.f * scale ? 0.f :
                std::min(126.f * scale, bodyHeight - 3.f * ImGui::GetTextLineHeightWithSpacing() - style.ItemSpacing.y);
            if (portraitHeight > 0.f)
            {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(previewStart, ImVec2(previewStart.x + previewWidth, previewStart.y + portraitHeight),
                    IM_COL32(23, 36, 41, 255), 2.f);
                const ImVec2 center(previewStart.x + previewWidth * .5f, previewStart.y + portraitHeight * .48f);
                draw->AddLine(ImVec2(center.x - 25.f * scale, previewStart.y + portraitHeight - 8.f),
                    ImVec2(center.x + 25.f * scale, previewStart.y + portraitHeight - 8.f), IM_COL32(103, 128, 121, 120));
                if (inspected.amount > 0) drawItemPortrait(inspected.materialId, center, portraitHeight * .62f);
                ImGui::Dummy(ImVec2(previewWidth, portraitHeight));
            }
            ImGui::TextWrapped("%s", inspected.amount > 0 ? materialName(inspected.materialId).c_str() : tr("common.empty").c_str());
            if (inspected.amount > 0)
                ImGui::TextColored(WarmAccent, "%s  %d", tr("inventory.quantity").c_str(), inspected.amount);
            ImGui::PushStyleColor(ImGuiCol_Text, WarmMuted);
            ImGui::TextWrapped("%s", tr("inventory.take_hint").c_str());
            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::EndGroup();
            ImGui::Separator();
            drawInventoryHeading(tr("inventory.carried"), tr("inventory.store_hint"));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(slotGap, slotGap));
            for (int slot = 0; slot < playerSlots; ++slot)
            {
                if (slot > 0) ImGui::SameLine();
                const ItemStack &stack = player->getInventorySlot(slot);
                const bool clicked = drawInventoryCard(stack.getMaterial().id, stack.getNumInStack(),
                    "player" + std::to_string(slot), ImVec2(hotbarCell, hotbarCell), false, true, slot + 1);
                if (clicked && !stack.isEmpty() && provider.transferFromPlayer(*world, *player,
                    InventoryProvider::AutomaticSlot, slot, stack.getNumInStack(), runtimeSmeltingRegistry())) playUiFeedback();
            }
            ImGui::PopStyleVar();
            ImGui::Spacing();
            if (ImGui::Button((tr("common.close") + "  [Esc]##CloseChest").c_str())) { open = false; playUiFeedback(); }
        }
        ImGui::End();
        if (!open) player->closeContainer();
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
        std::size_t eligibleRecipes = 0;
        std::size_t learnedRecipes = 0;
        for (const RecipeDefinition &recipe :
             runtimeRecipeRegistry().recipes())
        {
            if (recipeFitsGrid(recipe, gridSize))
            {
                ++eligibleRecipes;
                if (world != nullptr && world->isRecipeDiscovered(recipe.id))
                    ++learnedRecipes;
            }
        }

        const ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x * 0.5f, (io.DisplaySize.y - 64.f) * 0.5f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        const PresentationWindowLayout layout = fitPresentationWindow(
            io.DisplaySize.x, io.DisplaySize.y - 64.f,
            learnedRecipes == 0 ? 700.0f : 820.0f,
            (learnedRecipes == 0 ? 480.0f : 580.0f) * appliedSettings.uiScale,
            appliedSettings.uiScale);
        ImGui::SetNextWindowSize(ImVec2(layout.width, layout.height), ImGuiCond_Always);
        bool open = true;
        const std::string title =
            gridSize == CraftingSession::WorkbenchGridSize
                ? label("crafting.workbench_title", "##Crafting")
                : label("crafting.player_title", "##Crafting");
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), io.DisplaySize, IM_COL32(5, 12, 16, 85));
        if (ImGui::Begin(title.c_str(), nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings))
        {
            if (drawInventoryHeader(Material::ID::Workbench,
                tr(gridSize == CraftingSession::WorkbenchGridSize ? "crafting.workbench_title" : "crafting.player_title"),
                tr("inventory.craft_subtitle"))) open = false;
            // Recipes and materials may scroll without moving the complete input
            // grid or the result controls out of view.
            const float resultHeight = std::max(58.f * appliedSettings.uiScale,
                2.f * ImGui::GetTextLineHeightWithSpacing() + 2.f * ImGui::GetStyle().ItemSpacing.y);
            const float resultFooterHeight = resultHeight + 32.f * appliedSettings.uiScale +
                5.f * ImGui::GetStyle().ItemSpacing.y + 2.f;
            ImGui::BeginChild("##CraftingContent", ImVec2(0.f, -resultFooterHeight), false);
            const ImGuiStyle &craftingStyle = ImGui::GetStyle();
            const float preferredCellWidth = 46.f * appliedSettings.uiScale;
            const std::string gridTitle = std::to_string(gridSize) + "x" +
                std::to_string(gridSize) + " " + tr("crafting.input_grid");
            const float gridPanelWidth = std::max(
                gridSize * preferredCellWidth + (gridSize - 1) * craftingStyle.ItemSpacing.x,
                ImGui::CalcTextSize(gridTitle.c_str()).x) + 2.f * craftingStyle.WindowPadding.x;
            const float materialsWidth = std::max(180.f,
                ImGui::GetContentRegionAvail().x - gridPanelWidth - craftingStyle.ItemSpacing.x);
            ImGui::BeginChild("##CraftingMaterials", ImVec2(materialsWidth, 0.f), false);
            if (ImGui::CollapsingHeader(tr("crafting.recipe_book").c_str()))
            {
                ImGui::Text("%s: %zu / %zu",
                            tr("crafting.recipe_book_progress").c_str(),
                            learnedRecipes, eligibleRecipes);
                if (learnedRecipes == 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, WarmMuted);
                    ImGui::TextWrapped("%s", tr("crafting.recipe_book_hint").c_str());
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::BeginChild("##RecipeBook",
                                      ImVec2(0.0f, 135.0f), true);
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
                                    materialName(recipe.outputMaterialId) +
                                    ": " + tr("crafting.loaded");
                                playUiFeedback();
                            }
                        }
                        ImGui::SameLine();
                        const std::string ingredients =
                            recipeIngredientSummary(recipe,
                                                    appliedSettings.locale);
                        const std::string outputName =
                            materialName(recipe.outputMaterialId);
                        ImGui::TextWrapped("%s x%d  <-  %s", outputName.c_str(),
                                    recipe.outputCount,
                                    ingredients.c_str());
                    }
                    ImGui::EndChild();
                }
            }
            ImGui::Separator();

            const PlayerSaveState state = player->getSaveState();
            drawInventoryHeading(tr("crafting.inventory"), "");
            const float craftingInventoryWidth = ImGui::GetContentRegionAvail().x;
            const int inventoryColumns = craftingInventoryWidth >= 420.f * appliedSettings.uiScale ? 3 : 2;
            for (std::size_t index = 0; index < state.inventory.size();
                 ++index)
            {
                if (index % inventoryColumns != 0)
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
                ImGui::BeginDisabled(slot.amount <= 0);
                if (drawInventoryCard(slot.materialId, slot.amount, label,
                    ImVec2((craftingInventoryWidth - (inventoryColumns - 1) * craftingStyle.ItemSpacing.x) /
                               inventoryColumns, 56.f),
                    slot.materialId == selectedCraftingMaterial && slot.amount > 0))
                {
                    selectedCraftingMaterial = slot.materialId;
                    craftingMessage.clear();
                }
                ImGui::EndDisabled();
            }
            ImGui::TextWrapped("%s: %s", tr("crafting.selected").c_str(),
                        materialName(selectedCraftingMaterial).c_str());
            if (ImGui::SmallButton(label("crafting.clear_selection", "##ClearSelection").c_str()))
            {
                selectedCraftingMaterial = Material::ID::Nothing;
            }

            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##CraftingGrid", ImVec2(0.f, 0.f), false);
            ImGui::TextUnformatted(gridTitle.c_str());
            const ImVec2 gridSpace = ImGui::GetContentRegionAvail();
            const float craftingCellWidth = std::max(24.f, std::min({preferredCellWidth,
                (gridSpace.x - (gridSize - 1) * craftingStyle.ItemSpacing.x) / gridSize,
                (gridSpace.y - ImGui::GetTextLineHeight() -
                 gridSize * craftingStyle.ItemSpacing.y - 3.f * appliedSettings.uiScale) / gridSize}));
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
                const bool cellClicked = drawInventoryCard(cell.materialId, cell.amount, label,
                    ImVec2(craftingCellWidth, craftingCellWidth), false, true);
                const bool clearCell = ImGui::IsItemClicked(ImGuiMouseButton_Right);
                if (cellClicked && selectedCraftingMaterial != Material::ID::Nothing)
                {
                    craftingSession->setCell(
                        index, selectedCraftingMaterial);
                    craftingMessage.clear();
                }
                if (clearCell)
                {
                    craftingSession->clearCell(index);
                    craftingMessage.clear();
                }
            }
            if (ImGui::SmallButton(label("crafting.clear_grid", "##ClearGrid").c_str()))
            {
                craftingSession->clear();
                craftingMessage.clear();
            }

            ImGui::EndChild();
            ImGui::EndChild();
            const CraftingPreview preview = player->previewCrafting(
                *craftingSession, runtimeRecipeRegistry());
            ImGui::Separator();
            ImGui::BeginChild("##CraftResult", ImVec2(0.f, resultHeight), false);
            const ImVec2 resultStart = ImGui::GetCursorScreenPos();
            const float iconSize = 48.f * appliedSettings.uiScale;
            GameInterfaceWidgets::slotFrame(ImGui::GetWindowDrawList(), resultStart,
                ImVec2(resultStart.x + iconSize, resultStart.y + iconSize), preview.ready(), false, false, appliedSettings.uiScale);
            if (!preview.recipeId.empty())
                drawItemPortrait(preview.outputMaterialId,
                    ImVec2(resultStart.x + iconSize * .5f, resultStart.y + iconSize * .48f), iconSize * .66f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + iconSize + 14.f * appliedSettings.uiScale);
            ImGui::BeginGroup();
            if (preview.recipeId.empty())
                ImGui::TextColored(WarmMuted, "%s", tr("crafting.output").c_str());
            else
                ImGui::TextColored(preview.ready() ? WarmAccent : WarmMuted, "%s  x%d",
                    materialName(preview.outputMaterialId).c_str(), preview.outputCount);
            ImGui::SetWindowFontScale(.85f);
            if (!craftingMessage.empty())
                ImGui::TextWrapped("%s", craftingMessage.c_str());
            else if (preview.ready())
                ImGui::TextWrapped("%s: %d", tr("crafting.maximum_crafts").c_str(), preview.maxCrafts);
            else
                ImGui::TextWrapped("%s", craftingPreviewMessage(preview.status).c_str());
            ImGui::SetWindowFontScale(1.f);
            ImGui::EndGroup();
            ImGui::EndChild();
            const float actionsWidth = ImGui::GetContentRegionAvail().x - 2.f * ImGui::GetStyle().ItemSpacing.x;
            const float actionHeight = 32.f * appliedSettings.uiScale;
            ImGui::BeginDisabled(!preview.ready());
            pushPrimaryButtonStyle();
            if (ImGui::Button(label("crafting.craft_one", "##CraftOne").c_str(), ImVec2(actionsWidth * .36f, actionHeight)))
            {
                const CraftingCommitResult committed =
                    player->commitCrafting(
                        *craftingSession, runtimeRecipeRegistry(), preview,
                        1);
                craftingMessage = craftingCommitMessage(committed.status);
            }
            ImGui::PopStyleColor(4);
            ImGui::SameLine();
            const std::string maximumLabel = tr("crafting.craft_maximum") +
                (preview.ready() ? " (" + std::to_string(preview.maxCrafts) + ")" : "") + "###CraftMaximum";
            if (ImGui::Button(maximumLabel.c_str(), ImVec2(actionsWidth * .38f, actionHeight)))
            {
                const CraftingCommitResult committed =
                    player->commitCrafting(
                        *craftingSession, runtimeRecipeRegistry(), preview,
                        preview.maxCrafts);
                craftingMessage = craftingCommitMessage(committed.status);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button(label("common.close", "##CloseCrafting").c_str(), ImVec2(actionsWidth * .26f, actionHeight)))
            {
                open = false;
                playUiFeedback();
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
            for (const SimulationWorkPlan &plan :
                 worldStats.simulation.scheduledWorkloads) {
                ImGui::Text(
                    "  Scheduler %s: admitted / deferred / eligible: %llu / %llu / %llu | budget: %llu | first: %llu | service window: %llu ticks",
                    simulationScheduledWorkloadName(plan.workload),
                    static_cast<unsigned long long>(plan.admitted),
                    static_cast<unsigned long long>(plan.deferred),
                    static_cast<unsigned long long>(plan.eligible),
                    static_cast<unsigned long long>(plan.budget),
                    static_cast<unsigned long long>(plan.firstIndex),
                    static_cast<unsigned long long>(
                        plan.serviceWindowTicks));
            }
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
                        "  %s: %.3f ms | processed / deferred / eligible: %llu / %llu / %llu | budget: %llu %s (%s) | window: %llu%s",
                        worldSimulationPhaseName(phase.phase),
                        metrics->elapsedMilliseconds,
                        static_cast<unsigned long long>(metrics->processed),
                        static_cast<unsigned long long>(metrics->deferred),
                        static_cast<unsigned long long>(metrics->eligible),
                        static_cast<unsigned long long>(metrics->budget),
                        simulationPhaseBudgetScopeName(
                            metrics->budgetScope),
                        simulationPhaseBudgetStatusName(
                            metrics->budgetStatus()),
                        static_cast<unsigned long long>(
                            metrics->serviceWindowTicks),
                        metrics->schedulerManaged ? " scheduled" : "");
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
                "Jobs pending load/mesh/deferred-plan: %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.pendingGenerationJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.pendingMeshJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.deferredPlanJobs));
            ImGui::Text(
                "Backpressure %s pending/high/low/cap: %llu / %llu / %llu / %llu",
                WorldJobScheduler::pressureName(
                    worldStats.worldJobs.pressureLevel),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.pendingJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.pendingHighWatermark),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.pendingLowWatermark),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.maxPendingJobs));
            ImGui::Text(
                "Jobs submitted/started/completed: %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.submittedJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.startedJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.completedJobs));
            ImGui::Text(
                "Jobs load/mesh/work/none/reject/cancel: %llu / %llu / %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.chunkLoadOrGenerateJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.chunkMeshBuildJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.didWorkJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.noWorkJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.commitRejectedJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.cancelledJobs));
            ImGui::Text(
                "Job generation/current invalidations: %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.currentGeneration),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.generationInvalidations));
            ImGui::Text(
                "Job cancellation pending/submit/plan: %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.cancelledPendingJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.staleSubmitRejections),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.stalePlanRejections));
            ImGui::Text(
                "Job admission accepted/shed/duplicate/cap: %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.acceptedAdmissions),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.acceptedAfterSheddingJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.duplicateAdmissionRejections),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.capacityAdmissionRejections));
            ImGui::Text(
                "Job shedding total/load/mesh transitions/saturation: %llu / %llu / %llu / %llu / %llu",
                static_cast<unsigned long long>(
                    worldStats.worldJobs.shedPendingJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.shedGenerationJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.shedMeshJobs),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.pressureTransitions),
                static_cast<unsigned long long>(
                    worldStats.worldJobs.saturationEpisodes));
            ImGui::Text(
                "Streaming commits/uploads/unloads: %llu/%llu  %llu/%llu (+%llu)  %llu/%llu%s",
                static_cast<unsigned long long>(
                    worldStats.streamingBackpressure
                        .lastAuthoritativeCommits),
                static_cast<unsigned long long>(
                    worldStats.streamingBackpressure
                        .maxAuthoritativeCommitsPerPass),
                static_cast<unsigned long long>(
                    worldStats.streamingBackpressure
                        .lastSectionUploadsOffered),
                static_cast<unsigned long long>(
                    worldStats.streamingBackpressure
                        .maxSectionUploadsPerFrame),
                static_cast<unsigned long long>(
                    worldStats.streamingBackpressure
                        .lastSectionUploadsDeferred),
                static_cast<unsigned long long>(
                    worldStats.streamingBackpressure.lastUnloads),
                static_cast<unsigned long long>(
                    worldStats.streamingBackpressure.maxUnloadsPerUpdate),
                worldStats.streamingBackpressure.unloadBacklog
                    ? " backlog" : "");
            ImGui::Text(
                "Spatial interest R/N/S: %llu / %llu / %llu (total %llu, rev %llu)",
                static_cast<unsigned long long>(
                    worldStats.spatialInterest.residentDataCells),
                static_cast<unsigned long long>(
                    worldStats.spatialInterest.nearRepresentationCells),
                static_cast<unsigned long long>(
                    worldStats.spatialInterest.simulationRequestedCells),
                static_cast<unsigned long long>(
                    worldStats.spatialInterest.totalCells),
                static_cast<unsigned long long>(
                    worldStats.spatialInterest.demandRevision));
            ImGui::Text(
                "Mechanical nodes/edges/components: %llu / %llu / %llu (rev %llu, rebuild %llu, visited %llu%s)",
                static_cast<unsigned long long>(
                    worldStats.mechanicalTopology.nodes),
                static_cast<unsigned long long>(
                    worldStats.mechanicalTopology.connections),
                static_cast<unsigned long long>(
                    worldStats.mechanicalTopology.components),
                static_cast<unsigned long long>(
                    worldStats.mechanicalTopology.revision),
                static_cast<unsigned long long>(
                    worldStats.mechanicalTopology.rebuildCount),
                static_cast<unsigned long long>(
                    worldStats.mechanicalTopology.lastVisitedNodes),
                worldStats.mechanicalTopology.dirty ? ", dirty" : "");
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
    double hudElapsedSeconds = 0.0;
    float heldMovement = 0.f;
    float performanceSampleSeconds = 0.f;
    float performanceSamplePeakMs = 0.f;
    float displayedFramesPerSecond = 0.f;
    float displayedFrameMs = 0.f;
    float displayedPeakFrameMs = 0.f;
    float performanceOverlayBottom = 90.f;
    std::string displayedObjectiveId;
    std::string displayedObjectiveGuidanceKey;
    std::string selectedJournalId;
    std::string trackedObjectiveId;
    int journalFilter = 0;
    float objectiveHintSeconds = 12.f;
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
    std::array<MinimapCell,
               MinimapCellCount * MinimapCellCount> minimapCells{};
    int minimapStep = 2;
    TerrainBiome minimapBiome = TerrainBiome::Grassland;
    MinimapNavigation::Memory navigationMemory;
    HudInteraction hudInteraction;
    bool hotbarDetailsVisible = false;
    std::string hudPageFixture;
    bool hudPageFixtureOpened = false;
    int inspectSlotFixture = -1;
    float hudPageFixtureSeconds = 0;
    MapSurfaceRegion<MinimapCell> detailMap;
    double detailMapNextRefresh = 0;
    std::uint64_t overviewSourceRevision = 0;
    TerrainMapView::View mapView;
    std::vector<TerrainMapView::Face> mapFaces;
    TerrainMapView::Bounds mapBounds;
    float mapFloorHeight = 0.f;
    static constexpr int OverviewCellCount = 65;
    std::array<MinimapCell, OverviewCellCount * OverviewCellCount> overviewCells{};
    std::array<std::pair<int, int>, OverviewCellCount * OverviewCellCount>
        overviewObservedPositions{};
    TerrainMapView::OverviewScale overviewScale;
    bool overviewAutoFit = true, overviewFollowsFit = true;
    ImVec2 overviewLastViewport{};
    bool mapFlatOverview = false;
    bool mapMarkerPanel = false;
    ExplorationMarkerEditor mapMarkerEditor;
    std::array<char, ExplorationMarkers::MaxNameBytes + 1> newMapMarkerName{};
    std::string mapMarkerFeedbackKey;
    std::int64_t overviewOffsetX = 0, overviewOffsetZ = 0;
    std::int64_t overviewCenterX = 0, overviewCenterZ = 0;
    float overviewPanRemainderX = 0.f, overviewPanRemainderZ = 0.f;
    double overviewNextRefresh = 0.0;
    bool overviewValid = false;
    int selectedOverviewCell = -1;
    std::uint64_t minimapRevision = 1, mapBuiltRevision = 0;
    float mapBuiltYaw = 0, mapBuiltPitch = 0, mapBuiltBase = 0;
    int selectedMapCell = -1;
    TerrainMapView::Gesture mapGesture;
    int minimapRefreshRow = 0;
    double minimapNextRefresh = 0.0;
    int minimapSeed = 0;
    int minimapGenerationVersion = 0;
    int minimapCenterX = std::numeric_limits<int>::min();
    int minimapCenterZ = std::numeric_limits<int>::min();
    float minimapOverlayWidth = 0.f;
    float hudNoticeLeftTop = 18.f;
    float hudNoticeRightTop = 18.f;
    bool minimapValid = false;
    bool settingsFixtureRequested = false;
    bool settingsFixtureOpened = false;
    bool showCredits = false;
    bool initialized = false;
    bool listenerInstalled = false;
    bool framePending = false;
    Ogre::TexturePtr atlasTexture;
    ImTextureID atlasTextureId = ImTextureID_Invalid;
    Ogre::TexturePtr hudPanelTexture;
    ImTextureID hudPanelTextureId = ImTextureID_Invalid;
    Ogre::TexturePtr hudGlyphTexture;
    ImTextureID hudGlyphTextureId = ImTextureID_Invalid;
    Ogre::TexturePtr menuTexture;
    ImTextureID menuTextureId = ImTextureID_Invalid;
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

void OgreUserInterface::focusChanged(bool focused)
{
    ImGuiIO &io = ImGui::GetIO();
    if (!focused)
    {
        // Cancel the interaction even when focus returns before the next frame.
        // A backend release after focus loss must not activate a pressed widget.
        io.ClearEventsQueue();
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
    io.AddFocusEvent(focused);
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
    return m_impl->hudInteraction.ownsInput() || m_impl->flow->state() != GameApplicationState::Playing ||
           (m_impl->player != nullptr &&
            (m_impl->player->hasOpenContainer() ||
             m_impl->player->hasOpenCrafting())) ||
           m_impl->victoryOverlayVisible() ||
           ImGui::GetIO().WantCaptureKeyboard;
}

bool OgreUserInterface::wantsMouseInput() const
{
    return m_impl->hudInteraction.ownsInput() || m_impl->flow->state() != GameApplicationState::Playing ||
           (m_impl->player != nullptr &&
            (m_impl->player->hasOpenContainer() ||
             m_impl->player->hasOpenCrafting())) ||
           m_impl->victoryOverlayVisible() ||
           ImGui::GetIO().WantCaptureMouse;
}

bool OgreUserInterface::wantsHudPointer() const noexcept
{
    return m_impl->flow->state() == GameApplicationState::Playing &&
        m_impl->hudInteraction.ownsInput();
}

bool OgreUserInterface::toggleHudPointer() noexcept
{
    if (m_impl->flow->state() != GameApplicationState::Playing || m_impl->player == nullptr ||
        m_impl->player->hasOpenContainer() || m_impl->player->hasOpenCrafting() || hasBlockingModal())
        return false;
    return m_impl->hudInteraction.togglePointer(ImGui::GetIO().WantTextInput ||
        ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
}

bool OgreUserInterface::dismissHudInteraction() noexcept
{
    return m_impl->hudInteraction.dismiss();
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
    m_impl->displayedObjectiveId.clear();
    m_impl->displayedObjectiveGuidanceKey.clear();
    m_impl->selectedJournalId.clear();
    m_impl->trackedObjectiveId.clear();
    m_impl->journalFilter = 0;
    m_impl->objectiveHintSeconds = 12.f;
    m_impl->navigationMemory.clear();
    m_impl->hudInteraction.dismiss();
    m_impl->minimapRefreshRow = 0;
    m_impl->minimapNextRefresh = 0.f;
    m_impl->minimapCenterX = std::numeric_limits<int>::min();
    m_impl->minimapCenterZ = std::numeric_limits<int>::min();
    m_impl->minimapValid = false;
    ++m_impl->minimapRevision;
    m_impl->detailMap = {};
    m_impl->detailMapNextRefresh = 0;
    m_impl->overviewSourceRevision = 0;
    m_impl->mapBuiltRevision = 0;
    m_impl->mapFaces.clear();
    m_impl->mapView = {};
    m_impl->mapGesture = {};
    m_impl->selectedMapCell = -1;
    m_impl->selectedOverviewCell = -1;
    m_impl->mapMarkerEditor.clear();
    m_impl->mapMarkerPanel = false;
    m_impl->newMapMarkerName.fill(0);
    m_impl->mapMarkerFeedbackKey.clear();
    m_impl->overviewOffsetX = m_impl->overviewOffsetZ = 0;
    m_impl->overviewScale = {};
    m_impl->overviewAutoFit = m_impl->overviewFollowsFit = true;
    m_impl->overviewLastViewport = {};
    m_impl->mapFlatOverview = false;
    m_impl->overviewPanRemainderX = m_impl->overviewPanRemainderZ = 0.f;
    m_impl->overviewValid = false;
    if (world != nullptr)
    {
        m_impl->statusMessage.clear();
        m_impl->statusMessageSeconds = 0.f;
        const DifficultyRuntimeSnapshot snapshot =
            world->getDifficultySnapshot();
        m_impl->pauseDifficulty = static_cast<int>(
            snapshot.changePending ? snapshot.pending : snapshot.active);
        m_impl->difficultyDraftInitialized = true;
    }
}

void OgreUserInterface::showWorldBackups(const std::string& worldId)
{
    m_impl->selectedWorldId.clear();
    m_impl->backups.clear();
    m_impl->statusMessage.clear();
    m_impl->refreshCatalogue();
    const auto found = std::find_if(m_impl->worlds.begin(), m_impl->worlds.end(),
        [&](const auto& entry) { return entry.id == worldId; });
    if (found == m_impl->worlds.end()) {
        if (m_impl->statusMessage.empty())
            m_impl->setStatusMessage(m_impl->tr("world.operation_failed"));
        return;
    }
    m_impl->selectWorld(*found);
    if (m_impl->statusMessage.empty())
        m_impl->setStatusMessage(m_impl->tr("map.backup_choose"));
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
