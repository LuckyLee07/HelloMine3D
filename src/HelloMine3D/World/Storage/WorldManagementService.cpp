#include "WorldManagementService.h"

#include "WorldSave.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>
#include <utility>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char *RecoveryManifestName = "recovery.meta";

    std::int64_t currentUtcSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    std::string createWorldId()
    {
        static std::atomic<std::uint64_t> sequence{0};
        const std::uint64_t high =
            static_cast<std::uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count()) ^
            (++sequence * 0x9e3779b97f4a7c15ULL);
        std::uint64_t low = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        try {
            std::random_device source;
            low ^= (static_cast<std::uint64_t>(source()) << 32u) ^ source();
        }
        catch (...) {
            low ^= high * 0xbf58476d1ce4e5b9ULL;
        }
        std::ostringstream output;
        output << "world-" << std::hex << std::setfill('0')
               << std::setw(16) << high << std::setw(16) << low;
        return output.str();
    }

    bool validDirectoryName(const std::string &value)
    {
        if (value.empty() || value == "." || value == "..") {
            return false;
        }
        const fs::path path(value);
        return !path.is_absolute() && path.filename() == path &&
               value.find('/') == std::string::npos &&
               value.find('\\') == std::string::npos;
    }

    WorldManagementResult result(WorldManagementStatus status,
                                 std::string message,
                                 const std::string &worldId = {},
                                 const std::string &directoryPath = {})
    {
        WorldManagementResult value;
        value.status = status;
        value.worldId = worldId;
        value.directoryPath = directoryPath;
        value.message = std::move(message);
        return value;
    }

    bool findWorld(const std::vector<WorldCatalogueEntry> &worlds,
                   const std::string &worldId, WorldCatalogueEntry &entry)
    {
        const auto found = std::find_if(
            worlds.begin(), worlds.end(), [&](const WorldCatalogueEntry &item) {
                return item.id == worldId;
            });
        if (found == worlds.end()) {
            return false;
        }
        entry = *found;
        return true;
    }

    WorldManagementResult loadMutableWorld(
        const WorldCatalogueEntry &entry, WorldSaveData &data)
    {
        std::string error;
        if (!WorldSave::loadFromPath(
                (fs::path(entry.directoryPath) / "world.meta").string(),
                data, &error)) {
            return result(WorldManagementStatus::CatalogueInvalid,
                          "World metadata cannot be loaded: " + error,
                          entry.id, entry.directoryPath);
        }
        if (data.version < WorldSaveFormatVersion) {
            if (data.version < 5) {
                data.objectiveState.definitionVersion =
                    ObjectiveSaveState::CurrentDefinitionVersion;
                data.objectiveState.completedIds =
                    ObjectiveState::completedFromLegacyFlags(
                        data.alphaJourneyFlags);
                data.objectiveState.progress.clear();
            }
            if (data.version < 10) {
                data.difficultyProfileVersion =
                    CurrentDifficultyProfileVersion;
                data.difficulty = WorldDifficulty::Normal;
            }
            if (data.version < 11) {
                data.postVictoryEventVersion =
                    PostVictoryEvents::CurrentVersion;
                data.completedPostVictoryEvents = 0;
            }
            if (data.version < 12) {
                data.explorationRewardVersion =
                    ExplorationRewards::LegacyVersion;
            }
            data.version = WorldSaveFormatVersion;
            data.worldId = entry.id;
            data.worldName = entry.displayName;
            data.createdUtc = entry.createdUtc;
            data.lastPlayedUtc = entry.lastPlayedUtc;
            data.lastBuildIdentity = "development";
        }
        return result(WorldManagementStatus::Success, "ok", entry.id,
                      entry.directoryPath);
    }

    bool writeRecoveryManifest(const fs::path &directory,
                               const std::string &originalDirectory,
                               std::int64_t deletedUtc,
                               std::string &error)
    {
        std::ofstream output(directory / RecoveryManifestName,
                             std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Cannot create recovery manifest.";
            return false;
        }
        output << "version 1\n"
               << "original_directory " << std::quoted(originalDirectory)
               << "\n"
               << "deleted_utc " << deletedUtc << "\n";
        output.flush();
        if (!output) {
            error = "Cannot write recovery manifest.";
            return false;
        }
        return true;
    }

    bool readRecoveryManifest(const fs::path &directory,
                              std::string &originalDirectory,
                              std::int64_t &deletedUtc, std::string &error)
    {
        std::ifstream input(directory / RecoveryManifestName,
                            std::ios::binary);
        int version = 0;
        std::string key;
        if (!(input >> key >> version) || key != "version" || version != 1 ||
            !(input >> key) || key != "original_directory" ||
            !(input >> std::quoted(originalDirectory)) ||
            !(input >> key >> deletedUtc) || key != "deleted_utc") {
            error = "Recovery manifest is invalid.";
            return false;
        }
        input >> std::ws;
        if (!input.eof() || !validDirectoryName(originalDirectory) ||
            deletedUtc < LegacyWorldTimestampUtc) {
            error = "Recovery manifest contains invalid values.";
            return false;
        }
        return true;
    }

    std::string recoveryId(std::int64_t timestamp)
    {
        static std::atomic<std::uint64_t> sequence{0};
        std::ostringstream output;
        output << "deleted-" << std::setfill('0') << std::setw(20)
               << timestamp << '-' << std::setw(6) << ++sequence;
        return output.str();
    }

    WorldSaveData initialSave(const std::string &worldId,
                              const std::string &displayName, int seed,
                              WorldDifficulty difficulty)
    {
        WorldSaveData data;
        data.worldId = worldId;
        data.worldName = displayName;
        data.seed = seed;
        data.createdUtc = currentUtcSeconds();
        data.lastPlayedUtc = data.createdUtc;
        data.lastBuildIdentity = "development";
        data.difficultyProfileVersion = CurrentDifficultyProfileVersion;
        data.difficulty = difficulty;
        data.spawnPoint = initialWorldSpawnPlaceholder();
        data.playerState.position = data.spawnPoint;
        return data;
    }
}

