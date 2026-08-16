#ifndef WORLDCATALOGUE_H_INCLUDED
#define WORLDCATALOGUE_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

inline constexpr std::int64_t LegacyWorldTimestampUtc = 946684800;

struct WorldCatalogueEntry {
    std::string id;
    std::string displayName;
    std::string directoryName;
    std::string directoryPath;
    std::string lastBuildIdentity;
    int seed = 0;
    int saveFormatVersion = 0;
    std::int64_t createdUtc = 0;
    std::int64_t lastPlayedUtc = 0;
    bool legacyMetadata = false;
};

class WorldCatalogueError : public std::runtime_error {
  public:
    explicit WorldCatalogueError(const std::string &message)
        : std::runtime_error(message)
    {
    }
};

/// Enumerates immediate world directories without creating, repairing or
/// rewriting anything. A malformed candidate invalidates the whole result so
/// callers never present a partial catalogue as authoritative.
class WorldCatalogue {
  public:
    static constexpr int CurrentSaveFormatVersion = 3;
    static constexpr std::size_t MaxMetadataBytes = 64 * 1024;
    static constexpr std::size_t MaxWorldIdBytes = 64;
    static constexpr std::size_t MaxDisplayNameCodepoints = 80;
    static constexpr std::size_t MaxBuildIdentityBytes = 80;

    static std::vector<WorldCatalogueEntry>
    enumerate(const std::string &catalogueRoot);

    static bool isValidWorldId(const std::string &value) noexcept;
    static bool isValidDisplayName(const std::string &value) noexcept;
    static bool isValidBuildIdentity(const std::string &value) noexcept;
    static bool isValidTimestamps(std::int64_t createdUtc,
                                  std::int64_t lastPlayedUtc) noexcept;
};

#endif // WORLDCATALOGUE_H_INCLUDED
