#include "AdventureTerrainPlanner.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
std::uint64_t mix(std::uint64_t v) noexcept
{
    v ^= v >> 30; v *= 0xbf58476d1ce4e5b9ull;
    v ^= v >> 27; v *= 0x94d049bb133111ebull;
    return v ^ (v >> 31);
}

double smooth(double lo, double hi, double value) noexcept
{
    const double t = std::clamp((value - lo) / (hi - lo), 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double blend(double a, double b, double t) noexcept
{
    return a + (b - a) * t;
}

AdventureRegion regionFor(double temperature, double moisture,
                          double relief) noexcept
{
    if (relief > .30) {
        return temperature > .04 ? AdventureRegion::Canyon : AdventureRegion::Alpine;
    }
    if (temperature < -.22) { return AdventureRegion::ConiferHighland; }
    if (temperature > .12 && moisture < -.08) { return AdventureRegion::Dunes; }
    if (moisture > .28) { return AdventureRegion::Wetland; }
    return moisture > -.02 ? AdventureRegion::Woodland : AdventureRegion::Meadow;
}

struct CachedAnchor {
    std::uint64_t seed = 0;
    std::int64_t cellX = 0;
    std::int64_t cellZ = 0;
    double x = 0;
    double z = 0;
    double temperature = 0;
    double moisture = 0;
    double relief = 0;
    AdventureRegion region = AdventureRegion::Meadow;
    bool valid = false;
};

// A terrain sample visits the same nine region anchors for every column in a
// distorted cell. Keep only the anchor values that do not depend on the query
// position. The complete key makes direct-map collisions replacement-only;
// they can reduce the hit rate, but cannot change a sample result.
constexpr std::size_t AnchorCacheCapacity = 128;
static_assert((AnchorCacheCapacity & (AnchorCacheCapacity - 1)) == 0,
              "anchor cache capacity must be a power of two");
static_assert(sizeof(std::array<CachedAnchor, AnchorCacheCapacity>) <= 16 * 1024,
              "per-thread anchor cache must stay within its fixed budget");
thread_local std::array<CachedAnchor, AnchorCacheCapacity> AnchorCache{};

struct CachedNoiseCorners {
    std::uint64_t seed = 0;
    std::uint64_t salt = 0;
    std::int64_t x = 0;
    std::int64_t z = 0;
    double lowerLeft = 0;
    double lowerRight = 0;
    double upperLeft = 0;
    double upperRight = 0;
    bool valid = false;
};

// Nearby samples revisit a small set of noise cells at every scale. Cache the
// exact four random corners, not an interpolated value, so tx/tz and all blend
// operations remain on the original path. Full keys make collisions replace
// entries without ever aliasing a result.
constexpr std::size_t NoiseCornerCacheCapacity = 512;
static_assert((NoiseCornerCacheCapacity &
               (NoiseCornerCacheCapacity - 1)) == 0,
              "noise corner cache capacity must be a power of two");
static_assert(sizeof(std::array<CachedNoiseCorners,
                                NoiseCornerCacheCapacity>) <= 64 * 1024,
              "per-thread noise corner cache must stay within budget");
thread_local std::array<CachedNoiseCorners,
                        NoiseCornerCacheCapacity> NoiseCornerCache{};

std::size_t anchorCacheIndex(std::uint64_t seed, std::int64_t cellX,
                             std::int64_t cellZ) noexcept
{
    const auto hash = mix(seed ^
        mix(static_cast<std::uint64_t>(cellX) + 0x632be59bd9b4e019ull) ^
        mix(static_cast<std::uint64_t>(cellZ) + 0x8cb92baa3f3d8dd7ull));
    return static_cast<std::size_t>(hash & (AnchorCacheCapacity - 1));
}

std::size_t noiseCornerCacheIndex(std::uint64_t seed,
                                  std::uint64_t salt,
                                  std::int64_t x,
                                  std::int64_t z) noexcept
{
    std::uint64_t hash = seed ^ salt;
    hash ^= static_cast<std::uint64_t>(x) * 0x9e3779b185ebca87ull;
    hash ^= static_cast<std::uint64_t>(z) * 0xc2b2ae3d27d4eb4full;
    hash ^= hash >> 32;
    return static_cast<std::size_t>(
        hash & (NoiseCornerCacheCapacity - 1));
}
} // namespace

AdventureTerrainPlanner::AdventureTerrainPlanner(int seed) noexcept
    : m_seed(mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(seed))))
{
}

