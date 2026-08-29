#include "GameplayInput.h"

#include <algorithm>
#include <array>

namespace {
constexpr std::array<const char *, GameplayActionCount> ActionNames{
    "Move forward", "Move backward", "Move left", "Move right", "Jump",
    "Sneak / descend", "Sprint", "Open crafting", "Consume held food"};

constexpr std::array<const char *, GameplayActionCount> ActionConfigKeys{
    "key_move_forward", "key_move_backward", "key_move_left",
    "key_move_right", "key_jump", "key_sneak", "key_sprint",
    "key_open_crafting", "key_consume_food"};

constexpr std::array<const char *, GameplayKeyCount> KeyNames{
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
    "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V",
    "W", "X", "Y", "Z", "Space", "Left Shift", "Left Control",
    "Up Arrow", "Down Arrow", "Left Arrow", "Right Arrow"};

constexpr std::array<const char *, GameplayKeyCount> KeyTokens{
    "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",
    "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v",
    "w", "x", "y", "z", "space", "left_shift", "left_control",
    "up", "down", "left", "right"};

constexpr std::array<const char *, GameplayWorldActionCount>
    WorldActionNames{"Break / attack", "Use", "Place", "Guard"};

constexpr std::array<const char *, GameplayWorldActionCount>
    WorldActionConfigKeys{"mouse_break_attack", "mouse_use",
                          "mouse_place", "mouse_guard"};

constexpr std::array<const char *, GameplayMouseButtonCount>
    MouseButtonNames{"Mouse primary", "Mouse secondary", "Mouse middle",
                     "Mouse side 1", "Mouse side 2"};

constexpr std::array<const char *, GameplayMouseButtonCount>
    MouseButtonTokens{"primary", "secondary", "middle", "side1", "side2"};

template <typename Enum>
std::size_t indexOf(Enum value) noexcept
{
    return static_cast<std::size_t>(value);
}
} // namespace

GameplayKey GameplayInputBindings::get(GameplayAction action) const noexcept
{
    const std::size_t index = indexOf(action);
    return index < keys.size() ? keys[index] : GameplayKey::W;
}

void GameplayInputBindings::set(GameplayAction action,
                                GameplayKey key) noexcept
{
    const std::size_t index = indexOf(action);
    if (index < keys.size()) {
        keys[index] = key;
    }
}

GameplayMouseButton GameplayMouseBindings::get(
    GameplayWorldAction action) const noexcept
{
    const std::size_t index = indexOf(action);
    return index < buttons.size() ? buttons[index]
                                  : GameplayMouseButton::Primary;
}

void GameplayMouseBindings::set(GameplayWorldAction action,
                                GameplayMouseButton button) noexcept
{
    const std::size_t index = indexOf(action);
    if (index < buttons.size()) {
        buttons[index] = button;
    }
}

const char *gameplayActionName(GameplayAction action) noexcept
{
    const std::size_t index = indexOf(action);
    return index < ActionNames.size() ? ActionNames[index] : "Unknown action";
}

const char *gameplayActionConfigKey(GameplayAction action) noexcept
{
    const std::size_t index = indexOf(action);
    return index < ActionConfigKeys.size() ? ActionConfigKeys[index]
                                           : "key_unknown";
}

const char *gameplayKeyName(GameplayKey key) noexcept
{
    const std::size_t index = indexOf(key);
    return index < KeyNames.size() ? KeyNames[index] : "Unknown";
}

const char *gameplayKeyToken(GameplayKey key) noexcept
{
    const std::size_t index = indexOf(key);
    return index < KeyTokens.size() ? KeyTokens[index] : "unknown";
}

bool tryParseGameplayKey(const std::string &token,
                         GameplayKey &key) noexcept
{
    const auto found = std::find_if(
        KeyTokens.begin(), KeyTokens.end(),
        [&token](const char *candidate) { return token == candidate; });
    if (found == KeyTokens.end()) {
        return false;
    }
    key = static_cast<GameplayKey>(
        static_cast<std::size_t>(found - KeyTokens.begin()));
    return true;
}

