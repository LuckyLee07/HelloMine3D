#include "TerrainFoundation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
std::uint64_t mix(std::uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ull;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

double smooth(double lower, double upper, double value) noexcept
{
    const double t = std::max(0.0, std::min(1.0,
        (value - lower) / (upper - lower)));
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double lattice(std::uint64_t seed, std::int64_t x, std::int64_t z,
               std::uint64_t salt) noexcept
{
    const std::uint64_t hash = mix(seed ^ salt ^
        mix(static_cast<std::uint64_t>(x) + 0x632be59bd9b4e019ull) ^
        mix(static_cast<std::uint64_t>(z) + 0x8cb92baa3f3d8dd7ull));
    return static_cast<double>(hash >> 11) *
        (2.0 / 9007199254740991.0) - 1.0;
}
} // namespace

TerrainFoundation::TerrainFoundation(int seed) noexcept
    : m_seed(mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(seed))))
{
}

double TerrainFoundation::noise(double x, double z, double scale,
                                std::uint64_t salt) const noexcept
{
    // Even INT_MIN/INT_MAX inputs stay well inside int64 lattice range. Offset
    // arithmetic happens in double, never as overflowing signed world ints.
    x /= scale;
    z /= scale;
    const auto ix = static_cast<std::int64_t>(std::floor(x));
    const auto iz = static_cast<std::int64_t>(std::floor(z));
    const double tx = smooth(0.0, 1.0, x - static_cast<double>(ix));
    const double tz = smooth(0.0, 1.0, z - static_cast<double>(iz));
    const double a = lattice(m_seed, ix, iz, salt);
    const double b = lattice(m_seed, ix + 1, iz, salt);
    const double c = lattice(m_seed, ix, iz + 1, salt);
    const double d = lattice(m_seed, ix + 1, iz + 1, salt);
    const double front = a + (b - a) * tx;
    const double back = c + (d - c) * tx;
    return front + (back - front) * tz;
}

TerrainFoundation::Column TerrainFoundation::sample(int worldX, int worldZ) const noexcept
{
    return sampleBase(worldX, worldZ, nullptr);
}

TerrainFoundation::Column TerrainFoundation::sampleBase(
    int worldX, int worldZ, double *rawHeight, bool heightOnly) const noexcept
{
    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    // Wide landmass, intermediate rolling hills, and small bounded detail.
    // Biome labels are assigned afterwards, so biome thresholds cannot create
    // a step in the planning surface or an axis-specific ocean shelf.
    const double landmass = noise(x, z, 700.0, 0x243f6a8885a308d3ull);
    const double hills = smooth(-0.4, 0.5,
        noise(x, z, 420.0, 0x13198a2e03707344ull));
    const double base = 70.0 + landmass * 28.0 +
        hills * noise(x, z, 145.0, 0xa4093822299f31d0ull) * 8.0 +
        noise(x, z, 48.0, 0x082efa98ec4e6c89ull) * 1.5;

    const double mountainDomain = smooth(0.05, 0.65,
        noise(x, z, 620.0, 0x452821e638d01377ull));
    const double ridge = 1.0 - std::abs(
        noise(x, z, 190.0, 0xbe5466cf34e90c6cull));
    // A smooth ribbon lowers mountain relief into connected local valleys.
    // It does not carve rivers or place water above the existing sea level.
    const double valley = 1.0 - smooth(0.035, 0.45, std::abs(
        noise(x, z, 400.0, 0xc0ac29b7c97c50ddull)));
    const double mountain = mountainDomain * (1.0 - valley * 0.82);
    double height = base + mountain * (18.0 + ridge * ridge * 58.0);
    // Soft upper shoulder prevents a hard-clipped plateau while retaining the
    // final safety clamp. C1 at 148; no discontinuity in height or first slope.
    if (height > 148.0) {
        height = 148.0 + 28.0 * (1.0 - std::exp(-(height - 148.0) / 28.0));
    }
    if (rawHeight != nullptr) { *rawHeight = height; }
    Column column;
    column.height = std::max(1, std::min(176, static_cast<int>(std::lround(height))));
    // Shore probes only need the raw/rounded height. Avoid the ecology noise
    // for those pure coordinate lookups without changing any column result.
    if (heightOnly) { return column; }
    if (column.height < 64) {
        column.biome = TerrainBiome::Ocean;
    }
    else if (mountain >= 0.48 && column.height >= 80) {
        column.biome = TerrainBiome::Mountain;
    }
    else {
        const double ecology = noise(x, z, 300.0, 0x3f84d5b5b5470917ull);
        column.biome = ecology < -0.35 ? TerrainBiome::Desert :
                       ecology < -0.05 ? TerrainBiome::Grassland :
                       ecology < 0.30 ? TerrainBiome::LightForest :
                                        TerrainBiome::TemperateForest;
    }
    return column;
}

