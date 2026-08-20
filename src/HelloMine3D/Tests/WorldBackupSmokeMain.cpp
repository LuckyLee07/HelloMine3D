#include "../World/Block/BlockId.h"
#include "../Diagnostics/OperationPerformanceTiming.h"
#include "../World/Storage/ChunkStorageData.h"
#include "../World/Storage/WorldBackup.h"
#include "../World/Storage/WorldSave.h"
#include "../World/WorldConstants.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
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
                     ("hellomine-backup-" + label + "-" +
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
            std::cout << "[WORLD_BACKUP_TEST] "
                      << (passed ? "PASS " : "FAIL ") << id;
            if (!detail.empty()) {
                std::cout << " :: " << detail;
            }
            std::cout << '\n';
        }

        int finish() const
        {
            std::cout << "[WORLD_BACKUP_TEST] checks=" << m_checks
                      << " failures=" << m_failures << '\n';
            std::cout << "[WORLD_BACKUP_TEST] status="
                      << (m_failures == 0 ? "PASS" : "FAIL") << '\n';
            return m_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

      private:
        int m_checks = 0;
        int m_failures = 0;
    };

    WorldBackupPolicy testPolicy()
    {
        WorldBackupPolicy policy;
        policy.maxBackups = 3;
        policy.maxTotalBytes = 2 * 1024 * 1024;
        policy.maxFiles = 16;
        policy.maxFileBytes = 1024 * 1024;
        return policy;
    }

    std::size_t blockIndex(int x, int y, int z)
    {
        return static_cast<std::size_t>(y * CHUNK_AREA + z * CHUNK_SIZE + x);
    }

    WorldSaveData makeWorld(int generation)
    {
        WorldSaveData data;
        data.worldId = "world-backup-fixture";
        data.worldName = "Backup Fixture";
        data.seed = 9000 + generation;
        data.createdUtc = 1786838400;
        data.lastPlayedUtc = 1786838500 + generation;
        data.lastBuildIdentity = "k3-fixture";
        data.spawnPoint = {1.f, 72.f, 3.f};
        data.worldTime = 4000.f + static_cast<float>(generation);
        data.hasPlayerState = true;
        data.playerState.position = {8.f, 73.f, 9.f};
        data.playerState.rotation = {10.f, 20.f, 0.f};
        data.playerState.heldItem = 0;
        data.playerState.inventory.push_back(
            {Material::ID::Dirt, generation + 2});

        ActorSaveState actor;
        actor.kind = ActorSaveKind::Mob;
        actor.id = 25;
        actor.type = "hellomine:backup_mob";
        actor.position = {11.f, 73.f, 12.f};
        actor.rotation = {0.f, 90.f, 0.f};
        actor.velocity = {0.1f, 0.f, -0.1f};
        actor.alive = true;
        actor.health = 7.f + static_cast<float>(generation);
        actor.wanderTime = 2.f;
        actor.wanderSpeed = 1.5f;
        actor.dropMaterialId = static_cast<int>(Material::ID::CoalOre);
        actor.dropAmount = generation + 1;
        data.actors.push_back(actor);
        return data;
    }

    StoredChunkData makeChunk(int x, int z, int generation)
    {
        StoredChunkData data;
        data.x = x;
        data.z = z;
        data.sectionCount = 1;
        data.blockIds.assign(CHUNK_VOLUME,
                             static_cast<Block_t>(BlockId::Air));
        data.metadata.assign(CHUNK_VOLUME, 0);
        const std::size_t chest = blockIndex(2, 5, 6);
        const std::size_t crop = blockIndex(3, 5, 6);
        data.blockIds[chest] = static_cast<Block_t>(BlockId::Chest);
        data.blockIds[crop] = static_cast<Block_t>(BlockId::WheatCrop);
        data.metadata[crop] = static_cast<BlockMetadata_t>(generation % 8);
        data.blockEntities.push_back(
            {{2, 5, 6}, "hellomine:chest",
             "{\"generation\":" + std::to_string(generation) + "}"});
        return data;
    }

    bool saveGeneration(const fs::path &root, int generation,
                        bool includeExtra = false)
    {
        const fs::path chunks = root / "chunks";
        const WorldSave world(root.string());
        const ChunkStorageData storage(chunks.string());
        bool saved = world.save(makeWorld(generation)) &&
                     storage.saveChunkData(makeChunk(1, -2, generation));
        if (includeExtra) {
            saved = storage.saveChunkData(makeChunk(9, 9, generation)) && saved;
        }
        return saved;
    }

    bool generationMatches(const fs::path &root, int generation,
                           bool expectExtra = false)
    {
        WorldSaveData world;
        StoredChunkData chunk;
        const bool worldLoaded = WorldSave(root.string()).load(world);
        const ChunkStorageData chunks((root / "chunks").string());
        const bool chunkLoaded = chunks.loadChunkData(1, -2, chunk);
        const std::size_t crop = blockIndex(3, 5, 6);
        const bool extraExists =
            fs::exists(chunks.chunkPath(9, 9));
        return worldLoaded && chunkLoaded &&
               world.seed == 9000 + generation &&
               world.playerState.inventory.size() == 1 &&
               world.playerState.inventory.front().amount == generation + 2 &&
               world.actors.size() == 1 &&
               world.actors.front().dropAmount == generation + 1 &&
               chunk.metadata[crop] == generation % 8 &&
               chunk.blockEntities.size() == 1 &&
               chunk.blockEntities.front().payload ==
                   "{\"generation\":" + std::to_string(generation) + "}" &&
               extraExists == expectExtra;
    }

    std::string readFile(const fs::path &path)
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
    }

    void writeFile(const fs::path &path, const std::string &contents)
    {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
    }

    template <typename T>
    void writeValue(std::ostream &stream, const T &value)
    {
        stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
    }

    bool writeLegacyGeneration(const fs::path &root)
    {
        fs::create_directories(root / "chunks");
        writeFile(root / "world.meta",
                  "version 1\n"
                  "world_id legacy\n"
                  "world_name LegacyWorld\n"
                  "seed 123\n"
                  "spawn 1 2 3\n"
                  "world_time 4\n"
                  "generator ClassicOverWorld\n"
                  "player_present 0\n");

        const int x = 4;
        const int z = -3;
        const std::array<char, 8> magic{
            {'H', 'M', 'C', 'H', 'N', 'K', '1', '\0'}};
        const std::uint32_t version = 1;
        const std::int32_t storedX = x;
        const std::int32_t storedZ = z;
        const std::uint32_t chunkSize = CHUNK_SIZE;
        const std::uint32_t sectionCount = 1;
        std::vector<Block_t> blocks(
            CHUNK_VOLUME, static_cast<Block_t>(BlockId::Air));
        blocks[blockIndex(1, 5, 1)] =
            static_cast<Block_t>(BlockId::CoalOre);
        std::ofstream output(
            ChunkStorageData((root / "chunks").string()).chunkPath(x, z),
            std::ios::binary | std::ios::trunc);
        output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        writeValue(output, version);
        writeValue(output, storedX);
        writeValue(output, storedZ);
        writeValue(output, chunkSize);
        writeValue(output, sectionCount);
        output.write(reinterpret_cast<const char *>(blocks.data()),
                     static_cast<std::streamsize>(blocks.size() *
                                                  sizeof(Block_t)));
        return static_cast<bool>(output);
    }

    bool legacyMatches(const fs::path &root)
    {
        WorldSaveData world;
        StoredChunkData chunk;
        return WorldSave(root.string()).load(world) && world.version == 1 &&
               world.seed == 123 &&
               ChunkStorageData((root / "chunks").string())
                   .loadChunkData(4, -3, chunk) &&
               chunk.metadata[blockIndex(1, 5, 1)] == 0 &&
               chunk.blockIds[blockIndex(1, 5, 1)] ==
                   static_cast<Block_t>(BlockId::CoalOre);
    }
}

