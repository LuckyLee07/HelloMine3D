#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include <optional>

/// @brief Default configuration for program.
struct Config {
    int windowX = 1280;
    int windowY = 720;
    bool isFullscreen = false;
    int renderDistance = 8; // Set initial RD low to prevent long load times
    int fov = 90;
    std::optional<int> worldSeed;
};

#endif // CONFIG_H_INCLUDED
