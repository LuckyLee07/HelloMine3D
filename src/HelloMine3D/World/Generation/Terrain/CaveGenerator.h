#ifndef CAVEGENERATOR_H_INCLUDED
#define CAVEGENERATOR_H_INCLUDED

#include "../../../Util/Array2D.h"
#include "../../WorldConstants.h"

#include <cstddef>
#include <cstdint>

class Chunk;

/// Deterministic world-space cave pass shared by every generated chunk.
class CaveGenerator {
  public:
    explicit CaveGenerator(int seed);

    std::size_t carve(
        Chunk &chunk,
        const Array2D<int, CHUNK_SIZE> &surfaceHeights) const;

  private:
    bool shouldCarve(int worldX, int y, int worldZ) const noexcept;
    double sample(double x, double y, double z,
                  std::uint64_t salt) const noexcept;
    double lattice(int x, int y, int z,
                   std::uint64_t salt) const noexcept;

    std::uint64_t m_seed = 0;
};

#endif // CAVEGENERATOR_H_INCLUDED
