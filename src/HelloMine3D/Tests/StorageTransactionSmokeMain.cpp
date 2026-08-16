#include "../World/Block/BlockId.h"
#include "../World/Storage/ChunkStorageData.h"
#include "../World/Storage/StorageTransaction.h"
#include "../World/Storage/WorldSave.h"
#include "../World/WorldConstants.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;

    class TemporaryDirectory {
      public:
        explicit TemporaryDirectory(const std::string &label)
        {
            static std::uint64_t sequence = 0;
            const auto tick = std::chrono::steady_clock::now()
                                  .time_since_epoch()
                                  .count();
            m_path = fs::temp_directory_path() /
                     ("hellomine-storage-" + label + "-" +
                      std::to_string(tick) + "-" +
                      std::to_string(++sequence));
            fs::create_directories(m_path);
        }

        ~TemporaryDirectory()
        {
            std::error_code error;
            fs::remove_all(m_path, error);
        }

        const fs::path &path() const
        {
            return m_path;
        }

      private:
        fs::path m_path;
    };

    class TestSuite {
      public:
        void check(const std::string &id, bool passed,
                   const std::string &detail = std::string())
        {
            ++m_checks;
            if (!passed) {
                ++m_failures;
            }
            std::cout << "[STORAGE_TRANSACTION_TEST] "
                      << (passed ? "PASS " : "FAIL ") << id;
            if (!detail.empty()) {
                std::cout << " :: " << detail;
            }
            std::cout << '\n';
        }

        int finish() const
        {
            std::cout << "[STORAGE_TRANSACTION_TEST] checks=" << m_checks
                      << " failures=" << m_failures << '\n';
            std::cout << "[STORAGE_TRANSACTION_TEST] status="
                      << (m_failures == 0 ? "PASS" : "FAIL") << '\n';
            return m_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

      private:
        int m_checks = 0;
        int m_failures = 0;
    };

    std::string readFile(const fs::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
    }

    void writeFile(const fs::path &path, const std::string &contents)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
    }

    WorldSaveData makeWorld(int seed, const std::string &name)
    {
        WorldSaveData data;
        data.worldId = "world-transaction-fixture";
        data.worldName = name;
        data.seed = seed;
        data.createdUtc = 1786838400;
        data.lastPlayedUtc = 1786838460 + seed;
        data.lastBuildIdentity = "k2-fixture";
        data.spawnPoint = {1.25f, 70.f, -4.5f};
        data.worldTime = 1234.5f + static_cast<float>(seed);
        data.hasPlayerState = true;
        data.playerState.position = {4.5f, 71.f, 8.25f};
        data.playerState.rotation = {15.f, 30.f, 0.f};
        data.playerState.heldItem = 0;
        data.playerState.inventory.push_back(
            {Material::ID::Stone, seed + 1});

        ActorSaveState actor;
        actor.kind = ActorSaveKind::Item;
        actor.id = 17;
        actor.type = "hellomine:transaction_item";
        actor.position = {9.f, 72.f, 10.f};
        actor.velocity = {0.25f, 0.5f, -0.25f};
        actor.alive = true;
        actor.materialId = static_cast<int>(Material::ID::CoalOre);
        actor.amount = seed + 2;
        actor.pickupDelay = 1.25f;
        data.actors.push_back(actor);
        return data;
    }

    std::size_t blockIndex(int x, int y, int z)
    {
        return static_cast<std::size_t>(y * CHUNK_AREA + z * CHUNK_SIZE + x);
    }

    StoredChunkData makeChunk(BlockId block, BlockMetadata_t metadata,
                              const std::string &payload)
    {
        StoredChunkData data;
        data.x = -3;
        data.z = 7;
        data.sectionCount = 1;
        data.blockIds.assign(CHUNK_VOLUME,
                             static_cast<Block_t>(BlockId::Air));
        data.metadata.assign(CHUNK_VOLUME, 0);
        const std::size_t index = blockIndex(2, 5, 6);
        data.blockIds[index] = static_cast<Block_t>(block);
        data.metadata[index] = metadata;
        data.blockEntities.push_back(
            {{2, 5, 6}, "hellomine:transaction", payload});
        return data;
    }

    bool worldMatches(const WorldSaveData &data, int seed,
                      const std::string &name)
    {
        return data.version == WorldSaveFormatVersion && data.seed == seed &&
               data.worldName == name && data.hasPlayerState &&
               data.playerState.inventory.size() == 1 &&
               data.playerState.inventory.front().amount == seed + 1 &&
               data.actors.size() == 1 &&
               data.actors.front().amount == seed + 2;
    }

    bool chunkMatches(const StoredChunkData &data, BlockId block,
                      BlockMetadata_t metadata, const std::string &payload)
    {
        const std::size_t index = blockIndex(2, 5, 6);
        return data.x == -3 && data.z == 7 && data.sectionCount == 1 &&
               data.blockIds.size() == CHUNK_VOLUME &&
               data.metadata.size() == CHUNK_VOLUME &&
               data.blockIds[index] == static_cast<Block_t>(block) &&
               data.metadata[index] == metadata &&
               data.blockEntities.size() == 1 &&
               data.blockEntities.front().payload == payload;
    }

    const std::array<StorageFaultPoint, 5> FaultPoints = {
        StorageFaultPoint::BeforeWrite,
        StorageFaultPoint::MidWrite,
        StorageFaultPoint::BeforeFlush,
        StorageFaultPoint::BeforeValidation,
        StorageFaultPoint::BeforeReplace};
}

