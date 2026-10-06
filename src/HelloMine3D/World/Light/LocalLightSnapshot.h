#pragma once
#include "../../Maths/glm.h"
#include <array>
#include <cstddef>
#include <cstdint>

struct LocalLightSource
{
    glm::vec3 position{0.f};
    glm::vec3 colour{1.f,.73f,.42f}; // Linear radiance, never display colour.
    float radius = 0.f;
    float energy = 0.f;
};
struct LocalLightSnapshot
{
    static constexpr std::size_t MaximumSources = 8;
    static constexpr int MaximumRadius = 12;
    static constexpr std::size_t MaximumSections = 27;
    std::array<LocalLightSource,MaximumSources> sources{};
    std::size_t count = 0, inspectedSections = 0, inspectedCells = 0;
    std::size_t candidates = 0;
    std::uint64_t revision = 0;
};
