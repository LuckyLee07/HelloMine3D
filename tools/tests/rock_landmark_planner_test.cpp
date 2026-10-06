#include "World/Generation/Ecology/AdventureEcologyPlanner.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
constexpr int BaselineVersion = 29;
#ifdef HELLOMINE3D_ROCK_FREEZE_BASELINE
constexpr int CandidateVersion = 30;
#else
constexpr int CandidateVersion = RockLandmarkTerrainGenerationVersion;
#endif
constexpr std::array<int, 3> Seeds{{42, 20260807, 239701883}};

template <typename T, typename = void> struct HasRockCore : std::false_type {};
template <typename T>
struct HasRockCore<T, std::void_t<decltype(std::declval<T>().rockCore)>>
    : std::true_type {};

template <typename T> bool isRockCore(const T &sample)
{
    if constexpr (HasRockCore<T>::value) return sample.rockCore;
    else return false; // Only used when recording the frozen pre-v30 witness.
}

std::uint64_t bits(double value)
{
    static_assert(sizeof(value) == sizeof(std::uint64_t), "64-bit double evidence");
    std::uint64_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

void hashWord(std::uint64_t &hash, std::uint64_t value)
{
    for (int byte = 0; byte < 8; ++byte) {
        hash ^= (value >> (byte * 8)) & 0xffu;
        hash *= 1099511628211ull;
    }
}

void hashColumn(std::uint64_t &hash, const TerrainFoundation::Column &column)
{
    hashWord(hash, static_cast<std::uint64_t>(column.height));
    hashWord(hash, static_cast<std::uint64_t>(column.biome));
    hashWord(hash, static_cast<std::uint64_t>(column.surface));
}

struct Point { int x, z; };

std::vector<Point> identityPoints()
{
    constexpr int Low = std::numeric_limits<int>::min();
    constexpr int High = std::numeric_limits<int>::max();
    constexpr std::array<int, 19> Coordinates{{
        Low, Low + 1, -2048, -513, -65, -64, -63, -17, -1, 0,
        1, 16, 63, 64, 65, 512, 2048, High - 1, High}};
    std::vector<Point> result;
    for (int z : Coordinates) for (int x : Coordinates) result.push_back({x, z});
    return result;
}

// Field-wise fingerprints recorded from committed 6992b77b, before v30 exists.
// These cover the pure planners only. Production World T0 remains a separate gate.
// Never record struct padding or include the newly added rockCore field here.
constexpr std::array<std::array<std::uint64_t, 29>, 3> LegacyFingerprints{{
    {{
        15203783628701354360ull, // seed 42, version 1
        6859551531088745923ull, // seed 42, version 2
        7733752472627975850ull, // seed 42, version 3
        18329419940895462869ull, // seed 42, version 4
        4052096012347584492ull, // seed 42, version 5
        4332054029125124263ull, // seed 42, version 6
        11613007244954427950ull, // seed 42, version 7
        10547921067140233369ull, // seed 42, version 8
        9494862885323368736ull, // seed 42, version 9
        8047521529811500587ull, // seed 42, version 10
        17608783209480217010ull, // seed 42, version 11
        13422072162130479677ull, // seed 42, version 12
        14776208736123522516ull, // seed 42, version 13
        10015428014816958767ull, // seed 42, version 14
        16935782460011858838ull, // seed 42, version 15
        14608355067287194177ull, // seed 42, version 16
        16218893535937362760ull, // seed 42, version 17
        15228417791830780051ull, // seed 42, version 18
        5297710660424033338ull, // seed 42, version 19
        10373750365127225189ull, // seed 42, version 20
        14600592995021229500ull, // seed 42, version 21
        15546977455660731703ull, // seed 42, version 22
        559484588480633278ull, // seed 42, version 23
        16438825380803711593ull, // seed 42, version 24
        12128640618988455135ull, // seed 42, version 25
        8454260487615756751ull, // seed 42, version 26
        1893354126904299574ull, // seed 42, version 27
        6675374339789365857ull, // seed 42, version 28
        14675085577552407776ull // seed 42, version 29
    }},
    {{
        4137333027715755978ull, // seed 20260807, version 1
        4566963002989720617ull, // seed 20260807, version 2
        11902876117280696800ull, // seed 20260807, version 3
        14752813078485845815ull, // seed 20260807, version 4
        5241939983712795598ull, // seed 20260807, version 5
        9558294111060916029ull, // seed 20260807, version 6
        17649118956663753620ull, // seed 20260807, version 7
        4729168841371188235ull, // seed 20260807, version 8
        418053297352566882ull, // seed 20260807, version 9
        3531925111860032577ull, // seed 20260807, version 10
        7693808960480457656ull, // seed 20260807, version 11
        11873965848786198319ull, // seed 20260807, version 12
        13191147871034961446ull, // seed 20260807, version 13
        12459001018918230261ull, // seed 20260807, version 14
        17178903319257069164ull, // seed 20260807, version 15
        5456245216722668259ull, // seed 20260807, version 16
        14749358809933752602ull, // seed 20260807, version 17
        9695733175459036985ull, // seed 20260807, version 18
        5466352842645114224ull, // seed 20260807, version 19
        1974397932069090759ull, // seed 20260807, version 20
        94965854992503006ull, // seed 20260807, version 21
        2129372281442074637ull, // seed 20260807, version 22
        10826398394090452580ull, // seed 20260807, version 23
        3363955764584021723ull, // seed 20260807, version 24
        16507545467922993650ull, // seed 20260807, version 25
        2344294549572497039ull, // seed 20260807, version 26
        1363268138555233222ull, // seed 20260807, version 27
        2316623660299876533ull, // seed 20260807, version 28
        7148939775029739932ull // seed 20260807, version 29
    }},
    {{
        13885441888309971019ull, // seed 239701883, version 1
        11432108213569970728ull, // seed 239701883, version 2
        15780174745185141725ull, // seed 239701883, version 3
        12350189885533479594ull, // seed 239701883, version 4
        15393152265414086959ull, // seed 239701883, version 5
        8982638458074666796ull, // seed 239701883, version 6
        5808616473080281409ull, // seed 239701883, version 7
        1301433860298648926ull, // seed 239701883, version 8
        15418148111189064611ull, // seed 239701883, version 9
        9014615737608622592ull, // seed 239701883, version 10
        2734557841519995573ull, // seed 239701883, version 11
        5749047267074712322ull, // seed 239701883, version 12
        10903416194237486887ull, // seed 239701883, version 13
        10887850532349145892ull, // seed 239701883, version 14
        3023837759817441593ull, // seed 239701883, version 15
        15097244159709830806ull, // seed 239701883, version 16
        9097037961912061179ull, // seed 239701883, version 17
        12082632664632425752ull, // seed 239701883, version 18
        14559775580005711181ull, // seed 239701883, version 19
        18401085220324193242ull, // seed 239701883, version 20
        12447354702370553375ull, // seed 239701883, version 21
        18202833836543282844ull, // seed 239701883, version 22
        7150443723102068913ull, // seed 239701883, version 23
        11006632761880113614ull, // seed 239701883, version 24
        12652329734024781672ull, // seed 239701883, version 25
        477459362644942344ull, // seed 239701883, version 26
        9312042261717395781ull, // seed 239701883, version 27
        4019257566358602598ull, // seed 239701883, version 28
        6718157068931050075ull // seed 239701883, version 29
    }},
}};

std::uint64_t legacyFingerprint(int seed, int version, bool &noRockCore)
{
    AdventureWaterPlanner water(seed, version);
    LocalTerrainPlanner local(seed, version);
    AdventureEcologyPlanner ecology(seed, version);
    std::uint64_t hash = 14695981039346656037ull;
    hashWord(hash, static_cast<std::uint64_t>(seed));
    hashWord(hash, static_cast<std::uint64_t>(version));
    for (const auto point : identityPoints()) {
        const auto parent = water.sample(point.x, point.z);
        const auto relief = local.sample(point.x, point.z, parent);
        const auto full = ecology.sample(point.x, point.z).column;
        const auto shape = ecology.sampleWaterColumn(point.x, point.z);
        hashWord(hash, static_cast<std::uint64_t>(point.x));
        hashWord(hash, static_cast<std::uint64_t>(point.z));
        hashColumn(hash, parent.column);
        hashWord(hash, bits(parent.riverInfluence));
        hashWord(hash, bits(parent.lakeInfluence));
        hashWord(hash, static_cast<std::uint64_t>(relief.height));
        hashWord(hash, bits(relief.slope));
        hashWord(hash, bits(relief.deposit));
        hashColumn(hash, full);
        hashColumn(hash, shape);
        noRockCore &= !isRockCore(relief);
    }
    return hash;
}

struct WitnessSite { int seed, x, z; };
// Production-planner sites supplied by the implementation owner.
// Dense windows exercise actual columns; the test does not reproduce anchor/hash maths.
constexpr std::array<WitnessSite, 3> WitnessSites{{
    {42, -1312, -3808}, {20260807, -2464, -1312}, {239701883, -1568, 2784}
}};

struct Query {
    int seed;
    int version;
    AdventureWaterPlanner water;
    LocalTerrainPlanner local;
    AdventureEcologyPlanner ecology;
    Query(int querySeed, int queryVersion)
        : seed(querySeed), version(queryVersion), water(seed, version),
          local(seed, version), ecology(seed, version) {}
    static Query candidate(int seed, bool withoutRock)
    {
        // Every candidate query, including neighbours and cache probes, uses
        // this adapter. The negative run must fail the same existence checks.
        return Query(seed, withoutRock ? BaselineVersion : CandidateVersion);
    }
};

struct Record {
    LocalTerrainPlanner::Sample relief;
    TerrainFoundation::Column ecology, shape, parent;
    double riverInfluence, lakeInfluence;
};

Record record(Query &query, Point point)
{
    const auto parent = query.water.sample(point.x, point.z);
    return {query.local.sample(point.x, point.z, parent),
        query.ecology.sample(point.x, point.z).column,
        query.ecology.sampleWaterColumn(point.x, point.z), parent.column,
        parent.riverInfluence, parent.lakeInfluence};
}

bool sameColumn(const TerrainFoundation::Column &a, const TerrainFoundation::Column &b)
{
    return a.height == b.height && a.biome == b.biome && a.surface == b.surface;
}

bool sameRecord(const Record &a, const Record &b)
{
    return a.relief.height == b.relief.height &&
        bits(a.relief.slope) == bits(b.relief.slope) &&
        bits(a.relief.deposit) == bits(b.relief.deposit) &&
        isRockCore(a.relief) == isRockCore(b.relief) &&
        sameColumn(a.ecology, b.ecology) && sameColumn(a.shape, b.shape) &&
        sameColumn(a.parent, b.parent) &&
        bits(a.riverInfluence) == bits(b.riverInfluence) &&
        bits(a.lakeInfluence) == bits(b.lakeInfluence);
}

struct Checks {
    int checks = 0, failures = 0;
    void check(const std::string &name, bool pass)
    {
        ++checks;
        failures += !pass;
        std::cout << "[ROCK30] " << (pass ? "PASS " : "FAIL ") << name << '\n';
    }
};

struct Counts {
    std::size_t samples = 0, changed = 0, core = 0, protectedColumns = 0;
    std::size_t wet = 0, tinyWaterInfluence = 0, wetlands = 0, dunes = 0, ocean = 0;
    std::size_t fineWetlands = 0, fineDunes = 0;
    std::size_t fullEcology = 0, changedWithNeighbour = 0;
    int maximumDelta = 0;
    bool bounds = true, protectedHeight = true, coreEligibility = true;
    bool waterIdentity = true, shapeIdentity = true, numeric = true, legacyReliefFields = true;
    bool fineEcologyProtected = true;
};

void sampleColumn(Query &old, Query &now, Point point, const char *kind,
    bool inspectEcology, std::ofstream &csv, Counts &counts)
{
    const auto parent = now.water.sample(point.x, point.z);
    const auto priorWater = old.water.sample(point.x, point.z);
    const auto prior = old.local.sample(point.x, point.z, priorWater);
    const auto next = now.local.sample(point.x, point.z, parent);
    const int delta = next.height - prior.height;
    const bool core = isRockCore(next);
    const bool watery = parent.riverInfluence > 0 || parent.lakeInfluence > 0;
    const bool wetland = parent.base.region == AdventureRegion::Wetland ||
        parent.column.biome == TerrainBiome::Wetland;
    const bool dunes = parent.base.region == AdventureRegion::Dunes;
    const bool ocean = parent.column.biome == TerrainBiome::Ocean;
    const bool low = prior.height < 94 || parent.column.height < 94;
    const bool canyon = parent.base.region == AdventureRegion::Canyon ||
        parent.column.biome == TerrainBiome::RockPlateau;
    const bool protectedColumn = watery || wetland || dunes || ocean || low || !canyon;
    ++counts.samples;
    counts.changed += delta > 0;
    counts.core += core;
    counts.protectedColumns += protectedColumn;
    counts.wet += watery;
    const double influence = std::max(parent.riverInfluence, parent.lakeInfluence);
    counts.tinyWaterInfluence += influence > 0 && influence < .02;
    counts.wetlands += wetland;
    counts.dunes += dunes;
    counts.ocean += ocean;
    counts.maximumDelta = std::max(counts.maximumDelta, delta);
    counts.bounds &= delta >= 0 && delta <= 6 && next.height >= 1 && next.height <= 176;
    counts.bounds &= core == (delta > 0);
    counts.numeric &= std::isfinite(next.slope) && std::isfinite(next.deposit);
    counts.legacyReliefFields &= bits(prior.slope) == bits(next.slope) &&
        bits(prior.deposit) == bits(next.deposit);
    if (protectedColumn) counts.protectedHeight &= delta == 0 && !core;
    if (core) counts.coreEligibility &= canyon && !watery && !low;
    counts.waterIdentity &= sameColumn(parent.column, priorWater.column) &&
        bits(parent.riverInfluence) == bits(priorWater.riverInfluence) &&
        bits(parent.lakeInfluence) == bits(priorWater.lakeInfluence);
    if (delta > 0) {
        bool joined = false;
        for (const Point d : {Point{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const auto px = static_cast<std::int64_t>(point.x) + d.x;
            const auto pz = static_cast<std::int64_t>(point.z) + d.z;
            if (px < std::numeric_limits<int>::min() || px > std::numeric_limits<int>::max() ||
                pz < std::numeric_limits<int>::min() || pz > std::numeric_limits<int>::max()) continue;
            const int x = static_cast<int>(px), z = static_cast<int>(pz);
            const auto wp = now.water.sample(x, z), wo = old.water.sample(x, z);
            joined |= now.local.sample(x, z, wp).height > old.local.sample(x, z, wo).height;
        }
        counts.changedWithNeighbour += joined;
    }
    if (inspectEcology || delta > 0 || core) {
        const auto full = now.ecology.sample(point.x, point.z);
        const auto light = now.ecology.sampleWaterColumn(point.x, point.z);
        const bool fineWetland = full.region == AdventureRegion::Wetland;
        const bool fineDunes = full.region == AdventureRegion::Dunes;
        counts.fineWetlands += fineWetland;
        counts.fineDunes += fineDunes;
        if (fineWetland || fineDunes) counts.fineEcologyProtected &= delta == 0 && !core;
        counts.shapeIdentity &= full.column.height == next.height && light.height == next.height &&
            full.column.biome == light.biome;
        // Snow has priority; non-core neighbouring materials can legitimately
        // change through the existing two-metre slope probes.
        if (core) counts.shapeIdentity &= full.column.surface == TerrainFoundation::Surface::Stone ||
            (full.column.surface == TerrainFoundation::Surface::Snow && full.column.height >= full.snowLine);
        ++counts.fullEcology;
    }
    csv << old.seed << ',' << kind << ',' << point.x << ',' << point.z << ','
        << prior.height << ',' << next.height << ',' << delta << ',' << core << ','
        << static_cast<int>(parent.column.biome) << ',' << static_cast<int>(parent.base.region) << ','
        << parent.riverInfluence << ',' << parent.lakeInfluence << ',' << protectedColumn << '\n';
}

bool cacheIdentity(int seed, bool withoutRock)
{
    auto points = identityPoints();
    for (const auto site : WitnessSites) if (site.seed == seed) points.push_back({site.x, site.z});
    Query old(seed, BaselineVersion);
    auto now = Query::candidate(seed, withoutRock);
    std::vector<Record> prior, current;
    for (const auto point : points) {
        prior.push_back(record(old, point));
        current.push_back(record(now, point));
    }
    // More distinct patch locations than the fixed 128-entry local cache;
    // alternate seed and version to exercise complete identity keys.
    auto foreign = Query::candidate(seed + 7, withoutRock);
    Query foreignOld(seed + 19, BaselineVersion);
    for (int i = 0; i < 512; ++i) {
        const Point point{i * 4099 - 1048576, 2097152 - i * 6113};
        (void)record(foreign, point);
        (void)record(foreignOld, {point.z, point.x});
    }
    bool same = true;
    for (std::size_t i = points.size(); i-- > 0;) {
        same &= sameRecord(current[i], record(now, points[i]));
        same &= sameRecord(prior[i], record(old, points[i]));
        same &= sameRecord(current[i], record(now, points[i]));
    }
    auto parallel = std::async(std::launch::async, [=] {
        Query independentOld(seed, BaselineVersion);
        auto independent = Query::candidate(seed, withoutRock);
        bool equal = true;
        for (std::size_t i = points.size(); i-- > 0;) {
            equal &= sameRecord(prior[i], record(independentOld, points[i]));
            equal &= sameRecord(current[i], record(independent, points[i]));
            equal &= sameRecord(prior[i], record(independentOld, points[i]));
        }
        return equal;
    });
    return same && parallel.get();
}

#ifdef HELLOMINE3D_ROCK_FREEZE_BASELINE
void emitLegacy()
{
    for (const int seed : Seeds) {
        std::cout << "    {{\n";
        for (int version = 1; version <= BaselineVersion; ++version) {
            bool noRock = true;
            const auto hash = legacyFingerprint(seed, version, noRock);
            std::cout << "        " << hash << "ull" << (version < BaselineVersion ? "," : "")
                << " // seed " << seed << ", version " << version << '\n';
        }
        std::cout << "    }},\n";
    }
}
#endif
} // namespace

int main(int argc, char **argv)
{
#ifdef HELLOMINE3D_ROCK_FREEZE_BASELINE
    if (argc == 2 && std::string(argv[1]) == "--emit-legacy-fingerprints") {
        emitLegacy();
        return 0;
    }
#endif
    if (argc < 2 || argc > 3 ||
        std::string(argv[1]).rfind("--", 0) == 0 ||
        (argc == 3 && std::string(argv[2]) != "--without-rock-field")) {
        std::cerr << "Usage: rock_landmark_planner_test samples.csv [--without-rock-field]\n";
        return 2;
    }
    const bool withoutRock = argc == 3;
    std::ofstream csv(argv[1]);
    if (!csv) { std::cerr << "Cannot write samples CSV\n"; return 2; }
    csv.precision(17);
    csv << "seed,kind,x,z,v29_height,height,delta,rock_core,biome,region,river_influence,lake_influence,protected\n";
    Checks checks;
    checks.check("appended-v30-interface", CandidateVersion == 30 &&
        CurrentTerrainGenerationVersion >= CandidateVersion &&
        WorkshopCourtyardTerrainGenerationVersion == BaselineVersion && HasRockCore<LocalTerrainPlanner::Sample>::value);
    for (std::size_t seedIndex = 0; seedIndex < Seeds.size(); ++seedIndex) {
        const int seed = Seeds[seedIndex];
        const std::string prefix = std::to_string(seed) + '/';
        bool legacy = true, noHistoricalRock = true;
        for (int version = 1; version <= BaselineVersion; ++version) {
            const auto hash = legacyFingerprint(seed, version, noHistoricalRock);
            const bool matches = hash == LegacyFingerprints[seedIndex][static_cast<std::size_t>(version - 1)];
            legacy &= matches;
            if (!matches) std::cout << "[ROCK30_LEGACY_MISMATCH] seed=" << seed << " version=" << version
                << " expected=" << LegacyFingerprints[seedIndex][static_cast<std::size_t>(version - 1)]
                << " actual=" << hash << '\n';
        }
        checks.check(prefix + "frozen-pure-v1-v29-fields", legacy);
        checks.check(prefix + "no-historical-rock-core", noHistoricalRock);
        Query old(seed, BaselineVersion);
        auto now = Query::candidate(seed, withoutRock);
        Counts counts;
        for (int z = -2048; z <= 2048; z += 16) for (int x = -2048; x <= 2048; x += 16)
            sampleColumn(old, now, {x, z}, "macro", x % 64 == 0 && z % 64 == 0, csv, counts);
        for (int z = -96; z <= 96; ++z) for (int x = -96; x <= 96; ++x)
            sampleColumn(old, now, {x, z}, "spawn", x % 16 == 0 && z % 16 == 0, csv, counts);
        for (int edge : {-65, -64, -63, -17, -16, -15, -1, 0, 1, 15, 16, 17, 63, 64, 65, 511, 512, 513})
            for (int other = -128; other <= 128; ++other) {
                sampleColumn(old, now, {edge, other}, "seam", false, csv, counts);
                sampleColumn(old, now, {other, edge}, "seam", false, csv, counts);
            }
        for (const auto point : identityPoints())
            sampleColumn(old, now, point, "extreme", true, csv, counts);
        for (const auto site : WitnessSites) if (site.seed == seed)
            for (int dz = -12; dz <= 12; ++dz) for (int dx = -12; dx <= 12; ++dx)
                sampleColumn(old, now, {site.x + dx, site.z + dz}, "witness", true, csv, counts);
        checks.check(prefix + "increment-zero-to-six-height-one-to-176", counts.bounds && counts.numeric);
        checks.check(prefix + "water-wetland-dunes-ocean-lowland-protected", counts.protectedHeight);
        checks.check(prefix + "fine-ecology-wetland-dunes-height-protected", counts.fineEcologyProtected &&
            counts.fineWetlands > 0 && counts.fineDunes > 0);
        checks.check(prefix + "water-planner-fields-unchanged", counts.waterIdentity);
        checks.check(prefix + "existing-relief-slope-deposit-bits-unchanged", counts.legacyReliefFields);
        checks.check(prefix + "dry-canyon-core-eligibility", counts.coreEligibility);
        checks.check(prefix + "shared-shape-ecology-and-stone-surface", counts.shapeIdentity && counts.fullEcology > 0);
        checks.check(prefix + "protected-regions-and-small-water-influence-exercised", counts.wet > 0 &&
            counts.tinyWaterInfluence > 0 && counts.wetlands > 0 && counts.dunes > 0 && counts.ocean > 0);
        checks.check(prefix + "nonzero-rock-height", counts.changed > 0);
        checks.check(prefix + "nonzero-rock-core", counts.core > 0);
        checks.check(prefix + "connected-column-footprint", counts.changed > 0 &&
            counts.changedWithNeighbour == counts.changed);
        checks.check(prefix + "signed-seed-version-cache-eviction-independent-thread", cacheIdentity(seed, withoutRock));
        std::cout << "[ROCK30_COUNTS] seed=" << seed << " samples=" << counts.samples << " changed=" << counts.changed
            << " core=" << counts.core << " max_delta=" << counts.maximumDelta
            << " protected=" << counts.protectedColumns << " wet=" << counts.wet
            << " tiny_water_influence=" << counts.tinyWaterInfluence << " wetlands=" << counts.wetlands
            << " dunes=" << counts.dunes << " ocean=" << counts.ocean << " full_ecology=" << counts.fullEcology
            << " fine_wetlands=" << counts.fineWetlands << " fine_dunes=" << counts.fineDunes << '\n';
    }
    std::cout << "[ROCK30_RESULT] mode=" << (withoutRock ? "without-rock-field" : "production")
        << " checks=" << checks.checks << " failures=" << checks.failures << '\n';
    return checks.failures == 0 ? 0 : 1;
}
