#include "../../src/HelloMine3D/World/Exploration/ExplorationMapStore.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs = std::filesystem;

void require(bool condition, const char* label)
{
    if (!condition) {
        throw std::runtime_error(label);
    }
}

std::vector<char> readBytes(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}

void writeBytes(const fs::path& path, const std::vector<char>& bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(output.good(), "test fixture write failed");
}

void refreshHash(std::vector<char>& bytes)
{
    std::uint64_t value = 14695981039346656037ull;
    for (std::size_t index = 0; index < bytes.size() - 8; ++index) {
        value ^= static_cast<unsigned char>(bytes[index]);
        value *= 1099511628211ull;
    }
    for (int offset = 0; offset < 8; ++offset) {
        bytes[bytes.size() - 8 + offset] =
            static_cast<char>(value >> (offset * 8));
    }
}

struct TemporaryWorld {
    fs::path path;
    TemporaryWorld()
    {
        const auto ticks = std::chrono::steady_clock::now()
                               .time_since_epoch().count();
        path = fs::temp_directory_path() /
               ("hm3d-exploration-store-" + std::to_string(ticks));
        fs::create_directories(path);
    }
    ~TemporaryWorld()
    {
        std::error_code error;
        fs::remove_all(path, error);
    }
};
}

int main()
{
    try {
        using Status = ExplorationMapStore::LoadStatus;
        using Result = ExplorationAtlas::ObserveResult;
        TemporaryWorld world;
        const ExplorationMapStore store(world.path.string());
        const ExplorationMapStore::Identity identity{"world-map-test", 42, 22};
        ExplorationAtlas atlas;
        std::string error;
        require(store.load(identity, atlas, &error) == Status::Absent &&
                    atlas.tileCount() == 0 && error.empty(),
                "missing sidecar defaults to an empty map");

        require(atlas.observe({-1, -1, 70, BlockId::ForestFloor, true}) ==
                    Result::Updated &&
                    atlas.observe({128, 0, 0, BlockId::Air, true}) ==
                    Result::Updated,
                "test observations were not accepted");
        StorageTransactionMetrics metrics;
        require(store.save(identity, atlas, {}, &metrics) &&
                    metrics.published && metrics.candidateValidated &&
                    store.filePath() ==
                        (world.path / "exploration.hmap").string() &&
                    ExplorationMapStore::validateFile(store.filePath(),
                                                      &error),
                "sidecar candidate was not validated and published");
        const std::vector<char> original = readBytes(store.filePath());
        ExplorationAtlas loaded;
        require(store.load(identity, loaded, &error) == Status::Loaded &&
                    loaded.tileCount() == 2 &&
                    loaded.surfaceAt(-4, -4)->material ==
                        BlockId::ForestFloor &&
                    loaded.surfaceAt(128, 0)->material == BlockId::Air &&
                    !loaded.surfaceAt(256, 0).has_value(),
                "known, known-empty and unknown cells did not round trip");

        const ExplorationMapStore::Identity otherWorld{"world-other", 42, 22};
        const ExplorationMapStore::Identity otherSeed{"world-map-test", 43, 22};
        const ExplorationMapStore::Identity otherTerrain{"world-map-test", 42, 21};
        for (const auto& foreign : {otherWorld, otherSeed, otherTerrain}) {
            require(store.load(foreign, loaded, &error) ==
                        Status::IdentityMismatch &&
                        loaded.tileCount() == 0 &&
                        readBytes(store.filePath()) == original,
                    "foreign world identity leaked or rewrote map data");
        }

        atlas.observe({-1, -1, 71, BlockId::MossStone, true});
        StorageTransactionOptions fault;
        fault.faultPoint = StorageFaultPoint::BeforeReplace;
        require(!store.save(identity, atlas, fault, &metrics) &&
                    !metrics.published &&
                    readBytes(store.filePath()) == original &&
                    store.load(identity, loaded, &error) == Status::Loaded &&
                    loaded.surfaceAt(-4, -4)->height == 70,
                "failed candidate replaced the last valid map");

        std::vector<char> corrupted = original;
        corrupted[corrupted.size() / 2] ^= 1;
        writeBytes(store.filePath(), corrupted);
        require(store.load(identity, loaded, &error) == Status::Corrupt &&
                    loaded.tileCount() == 0 && !error.empty() &&
                    !ExplorationMapStore::validateFile(store.filePath()) &&
                    readBytes(store.filePath()) == corrupted,
                "damaged sidecar did not fall back without mutation");
        writeBytes(store.filePath(), original);

        std::vector<char> unsupported = original;
        // The current format reserves this count for a later marker revision.
        const std::size_t markerCount = 8 + 4 + 1 + identity.worldId.size() + 12;
        unsupported[markerCount] = 1;
        refreshHash(unsupported);
        writeBytes(store.filePath(), unsupported);
        require(!ExplorationMapStore::validateFile(store.filePath()),
                "unsupported marker payload passed validation");
        writeBytes(store.filePath(), original);

        std::vector<char> repeatedPage = original;
        constexpr std::size_t tileBytes = 8 + 128 + 2048;
        const std::size_t firstPage = 8 + 4 + 1 + identity.worldId.size() + 16;
        for (std::size_t index = 0; index < 8; ++index) {
            repeatedPage[firstPage + tileBytes + index] =
                repeatedPage[firstPage + index];
        }
        refreshHash(repeatedPage);
        writeBytes(store.filePath(), repeatedPage);
        require(!ExplorationMapStore::validateFile(store.filePath()),
                "repeated page coordinates passed validation");
        writeBytes(store.filePath(), original);

        std::ofstream(world.path / "world.meta") << "world-untouched\n";
        require(store.load(identity, loaded, &error) == Status::Loaded &&
                    readBytes(world.path / "world.meta") ==
                        std::vector<char>({'w','o','r','l','d','-','u','n',
                                           't','o','u','c','h','e','d','\n'}),
                "map recovery touched authoritative world metadata");
        std::cout << "[EXPLORATION_MAP_STORE] checks=8 status=PASS\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "[EXPLORATION_MAP_STORE] FAIL "
                  << exception.what() << '\n';
        return 1;
    }
}