bool validateGameplayInputBindings(const GameplayInputBindings &bindings,
                                   std::string &error) noexcept
{
    std::array<bool, GameplayKeyCount> used{};
    for (std::size_t index = 0; index < bindings.keys.size(); ++index) {
        const std::size_t keyIndex = indexOf(bindings.keys[index]);
        if (keyIndex >= used.size()) {
            error = std::string(gameplayActionName(
                        static_cast<GameplayAction>(index))) +
                    " uses an unknown key";
            return false;
        }
        if (used[keyIndex]) {
            error = std::string("key '") + gameplayKeyName(bindings.keys[index]) +
                    "' is assigned more than once";
            return false;
        }
        used[keyIndex] = true;
    }
    error.clear();
    return true;
}

const char *gameplayWorldActionName(GameplayWorldAction action) noexcept
{
    const std::size_t index = indexOf(action);
    return index < WorldActionNames.size() ? WorldActionNames[index]
                                           : "Unknown world action";
}

const char *gameplayWorldActionConfigKey(
    GameplayWorldAction action) noexcept
{
    const std::size_t index = indexOf(action);
    return index < WorldActionConfigKeys.size()
               ? WorldActionConfigKeys[index]
               : "mouse_unknown";
}

const char *gameplayMouseButtonName(GameplayMouseButton button) noexcept
{
    const std::size_t index = indexOf(button);
    return index < MouseButtonNames.size() ? MouseButtonNames[index]
                                           : "Unknown mouse button";
}

const char *gameplayMouseButtonToken(GameplayMouseButton button) noexcept
{
    const std::size_t index = indexOf(button);
    return index < MouseButtonTokens.size() ? MouseButtonTokens[index]
                                            : "unknown";
}

bool tryParseGameplayMouseButton(const std::string &token,
                                 GameplayMouseButton &button) noexcept
{
    const auto found = std::find_if(
        MouseButtonTokens.begin(), MouseButtonTokens.end(),
        [&token](const char *candidate) { return token == candidate; });
    if (found == MouseButtonTokens.end()) {
        return false;
    }
    button = static_cast<GameplayMouseButton>(
        static_cast<std::size_t>(found - MouseButtonTokens.begin()));
    return true;
}

const char *gameplayHoldModeName(GameplayHoldMode mode) noexcept
{
    return mode == GameplayHoldMode::Toggle ? "Toggle" : "Hold";
}

const char *gameplayHoldModeToken(GameplayHoldMode mode) noexcept
{
    return mode == GameplayHoldMode::Toggle ? "toggle" : "hold";
}

bool tryParseGameplayHoldMode(const std::string &token,
                              GameplayHoldMode &mode) noexcept
{
    if (token == "hold") {
        mode = GameplayHoldMode::Hold;
        return true;
    }
    if (token == "toggle") {
        mode = GameplayHoldMode::Toggle;
        return true;
    }
    return false;
}

bool validateGameplayMouseBindings(const GameplayMouseBindings &bindings,
                                   std::string &error) noexcept
{
    for (std::size_t index = 0; index < bindings.buttons.size(); ++index) {
        if (indexOf(bindings.buttons[index]) >= GameplayMouseButtonCount) {
            error = std::string(gameplayWorldActionName(
                        static_cast<GameplayWorldAction>(index))) +
                    " uses an unknown mouse button";
            return false;
        }
    }

    const GameplayMouseButton breakButton =
        bindings.get(GameplayWorldAction::BreakAttack);
    for (GameplayWorldAction action : {
             GameplayWorldAction::Use,
             GameplayWorldAction::Place,
             GameplayWorldAction::Guard}) {
        if (bindings.get(action) == breakButton) {
            error = std::string("mouse button '") +
                    gameplayMouseButtonName(breakButton) +
                    "' cannot be shared by Break / attack and " +
                    gameplayWorldActionName(action);
            return false;
        }
    }
    error.clear();
    return true;
}

