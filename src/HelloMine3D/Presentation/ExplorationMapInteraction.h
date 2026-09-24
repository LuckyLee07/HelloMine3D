#pragma once

#include "../World/Exploration/ExplorationMarkers.h"
#include <array>
#include <cstdio>

// The map canvas, marker list and newly-created marker share one edit target.
// Switching targets must replace both the stable ID and its text buffer.
struct ExplorationMarkerEditor {
    std::uint32_t id = 0;
    std::array<char, ExplorationMarkers::MaxNameBytes + 1> name{};

    void select(std::uint32_t selectedId, const std::string& selectedName)
    {
        id = selectedId;
        name.fill(0);
        std::snprintf(name.data(), name.size(), "%s", selectedName.c_str());
    }
    void clear() noexcept { id = 0; name.fill(0); }
};