double AdventureTerrainPlanner::random(std::int64_t x, std::int64_t z,
                                       std::uint64_t salt) const noexcept
{
    return static_cast<double>(mix(m_seed ^ salt ^
        mix(static_cast<std::uint64_t>(x) + 0x632be59bd9b4e019ull) ^
        mix(static_cast<std::uint64_t>(z) + 0x8cb92baa3f3d8dd7ull)) >> 11) /
        9007199254740991.0;
}

double AdventureTerrainPlanner::noise(double x, double z, double scale,
                                      std::uint64_t salt) const noexcept
{
    x /= scale; z /= scale;
    const auto ix = static_cast<std::int64_t>(std::floor(x));
    const auto iz = static_cast<std::int64_t>(std::floor(z));
    const double tx = smooth(0, 1, x - static_cast<double>(ix));
    const double tz = smooth(0, 1, z - static_cast<double>(iz));
    auto &corners = NoiseCornerCache[
        noiseCornerCacheIndex(m_seed, salt, ix, iz)];
    if (!corners.valid || corners.seed != m_seed ||
        corners.salt != salt || corners.x != ix || corners.z != iz) {
        corners.seed = m_seed;
        corners.salt = salt;
        corners.x = ix;
        corners.z = iz;
        corners.lowerLeft = random(ix, iz, salt);
        corners.lowerRight = random(ix + 1, iz, salt);
        corners.upperLeft = random(ix, iz + 1, salt);
        corners.upperRight = random(ix + 1, iz + 1, salt);
        corners.valid = true;
    }
    return blend(blend(corners.lowerLeft, corners.lowerRight, tx),
                 blend(corners.upperLeft, corners.upperRight, tx), tz) * 2 - 1;
}