int WorldManagementService::suggestWorldSeed() noexcept
{
    static std::atomic<std::uint64_t> sequence{0};
    std::uint64_t value = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    value ^= ++sequence * 0x9e3779b97f4a7c15ULL;
    try {
        std::random_device source;
        value ^= (static_cast<std::uint64_t>(source()) << 32u) ^ source();
    }
    catch (...) {
        value ^= value >> 29u;
    }
    value ^= value >> 30u;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27u;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31u;
    constexpr int MinimumSuggestedSeed = 1;
    constexpr int MaximumSuggestedSeed = 2000000000;
    return MinimumSuggestedSeed + static_cast<int>(
        value % static_cast<std::uint64_t>(MaximumSuggestedSeed));
}

const char *worldManagementStatusName(WorldManagementStatus status) noexcept
{
    switch (status) {
    case WorldManagementStatus::Success:
        return "success";
    case WorldManagementStatus::InvalidArgument:
        return "invalid-argument";
    case WorldManagementStatus::NotFound:
        return "not-found";
    case WorldManagementStatus::Conflict:
        return "conflict";
    case WorldManagementStatus::CatalogueInvalid:
        return "catalogue-invalid";
    case WorldManagementStatus::StorageFailure:
        return "storage-failure";
    }
    return "unknown";
}

WorldManagementService::WorldManagementService(
    std::string catalogueRoot, WorldManagementPolicy policy)
    : m_catalogueRoot(std::move(catalogueRoot)), m_policy(policy)
{
}

WorldManagementListResult WorldManagementService::listWorlds() const
{
    WorldManagementListResult listed;
    if (m_catalogueRoot.empty()) {
        listed.status = WorldManagementStatus::InvalidArgument;
        listed.message = "World catalogue root must not be empty.";
        return listed;
    }
    try {
        listed.worlds = WorldCatalogue::enumerate(m_catalogueRoot);
        listed.status = WorldManagementStatus::Success;
        listed.message = "ok";
    }
    catch (const WorldCatalogueError &error) {
        listed.status = WorldManagementStatus::CatalogueInvalid;
        listed.message = error.what();
    }
    catch (const std::exception &error) {
        listed.status = WorldManagementStatus::StorageFailure;
        listed.message = error.what();
    }
    return listed;
}

