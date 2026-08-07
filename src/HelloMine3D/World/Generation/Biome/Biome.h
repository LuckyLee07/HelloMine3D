#ifndef BIOME_H_INCLUDED
#define BIOME_H_INCLUDED

#include "../../../Maths/NoiseGenerator.h"
#include "../../../Util/Random.h"
#include "../../Block/ChunkBlock.h"

using Rand = Random<std::minstd_rand>;

class Chunk;

struct BiomeDefinition {
    NoiseParameters heightNoise;
    int treeFrequency = 0;
    int plantFrequency = 0;
    ChunkBlock topBlock;
    ChunkBlock underWaterBlock;
    ChunkBlock beachBlock;
    ChunkBlock plantBlock;
};

struct Biome {
  public:
    Biome(const BiomeDefinition &definition, int seed);
    virtual ~Biome() = default;

    virtual ChunkBlock getPlant(Rand &rand) const = 0;
    virtual ChunkBlock getTopBlock(Rand &rand) const = 0;
    virtual ChunkBlock getUnderWaterBlock(Rand &rand) const = 0;
    virtual ChunkBlock getBeachBlock(Rand &rand) const;
    virtual void makeTree(Rand &rand, Chunk &chunk, int x, int y,
                          int z) const = 0;

    int getHeight(int x, int z, int chunkX, int chunkZ) const;
    int getTreeFrequency() const noexcept;
    int getPlantFrequency() const noexcept;

  protected:
    const BiomeDefinition &getDefinition() const noexcept;

  private:
    BiomeDefinition m_definition;
    NoiseGenerator m_heightGenerator;
};

#endif // BIOME_H_INCLUDED
