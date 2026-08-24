#ifndef SUPERFLATGENERATOR_H_INCLUDED
#define SUPERFLATGENERATOR_H_INCLUDED

#include "TerrainGenerator.h"

class SuperFlatGenerator : public TerrainGenerator {
  public:
    void generateTerrainFor(Chunk &chunk) override;
    int getMinimumSpawnHeight() const noexcept override;
    int getGenerationVersion() const noexcept override;
    TerrainBiome getBiomeAtWorld(int worldX,
                                 int worldZ) const noexcept override;
    int getSurfaceHeightAtWorld(int worldX,
                                int worldZ) const noexcept override;
};

#endif // SUPERFLATGENERATOR_H_INCLUDED
