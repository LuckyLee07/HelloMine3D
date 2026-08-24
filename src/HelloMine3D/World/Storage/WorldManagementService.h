#ifndef WORLDMANAGEMENTSERVICE_H_INCLUDED
#define WORLDMANAGEMENTSERVICE_H_INCLUDED

#include "WorldBackup.h"
#include "WorldCatalogue.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class WorldManagementStatus {
    Success,
    InvalidArgument,
    NotFound,
    Conflict,
    CatalogueInvalid,
    StorageFailure
};

const char *worldManagementStatusName(WorldManagementStatus status) noexcept;

struct WorldManagementResult {
    WorldManagementStatus status = WorldManagementStatus::StorageFailure;
    std::string worldId;
    std::string directoryPath;
    std::string message;

    bool succeeded() const noexcept
    {
        return status == WorldManagementStatus::Success;
    }
};

struct WorldManagementListResult {
    WorldManagementStatus status = WorldManagementStatus::StorageFailure;
    std::vector<WorldCatalogueEntry> worlds;
    std::string message;

    bool succeeded() const noexcept
    {
        return status == WorldManagementStatus::Success;
    }
};

struct DeletedWorldInfo {
    WorldCatalogueEntry world;
    std::string recoveryId;
    std::string originalDirectoryName;
    std::int64_t deletedUtc = 0;
};

struct DeletedWorldListResult {
    WorldManagementStatus status = WorldManagementStatus::StorageFailure;
    std::vector<DeletedWorldInfo> worlds;
    std::string message;

    bool succeeded() const noexcept
    {
        return status == WorldManagementStatus::Success;
    }
};

struct WorldManagementPolicy {
    std::size_t maxRecoverableDeletes = 3;
};

/// Owns world-management commands above the read-only catalogue and the
/// transactional save/backup primitives. Callers receive structured outcomes
/// and never need to manipulate world directories directly.
class WorldManagementService {
  public:
    explicit WorldManagementService(
        std::string catalogueRoot, WorldManagementPolicy policy = {});

    static int suggestWorldSeed() noexcept;

    WorldManagementListResult listWorlds() const;
    DeletedWorldListResult listDeletedWorlds() const;

    WorldManagementResult createWorld(const std::string &displayName,
                                      int seed) const;
    WorldManagementResult prepareWorldForOpen(
        const std::string &worldId) const;
    WorldManagementResult renameWorld(const std::string &worldId,
                                      const std::string &displayName) const;
    WorldManagementResult deleteWorld(const std::string &worldId) const;
    WorldManagementResult restoreDeletedWorld(
        const std::string &worldId) const;
    WorldManagementResult permanentlyDeleteWorld(
        const std::string &worldId) const;

    bool listBackups(const std::string &worldId,
                     std::vector<WorldBackupInfo> &backups,
                     WorldManagementResult *result = nullptr) const;
    WorldManagementResult restoreBackup(
        const std::string &worldId, const std::string &backupId,
        const WorldBackupOptions &options = {}) const;

    const std::string &catalogueRoot() const noexcept;
    std::string recoveryRoot() const;

  private:
    std::string m_catalogueRoot;
    WorldManagementPolicy m_policy;
};

#endif // WORLDMANAGEMENTSERVICE_H_INCLUDED
