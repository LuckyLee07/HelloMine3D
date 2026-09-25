#ifndef WORLDPREVIEWSTORE_H_INCLUDED
#define WORLDPREVIEWSTORE_H_INCLUDED

#include "ExplorationAtlas.h"
#include "ExplorationMapStore.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Disposable, bounded menu cache derived exclusively from explored surfaces.
// Failure to load or save this sidecar never makes the world itself invalid.
class WorldPreviewStore {
  public:
    static constexpr std::uint32_t FormatVersion = 1;
    static constexpr int Width = 49;
    static constexpr int Height = 25;
    static constexpr int MetresPerPixel = 8;
    static constexpr std::size_t CellCount =
        static_cast<std::size_t>(Width * Height);
    static constexpr std::size_t MaxFileBytes = 4096;

    struct Identity {
        std::string worldId;
        std::int32_t seed = 0;
        std::int32_t terrainGenerationVersion = 0;
        std::int64_t lastPlayedUtc = 0;
    };

    struct Cell {
        bool known = false;
        std::uint8_t height = 0;
        BlockId material = BlockId::Air;
    };

    struct Preview {
        std::int32_t centerX = 0;
        std::int32_t centerZ = 0;
        float playerYaw = 0.f;
        int width = Width;
        int height = Height;
        int metresPerCell = MetresPerPixel;
        std::vector<Cell> cells;

        const Cell& at(int column, int row) const;
        std::size_t knownCellCount() const noexcept;
        std::int64_t worldXAt(int column) const noexcept;
        std::int64_t worldZAt(int row) const noexcept;
    };

    enum class LoadStatus {
        Absent,
        Loaded,
        Corrupt,
        IdentityMismatch,
        StaleSource,
        UnsafePath,
        Unreadable
    };

    explicit WorldPreviewStore(std::string worldDirectory);

    // Internally compares the cache token with the cheap source revision from
    // exploration.hmap; the full exploration atlas is never loaded here.
    LoadStatus load(const Identity& expected, Preview& preview,
                    std::string* error = nullptr) const;

    // Samples a 49x49 real atlas overview and stores its central 49x25 rows.
    // Existing corrupt regular cache files may be replaced; unsafe paths may
    // not. Callers treat false as a soft cache failure.
    bool save(
        const Identity& identity, int centerX, int centerZ, float playerYaw,
        const ExplorationAtlas& atlas,
        const StorageTransactionOptions& options = {},
        StorageTransactionMetrics* metrics = nullptr) const;

    static bool validateFile(const std::string& path,
                             std::string* error = nullptr);
    std::string filePath() const;

  private:
    static bool parseFile(
        const std::string& path, Identity& identity,
        ExplorationMapStore::SourceRevision& sourceRevision,
        Preview& preview, std::string& error);

    std::string m_worldDirectory;
};

#endif // WORLDPREVIEWSTORE_H_INCLUDED
