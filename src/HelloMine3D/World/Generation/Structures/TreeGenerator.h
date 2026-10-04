#ifndef TREEGENERATOR_H_INCLUDED
#define TREEGENERATOR_H_INCLUDED

#include "../../../Util/Random.h"
#include "../../Block/ChunkBlock.h"
#include <cstddef>
#include <cstdint>
#include <functional>

class Chunk;
enum class EcologyTreeShape : std::uint8_t;
enum class AdventureTreeKind : std::uint8_t;
void makeAdventureTree(Chunk &chunk, int randomSeed, int x, int y, int z,
                       AdventureTreeKind kind);
// v24 only; all shapes stay within the existing six-block projection halo.
void makePolishedAdventureTree(Chunk &chunk, int randomSeed, int x, int y,
                              int z, AdventureTreeKind kind, int stature);
// The exact v24+ production block sequence, before target-chunk projection.
// It does not read or mutate a Chunk, and retains duplicate writes and their
// order so presentation ownership can mirror vegetation-only replacement.
using TreeBlockVisitor = std::function<void(int, int, int, ChunkBlock)>;
inline constexpr int PolishedTreeMaximumHorizontalRadius = 6;
inline constexpr std::size_t PolishedTreeMaximumPlannedBlocks = 1024;
void visitPolishedAdventureTreeBlocks(int randomSeed, int x, int y, int z,
                                     AdventureTreeKind kind, int stature,
                                     const TreeBlockVisitor &visitor);

/// Structure origins use world block coordinates; each target chunk receives
/// only the portion of the generated structure that overlaps it.
void makeOakTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z);
void makeVoxelOakTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x,
                      int y, int z, bool preserveTerrain = false);
void makeEcologyOakTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x,
                        int y, int z, EcologyTreeShape shape, bool preserveTerrain = false);
void makePalmTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                  int z, bool preserveTerrain = false);

void makeCactus(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                int z, bool preserveTerrain = false);

#endif // TREEGENERATOR_H_INCLUDED
