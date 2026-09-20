#include "World/Block/TerrainEcologyColour.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

int main()
{
    int checks = 0;
    const auto check = [&](bool ok, const char *name) {
        ++checks;
        if (!ok) throw std::runtime_error(name);
        std::cout << "PASS " << name << '\n';
    };
    const auto near = [](glm::vec2 a, glm::vec2 b) {
        return std::abs(a.x - b.x) < .00001f && std::abs(a.y - b.y) < .00001f;
    };
    for (auto biome : {TerrainBiome::Desert, TerrainBiome::Grassland,
            TerrainBiome::LightForest, TerrainBiome::TemperateForest, TerrainBiome::Ocean}) {
        TerrainEcologyColour field;
        int calls = 0;
        field.capture(-16, 0, [&](int, int) { ++calls; return biome; });
        check(calls == 81 && near(field.at(0, 0), TerrainEcologyColour::climate(biome)) &&
              near(field.at(13.7f, 16), TerrainEcologyColour::climate(biome)), "uniform-region-retains-character-and-bounded-cost");
    }
    const auto boundary = [](int x, int z) {
        const std::int64_t bend = (std::int64_t(z) * z % 61) - 30;
        return std::int64_t(x) < bend ? TerrainBiome::TemperateForest : TerrainBiome::RockPlateau;
    };
    bool seams = true, stable = true, bounded = true;
    float maximumStep = 0.f;
    for (int z = -48; z <= 48; z += 16) for (int x = -48; x <= 48; x += 16) {
        TerrainEcologyColour field, east, south, repeat;
        field.capture(x, z, boundary); east.capture(x + 16, z, boundary);
        south.capture(x, z + 16, boundary); repeat.capture(x, z, boundary);
        for (int p = 0; p <= 64; ++p) {
            const float t = p * .25f;
            seams &= near(field.at(16, t), east.at(0, t)) && near(field.at(t, 16), south.at(t, 0));
        }
        for (int dz = 0; dz <= 16; ++dz) for (int dx = 0; dx <= 16; ++dx) {
            const auto v = field.at(dx, dz);
            stable &= near(v, repeat.at(dx, dz));
            bounded &= v.x >= 0 && v.x <= 1 && v.y >= -1 && v.y <= 1;
            if (dx < 16) maximumStep = std::max(maximumStep, std::abs(v.x - field.at(dx + 1, dz).x));
            if (dz < 16) maximumStep = std::max(maximumStep, std::abs(v.x - field.at(dx, dz + 1).x));
        }
    }
    check(seams, "positive-negative-section-edges-share-exact-colours");
    check(stable && bounded, "capture-order-stable-and-colours-bounded");
    check(maximumStep > .02f && maximumStep < .09f, "one-metre-transition-is-soft-not-flat");
    bool uv = true;
    for (int row = 3; row <= 7; ++row) for (int col : {0, 2, 3, 5, 6, 8, 12, 14})
        for (auto v : {glm::vec2(0, -1), glm::vec2(1, 1), glm::vec2(.43f, .17f)}) {
            const auto encoded = TerrainEcologyColour::encode(col, row, v, 16.f) * 16.f;
            uv &= std::floor(encoded.x) == col && std::floor(encoded.y) == row;
            uv &= near({(encoded.x - col - .25f) * 2.f, (encoded.y - row - .5f) * 4.f}, v);
        }
    check(uv && !TerrainEcologyColour::plantTile(9, 4) && !TerrainEcologyColour::plantTile(0, 0),
          "tile-identity-and-uv-roundtrip-water-base-excluded");
    bool extremes = true;
    for (int origin : {std::numeric_limits<int>::min(), std::numeric_limits<int>::max()}) {
        TerrainEcologyColour field;
        field.capture(origin, origin, [&](int x, int z) {
            extremes &= origin < 0 ? x < 0 && z < 0 : x > 0 && z > 0;
            return TerrainBiome::Ocean;
        });
        extremes &= near(field.at(8, 8), {0, -1});
    }
    check(extremes, "signed-extremes-saturate-without-wrap");
    std::cout << "[ECOLOGY-COLOUR] " << checks << '/' << checks
              << " max_one_metre_step=" << maximumStep << '\n';
}
