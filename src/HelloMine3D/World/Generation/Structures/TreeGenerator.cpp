#include "TreeGenerator.h"

#include "../../Chunk/Chunk.h"
#include "StructureBuilder.h"

#include <cstdint>

constexpr BlockId CACTUS = BlockId::Cactus;

namespace {
void makeCactus1(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z)
{
    StructureBuilder builder;
    builder.makeColumn(x, z, y, rand.intInRange(4, 7), CACTUS);
    builder.build(chunk);
}

void makeCactus2(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z)
{
    StructureBuilder builder;
    int height = rand.intInRange(6, 8);
    builder.makeColumn(x, z, y, height, CACTUS);

    int stem = height / 2;

    builder.makeRowX(x - 2, x + 2, stem + y, z, CACTUS);
    builder.addBlock(x - 2, stem + y + 1, z, CACTUS);
    builder.addBlock(x - 2, stem + y + 2, z, CACTUS);
    builder.addBlock(x + 2, stem + y + 1, z, CACTUS);

    builder.build(chunk);
}

void makeCactus3(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z)
{
    StructureBuilder builder;
    int height = rand.intInRange(6, 8);
    builder.makeColumn(x, z, y, height, CACTUS);

    int stem = height / 2;

    builder.makeRowZ(z - 2, z + 2, x, stem + y, CACTUS);
    builder.addBlock(x, stem + y + 1, z - 2, CACTUS);
    builder.addBlock(x, stem + y + 2, z - 2, CACTUS);
    builder.addBlock(x, stem + y + 1, z + 2, CACTUS);

    builder.build(chunk);
}
} // namespace

void makeOakTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z)
{
    StructureBuilder builder;

    int h = rand.intInRange(4, 7);
    int leafSize = 2;

    int newY = h + y;
    builder.fill(newY, x - leafSize, x + leafSize, z - leafSize, z + leafSize,
                 BlockId::OakLeaf);
    builder.fill(newY - 1, x - leafSize, x + leafSize, z - leafSize,
                 z + leafSize, BlockId::OakLeaf);

    for (int32_t zLeaf = -leafSize + 1; zLeaf <= leafSize - 1; zLeaf++) {
        builder.addBlock(x, newY + 1, z + zLeaf, BlockId::OakLeaf);
    }

    for (int32_t xLeaf = -leafSize + 1; xLeaf <= leafSize - 1; xLeaf++) {
        builder.addBlock(x + xLeaf, newY + 1, z, BlockId::OakLeaf);
    }

    builder.makeColumn(x, z, y, h, BlockId::OakBark);
    builder.build(chunk);
}

void makeVoxelOakTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x,
                      int y, int z)
{
    StructureBuilder builder;
    const int height = rand.intInRange(4, 7);
    const std::uint32_t origin =
        static_cast<std::uint32_t>(x) * 0x9e3779b9u ^
        static_cast<std::uint32_t>(z) * 0x85ebca6bu;

    // Two broad layers and a narrower top keep the canopy cubic. Sparse edge
    // cells break long straight lines without lowering the crown into a bush.
    for (int layer = 0; layer < 3; ++layer) {
        const int radius = layer < 2 ? 2 : 1;
        const int leafY = y + height - 1 + layer;
        for (int dz = -radius; dz <= radius; ++dz) {
            for (int dx = -radius; dx <= radius; ++dx) {
                const bool edgeX = dx == -radius || dx == radius;
                const bool edgeZ = dz == -radius || dz == radius;
                if (!edgeX && !edgeZ) {
                    builder.addBlock(x + dx, leafY, z + dz,
                                     BlockId::OakLeaf);
                    continue;
                }
                std::uint32_t cell = origin ^
                    (static_cast<std::uint32_t>(dx + 2) * 0xc2b2ae35u) ^
                    (static_cast<std::uint32_t>(dz + 2) * 0x27d4eb2fu) ^
                    (static_cast<std::uint32_t>(layer) * 0x165667b1u);
                cell ^= cell >> 16;
                cell *= 0x7feb352du;
                cell ^= cell >> 15;
                const bool place = edgeX && edgeZ
                    ? (cell & (layer == 2 ? 1u : 3u)) == 0u
                    : (cell & 3u) != 0u;
                if (place) {
                    builder.addBlock(x + dx, leafY, z + dz,
                                     BlockId::OakLeaf);
                }
            }
        }
    }

    builder.makeColumn(x, z, y, height, BlockId::OakBark);
    builder.build(chunk);
}

void makePalmTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                  int z)
{
    StructureBuilder builder;

    int height = rand.intInRange(7, 9);
    int diameter = rand.intInRange(4, 6);

    for (int xLeaf = -diameter; xLeaf < diameter; xLeaf++) {
        builder.addBlock(xLeaf + x, y + height, z, BlockId::OakLeaf);
    }
    for (int zLeaf = -diameter; zLeaf < diameter; zLeaf++) {
        builder.addBlock(x, y + height, zLeaf + z, BlockId::OakLeaf);
    }

    builder.addBlock(x, y + height - 1, z + diameter, BlockId::OakLeaf);
    builder.addBlock(x, y + height - 1, z - diameter, BlockId::OakLeaf);
    builder.addBlock(x + diameter, y + height - 1, z, BlockId::OakLeaf);
    builder.addBlock(x - diameter, y + height - 1, z, BlockId::OakLeaf);
    builder.addBlock(x, y + height + 1, z, BlockId::OakLeaf);

    builder.makeColumn(x, z, y, height, BlockId::OakBark);
    builder.build(chunk);
}

void makeCactus(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                int z)
{
    int cac = rand.intInRange(0, 2);

    switch (cac) {
        case 0:
            makeCactus1(chunk, rand, x, y, z);
            break;

        case 1:
            makeCactus2(chunk, rand, x, y, z);
            break;

        case 2:
            makeCactus3(chunk, rand, x, y, z);
    }
}