TerrainFoundation::Column TerrainFoundation::sampleV8(
    int worldX, int worldZ) const noexcept
{
    // v7 remains a separate, byte-for-byte stable route. Shore shaping only
    // reads the seed and signed world coordinates; it never asks for a Chunk.
    double rawHeight = 0.0;
    Column column = sampleBase(worldX, worldZ, &rawHeight);
    if (column.height < 56 || column.height > 80 ||
        column.biome == TerrainBiome::Mountain) {
        return column;
    }

    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    const double bend = noise(x, z, 116.0, 0xc474dce0f0b68b6full);
    const double detail = noise(x, z, 46.0, 0x9031df796ea784a5ull);
    const double shallowShelf = 1.0 * smooth(56.0, 60.0, rawHeight) *
        (1.0 - smooth(60.0, 64.0, rawHeight));
    const auto clampedWorld = [](std::int64_t value) {
        return static_cast<int>(std::max(
            static_cast<std::int64_t>(std::numeric_limits<int>::min()),
            std::min(static_cast<std::int64_t>(
                         std::numeric_limits<int>::max()), value)));
    };
    double shoulder = 0.0;
    if (rawHeight >= 63.5 && rawHeight < 72.0) {
        // A bounded, world-coordinate shore probe gives flat v7 lowlands a
        // gentle dry ramp without creating an equal-width height stripe.
        // The probe only samples the pure foundation, never neighbour Chunks.
        double nearestWater = std::numeric_limits<double>::infinity();
        for (const auto offset : {std::pair<int, int>{96, 0},
                                  {-96, 0}, {0, 96}, {0, -96}}) {
            const int otherX = clampedWorld(static_cast<std::int64_t>(worldX) +
                                       offset.first);
            const int otherZ = clampedWorld(static_cast<std::int64_t>(worldZ) +
                                       offset.second);
            double otherRaw = 0.0;
            (void)sampleBase(otherX, otherZ, &otherRaw, true);
            if (otherRaw >= 63.5) { continue; }
            const double distance = std::hypot(
                static_cast<double>(otherX) - x,
                static_cast<double>(otherZ) - z);
            const double estimate = distance *
                (rawHeight - 63.5) / (rawHeight - otherRaw);
            nearestWater = std::min(nearestWater, estimate);
        }
        if (nearestWater < 64.0) {
            const double rise = 18.0 + detail * 4.0;
            const double amplitude = 3.5 + bend * 0.7;
            shoulder = amplitude * smooth(0.0, rise, nearestWater) *
                (1.0 - smooth(32.0, 64.0, nearestWater)) *
                (1.0 - smooth(66.0, 72.0, rawHeight));
        }
        if (shoulder > 0.0) {
            // A nearby water column wins over the coarse 96-block estimate.
            // This prevents a single raised shore column from becoming a
            // four-block lip at an oblique or narrow inlet.
            for (const auto offset : {std::pair<int, int>{1, 0},
                                      {-1, 0}, {0, 1}, {0, -1}}) {
                const int otherX = clampedWorld(
                    static_cast<std::int64_t>(worldX) + offset.first);
                const int otherZ = clampedWorld(
                    static_cast<std::int64_t>(worldZ) + offset.second);
                if (otherX == worldX && otherZ == worldZ) { continue; }
                const Column neighbour = sampleBase(otherX, otherZ, nullptr, true);
                const int allowance = neighbour.height < 64 ? 2 : 3;
                shoulder = std::max(0.0, std::min(shoulder,
                    static_cast<double>(neighbour.height + allowance) -
                    rawHeight - shallowShelf));
            }
        }
    }
    const double height = rawHeight + shoulder + shallowShelf;
    column.height = std::max(1, std::min(176,
        static_cast<int>(std::lround(height))));

    // The v7 ocean threshold is height-based. Keep labels and surface height
    // in agreement when a shaped column crosses that threshold.
    if (column.height < 64) {
        column.biome = TerrainBiome::Ocean;
    }
    else if (column.biome == TerrainBiome::Ocean) {
        const double ecology = noise(x, z, 300.0, 0x3f84d5b5b5470917ull);
        column.biome = ecology < -0.35 ? TerrainBiome::Desert :
                       ecology < -0.05 ? TerrainBiome::Grassland :
                       ecology < 0.30 ? TerrainBiome::LightForest :
                                            TerrainBiome::TemperateForest;
    }

    // Low-frequency boundaries make a variable-width, coherent dry shore;
    // material decisions use the same column as generation and placement.
    const double dryLimit = 64.5 + bend * 1.8 + detail * 0.15;
    if (column.height < 64) {
        if (column.height >= 59) {
            const double rock = noise(x, z, 82.0, 0x91e10da5c79e7b1dull);
            column.surface = column.height <= 60 && rock > 0.45
                ? Surface::Stone : Surface::Sand;
        }
        return column;
    }
    if (static_cast<double>(column.height) <= dryLimit) {
        column.surface = Surface::Sand;
        return column;
    }
    if (static_cast<double>(column.height) <= dryLimit + 1.35 &&
        column.biome != TerrainBiome::Desert) {
        column.surface = Surface::Dirt;
        return column;
    }

    // Widen the sand-to-grass ecotone on both sides of the biome boundary.
    // Smooth coordinate noise bends the contour without per-block random dots.
    if (column.biome == TerrainBiome::Desert ||
        column.biome == TerrainBiome::Grassland) {
        const double ecology = noise(x, z, 300.0, 0x3f84d5b5b5470917ull) +
            noise(x, z, 75.0, 0x3ab095d86cdb72e4ull) * 0.12;
        if (ecology < -0.375) {
            column.surface = Surface::Sand;
        }
        else if (ecology < -0.305) {
            column.surface = Surface::Dirt;
        }
        else if (column.height <= 76) {
            column.surface = Surface::Grass;
        }
    }
    return column;
}

