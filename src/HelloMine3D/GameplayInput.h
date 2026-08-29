#ifndef GAMEPLAYINPUT_H_INCLUDED
#define GAMEPLAYINPUT_H_INCLUDED

#include <array>
#include <cstddef>
#include <string>

enum class GameplayAction {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Jump,
    Sneak,
    Sprint,
    OpenCrafting,
    ConsumeFood,
    Count
};

enum class GameplayKey {
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Space,
    LeftShift,
    LeftControl,
    Up,
    Down,
    Left,
    Right,
    Count
};

enum class GameplayWorldAction {
    BreakAttack,
    Use,
    Place,
    Guard,
    Count,
    None = Count
};

enum class GameplayMouseButton {
    Primary,
    Secondary,
    Middle,
    Side1,
    Side2,
    Count
};

enum class GameplayHoldMode {
    Hold,
    Toggle
};

inline constexpr std::size_t GameplayActionCount =
    static_cast<std::size_t>(GameplayAction::Count);
inline constexpr std::size_t GameplayKeyCount =
    static_cast<std::size_t>(GameplayKey::Count);
inline constexpr std::size_t GameplayWorldActionCount =
    static_cast<std::size_t>(GameplayWorldAction::Count);
inline constexpr std::size_t GameplayMouseButtonCount =
    static_cast<std::size_t>(GameplayMouseButton::Count);

struct GameplayInputBindings {
    std::array<GameplayKey, GameplayActionCount> keys{
        GameplayKey::W,
        GameplayKey::S,
        GameplayKey::A,
        GameplayKey::D,
        GameplayKey::Space,
        GameplayKey::LeftShift,
        GameplayKey::LeftControl,
        GameplayKey::E,
        GameplayKey::R};

    GameplayKey get(GameplayAction action) const noexcept;
    void set(GameplayAction action, GameplayKey key) noexcept;
};

struct GameplayMouseBindings {
    std::array<GameplayMouseButton, GameplayWorldActionCount> buttons{
        GameplayMouseButton::Primary,
        GameplayMouseButton::Secondary,
        GameplayMouseButton::Secondary,
        GameplayMouseButton::Secondary};

    GameplayMouseButton get(GameplayWorldAction action) const noexcept;
    void set(GameplayWorldAction action,
             GameplayMouseButton button) noexcept;
};

struct GameplayLookDelta {
    float yaw = 0.f;
    float pitch = 0.f;
};

struct GameplayWorldActionIntent {
    bool breakAttack = false;
    bool use = false;
    bool place = false;
    bool guard = false;
};

struct GameplayWorldActionContext {
    bool actorTarget = false;
    bool guardAvailable = false;
    bool usableBlockTarget = false;
    bool placeableHeldItem = false;
};

struct GameplayMovementModeState {
    bool sprint = false;
    bool sneak = false;
};

/// Deterministic edge tracker for hold/toggle movement modes. Inactive samples
/// clear toggles while remembering held keys so resuming cannot create an edge.
class GameplayMovementModeTracker {
  public:
    GameplayMovementModeState update(
        bool active, bool sprintDown, bool sneakDown,
        GameplayHoldMode sprintMode,
        GameplayHoldMode sneakMode) noexcept;
    void reset() noexcept;

  private:
    bool m_previousSprintDown = false;
    bool m_previousSneakDown = false;
    bool m_sprintToggled = false;
    bool m_sneakToggled = false;
};

/// Focus boundary shared by the real OIS path and headless tests. UI callbacks
/// still receive mouse presses while world buttons remain suppressed.
class GameplayFocusGate {
  public:
    void setFocused(bool focused) noexcept;
    bool isFocused() const noexcept;
    bool allowsWorldButtons(bool anyButtonDown) noexcept;
    bool acceptsLookSample() noexcept;

  private:
    bool m_focused = true;
    bool m_suppressButtonsUntilRelease = false;
    bool m_discardNextLookSample = false;
};

const char *gameplayActionName(GameplayAction action) noexcept;
const char *gameplayActionConfigKey(GameplayAction action) noexcept;
const char *gameplayKeyName(GameplayKey key) noexcept;
const char *gameplayKeyToken(GameplayKey key) noexcept;
bool tryParseGameplayKey(const std::string &token,
                         GameplayKey &key) noexcept;
bool validateGameplayInputBindings(const GameplayInputBindings &bindings,
                                   std::string &error) noexcept;
const char *gameplayWorldActionName(GameplayWorldAction action) noexcept;
const char *gameplayWorldActionConfigKey(
    GameplayWorldAction action) noexcept;
const char *gameplayMouseButtonName(GameplayMouseButton button) noexcept;
const char *gameplayMouseButtonToken(GameplayMouseButton button) noexcept;
bool tryParseGameplayMouseButton(const std::string &token,
                                 GameplayMouseButton &button) noexcept;
const char *gameplayHoldModeName(GameplayHoldMode mode) noexcept;
const char *gameplayHoldModeToken(GameplayHoldMode mode) noexcept;
bool tryParseGameplayHoldMode(const std::string &token,
                              GameplayHoldMode &mode) noexcept;
bool validateGameplayMouseBindings(const GameplayMouseBindings &bindings,
                                   std::string &error) noexcept;
std::string describeGameplayMouseBindingSharing(
    const GameplayMouseBindings &bindings);
GameplayLookDelta calculateGameplayLookDelta(
    float rawX, float rawY, float sensitivity,
    bool invertMouseY) noexcept;
GameplayWorldAction resolveGameplayWorldAction(
    const GameplayWorldActionIntent &intent,
    const GameplayWorldActionContext &context) noexcept;

#endif // GAMEPLAYINPUT_H_INCLUDED
