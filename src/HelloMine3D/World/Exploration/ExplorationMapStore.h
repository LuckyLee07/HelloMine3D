#ifndef EXPLORATIONMAPSTORE_H_INCLUDED
#define EXPLORATIONMAPSTORE_H_INCLUDED

#include "ExplorationAtlas.h"
#include "../Storage/StorageTransaction.h"

#include <cstdint>
#include <string>

// Versioned map sidecar. A damaged map is a presentation-data failure, never
// a reason to reject the authoritative world metadata or chunk storage.
class ExplorationMapStore {
  public:
    static constexpr std::uint32_t FormatVersion = 1;
    static constexpr std::size_t MaxFileBytes = 64u * 1024u * 1024u;

    struct Identity {
        std::string worldId;
        std::int32_t seed = 0;
        std::int32_t terrainGenerationVersion = 0;
    };

    enum class LoadStatus { Absent, Loaded, Corrupt, IdentityMismatch };

    explicit ExplorationMapStore(std::string worldDirectory);

    LoadStatus load(const Identity& expected, ExplorationAtlas& atlas,
                    std::string* error = nullptr) const;
    bool save(const Identity& identity, const ExplorationAtlas& atlas,
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
                          ExplorationAtlas& atlas, std::string& error);
    std::string m_worldDirectory;
};

#endif // EXPLORATIONMAPSTORE_H_INCLUDED