TerrainFoundation::Column TerrainFoundation::sampleV9(
    int worldX, int worldZ) const noexcept
{
    Column column = sampleV8(worldX, worldZ);
    if (column.height <= 80 || column.height >= 135 ||
        (column.biome != TerrainBiome::LightForest &&
         column.biome != TerrainBiome::TemperateForest)) {
        return column;
    }

    // Continuous world-space contours open occasional walkable meadows in the
    // inland forest. The old v8 shore and all terrain heights stay untouched.
    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    const double clearing =
        noise(x, z, 132.0, 0xe43d17bc92f6a805ull) +
        noise(x, z, 55.0, 0x4c91eac7096b32dfull) * 0.18;
    const double threshold = column.biome == TerrainBiome::LightForest
        ? 0.42 : 0.54;
    if (clearing > threshold) {
        column.biome = TerrainBiome::Grassland;
        column.surface = Surface::Grass;
    }
    return column;
}

TerrainFoundation::Column TerrainFoundation::sampleV10(
    int worldX, int worldZ) const noexcept
{
    Column column = sampleV9(worldX, worldZ);
    const int oldHeight = column.height;
    if (oldHeight <= 80 || oldHeight >= 135) {
        return column;
    }

    // Broad world-space fields add readable inland swells, shallow basins and
    // shoulders without changing v9 ecology or surface materials. The height
    // band fades over many old contour levels so the untouched coast and high
    // ridge cannot acquire a one-column lip at the version boundary.
    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    const double broad = noise(
        x, z, 540.0, 0x7f4a7c159e3779b9ull) * 8.0;
    const double ridge = 1.0 - std::abs(noise(
        x, z, 280.0, 0x94d049bb133111ebull));
    const double shoulder =
        (smooth(0.28, 0.88, ridge) - 0.45) * 3.0;
    const double basinField = noise(
        x, z, 360.0, 0xd1b54a32d192ed03ull);
    const double basin = -smooth(0.38, 0.82, -basinField);
    const double band = smooth(80.0, 92.0,
                               static_cast<double>(oldHeight)) *
        (1.0 - smooth(108.0, 135.0,
                      static_cast<double>(oldHeight)));
    double relief = std::max(-8.0, std::min(
        8.0, (broad + shoulder + basin) * band));
    column.height = std::max(1, std::min(
        176, oldHeight + static_cast<int>(std::lround(relief))));
    return column;
}