int main()
{
    TestSuite suite;
    RuntimeOperationTimings &operationTimings = runtimeOperationTimings();
    operationTimings.reset(true);

    {
        TemporaryDirectory root("complete");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupInfo created;
        WorldBackupMetrics createMetrics;
        const bool initialSaved = saveGeneration(root.path(), 1);
        const bool backedUp =
            backup.createBackup(&created, &createMetrics);
        suite.check(
            "K3/create-complete-backup",
            initialSaved && backedUp && created.fileCount == 2 &&
                created.totalBytes > 0 && createMetrics.candidateValidated &&
                createMetrics.published &&
                fs::is_regular_file(fs::path(created.directoryPath) /
                                    "manifest.hmb"),
            createMetrics.error);

        std::vector<WorldBackupInfo> listed;
        std::string listError;
        suite.check("K3/list-validates-manifest-and-payload",
                    backup.listBackups(listed, &listError) &&
                        listed.size() == 1 && listed.front().id == created.id &&
                        listed.front().totalBytes <=
                            testPolicy().maxTotalBytes,
                    listError);

        const bool newerSaved = saveGeneration(root.path(), 2, true);
        WorldBackupMetrics restoreMetrics;
        const bool restored =
            backup.restoreBackup(created.id, {}, &restoreMetrics);
        suite.check(
            "K3/restore-complete-gameplay-state",
            newerSaved && restored && generationMatches(root.path(), 1) &&
                restoreMetrics.candidateValidated &&
                restoreMetrics.published &&
                fs::is_directory(created.directoryPath),
            restoreMetrics.error);
        suite.check(
            "K3/restore-quarantines-prior-primary",
            readFile(fs::path(backup.recoveryQuarantineDirectory()) /
                     "world.meta")
                    .find("seed 9002") != std::string::npos &&
                fs::is_regular_file(
                    fs::path(backup.recoveryQuarantineDirectory()) /
                    "chunks/chunk_9_9.hmcchunk"),
            backup.recoveryQuarantineDirectory());
    }

    {
        TemporaryDirectory root("rotation");
        const WorldBackupPolicy policy = testPolicy();
        const WorldBackup backup(root.path().string(), policy);
        std::vector<std::string> ids;
        bool createdAll = true;
        for (int generation = 1; generation <= 4; ++generation) {
            WorldBackupInfo info;
            createdAll = saveGeneration(root.path(), generation) &&
                         backup.createBackup(&info) && createdAll;
            ids.push_back(info.id);
        }
        std::vector<WorldBackupInfo> listed;
        std::string error;
        const bool listedOk = backup.listBackups(listed, &error);
        const std::uintmax_t total = std::accumulate(
            listed.begin(), listed.end(), std::uintmax_t{0},
            [](std::uintmax_t value, const WorldBackupInfo &info) {
                return value + info.totalBytes;
            });
        suite.check(
            "K3/rotation-bounds-count-and-bytes",
            createdAll && listedOk && listed.size() == policy.maxBackups &&
                listed.front().id == ids[1] &&
                listed.back().id == ids[3] && total <= policy.maxTotalBytes &&
                !fs::exists(fs::path(backup.backupRootDirectory()) / ids[0]),
            error);
    }

    {
        TemporaryDirectory root("prepublish");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupMetrics interruptedMetrics;
        const bool saved = saveGeneration(root.path(), 1);
        const bool interrupted = backup.createBackup(
            nullptr, &interruptedMetrics,
            {WorldBackupFaultPoint::BeforeBackupPublish});
        std::vector<WorldBackupInfo> interruptedList;
        std::string listError;
        const bool listed =
            backup.listBackups(interruptedList, &listError);
        WorldBackupInfo retryInfo;
        const bool retried = backup.createBackup(&retryInfo);
        suite.check(
            "K3/validated-candidate-is-not-visible-before-publication",
            saved && !interrupted &&
                interruptedMetrics.candidateValidated &&
                !interruptedMetrics.published &&
                interruptedMetrics.error.find("before-backup-publish") !=
                    std::string::npos &&
                listed && interruptedList.empty() &&
                fs::is_directory(fs::path(backup.backupRootDirectory()) /
                                 ".failed") &&
                retried && retryInfo.id ==
                               "backup-00000000000000000001",
            interruptedMetrics.error.empty() ? listError
                                             : interruptedMetrics.error);
    }

    {
        TemporaryDirectory root("budget");
        WorldBackupPolicy policy = testPolicy();
        policy.maxTotalBytes = 64;
        const bool saved = saveGeneration(root.path(), 1);
        WorldBackupMetrics metrics;
        const bool backedUp =
            WorldBackup(root.path().string(), policy).createBackup(nullptr,
                                                                   &metrics);
        suite.check("K3/oversized-backup-is-rejected",
                    saved && !backedUp &&
                        metrics.error.find("limit") != std::string::npos,
                    metrics.error);
    }

    {
        TemporaryDirectory root("corrupt-backup");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupInfo info;
        const bool created = saveGeneration(root.path(), 1) &&
                             backup.createBackup(&info) &&
                             saveGeneration(root.path(), 2);
        writeFile(fs::path(info.directoryPath) /
                      "chunks/chunk_1_-2.hmcchunk",
                  "corrupt-backup");
        WorldBackupMetrics metrics;
        const bool restored = backup.restoreBackup(info.id, {}, &metrics);
        suite.check(
            "K3/corrupt-backup-is-quarantined",
            created && !restored &&
                metrics.error.find("backup validation failed") !=
                    std::string::npos &&
                !fs::exists(info.directoryPath) &&
                fs::is_directory(metrics.quarantinePath),
            metrics.error);
        suite.check("K3/corrupt-backup-preserves-primary",
                    generationMatches(root.path(), 2), metrics.error);
    }

    {
        TemporaryDirectory root("policy");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupInfo info;
        const bool created = saveGeneration(root.path(), 1) &&
                             backup.createBackup(&info);

        WorldBackupPolicy invalidPolicy = testPolicy();
        invalidPolicy.maxBackups = 0;
        WorldBackupMetrics invalidMetrics;
        const bool invalidRestored =
            WorldBackup(root.path().string(), invalidPolicy)
                .restoreBackup(info.id, {}, &invalidMetrics);

        WorldBackupPolicy restrictivePolicy = testPolicy();
        restrictivePolicy.maxTotalBytes = 64;
        WorldBackupMetrics restrictiveMetrics;
        const bool restrictiveRestored =
            WorldBackup(root.path().string(), restrictivePolicy)
                .restoreBackup(info.id, {}, &restrictiveMetrics);
        suite.check(
            "K3/policy-rejection-never-quarantines-valid-backup",
            created && !invalidRestored && !restrictiveRestored &&
                invalidMetrics.error.find("must be positive") !=
                    std::string::npos &&
                restrictiveMetrics.error.find("exceeds policy") !=
                    std::string::npos &&
                invalidMetrics.quarantinePath.empty() &&
                restrictiveMetrics.quarantinePath.empty() &&
                fs::is_directory(info.directoryPath) &&
                !fs::exists(fs::path(backup.backupRootDirectory()) /
                            ".corrupt.failed"),
            invalidMetrics.error + "; " + restrictiveMetrics.error);
    }

    {
        TemporaryDirectory root("corrupt-primary");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupInfo info;
        const bool created = saveGeneration(root.path(), 1) &&
                             backup.createBackup(&info);
        writeFile(root.path() / "world.meta", "");
        writeFile(root.path() / "chunks/chunk_1_-2.hmcchunk",
                  "corrupt-primary-chunk");
        WorldBackupMetrics metrics;
        const bool restored = backup.restoreBackup(info.id, {}, &metrics);
        suite.check("K3/corrupt-primary-restores-valid-backup",
                    created && restored && generationMatches(root.path(), 1),
                    metrics.error);
        suite.check(
            "K3/corrupt-primary-is-bounded-in-recovery-slot",
            fs::is_regular_file(
                fs::path(backup.recoveryQuarantineDirectory()) /
                "world.meta") &&
                readFile(fs::path(backup.recoveryQuarantineDirectory()) /
                         "world.meta")
                    .empty() &&
                readFile(fs::path(backup.recoveryQuarantineDirectory()) /
                         "chunks/chunk_1_-2.hmcchunk") ==
                    "corrupt-primary-chunk",
            backup.recoveryQuarantineDirectory());
    }

    {
        TemporaryDirectory root("interrupt-before");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupInfo info;
        const bool created = saveGeneration(root.path(), 1) &&
                             backup.createBackup(&info) &&
                             saveGeneration(root.path(), 2);
        WorldBackupMetrics metrics;
        const bool restored = backup.restoreBackup(
            info.id,
            {WorldBackupFaultPoint::BeforeRestoreValidation}, &metrics);
        suite.check(
            "K3/interrupted-restore-before-validation-preserves-primary",
            created && !restored && generationMatches(root.path(), 2) &&
                metrics.error.find("before-restore-validation") !=
                    std::string::npos &&
                !fs::exists(fs::path(backup.backupRootDirectory()) /
                            ".restore.pending") &&
                fs::is_directory(metrics.quarantinePath),
            metrics.error);
    }

    {
        TemporaryDirectory root("interrupt-publish");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupInfo info;
        const bool created = saveGeneration(root.path(), 1) &&
                             backup.createBackup(&info) &&
                             saveGeneration(root.path(), 2, true);
        WorldBackupMetrics metrics;
        const bool restored = backup.restoreBackup(
            info.id,
            {WorldBackupFaultPoint::AfterFirstRestorePublish}, &metrics);
        suite.check(
            "K3/interrupted-publish-rolls-back-primary",
            created && !restored && metrics.rolledBack &&
                generationMatches(root.path(), 2, true) &&
                metrics.error.find("after-first-restore-publish") !=
                    std::string::npos,
            metrics.error);
    }

    {
        TemporaryDirectory root("legacy");
        const WorldBackup backup(root.path().string(), testPolicy());
        WorldBackupInfo info;
        WorldBackupMetrics createMetrics;
        const bool legacyWritten = writeLegacyGeneration(root.path());
        const bool backedUp =
            backup.createBackup(&info, &createMetrics);
        suite.check("K3/version-one-generation-can-be-backed-up",
                    legacyWritten && backedUp &&
                        info.worldFormatVersion == 1,
                    createMetrics.error);

        const bool currentSaved = saveGeneration(root.path(), 3);
        WorldBackupMetrics restoreMetrics;
        const bool restored =
            backup.restoreBackup(info.id, {}, &restoreMetrics);
        suite.check("K3/version-one-generation-restores-and-loads",
                    currentSaved && restored && legacyMatches(root.path()),
                    restoreMetrics.error);
    }

    const std::vector<RuntimeOperationRecord> timingRecords =
        operationTimings.snapshot();
    const auto completeSuccess = [&](RuntimeOperationKind kind) {
        return std::find_if(
                   timingRecords.begin(), timingRecords.end(),
                   [kind](const RuntimeOperationRecord &record) {
                       return record.kind == kind && record.complete &&
                              record.success && record.phaseCount == 4 &&
                              record.filesWritten > 0 &&
                              record.bytesRead > 0 &&
                              record.bytesWritten > 0 &&
                              record.totalMilliseconds >=
                                  record.cumulativeMilliseconds[3] &&
                              record.totalMilliseconds >=
                                  record.mainThreadMaxStallMilliseconds;
                   }) != timingRecords.end();
    };
    const bool hasFailedBackupOrRestore = std::any_of(
        timingRecords.begin(), timingRecords.end(),
        [](const RuntimeOperationRecord &record) {
            return (record.kind == RuntimeOperationKind::Backup ||
                    record.kind == RuntimeOperationKind::Restore) &&
                   record.complete && !record.success &&
                   record.phaseCount == 4 &&
                   record.totalMilliseconds >=
                       record.mainThreadMaxStallMilliseconds;
        });
    suite.check("Q2/backup-success-emits-complete-timing",
                completeSuccess(RuntimeOperationKind::Backup));
    suite.check("Q2/restore-success-emits-complete-timing",
                completeSuccess(RuntimeOperationKind::Restore));
    suite.check("Q2/backup-restore-failure-emits-complete-timing",
                hasFailedBackupOrRestore);
    const char *operationSummaryOutput =
        std::getenv("HELLO_OPERATION_SUMMARY_OUT");
    if (operationSummaryOutput != nullptr &&
        operationSummaryOutput[0] != '\0') {
        std::ofstream output(operationSummaryOutput, std::ios::trunc);
        operationTimings.appendLatestSummary(output);
    }
    operationTimings.reset(false);

    return suite.finish();
}
