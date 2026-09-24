#ifndef EXPLORATIONMAPSTORE_H_INCLUDED
#define EXPLORATIONMAPSTORE_H_INCLUDED

#include "ExplorationAtlas.h"
#include "ExplorationMarkers.h"
#include "../Storage/StorageTransaction.h"

#include <cstdint>
#include <optional>
#include <string>

// Versioned map sidecar. A damaged map is a presentation-data failure, never
// a reason to reject the authoritative world metadata or chunk storage.
class ExplorationMapStore {
  public:
    static constexpr std::uint32_t FormatVersion = 4;
    static constexpr std::size_t MaxFileBytes = 64u * 1024u * 1024u;

    struct Identity {
        std::string worldId;
        std::int32_t seed = 0;
        std::int32_t terrainGenerationVersion = 0;
    };

    struct KnownSite {
        int worldX = 0;
        int worldY = 0;
        int worldZ = 0;
        bool operator==(const KnownSite& other) const noexcept
        {
            return worldX == other.worldX && worldY == other.worldY &&
                   worldZ == other.worldZ;
        }
        bool operator!=(const KnownSite& other) const noexcept
        {
            return !(*this == other);
        }
    };

    enum class LoadStatus { Absent, Loaded, Corrupt, IdentityMismatch };

    explicit ExplorationMapStore(std::string worldDirectory);

    LoadStatus load(const Identity& expected, ExplorationAtlas& atlas,
                    std::string* error = nullptr) const;
    LoadStatus load(const Identity& expected, ExplorationAtlas& atlas,
                    ExplorationMarkers& markers,
                    std::string* error = nullptr) const;
    LoadStatus load(const Identity& expected, ExplorationAtlas& atlas,
                    ExplorationMarkers& markers,
                    std::optional<KnownSite>& knownWaystone,
                    std::string* error = nullptr) const;
    LoadStatus load(const Identity& expected, ExplorationAtlas& atlas,
                    ExplorationMarkers& markers,
                    std::optional<KnownSite>& knownWaystone,
                    std::optional<KnownSite>& boundWaystone,
                    std::string* error = nullptr) const;
    bool save(const Identity& identity, const ExplorationAtlas& atlas,
              const ExplorationMarkers& markers,
              const std::optional<KnownSite>& knownWaystone,
              const std::optional<KnownSite>& boundWaystone,
              const StorageTransactionOptions& options = {},
              StorageTransactionMetrics* metrics = nullptr) const;
    // Keep a damaged or foreign primary for inspection before a new map is
    // published. Never overwrite an existing quarantine slot or a symlink.
    bool quarantineInvalid(const Identity& expected,
                           std::string* error = nullptr) const;

    static bool validateFile(const std::string& path,
                             std::string* error = nullptr);
    std::string filePath() const;

  private:
    static bool parseFile(const std::string& path, Identity& identity,
                          ExplorationAtlas& atlas,
                          ExplorationMarkers& markers,
                          std::optional<KnownSite>& knownWaystone,
                          std::optional<KnownSite>& boundWaystone,
                          std::string& error);
    static bool validateMarkers(const ExplorationAtlas& atlas,
                                const ExplorationMarkers& markers,
                                std::string& error);
    std::string m_worldDirectory;
};

#endif // EXPLORATIONMAPSTORE_H_INCLUDED