TerrainFoundation::Column TerrainFoundation::sampleV11(
    int worldX, int worldZ) const noexcept
{
    Column column = sampleV10(worldX, worldZ);
    const int oldHeight = column.height;
    if (oldHeight < 64 || oldHeight > 95) {
        return column;
    }

    // A continuous zero contour supplies the stream centreline. A second,
    // shorter field bends the line while a third varies its width; all three
    // are pure world-coordinate samples, so the same corridor crosses Chunk
    // borders without neighbour reads. A shallow cut leaves actual water in
    // the lowest reaches and becomes a dry valley higher up. Keeping the cut
    // small also preserves the walkable slope budget of the v10 parent.
    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    const double centreDistance = std::abs(
        noise(x, z, 1600.0, 0x6a09e667f3bcc909ull) +
        noise(x, z, 700.0, 0xbb67ae8584caa73bull) * 0.08);
    const double widthField = noise(
        x, z, 900.0, 0x3c6ef372fe94f82bull);
    const double innerWidth = 0.022 + widthField * 0.006;
    const double outerWidth = innerWidth + 0.18;
    const double corridor = 1.0 - smooth(
        innerWidth, outerWidth, centreDistance);
    int drop = static_cast<int>(std::lround(corridor * 3.0));
    // Taper the final two contour levels before the frozen highland band.
    // This avoids a cut/uncut step where a dry valley reaches height 96.
    drop = std::min(drop, std::max(0, (96 - oldHeight + 1) / 2));
    column.height = oldHeight - drop;
    if (column.height == oldHeight) {
        return column;
    }

    // Existing water filling supplies the actual 2..5-block water column.
    // Keep the parent biome identity, but give the altered bed and bank an
    // explicit material so vegetation and tree placement can reject it.
    if (column.height < 64) {
        const double bedMaterial = noise(
            x, z, 96.0, 0x510e527fade682d1ull);
        column.surface = bedMaterial > 0.32
            ? Surface::Stone : Surface::Sand;
    }
    else if (column.height <= 65) {
        column.surface = Surface::Sand;
    }
    else if (column.height <= 68) {
        column.surface = column.biome == TerrainBiome::Desert
            ? Surface::Sand : Surface::Dirt;
    }
    return column;
}

TerrainFoundation::Column TerrainFoundation::sampleV13(
    int worldX, int worldZ) const noexcept
{
    return sampleLandform(worldX, worldZ, false);
}

TerrainFoundation::Column TerrainFoundation::sampleV15(
    int worldX, int worldZ) const noexcept
{
    return sampleLandform(worldX, worldZ, true);
}

