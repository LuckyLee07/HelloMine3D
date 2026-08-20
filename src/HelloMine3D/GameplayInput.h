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

inline constexpr std::size_t GameplayActionCount =
    static_cast<std::size_t>(GameplayAction::Count);
inline constexpr std::size_t GameplayKeyCount =
    static_cast<std::size_t>(GameplayKey::Count);

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

const char *gameplayActionName(GameplayAction action) noexcept;
const char *gameplayActionConfigKey(GameplayAction action) noexcept;
const char *gameplayKeyName(GameplayKey key) noexcept;
const char *gameplayKeyToken(GameplayKey key) noexcept;
bool tryParseGameplayKey(const std::string &token,
                         GameplayKey &key) noexcept;
bool validateGameplayInputBindings(const GameplayInputBindings &bindings,
                                   std::string &error) noexcept;

#endif // GAMEPLAYINPUT_H_INCLUDED
