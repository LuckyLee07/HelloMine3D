#include "../../src/HelloMine3D/Presentation/HudInteraction.h"
#include "../../src/HelloMine3D/GameplayInput.h"
#include "../../src/HelloMine3D/Presentation/ExplorationMapInteraction.h"
#include "../../src/HelloMine3D/World/Exploration/ExplorationMapStatus.h"
#include <iostream>
#include <stdexcept>

int main()
{
    unsigned count = 0;
    const auto check = [&](bool value, const char* message) {
        if (!value) throw std::runtime_error(message);
        ++count;
    };
    ExplorationMarkerEditor editor;
    editor.select(1, "基地 A");
    std::snprintf(editor.name.data(), editor.name.size(), "%s", "A 的未保存编辑");
    editor.select(2, "营地 B");
    check(editor.id == 2 && std::string(editor.name.data()) == "营地 B",
          "canvas selection kept another marker's pending name");
    editor.select(1, "基地 A");
    check(editor.id == 1 && std::string(editor.name.data()) == "基地 A",
          "list reselection failed to restore the stored name");
    editor.clear();
    check(editor.id == 0 && editor.name[0] == 0,
          "world switch or deletion retained a marker edit target");
    ExplorationMapStatus health;
    check(!health.needsAttention() && !health.recoveryMessageKey(),
          "healthy map displayed a warning");
    health.full = true;
    check(health.needsAttention() && !health.recoveryMessageKey(),
          "full map lost its independent notice");
    health.resetReason = ExplorationMapStatus::ResetReason::Corrupt;
    check(std::string(health.recoveryMessageKey()) == "map.status_reset",
          "corrupt map did not explain history reset");
    health.resetReason = ExplorationMapStatus::ResetReason::ForeignIdentity;
    check(std::string(health.recoveryMessageKey()) == "map.status_foreign",
          "foreign map was described as a normal empty map");
    health.saveFailed = true;
    check(std::string(health.recoveryMessageKey()) == "map.status_save_failed",
          "save failure was hidden behind reset status");
    health.quarantineFailed = true;
    check(std::string(health.recoveryMessageKey()) == "map.status_isolation_failed",
          "failed isolation was incorrectly described as successful reset");
    HudInteraction ui;
    using Page = HudInteraction::Page;
    check(!ui.ownsInput() && ui.page() == Page::Game, "ordinary startup owns no UI input");
    check(!ui.open(Page::Journal), "captured gameplay cannot accidentally activate HUD");
    check(!ui.open(Page::Map), "map requires pointer ownership");
    check(!ui.dismiss(), "Escape outside HUD remains available to pause");
    for (const auto page : {Page::Pointer, Page::Journal, Page::Map}) {
        ui.togglePointer();
        check(ui.ownsInput() && ui.page() == Page::Pointer, "Tab releases pointer");
        if (page != Page::Pointer) check(ui.open(page), "click opens requested page");
        check(!ui.open(Page::Game), "page click cannot bypass close transition");
        check(ui.ownsInput(), "all HUD pages own movement/buttons/wheel");
        check(ui.dismiss() && ui.page() == Page::Game, "Escape returns to game");
        check(!ui.dismiss(), "close is consumed once");
    }
    ui.togglePointer(); ui.open(Page::Map); ui.togglePointer();
    check(!ui.ownsInput(), "Tab from map returns to game");
    ui.togglePointer(); ui.open(Page::Map);
    check(!ui.togglePointer(true) && ui.page()==Page::Map && ui.ownsInput(),
          "typing L or grave in a marker must not release input to gameplay");
    check(ui.togglePointer(false) && !ui.ownsInput(),
          "pointer shortcut is available again after text entry ends");
    GameplayFocusGate gate;
    check(gate.isFocused(), "initial focus");
    gate.suppressUntilRelease();
    check(gate.isFocused(), "UI transition does not pretend OS focus changed");
    check(!gate.allowsWorldButtons(true), "closing click cannot mine");
    check(!gate.allowsWorldButtons(true), "held click remains blocked");
    check(!gate.acceptsLookSample(), "cursor transition look discarded");
    check(gate.acceptsLookSample(), "later look accepted");
    check(gate.allowsWorldButtons(false), "release rearms world input");
    check(gate.allowsWorldButtons(true), "new world click accepted");
    gate.setFocused(false); gate.suppressUntilRelease();
    check(!gate.allowsWorldButtons(false) && !gate.acceptsLookSample(), "UI transition cannot grant background input");
    gate.setFocused(true);
    check(!gate.allowsWorldButtons(true), "focus click cannot penetrate");
    check(gate.allowsWorldButtons(false) && !gate.acceptsLookSample(), "focus restoration honors both gates");
    std::cout << "[HUD_INTERACTION] checks=" << count << " status=PASS\n";
}
