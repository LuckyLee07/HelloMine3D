#pragma once

#include "BlockShape.h"
#include "ChunkBlock.h"
#include "TerrainAppearance.h"

#include <array>
#include <cmath>

// A regional appearance of the existing harvestable tall grass. This owns no
// world state and fits inside the same block and section bounds as Cross.
namespace WetlandGrassGeometry {
constexpr std::size_t MaxFaces = 8;
struct Face {
    BlockShapeFace positions{};
    std::array<float, 8> repeat{};
    bool seedHead = false;
};
struct Model {
    std::array<Face, MaxFaces> faces{};
    std::size_t count = 0;
};

inline bool applies(BlockId block, TerrainBiome biome,
                    const BlockShape &shape, BlockMetadata_t metadata = 0) noexcept
{
    static constexpr BlockShapeFace crossA{0,0,0, 1,0,1, 1,1,1, 0,1,0};
    static constexpr BlockShapeFace crossB{0,0,1, 1,0,0, 1,1,0, 0,1,1};
    return block == BlockId::TallGrass && (biome == TerrainBiome::Wetland || metadata == BlockMetadata::TallGrass::Reed) &&
        shape.name == "Cross" && shape.faces.size() == 2 &&
        shape.faces[0] == crossA && shape.faces[1] == crossB;
}

inline Model build(unsigned variant, bool mature, float verticalScale) noexcept
{
    Model model;
    variant %= 3;
    const float turn = static_cast<float>(variant) * 1.04719755f;
    const float c = std::cos(turn), s = std::sin(turn);
    const float height = (mature ? 1.f : .72f) *
        (.86f + .07f * static_cast<float>(variant));
    const auto ribbon = [&](glm::vec3 bottom, glm::vec3 top,
                            glm::vec2 widthAxis, float bottomWidth,
                            float topWidth, bool seedHead) {
        Face &face = model.faces[model.count++];
        const glm::vec3 across(widthAxis.x, 0.f, widthAxis.y);
        const std::array<glm::vec3, 4> points{{
            bottom + across * bottomWidth, bottom - across * bottomWidth,
            top - across * topWidth, top + across * topWidth}};
        for (std::size_t i = 0; i < points.size(); ++i) {
            const auto &p = points[i];
            face.positions[i * 3] = .5f + c * p.x - s * p.z;
            face.positions[i * 3 + 1] = p.y * height * verticalScale;
            face.positions[i * 3 + 2] = .5f + s * p.x + c * p.z;
            face.repeat[i * 2] = (i == 0 || i == 3) ? 1.f : 0.f;
            // A joint at the same height bends identically in every face;
            // seed-head bases must not be pinned like plant roots.
            face.repeat[i * 2 + 1] = 1.f - p.y * height;
        }
        face.seedHead = seedHead;
    };
    ribbon({0,0,0}, {-.32f,.65f,0}, {0,1}, .08f, .006f, false);
    ribbon({0,0,0}, {.29f,.57f,0}, {0,1}, .07f, .006f, false);
    ribbon({0,0,0}, {0,.72f,-.28f}, {1,0}, .07f, .006f, false);
    ribbon({0,0,0}, {0,.62f,.31f}, {1,0}, .075f, .006f, false);
    // Two differently oriented stalks give the clump depth and an uneven
    // skyline without adding faces or filling the gap between their stems.
    const glm::vec3 tallBase(-.10f, mature ? .76f : .94f, -.04f);
    const glm::vec3 shortBase(.10f, mature ? .61f : .78f, .13f);
    ribbon({-.075f,0,0}, tallBase, {1,0}, .019f, .019f, false);
    ribbon({.09f,0,.02f}, shortBase, {0,1}, .019f, .019f, false);
    if (mature) {
        ribbon(tallBase, {-.10f,1,-.04f}, {1,0}, .055f, .045f, true);
        ribbon(shortBase, {.10f,.83f,.13f}, {0,1}, .05f, .04f, true);
    }
    return model;
}
} // namespace WetlandGrassGeometry
