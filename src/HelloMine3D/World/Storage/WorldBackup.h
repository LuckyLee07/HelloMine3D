#ifndef WORLDBACKUP_H_INCLUDED
#define WORLDBACKUP_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct WorldBackupPolicy {
    std::size_t maxBackups = 3;
    std::uintmax_t maxTotalBytes = 512ull * 1024ull * 1024ull;
    std::size_t maxFiles = 4097;
    std::uintmax_t maxFileBytes = 128ull * 1024ull * 1024ull;
};

enum class WorldBackupFaultPoint {
    None,
    BeforeBackupPublish,
    BeforeRestoreValidation,
    BeforeRestorePublish,
    AfterFirstRestorePublish
};

const char *worldBackupFaultPointName(WorldBackupFaultPoint point) noexcept;

struct WorldBackupOptions {
    WorldBackupFaultPoint faultPoint = WorldBackupFaultPoint::None;
};

struct WorldBackupInfo {
    std::string id;
    std::string directoryPath;
    std::uint64_t sequence = 0;
    std::int64_t createdUtc = 0;
    int worldFormatVersion = 0;
    std::size_t fileCount = 0;
    std::uintmax_t totalBytes = 0;
};

struct WorldBackupMetrics {
    double totalMilliseconds = 0.0;
    std::size_t filesCopied = 0;
    std::uintmax_t bytesRead = 0;
    std::uintmax_t bytesCopied = 0;
    bool candidateValidated = false;
    bool published = false;
    bool rolledBack = false;
    std::string quarantinePath;
    std::string error;
};

/// Owns bounded snapshots below one world directory. Backup candidates and
/// restore candidates are fully copied and validated before publication.
class WorldBackup {
  public:
    explicit WorldBackup(std::string worldDirectory,
                         WorldBackupPolicy policy = {});

    bool createBackup(WorldBackupInfo *created = nullptr,
                      WorldBackupMetrics *metrics = nullptr,
                      const WorldBackupOptions &options = {}) const;
    bool listBackups(std::vector<WorldBackupInfo> &backups,
                     std::string *errorMessage = nullptr) const;
    bool restoreBackup(const std::string &backupId,
                       const WorldBackupOptions &options = {},
                       WorldBackupMetrics *metrics = nullptr) const;

    std::string backupRootDirectory() const;
    std::string recoveryQuarantineDirectory() const;

  private:
    std::string m_worldDirectory;
    WorldBackupPolicy m_policy;
};

#endif // WORLDBACKUP_H_INCLUDED
