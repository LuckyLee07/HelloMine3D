#ifndef EXPLORATIONATLAS_H_INCLUDED
#define EXPLORATIONATLAS_H_INCLUDED

#include "../Block/BlockId.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>

// An in-memory archive of observed, resident surface columns. It never asks
// the terrain generator or chunk manager for data. Persistence and navigation
// are separate layers so unknown space cannot be filled by prediction.
class ExplorationAtlas {
  public:
    static constexpr int MetresPerCell = 4;
    static constexpr int CellsPerTile = 32;
    static constexpr std::size_t MaxTiles = 4096;
    static constexpr std::size_t MaxResidentBytes = 16u * 1024u * 1024u;

    struct Observation {
        int worldX = 0;
        int worldZ = 0;
        int height = 0;
        BlockId material = BlockId::Air;
        bool known = false;
    };

    struct Surface {
        std::uint8_t height = 0;
        BlockId material = BlockId::Air;
    };

    enum class ObserveResult { Unchanged, Updated, Full, Invalid };

    ObserveResult observe(const Observation& observation)
    {
        if (!observation.known) {
            return ObserveResult::Unchanged;
        }
        if (observation.height < 0 || observation.height > 255 ||
            observation.material >= BlockId::NUM_TYPES) {
            return ObserveResult::Invalid;
        }
        const Address address = locate(observation.worldX,
                                       observation.worldZ);
        const auto key = std::make_pair(address.tileX, address.tileZ);
        auto found = m_tiles.find(key);
        if (found == m_tiles.end()) {
            if (m_tiles.size() == MaxTiles) {
                return ObserveResult::Full;
            }
            found = m_tiles.try_emplace(key).first;
        }
        Cell& cell = found->second.cells[address.index];
        const auto height = static_cast<std::uint8_t>(observation.height);
        const auto material = static_cast<std::uint8_t>(
            observation.material);
        if (cell.known != 0 && cell.height == height &&
            cell.material == material) {
            return ObserveResult::Unchanged;
        }
        if (cell.known == 0) {
            ++m_knownCells;
        }
        cell = {1, height, material};
        return ObserveResult::Updated;
    }

    std::optional<Surface> surfaceAt(int worldX, int worldZ) const
    {
        const Address address = locate(worldX, worldZ);
        const auto found = m_tiles.find(
            std::make_pair(address.tileX, address.tileZ));
        if (found == m_tiles.end()) {
            return std::nullopt;
        }
        const Cell& cell = found->second.cells[address.index];
        if (cell.known == 0) {
            return std::nullopt;
        }
        return Surface{cell.height, static_cast<BlockId>(cell.material)};
    }

    void clear()
    {
        m_tiles.clear();
        m_knownCells = 0;
    }

    std::size_t tileCount() const noexcept { return m_tiles.size(); }
    std::size_t knownCellCount() const noexcept { return m_knownCells; }
    std::size_t residentBytesUpperBound() const noexcept
    {
        return m_tiles.size() * (sizeof(Tile) + 64);
    }

  private:
    friend class ExplorationMapStore;

    struct Cell {
        std::uint8_t known = 0;
        std::uint8_t height = 0;
        std::uint8_t material = 0;
    };
    static_assert(sizeof(Cell) == 3, "Atlas cell budget changed");

    struct Tile {
        std::array<Cell, CellsPerTile * CellsPerTile> cells{};
    };
    // A std::map node needs less than 64 additional bytes on supported
    // builds. This conservative bound remains below the frozen 16 MiB cap.
    static_assert(MaxTiles * (sizeof(Tile) + 64) < MaxResidentBytes,
                  "Atlas resident budget changed");

    struct Address {
        int tileX = 0;
        int tileZ = 0;
        std::size_t index = 0;
    };

    static int floorDiv(int value, int divisor) noexcept
    {
        const int quotient = value / divisor;
        return value % divisor < 0 ? quotient - 1 : quotient;
    }

    static Address locate(int worldX, int worldZ) noexcept
    {
        const int cellX = floorDiv(worldX, MetresPerCell);
        const int cellZ = floorDiv(worldZ, MetresPerCell);
        const int tileX = floorDiv(cellX, CellsPerTile);
        const int tileZ = floorDiv(cellZ, CellsPerTile);
        const int localX = cellX - tileX * CellsPerTile;
        const int localZ = cellZ - tileZ * CellsPerTile;
        return {tileX, tileZ,
                static_cast<std::size_t>(localZ * CellsPerTile + localX)};
    }

    std::map<std::pair<int, int>, Tile> m_tiles;
    std::size_t m_knownCells = 0;
};

#endif // EXPLORATIONATLAS_H_INCLUDED