DeletedWorldListResult WorldManagementService::listDeletedWorlds() const
{
    DeletedWorldListResult listed;
    const fs::path root(recoveryRoot());
    std::error_code error;
    if (!fs::exists(root, error)) {
        if (error) {
            listed.status = WorldManagementStatus::StorageFailure;
            listed.message = "Cannot inspect recovery root: " +
                             error.message();
            return listed;
        }
        listed.status = WorldManagementStatus::Success;
        listed.message = "ok";
        return listed;
    }
    try {
        const std::vector<WorldCatalogueEntry> worlds =
            WorldCatalogue::enumerate(root.string());
        for (const WorldCatalogueEntry &world : worlds) {
            DeletedWorldInfo deleted;
            deleted.world = world;
            deleted.recoveryId = world.directoryName;
            if (!readRecoveryManifest(world.directoryPath,
                                      deleted.originalDirectoryName,
                                      deleted.deletedUtc, listed.message)) {
                listed.status = WorldManagementStatus::CatalogueInvalid;
                listed.worlds.clear();
                return listed;
            }
            listed.worlds.push_back(std::move(deleted));
        }
        std::sort(listed.worlds.begin(), listed.worlds.end(),
                  [](const DeletedWorldInfo &left,
                     const DeletedWorldInfo &right) {
                      if (left.deletedUtc != right.deletedUtc) {
                          return left.deletedUtc > right.deletedUtc;
                      }
                      return left.recoveryId > right.recoveryId;
                  });
        listed.status = WorldManagementStatus::Success;
        listed.message = "ok";
    }
    catch (const WorldCatalogueError &catalogueError) {
        listed.status = WorldManagementStatus::CatalogueInvalid;
        listed.message = catalogueError.what();
    }
    catch (const std::exception &exception) {
        listed.status = WorldManagementStatus::StorageFailure;
        listed.message = exception.what();
    }
    return listed;
}

WorldManagementResult WorldManagementService::createWorld(
    const std::string &displayName, int seed,
    WorldDifficulty difficulty) const
{
    if (!WorldCatalogue::isValidDisplayName(displayName)) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World display name is invalid.");
    }
    if (!validWorldDifficulty(difficulty)) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World difficulty is invalid.");
    }
    const WorldManagementListResult existing = listWorlds();
    if (!existing.succeeded()) {
        return result(existing.status, existing.message);
    }

    std::error_code error;
    fs::create_directories(m_catalogueRoot, error);
    if (error) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot create catalogue root: " + error.message());
    }

    std::string id;
    fs::path directory;
    for (int attempt = 0; attempt < 16; ++attempt) {
        id = createWorldId();
        directory = fs::path(m_catalogueRoot) / id;
        if (!fs::exists(directory, error) && !error) {
            break;
        }
        if (error) {
            return result(WorldManagementStatus::StorageFailure,
                          "Cannot inspect new world path: " +
                              error.message());
        }
        id.clear();
    }
    if (id.empty() || !WorldCatalogue::isValidWorldId(id)) {
        return result(WorldManagementStatus::Conflict,
                      "Cannot allocate a unique world id.");
    }
    if (!fs::create_directory(directory, error) || error) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot create world directory: " + error.message(),
                      id, directory.string());
    }
    const WorldSaveData data = initialSave(id, displayName, seed,
                                           difficulty);
    if (!WorldSave(directory.string()).save(data)) {
        std::error_code cleanupError;
        fs::remove_all(directory, cleanupError);
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot publish initial world metadata.", id,
                      directory.string());
    }
    return result(WorldManagementStatus::Success, "World created.", id,
                  directory.string());
}

WorldManagementResult WorldManagementService::prepareWorldForOpen(
    const std::string &worldId) const
{
    if (!WorldCatalogue::isValidWorldId(worldId)) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World id is invalid.", worldId);
    }
    const WorldManagementListResult listed = listWorlds();
    if (!listed.succeeded()) {
        return result(listed.status, listed.message, worldId);
    }
    WorldCatalogueEntry entry;
    if (!findWorld(listed.worlds, worldId, entry)) {
        return result(WorldManagementStatus::NotFound,
                      "World was not found.", worldId);
    }
    WorldSaveData data;
    WorldManagementResult loaded = loadMutableWorld(entry, data);
    if (!loaded.succeeded()) {
        return loaded;
    }
    data.lastPlayedUtc = std::max(data.createdUtc, currentUtcSeconds());
    data.lastBuildIdentity = "development";
    if (!WorldSave(entry.directoryPath).save(data)) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot update last-played metadata.", entry.id,
                      entry.directoryPath);
    }
    return result(WorldManagementStatus::Success, "World ready.", entry.id,
                  entry.directoryPath);
}

WorldManagementResult WorldManagementService::renameWorld(
    const std::string &worldId, const std::string &displayName) const
{
    if (!WorldCatalogue::isValidWorldId(worldId) ||
        !WorldCatalogue::isValidDisplayName(displayName)) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World id or display name is invalid.", worldId);
    }
    const WorldManagementListResult listed = listWorlds();
    if (!listed.succeeded()) {
        return result(listed.status, listed.message, worldId);
    }
    WorldCatalogueEntry entry;
    if (!findWorld(listed.worlds, worldId, entry)) {
        return result(WorldManagementStatus::NotFound,
                      "World was not found.", worldId);
    }
    WorldSaveData data;
    WorldManagementResult loaded = loadMutableWorld(entry, data);
    if (!loaded.succeeded()) {
        return loaded;
    }
    data.worldName = displayName;
    data.lastBuildIdentity = "development";
    if (!WorldSave(entry.directoryPath).save(data)) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot publish renamed world metadata.", entry.id,
                      entry.directoryPath);
    }
    return result(WorldManagementStatus::Success, "World renamed.", entry.id,
                  entry.directoryPath);
}

