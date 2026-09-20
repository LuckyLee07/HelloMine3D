#ifndef TREEGENERATOR_H_INCLUDED
#define TREEGENERATOR_H_INCLUDED

#include "../../../Util/Random.h"
#include <cstdint>

class Chunk;
enum class EcologyTreeShape : std::uint8_t;
enum class AdventureTreeKind : std::uint8_t;
void makeAdventureTree(Chunk &chunk, int randomSeed, int x, int y, int z,
                       AdventureTreeKind kind);

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