std::string describeGameplayMouseBindingSharing(
    const GameplayMouseBindings &bindings)
{
    for (std::size_t buttonIndex = 0;
         buttonIndex < GameplayMouseButtonCount; ++buttonIndex) {
        const auto button = static_cast<GameplayMouseButton>(buttonIndex);
        std::string actions;
        int count = 0;
        for (GameplayWorldAction action : {
                 GameplayWorldAction::Use,
                 GameplayWorldAction::Place,
                 GameplayWorldAction::Guard}) {
            if (bindings.get(action) != button) {
                continue;
            }
            if (!actions.empty()) {
                actions += ", ";
            }
            actions += gameplayWorldActionName(action);
            ++count;
        }
        if (count > 1) {
            return std::string(gameplayMouseButtonName(button)) +
                   " is context-shared by " + actions;
        }
    }
    return {};
}

GameplayLookDelta calculateGameplayLookDelta(
    float rawX, float rawY, float sensitivity,
    bool invertMouseY) noexcept
{
    GameplayLookDelta result;
    result.yaw = rawX * sensitivity;
    result.pitch = rawY * sensitivity * (invertMouseY ? -1.f : 1.f);
    return result;
}

GameplayWorldAction resolveGameplayWorldAction(
    const GameplayWorldActionIntent &intent,
    const GameplayWorldActionContext &context) noexcept
{
    if (intent.breakAttack) {
        return GameplayWorldAction::BreakAttack;
    }
    if (intent.guard && context.actorTarget && context.guardAvailable) {
        return GameplayWorldAction::Guard;
    }
    if (intent.use && context.usableBlockTarget) {
        return GameplayWorldAction::Use;
    }
    if (intent.place && context.placeableHeldItem) {
        return GameplayWorldAction::Place;
    }
    return GameplayWorldAction::None;
}

GameplayMovementModeState GameplayMovementModeTracker::update(
    bool active, bool sprintDown, bool sneakDown,
    GameplayHoldMode sprintMode,
    GameplayHoldMode sneakMode) noexcept
{
    GameplayMovementModeState state;
    if (!active) {
        m_sprintToggled = false;
        m_sneakToggled = false;
        m_previousSprintDown = sprintDown;
        m_previousSneakDown = sneakDown;
        return state;
    }

    if (sprintMode == GameplayHoldMode::Toggle && sprintDown &&
        !m_previousSprintDown) {
        m_sprintToggled = !m_sprintToggled;
    }
    if (sneakMode == GameplayHoldMode::Toggle && sneakDown &&
        !m_previousSneakDown) {
        m_sneakToggled = !m_sneakToggled;
    }
    state.sprint = sprintMode == GameplayHoldMode::Toggle
                       ? m_sprintToggled
                       : sprintDown;
    state.sneak = sneakMode == GameplayHoldMode::Toggle
                      ? m_sneakToggled
                      : sneakDown;
    m_previousSprintDown = sprintDown;
    m_previousSneakDown = sneakDown;
    return state;
}

void GameplayMovementModeTracker::reset() noexcept
{
    m_previousSprintDown = false;
    m_previousSneakDown = false;
    m_sprintToggled = false;
    m_sneakToggled = false;
}

void GameplayFocusGate::setFocused(bool focused) noexcept
{
    if (focused == m_focused) {
        return;
    }
    m_focused = focused;
    m_suppressButtonsUntilRelease = true;
    m_discardNextLookSample = true;
}

bool GameplayFocusGate::isFocused() const noexcept
{
    return m_focused;
}

bool GameplayFocusGate::allowsWorldButtons(bool anyButtonDown) noexcept
{
    if (!m_focused) {
        return false;
    }
    if (!m_suppressButtonsUntilRelease) {
        return true;
    }
    if (anyButtonDown) {
        return false;
    }
    m_suppressButtonsUntilRelease = false;
    return true;
}

bool GameplayFocusGate::acceptsLookSample() noexcept
{
    if (!m_focused) {
        return false;
    }
    if (m_discardNextLookSample) {
        m_discardNextLookSample = false;
        return false;
    }
    return true;
}