WorldManagementResult WorldManagementService::deleteWorld(
    const std::string &worldId) const
{
    if (!WorldCatalogue::isValidWorldId(worldId)) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World id is invalid.", worldId);
    }
    const WorldManagementListResult listed = listWorlds();
    if (!listed.succeeded()) {
        return result(listed.status, listed.message, worldId);
    }
    WorldCatalogueEntry entry;
    if (!findWorld(listed.worlds, worldId, entry)) {
        return result(WorldManagementStatus::NotFound,
                      "World was not found.", worldId);
    }
    if (m_policy.maxRecoverableDeletes == 0) {
        return result(WorldManagementStatus::StorageFailure,
                      "Recoverable-delete capacity is disabled.", worldId);
    }

    const DeletedWorldListResult deletedBefore = listDeletedWorlds();
    if (!deletedBefore.succeeded()) {
        return result(deletedBefore.status, deletedBefore.message, worldId);
    }
    std::error_code error;
    fs::create_directories(recoveryRoot(), error);
    if (error) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot create recovery root: " + error.message(),
                      worldId);
    }
    for (const DeletedWorldInfo &deleted : deletedBefore.worlds) {
        if (deleted.world.id == worldId) {
            return result(WorldManagementStatus::Conflict,
                          "A recoverable world already owns this id.",
                          worldId, deleted.world.directoryPath);
        }
    }

    const std::int64_t deletedUtc = currentUtcSeconds();
    const std::string id = recoveryId(deletedUtc);
    const fs::path target = fs::path(recoveryRoot()) / id;
    fs::rename(entry.directoryPath, target, error);
    if (error) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot move world into recovery: " + error.message(),
                      worldId, entry.directoryPath);
    }
    std::string manifestError;
    if (!writeRecoveryManifest(target, entry.directoryName, deletedUtc,
                               manifestError)) {
        std::error_code rollbackError;
        fs::rename(target, entry.directoryPath, rollbackError);
        return result(WorldManagementStatus::StorageFailure,
                      manifestError +
                          (rollbackError
                               ? " Recovery rollback also failed: " +
                                     rollbackError.message()
                               : std::string()),
                      worldId, entry.directoryPath);
    }

    DeletedWorldListResult deletedAfter = listDeletedWorlds();
    if (!deletedAfter.succeeded()) {
        return result(deletedAfter.status, deletedAfter.message, worldId,
                      target.string());
    }
    while (deletedAfter.worlds.size() > m_policy.maxRecoverableDeletes) {
        const DeletedWorldInfo oldest = deletedAfter.worlds.back();
        fs::remove_all(oldest.world.directoryPath, error);
        if (error) {
            return result(WorldManagementStatus::StorageFailure,
                          "Cannot enforce recoverable-delete bound: " +
                              error.message(),
                          worldId, target.string());
        }
        deletedAfter.worlds.pop_back();
    }
    return result(WorldManagementStatus::Success,
                  "World moved to recoverable storage.", worldId,
                  target.string());
}

WorldManagementResult WorldManagementService::restoreDeletedWorld(
    const std::string &worldId) const
{
    if (!WorldCatalogue::isValidWorldId(worldId)) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World id is invalid.", worldId);
    }
    const WorldManagementListResult active = listWorlds();
    if (!active.succeeded()) {
        return result(active.status, active.message, worldId);
    }
    WorldCatalogueEntry ignored;
    if (findWorld(active.worlds, worldId, ignored)) {
        return result(WorldManagementStatus::Conflict,
                      "An active world already owns this id.", worldId);
    }
    const DeletedWorldListResult deleted = listDeletedWorlds();
    if (!deleted.succeeded()) {
        return result(deleted.status, deleted.message, worldId);
    }
    const auto found = std::find_if(
        deleted.worlds.begin(), deleted.worlds.end(),
        [&](const DeletedWorldInfo &item) { return item.world.id == worldId; });
    if (found == deleted.worlds.end()) {
        return result(WorldManagementStatus::NotFound,
                      "Recoverable world was not found.", worldId);
    }
    const fs::path target =
        fs::path(m_catalogueRoot) / found->originalDirectoryName;
    std::error_code error;
    if (fs::exists(target, error) || error) {
        return result(WorldManagementStatus::Conflict,
                      "Original world directory is not available.", worldId,
                      target.string());
    }
    fs::remove(fs::path(found->world.directoryPath) / RecoveryManifestName,
               error);
    if (error) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot remove recovery manifest: " + error.message(),
                      worldId, found->world.directoryPath);
    }
    fs::rename(found->world.directoryPath, target, error);
    if (error) {
        std::string manifestError;
        writeRecoveryManifest(found->world.directoryPath,
                              found->originalDirectoryName,
                              found->deletedUtc, manifestError);
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot restore world directory: " + error.message(),
                      worldId, target.string());
    }
    return result(WorldManagementStatus::Success, "World restored.", worldId,
                  target.string());
}