AdventureTerrainPlanner::Sample AdventureTerrainPlanner::sample(
    int worldX, int worldZ) const noexcept
{
    const double x = worldX, z = worldZ;
    // Distort region boundaries without tying them to chunk axes. All offset
    // arithmetic is double/int64, including at signed world-coordinate limits.
    const double px = x + 88 * noise(x, z, 510, 0x49068e890928432bull);
    const double pz = z + 88 * noise(x, z, 510, 0x750ca9e615433c97ull);
    constexpr double Cell = 384;
    const auto cx = static_cast<std::int64_t>(std::floor(px / Cell));
    const auto cz = static_cast<std::int64_t>(std::floor(pz / Cell));
    struct Anchor { double distance, temperature, moisture; AdventureRegion region; };
    std::array<Anchor, 9> anchors{};
    double nearest = std::numeric_limits<double>::max();
    std::size_t index = 0;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto ax = cx + dx, az = cz + dz;
            auto &cached = AnchorCache[anchorCacheIndex(m_seed, ax, az)];
            if (!cached.valid || cached.seed != m_seed ||
                cached.cellX != ax || cached.cellZ != az) {
                const double sx = (static_cast<double>(ax) + .3 +
                    .4 * random(ax, az, 0x6780abc41903ed53ull)) * Cell;
                const double sz = (static_cast<double>(az) + .3 +
                    .4 * random(ax, az, 0x17cf36da56044921ull)) * Cell;
                const double temperature = noise(sx, sz, 870, 0x63b1a908490c712full);
                const double moisture = noise(sx, sz, 690, 0x24db34175ab820cdull);
                const double relief = noise(sx, sz, 610, 0xd762547e1a945263ull);
                cached.seed = m_seed;
                cached.cellX = ax;
                cached.cellZ = az;
                cached.x = sx;
                cached.z = sz;
                cached.temperature = temperature;
                cached.moisture = moisture;
                cached.relief = relief;
                cached.region = regionFor(temperature, moisture, relief);
                cached.valid = true;
            }
            const double sx = cached.x;
            const double sz = cached.z;
            const double temperature = cached.temperature;
            const double moisture = cached.moisture;
            const double distance = std::hypot(px - sx, pz - sz);
            anchors[index++] = {distance, temperature, moisture,
                                cached.region};
            nearest = std::min(nearest, distance);
        }
    }
    Sample result;
    double total = 0;
    for (const auto &anchor : anchors) {
        // All nearby anchors contribute, including triple junctions. Selecting
        // only the nearest two would introduce a height seam at those junctions.
        const double weight = 1 - smooth(0, 104, anchor.distance - nearest);
        result.weights[static_cast<std::size_t>(anchor.region)] += weight;
        result.temperature += weight * anchor.temperature;
        result.moisture += weight * anchor.moisture;
        total += weight;
    }
    result.temperature /= total; result.moisture /= total;
    for (double &weight : result.weights) { weight /= total; }
    const auto dominant = static_cast<std::size_t>(std::distance(result.weights.begin(),
        std::max_element(result.weights.begin(), result.weights.end())));
    result.region = static_cast<AdventureRegion>(dominant);

    const double roll = noise(x, z, 155, 0x11cd3f7512db4591ull);
    const double detail = noise(x, z, 43, 0x83320ca714b5e963ull);
    const double ridge = 1 - std::abs(noise(px, pz, 205, 0x475d03c651fac328ull));
    const double valley = std::abs(noise(px, pz, 310, 0xb2096e47a138d50full));
    const double dunePhase = (x * .83 + z * .56 +
        35 * noise(x, z, 180, 0x143b296d086aa387ull)) / 17;
    const double dunes = std::pow((std::sin(dunePhase) + 1) * .5, 1.65);
    const double level = (roll + 1) * 1.8;
    const double shelf = std::floor(level) + smooth(.30, .87, level - std::floor(level));
    const std::array<double, InlandRegionCount> heights{{
        77 + roll * 6 + detail,
        84 + roll * 11 + detail * 1.8 - 7 * (1 - smooth(.10, .42, valley)),
        101 + ridge * ridge * 23 + roll * 10 + detail * 2,
        77 + dunes * 13 + roll * 4,
        99 + shelf * 12 - 36 * (1 - smooth(.055, .29, valley)) + detail,
        64.5 + noise(x, z, 67, 0x530ac5f158420ba9ull) * 3 + roll,
        108 + ridge * ridge * 43 + roll * 11 + detail * 3
    }};
    double inlandHeight = 0;
    for (std::size_t i = 0; i < heights.size(); ++i) {
        inlandHeight += result.weights[i] * heights[i];
    }
    const double continent = noise(px, pz, 1090, 0x9053acebc42a813full) * .73 +
        noise(px, pz, 430, 0x329abc50746dc153ull) * .27;
    result.land = smooth(-.25, .12, continent);
    const double seaFloor = 44 + continent * 17 + detail * 2;
    const double height = blend(seaFloor, inlandHeight, result.land);
    result.column.height = std::clamp(static_cast<int>(std::lround(height)), 1, 176);
    auto &column = result.column;
    const bool wetland = result.weights[static_cast<std::size_t>(AdventureRegion::Wetland)] > .55 &&
                         result.land > .94;
    if (column.height < 64 && !wetland) {
        result.region = AdventureRegion::Ocean;
        column.biome = TerrainBiome::Ocean;
        column.surface = column.height < 56 ? TerrainFoundation::Surface::Stone : TerrainFoundation::Surface::Sand;
        return result;
    }
    if (result.land < .90 && column.height <= 69) {
        result.region = AdventureRegion::Coast;
        column.biome = TerrainBiome::Grassland;
        column.surface = column.height <= 66 ? TerrainFoundation::Surface::Sand : TerrainFoundation::Surface::Grass;
        return result;
    }
    constexpr std::array<TerrainBiome, InlandRegionCount> Biomes{{
        TerrainBiome::Grassland, TerrainBiome::TemperateForest,
        TerrainBiome::TemperateForest, TerrainBiome::Desert,
        TerrainBiome::RockPlateau, TerrainBiome::Wetland, TerrainBiome::Mountain
    }};
    column.biome = Biomes[dominant];
    if (result.region == AdventureRegion::Woodland &&
        noise(x, z, 128, 0x92a17d74360c812full) < -.18) {
        column.biome = TerrainBiome::LightForest;
    }
    column.surface = TerrainFoundation::Surface::Grass;
    if (result.region == AdventureRegion::Dunes) { column.surface = TerrainFoundation::Surface::Sand; }
    if (result.region == AdventureRegion::Canyon) {
        column.surface = valley < .18 ? TerrainFoundation::Surface::Sand :
            roll < -.2 ? TerrainFoundation::Surface::Dirt : TerrainFoundation::Surface::Stone;
    }
    if (result.region == AdventureRegion::Alpine && column.height >= 100) {
        column.surface = TerrainFoundation::Surface::Stone;
    }
    if (wetland && column.height < 64) { column.surface = TerrainFoundation::Surface::Dirt; }
    return result;
}