int main()
{
    TestSuite suite;

    {
        TemporaryDirectory root("world");
        const WorldSave storage(root.path().string());
        const WorldSaveData original = makeWorld(10, "Stable World");
        suite.check("K2/world-initial-generation",
                    storage.save(original));

        for (std::size_t i = 0; i < FaultPoints.size(); ++i) {
            const StorageFaultPoint point = FaultPoints[i];
            const WorldSaveData candidate =
                makeWorld(20 + static_cast<int>(i), "Candidate World");
            StorageTransactionMetrics metrics;
            const bool published = storage.save(
                candidate, StorageTransactionOptions{point}, &metrics);
            WorldSaveData loaded;
            const bool preserved = storage.load(loaded);
            const fs::path pending =
                StorageTransaction::pendingPath(storage.metadataPath());
            const fs::path quarantine =
                StorageTransaction::quarantinePath(storage.metadataPath());
            const std::string pointName = storageFaultPointName(point);
            suite.check(
                "K2/world-fault-" + pointName,
                !published && preserved &&
                    worldMatches(loaded, 10, "Stable World") &&
                    !fs::exists(pending) && fs::is_regular_file(quarantine) &&
                    metrics.quarantinePath == quarantine.string() &&
                    metrics.error.find(pointName) != std::string::npos,
                metrics.error);
        }

        StorageTransactionMetrics metrics;
        const WorldSaveData replacement = makeWorld(42, "Published World");
        WorldSaveData loaded;
        const bool published = storage.save(replacement, {}, &metrics);
        suite.check(
            "K2/world-successful-replace",
            published && storage.load(loaded) &&
                worldMatches(loaded, 42, "Published World") &&
                metrics.durablyFlushed && metrics.candidateValidated &&
                metrics.published && metrics.bytesWritten > 0 &&
                metrics.totalMilliseconds > 0.0 &&
                !fs::exists(StorageTransaction::pendingPath(
                    storage.metadataPath())),
            "bytes=" + std::to_string(metrics.bytesWritten) +
                " total_ms=" + std::to_string(metrics.totalMilliseconds));
    }

    {
        TemporaryDirectory root("chunk");
        const ChunkStorageData storage(root.path().string());
        const StoredChunkData original =
            makeChunk(BlockId::Stone, 3, "{\"generation\":1}");
        suite.check("K2/chunk-initial-generation",
                    storage.saveChunkData(original));

        for (std::size_t i = 0; i < FaultPoints.size(); ++i) {
            const StorageFaultPoint point = FaultPoints[i];
            const StoredChunkData candidate = makeChunk(
                BlockId::IronOre, static_cast<BlockMetadata_t>(10 + i),
                "{\"generation\":2}");
            StorageTransactionMetrics metrics;
            const bool published = storage.saveChunkData(
                candidate, StorageTransactionOptions{point}, &metrics);
            StoredChunkData loaded;
            const bool preserved =
                storage.loadChunkData(original.x, original.z, loaded);
            const std::string target =
                storage.chunkPath(original.x, original.z);
            const fs::path pending = StorageTransaction::pendingPath(target);
            const fs::path quarantine =
                StorageTransaction::quarantinePath(target);
            const std::string pointName = storageFaultPointName(point);
            suite.check(
                "K2/chunk-fault-" + pointName,
                !published && preserved &&
                    chunkMatches(loaded, BlockId::Stone, 3,
                                 "{\"generation\":1}") &&
                    !fs::exists(pending) && fs::is_regular_file(quarantine) &&
                    metrics.quarantinePath == quarantine.string() &&
                    metrics.error.find(pointName) != std::string::npos,
                metrics.error);
        }

        const StoredChunkData replacement =
            makeChunk(BlockId::CoalOre, 12, "{\"generation\":3}");
        StorageTransactionMetrics metrics;
        StoredChunkData loaded;
        const bool published = storage.saveChunkData(replacement, {}, &metrics);
        suite.check(
            "K2/chunk-successful-replace",
            published &&
                storage.loadChunkData(replacement.x, replacement.z, loaded) &&
                chunkMatches(loaded, BlockId::CoalOre, 12,
                             "{\"generation\":3}") &&
                metrics.durablyFlushed && metrics.candidateValidated &&
                metrics.published && metrics.bytesWritten > 0 &&
                metrics.totalMilliseconds > 0.0 &&
                !fs::exists(StorageTransaction::pendingPath(
                    storage.chunkPath(replacement.x, replacement.z))),
            "bytes=" + std::to_string(metrics.bytesWritten) +
                " total_ms=" + std::to_string(metrics.totalMilliseconds));
    }

    {
        TemporaryDirectory root("validator");
        const std::string target = (root.path() / "published.bin").string();
        writeFile(target, "last-good");
        const std::vector<char> invalid{'b', 'a', 'd'};
        StorageTransactionMetrics metrics;
        const bool published = StorageTransaction::publish(
            target, invalid,
            [](const std::string &, std::string &error) {
                error = "fixture rejected candidate";
                return false;
            },
            {}, &metrics);
        suite.check(
            "K2/validator-rejection-preserves-published",
            !published && readFile(target) == "last-good" &&
                !fs::exists(StorageTransaction::pendingPath(target)) &&
                readFile(StorageTransaction::quarantinePath(target)) ==
                    "bad" &&
                metrics.error.find("fixture rejected candidate") !=
                    std::string::npos,
            metrics.error);
    }

    {
        TemporaryDirectory root("stale");
        const std::string target = (root.path() / "published.bin").string();
        writeFile(target, "generation-one");
        writeFile(StorageTransaction::pendingPath(target), "stale-pending");
        const std::string next = "generation-two";
        const std::vector<char> payload(next.begin(), next.end());
        StorageTransactionMetrics metrics;
        const bool published = StorageTransaction::publish(
            target, payload,
            [next](const std::string &candidate, std::string &error) {
                if (readFile(candidate) == next) {
                    return true;
                }
                error = "candidate bytes differ";
                return false;
            },
            {}, &metrics);
        suite.check(
            "K2/stale-pending-is-bounded-and-quarantined",
            published && readFile(target) == next &&
                !fs::exists(StorageTransaction::pendingPath(target)) &&
                readFile(StorageTransaction::quarantinePath(target)) ==
                    "stale-pending" &&
                metrics.published,
            metrics.error);
    }

    return suite.finish();
}