WorldManagementResult WorldManagementService::permanentlyDeleteWorld(
    const std::string &worldId) const
{
    if (!WorldCatalogue::isValidWorldId(worldId)) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World id is invalid.", worldId);
    }
    const DeletedWorldListResult deleted = listDeletedWorlds();
    if (!deleted.succeeded()) {
        return result(deleted.status, deleted.message, worldId);
    }
    const auto found = std::find_if(
        deleted.worlds.begin(), deleted.worlds.end(),
        [&](const DeletedWorldInfo &item) { return item.world.id == worldId; });
    if (found == deleted.worlds.end()) {
        return result(WorldManagementStatus::NotFound,
                      "Recoverable world was not found.", worldId);
    }
    std::error_code error;
    fs::remove_all(found->world.directoryPath, error);
    if (error) {
        return result(WorldManagementStatus::StorageFailure,
                      "Cannot permanently remove recovered world: " +
                          error.message(),
                      worldId, found->world.directoryPath);
    }
    return result(WorldManagementStatus::Success,
                  "Recoverable world permanently removed.", worldId);
}

bool WorldManagementService::listBackups(
    const std::string &worldId, std::vector<WorldBackupInfo> &backups,
    WorldManagementResult *resultOutput) const
{
    backups.clear();
    const WorldManagementListResult listed = listWorlds();
    if (!listed.succeeded()) {
        if (resultOutput != nullptr) {
            *resultOutput = result(listed.status, listed.message, worldId);
        }
        return false;
    }
    WorldCatalogueEntry entry;
    if (!findWorld(listed.worlds, worldId, entry)) {
        if (resultOutput != nullptr) {
            *resultOutput = result(WorldManagementStatus::NotFound,
                                   "World was not found.", worldId);
        }
        return false;
    }
    std::string error;
    if (!WorldBackup(entry.directoryPath).listBackups(backups, &error)) {
        if (resultOutput != nullptr) {
            *resultOutput = result(WorldManagementStatus::StorageFailure,
                                   error, worldId, entry.directoryPath);
        }
        return false;
    }
    if (resultOutput != nullptr) {
        *resultOutput = result(WorldManagementStatus::Success, "ok", worldId,
                               entry.directoryPath);
    }
    return true;
}

WorldManagementResult WorldManagementService::restoreBackup(
    const std::string &worldId, const std::string &backupId,
    const WorldBackupOptions &options) const
{
    if (!WorldCatalogue::isValidWorldId(worldId) || backupId.empty()) {
        return result(WorldManagementStatus::InvalidArgument,
                      "World id or backup id is invalid.", worldId);
    }
    const WorldManagementListResult listed = listWorlds();
    if (!listed.succeeded()) {
        return result(listed.status, listed.message, worldId);
    }
    WorldCatalogueEntry entry;
    if (!findWorld(listed.worlds, worldId, entry)) {
        return result(WorldManagementStatus::NotFound,
                      "World was not found.", worldId);
    }
    WorldBackupMetrics metrics;
    if (!WorldBackup(entry.directoryPath)
             .restoreBackup(backupId, options, &metrics)) {
        return result(WorldManagementStatus::StorageFailure,
                      metrics.error.empty() ? "Backup restore failed."
                                            : metrics.error,
                      worldId, entry.directoryPath);
    }
    return result(WorldManagementStatus::Success, "Backup restored.", worldId,
                  entry.directoryPath);
}

const std::string &WorldManagementService::catalogueRoot() const noexcept
{
    return m_catalogueRoot;
}

std::string WorldManagementService::recoveryRoot() const
{
    return m_catalogueRoot + ".recovery";
}
