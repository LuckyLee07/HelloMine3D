#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <optional>
#include <string>

#include "GameplayInput.h"

/// Settings owned by the player and safe to change without recreating a world.
struct UserSettings {
    int windowX = 1280;
    int windowY = 720;
    bool isFullscreen = false;
    int renderDistance = 8; // Set initial RD low to prevent long load times
    int fov = 90;
    float mouseSensitivity = 0.05f;
    bool invertMouseY = false;
    float masterVolume = 1.0f;
    float uiVolume = 1.0f;
    float effectsVolume = 1.0f;
    float ambientVolume = 1.0f;
    float musicVolume = 0.65f;
    float uiScale = 1.0f;
    std::string locale = "en-US";
    bool audioCaptions = true;
    bool showActionHints = true;
    GameplayInputBindings inputBindings;
};

/// Parameters used only when a world is created.
struct WorldCreationConfig {
    std::optional<int> worldSeed;
};

/// @brief Complete startup configuration.
struct Config : UserSettings, WorldCreationConfig {};

inline UserSettings &userSettings(Config &config) noexcept
{
    return config;
}

inline const UserSettings &userSettings(const Config &config) noexcept
{
    return config;
}

inline WorldCreationConfig &worldCreationConfig(Config &config) noexcept
{
    return config;
}

inline const WorldCreationConfig &worldCreationConfig(
    const Config &config) noexcept
{
    return config;
}

#endif // CONFIG_H_INCLUDED
