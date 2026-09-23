// clang++ -std=c++17 -O3 -Isrc/external/glm tools/validate_regional_atmosphere.cpp \
//   src/HelloMine3D/World/Environment/WorldEnvironment.cpp -o /tmp/regional-atmosphere
// B1c pure geography-to-atmosphere checks; no window, input or world writes.
#include "../src/HelloMine3D/World/Environment/RegionalAtmosphere.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace {
int checks = 0;
void check(bool passed, const char* name) {
    if (!passed) throw std::runtime_error(name);
    ++checks;
    std::cout << "[REGIONAL_ATMOSPHERE] PASS " << name << '\n';
}
float difference(const RegionalAtmosphere::Profile& a, const RegionalAtmosphere::Profile& b) {
    return std::max(glm::length(a.tint - b.tint), std::abs(a.density - b.density));
}
}
int main() {
    try {
        using Field = RegionalAtmosphere;
        const auto meadow = Field::profile(TerrainBiome::Grassland, 72);
        const auto wood = Field::profile(TerrainBiome::TemperateForest, 82);
        const auto wet = Field::profile(TerrainBiome::Wetland, 64);
        const auto dune = Field::profile(TerrainBiome::Desert, 78);
        const auto coast = Field::profile(TerrainBiome::Ocean, 62);
        const auto conifer = Field::profile(TerrainBiome::Mountain, 98);
        const auto alpine = Field::profile(TerrainBiome::Mountain, 136);
        check(wet.density > wood.density && wood.density > meadow.density &&
              meadow.density > alpine.density, "habitat-distance-hierarchy");
        check(dune.tint.r > dune.tint.b && coast.tint.b > coast.tint.r &&
              alpine.tint.b > conifer.tint.b, "regional-light-palettes");
        check(conifer.density > alpine.density, "altitude-opens-mountain-distance");
        check(Field::profile(TerrainBiome::TemperateForest,124).tint.b > wood.tint.b &&
              Field::profile(TerrainBiome::Grassland,66).tint.b > meadow.tint.b,
              "high-forest-and-low-coast-follow-geography");
        Field cache;
        int queries = 0;
        std::set<std::pair<int,int>> observed;
        const auto biome = [&](int x, int z) {
            ++queries; observed.emplace(x,z);
            return x < 0 ? TerrainBiome::Wetland : TerrainBiome::Desert;
        };
        const auto height = [&](int, int) { ++queries; return 80; };
        const auto centre = cache.at(-32, -32, biome, height);
        check(queries == Field::MaximumQueriesPerCapture && observed.size() == 4,
              "four-corners-eight-query-budget");
        check(observed.count({-64,-64}) && observed.count({0,0}), "negative-floor-grid");
        for (int i=0; i<10000; ++i) cache.at(-32 + i*.001, -32, biome, height);
        check(queries == 8, "stationary-cell-no-repeat-generation-query");
        check(centre.density > dune.density && centre.density < wet.density,
              "actual-neighbour-geography-blended");
        const auto west = cache.at(-.0001, -32, biome, height);
        const auto east = cache.at(.0001, -32, biome, height);
        check(difference(west,east) < .00001f, "cell-boundary-continuity");
        check(queries == 16, "next-cell-query-budget");
        cache.reset();
        const auto changed = cache.at(.0001,-32,[](int,int){return TerrainBiome::Ocean;},height);
        check(difference(changed,coast) < .00001f, "world-reset-drops-old-region");
        const auto allFinite = [&](double x, double z) {
            auto p = cache.at(x,z,biome,height);
            return std::isfinite(p.density) && std::isfinite(p.tint.r) &&
                p.density >= .8f && p.density <= 1.3f;
        };
        check(allFinite(double(std::numeric_limits<int>::min()),double(std::numeric_limits<int>::max())) &&
              allFinite(-1e30,1e30), "extreme-coordinate-corners-bounded");
        check(allFinite(std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity()), "nonfinite-camera-safe-default");
        auto previous = cache.at(-128, -32, biome, height);
        float maxStep = 0;
        for (int i=1;i<=2560;++i) {
            auto next = cache.at(-128 + i*.1,-32,biome,height);
            maxStep = std::max(maxStep,difference(previous,next)); previous=next;
        }
        check(maxStep < .0013f, "continuous-region-traversal");
        for (float time : {0.f,1000.f,6000.f,11500.f,18000.f,23999.f}) {
            const auto air = WorldEnvironment::evaluate(time);
            const auto neutral = Field::apply(air,meadow);
            check(neutral.fogColour == air.fogColour && neutral.fogDensity == air.fogDensity &&
                  neutral.skyZenithColour == air.skyZenithColour, "neutral-region-exact");
            for (auto p : {wood,wet,dune,coast,conifer,alpine}) {
                const auto s = Field::apply(air,p);
                if (s.fogColour != s.skyHorizonColour || s.sunDirection != air.sunDirection ||
                    s.daylight != air.daylight || s.sunIntensity != air.sunIntensity ||
                    s.moonIntensity != air.moonIntensity || s.cycle != air.cycle ||
                    s.cloudCoverage != air.cloudCoverage || s.cloudVelocity != air.cloudVelocity ||
                    s.waterShallowColour != air.waterShallowColour || s.waterDeepColour != air.waterDeepColour)
                    throw std::runtime_error("authority-or-horizon-drift");
                const auto underwater = WorldEnvironment::forCameraMedium(s,1.f);
                const auto oldUnderwater = WorldEnvironment::forCameraMedium(air,1.f);
                if (glm::length(underwater.fogColour-oldUnderwater.fogColour) > .000001f ||
                    std::abs(underwater.fogDensity-oldUnderwater.fogDensity) > .0000001f)
                    throw std::runtime_error("underwater-medium-drift");
            }
        }
        check(true,"all-regions-all-times-preserve-authority-and-medium");
        const auto a = Field::apply(WorldEnvironment::evaluate(23999.99f),wet);
        const auto b = Field::apply(WorldEnvironment::evaluate(.01f),wet);
        check(glm::length(a.fogColour-b.fogColour) < .0001f &&
              std::abs(a.fogDensity-b.fogDensity) < .000001f, "day-cycle-wrap-continuity");
        std::cout << "[REGIONAL_ATMOSPHERE] checks=" << checks << " failures=0 cache_bytes=" << sizeof(Field) << '\n';
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "[REGIONAL_ATMOSPHERE] FAIL " << e.what() << '\n'; return 1;
    }
}