TerrainFoundation::Column TerrainFoundation::sampleLandform(
    int worldX, int worldZ, bool surfaceTransitions) const noexcept
{
    Column column = sampleV11(worldX, worldZ);
    const double oldHeight = column.height;
    if (oldHeight < 59.0 || oldHeight >= 164.0) { return column; }
    const double x = static_cast<double>(worldX);
    const double z = static_cast<double>(worldZ);
    const auto blend = [](double a, double b, double weight) {
        return a + (b - a) * weight;
    };
    const double ecology = noise(x, z, 300.0, 0x3f84d5b5b5470917ull);
    const double geology = noise(x, z, 512.0, 0x9b05688c2b3e6c1full);
    const double moisture = noise(x, z, 420.0, 0x1f83d9abfb41bd6bull);
    const double inland = smooth(61.0, 92.0, oldHeight) *
        (1.0 - smooth(108.0, 164.0, oldHeight));

    // Each region changes the silhouette, not just its biome label. All
    // Blend the new relief before rounding and assigning the final biome.
    const double rolling = oldHeight +
        noise(x, z, 165.0, 0x5be0cd19137e2179ull) * 10.0 +
        noise(x, z, 55.0, 0xcbbb9d5dc1059ed8ull) * 2.0;
    const double basin = oldHeight +
        noise(x, z, 300.0, 0x629a292a367cd507ull) * 8.0 + 4.0 -
        smooth(-0.2, 0.65, noise(x, z, 210.0, 0x9159015a3070dd17ull)) * 14.0;
    double height = blend(oldHeight,
        blend(rolling, basin, smooth(-0.1, 0.34, ecology)), inland);

    // Bent, uneven wind ridges: a broad warp avoids straight, equal-width
    // bands. Their amplitude and underlying grade stay bounded.
    const double wind = (x * .82 + z * .57 +
        noise(x, z, 140.0, 0x152fecd8f70e5939ull) * 35.0 +
        noise(x, z, 320.0, 0x67332667ffc00b31ull) * 60.0) / 14.0;
    const double crest = .5 + .5 * std::sin(wind);
    const double duneHeight = 78.0 + (oldHeight - 78.0) * .5 +
        crest * crest * 12.0 - 3.0;
    const double dunes = (1.0 - smooth(-.55, -.12, ecology)) * inland;
    height = blend(height, duneHeight, dunes);

    // Broad rock shelves separated by sloping shoulders, with a winding
    // incision through them. The terrace interpolation is continuous at
    // integer levels; a discrete biome boundary never creates the cliff.
    const double level = (noise(x, z, 230.0, 0x8eb44a8768581511ull) + 1.0) * 1.5;
    const double fraction = level - std::floor(level);
    const double shelf = std::floor(level) + smooth(.38, .92, fraction);
    const double valley = std::abs(noise(x, z, 300.0, 0xdb0c2e0d64f98fa7ull) +
        noise(x, z, 105.0, 0x47b5481dbefa4fa4ull) * .18);
    const double cut = 22.0 * (1.0 - smooth(.04, .26, valley));
    const double rockHeight = 74.0 + shelf * 15.0 - cut +
        noise(x, z, 65.0, 0x0fc19dc68b8cd5b5ull) * 1.3;
    const double rock = smooth(.05, .60, geology) * inland;
    // A low shelf must not pull down the shoulder of an existing high ridge
    // by its entire altitude difference. Bound the local displacement before
    // blending, retaining flat shelves in the lowlands and a gradual upland
    // transition instead of magnifying rounded parent contour steps.
    const double localRockHeight = oldHeight +
        std::max(-28.0, std::min(28.0, rockHeight - oldHeight));
    height = blend(height, localRockHeight, rock);

    // Low wet basins contain real water at the existing sea level and dry
    // grass islands. No per-column floating water level or neighbour reads.
    const double wet = smooth(.04, .60, moisture) * smooth(-.30, .08, ecology) *
        smooth(60.0, 74.0, oldHeight) * (1.0 - smooth(76.0, 99.0, oldHeight)) *
        (1.0 - rock * .7);
    const double wetHeight = 60.0 +
        noise(x, z, 55.0, 0x240ca1cc77ac9c65ull) * 4.5 +
        smooth(-.25, .55, noise(x, z, 180.0, 0x2de92c6f592b0275ull)) * 3.0;
    height = blend(height, wetHeight, wet);
    column.height = std::max(1, std::min(176, static_cast<int>(std::lround(height))));

    if (wet > .55) {
        column.biome = TerrainBiome::Wetland;
        column.surface = column.height < 64 ? Surface::Dirt : Surface::Grass;
    }
    else if (column.height < 64) {
        column.biome = TerrainBiome::Ocean;
        column.surface = Surface::Sand;
    }
    else if (rock > .55) {
        column.biome = TerrainBiome::RockPlateau;
        // A narrow sandy wash and thin soil belts leave the shelves visibly
        // rocky. Wide sand bands would make this silhouette read as dunes.
        column.surface = valley < .025 ? Surface::Sand :
            fraction > .45 && fraction < .53 ? Surface::Dirt : Surface::Stone;
    }
    else {
        if (column.biome == TerrainBiome::Ocean) {
            column.biome = ecology < -.35 ? TerrainBiome::Desert :
                ecology < -.05 ? TerrainBiome::Grassland :
                ecology < .30 ? TerrainBiome::LightForest : TerrainBiome::TemperateForest;
        }
        if (dunes > .5) { column.biome = TerrainBiome::Desert; }
        if (column.height <= 65) { column.surface = Surface::Sand; }
        else if (inland > .25) {
            column.surface = column.biome == TerrainBiome::Desert ? Surface::Sand :
                column.biome == TerrainBiome::Mountain && column.height >= 100
                    ? Surface::Stone : Surface::Grass;
        }
    }
    if (surfaceTransitions && column.height > 65 && rock > .38 &&
        column.biome != TerrainBiome::Wetland &&
        !(column.biome == TerrainBiome::Mountain && column.height >= 100)) {
        // Several-metre deposits break up contour-wide soil ribbons. The
        // broad rock field also blends outcrops into the surrounding ground
        // before/after the biome label switches, without changing elevation.
        // All samples are continuous world-coordinate fields, not per-block
        // random rolls or neighbour queries.
        const double patch = .5 + .5 * noise(
            x, z, 18.0, 0x81c2c92e47edaee6ull);
        const double rockCover = smooth(.38, .72, rock);
        const bool bareRock = patch < rockCover;
        const Surface surround = dunes > .5 || column.biome == TerrainBiome::Desert
            ? Surface::Sand : column.biome == TerrainBiome::Mountain
                ? Surface::Stone : Surface::Grass;
        column.surface = bareRock ? Surface::Stone : surround;
        if (bareRock) {
            const double sediment = noise(x, z, 24.0, 0xc6bc279692b5cc83ull) * .7 +
                noise(x, z, 9.0, 0x9e6c63d0676a9a99ull) * .3;
            const double soilBed = 1.0 - smooth(.025, .075, std::abs(fraction - .49));
            if (valley < .008 + .030 * patch) {
                column.surface = Surface::Sand;
            }
            else if (soilBed * smooth(-.50, .55, sediment) > .48) {
                column.surface = Surface::Dirt;
            }
        }
    }
    return column;
}

int TerrainFoundation::chunkSeed(int seed, int chunkX, int chunkZ,
                                 std::uint64_t salt) noexcept
{
    const std::uint64_t hash = mix(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(seed)) ^ salt ^
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkX))) ^
        mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(chunkZ)) +
            0x9e3779b97f4a7c15ull));
    return static_cast<int>(hash & 0x7fffffffull);
}
