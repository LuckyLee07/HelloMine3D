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
