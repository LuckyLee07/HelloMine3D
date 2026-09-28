#include "TreeGenerator.h"

#include "../../Chunk/Chunk.h"
#include "../Ecology/TerrainEcologyPlanner.h"
#include "../Ecology/AdventureEcologyPlanner.h"
#include "StructureBuilder.h"

#include <cstdint>
#include <cstdlib>
#include <algorithm>

constexpr BlockId CACTUS = BlockId::Cactus;

void makePolishedAdventureTree(Chunk &chunk, int randomSeed, int x, int y,
                              int z, AdventureTreeKind kind, int stature)
{
    if (kind == AdventureTreeKind::None) return;
    Random<std::minstd_rand> random(randomSeed);
    stature=std::clamp(stature,0,2);
    StructureBuilder builder;
    const auto metadata=kind==AdventureTreeKind::Spruce ? BlockMetadata::Tree::Spruce :
        kind==AdventureTreeKind::Birch ? BlockMetadata::Tree::Birch : BlockMetadata::Tree::Oak;
    const ChunkBlock leaf(BlockId::OakLeaf,metadata),log(BlockId::OakBark,metadata);
    const int facing=random.intInRange(0,3);
    const int dxs[4]={1,0,-1,0},dzs[4]={0,1,0,-1};
    // Overlapping, connected voxel masses at different heights break a flat
    // slab silhouette. The leaf blocks themselves remain full cubes.
    const auto mass=[&](int cx,int cy,int cz,int rx,int ry,int rz) {
        for(int dy=-ry;dy<=ry;++dy)for(int dx=-rx;dx<=rx;++dx)for(int dz=-rz;dz<=rz;++dz) {
            const double distance=double(dx*dx)/(rx*rx+.25)+
                double(dy*dy)/(ry*ry+.25)+double(dz*dz)/(rz*rz+.25);
            if(distance<=1.30 && cy+dy>=y+3)
                builder.addBlock(cx+dx,cy+dy,cz+dz,leaf);
        }
    };
    const auto branch=[&](int direction,int base,int length) {
        for(int step=1;step<=length;++step) {
            const int bx=x+dxs[direction]*step,bz=z+dzs[direction]*step;
            builder.addBlock(bx,y+base+(step-1)/2,bz,log);
            builder.addBlock(bx,y+base+step/2,bz,log);
        }
    };
    if(kind==AdventureTreeKind::Cactus) {
        const int height=3+2*stature+random.intInRange(0,2);
        builder.makeColumn(x,z,y,height,CACTUS);
        if(stature>0)for(int b=0;b<2;++b) {
            const int direction=(facing+b*2)%4;
            const int level=2+random.intInRange(0,1),length=1+random.intInRange(0,1);
            for(int step=1;step<=length;++step)
                builder.addBlock(x+dxs[direction]*step,y+level,z+dzs[direction]*step,CACTUS);
            builder.makeColumn(x+dxs[direction]*length,z+dzs[direction]*length,
                               y+level+1,1+random.intInRange(0,1),CACTUS);
        }
    }
    else if(kind==AdventureTreeKind::Spruce) {
        const int height=8+2*stature+random.intInRange(0,2);
        const int width=stature==0?2:3;
        const int phase=random.intInRange(0,2);
        for(int level=3;level<height;++level) {
            int radius=std::max(1,(width*(height-level)+height-4)/(height-3));
            if((level+phase)%3==0 && radius>1)--radius;
            for(int dx=-radius;dx<=radius;++dx)for(int dz=-radius;dz<=radius;++dz)
                if(dx*dx+dz*dz<=radius*radius+1)
                    builder.addBlock(x+dx,y+level,z+dz,leaf);
        }
        builder.addBlock(x,y+height,z,leaf);
        builder.makeColumn(x,z,y,height,log);
    }
    else if(kind==AdventureTreeKind::Palm) {
        const int height=6+stature+random.intInRange(0,2);
        const int lean=stature>0?1:0,ox=dxs[facing]*lean,oz=dzs[facing]*lean;
        builder.makeColumn(x,z,y,height-2,log);
        builder.addBlock(x,y+height-3,z,log);
        builder.addBlock(x+ox,y+height-3,z+oz,log);
        builder.makeColumn(x+ox,z+oz,y+height-2,3,log);
        builder.addBlock(x+ox,y+height+1,z+oz,leaf);
        for(int d=0;d<4;++d) {
            const int length=random.intInRange(3,5);
            for(int step=0;step<=length;++step) {
                const int level=height-(step>=3?1:0)-(step==5?1:0);
                const int bx=x+ox+dxs[d]*step,bz=z+oz+dzs[d]*step;
                builder.addBlock(bx,y+level,bz,leaf);
                if(step==3 || step==5)builder.addBlock(bx,y+level+1,bz,leaf);
            }
        }
    }
    else {
        const bool birch=kind==AdventureTreeKind::Birch;
        const bool willow=kind==AdventureTreeKind::Willow;
        const int height=(birch?6:5)+stature*(willow?1:2)+random.intInRange(0,2);
        const int lean=random.intInRange(0,1),ox=dxs[facing]*lean,oz=dzs[facing]*lean;
        mass(x+ox,y+height,z+oz,birch?1+(stature>0):2,birch?3:2,birch?1:2);
        const int branches=birch?1:2+random.intInRange(0,1);
        // Leaf masses first, connected wood last, so foliage cannot replace
        // its own fork. Vegetation-only projection protects terrain and water.
        int bases[3]{},lengths[3]{},directions[3]{};
        for(int b=0;b<branches;++b) {
            directions[b]=(facing+b+(b==2?1:0))%4;
            bases[b]=std::max(3,height-2+b%2);
            lengths[b]=birch?1:random.intInRange(1,2);
            const int bx=x+dxs[directions[b]]*lengths[b],bz=z+dzs[directions[b]]*lengths[b];
            const int by=y+bases[b]+lengths[b]/2;
            mass(bx,by+1,bz,birch?1:2,2,birch?1:2);
            if(willow) {
                const int ex=dxs[directions[b]],ez=dzs[directions[b]];
                const int drop=random.intInRange(1,3);
                for(int dy=0;dy<=drop && by-dy>=y+2;++dy)
                    builder.addBlock(bx+ex*2,by-dy,bz+ez*2,leaf);
            }
        }
        builder.makeColumn(x,z,y,height+1,log);
        for(int b=0;b<branches;++b)branch(directions[b],bases[b],lengths[b]);
        if(lean)builder.addBlock(x+ox,y+height-1,z+oz,log);
    }
    builder.build(chunk,true);
}

void makeAdventureTree(Chunk &chunk, int randomSeed, int x, int y, int z,
                       AdventureTreeKind kind)
{
    Random<std::minstd_rand> random(randomSeed);
    if(kind == AdventureTreeKind::Oak) {
        makeEcologyOakTree(chunk,random,x,y,z,
            random.intInRange(0,2)==0?EcologyTreeShape::Broad:EcologyTreeShape::Standard,true);
        return;
    }
    if(kind == AdventureTreeKind::Palm) { makePalmTree(chunk,random,x,y,z,true); return; }
    if(kind == AdventureTreeKind::Cactus) { makeCactus(chunk,random,x,y,z,true); return; }
    if(kind == AdventureTreeKind::None)return;
    StructureBuilder builder;
    const BlockMetadata_t metadata=kind==AdventureTreeKind::Spruce?BlockMetadata::Tree::Spruce:
        kind==AdventureTreeKind::Birch?BlockMetadata::Tree::Birch:BlockMetadata::Tree::Oak;
    const ChunkBlock leaf(BlockId::OakLeaf,metadata),log(BlockId::OakBark,metadata);
    const auto crown=[&](int cy,int radius,int ox=0,int oz=0) {
        for(int dx=-radius;dx<=radius;++dx)for(int dz=-radius;dz<=radius;++dz) {
            if(radius>1 && std::abs(dx)==radius && std::abs(dz)==radius)continue;
            builder.addBlock(x+ox+dx,cy,z+oz+dz,leaf);
        }
    };
    if(kind==AdventureTreeKind::Spruce) {
        const int height=random.intInRange(9,12);
        // Tiered wide skirts and narrowing whorls form a recognisable conifer,
        // with two clear blocks below the crown for walking through the grove.
        for(int level=3;level<height;++level) {
            const int remaining=height-level;
            int radius=remaining>=6?3:remaining>=3?2:1;
            if(level%2==0 && radius>1)--radius;
            crown(y+level,radius);
        }
        crown(y+height,0);
        builder.makeColumn(x,z,y,height,log);
    }
    else if(kind==AdventureTreeKind::Birch) {
        const int height=random.intInRange(7,10),lean=random.intInRange(0,1)*2-1;
        crown(y+height-3,1);crown(y+height-2,2);
        crown(y+height-1,2,lean,0);crown(y+height,1,lean,0);
        builder.makeColumn(x,z,y,height-1,log);
        builder.addBlock(x+lean,y+height-2,z,log);
    }
    else {
        const int height=random.intInRange(5,7);
        crown(y+height-1,3);crown(y+height,3);crown(y+height+1,2);
        // Hanging perimeter strands break the broad silhouette while leaving
        // the root and several sides open; no foliage replaces bank or water.
        for(int dx=-3;dx<=3;++dx)for(int dz=-3;dz<=3;++dz) {
            if(std::abs(dx)+std::abs(dz)>5 || (std::abs(dx)!=3 && std::abs(dz)!=3))continue;
            const int drop=random.intInRange(1,3);
            for(int level=1;level<=drop;++level)builder.addBlock(x+dx,y+height-1-level,z+dz,leaf);
        }
        builder.makeColumn(x,z,y,height,log);
        builder.makeRowX(x-2,x+2,y+height-2,z,log);
        builder.makeRowZ(z-2,z+2,x,y+height-2,log);
    }
    builder.build(chunk,true);
}

namespace {
void addCanopyLayer(StructureBuilder &builder, int centerX, int y,
                    int centerZ, int radius, bool trimCorners)
{
    for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (trimCorners && std::abs(dx) == radius &&
                std::abs(dz) == radius) {
                continue;
            }
            builder.addBlock(centerX + dx, y, centerZ + dz,
                             BlockId::OakLeaf);
        }
    }
}

void makeCactus1(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z, bool preserveTerrain)
{
    StructureBuilder builder;
    builder.makeColumn(x, z, y, rand.intInRange(4, 7), CACTUS);
    builder.build(chunk, preserveTerrain);
}

void makeCactus2(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z, bool preserveTerrain)
{
    StructureBuilder builder;
    int height = rand.intInRange(6, 8);
    builder.makeColumn(x, z, y, height, CACTUS);

    int stem = height / 2;

    builder.makeRowX(x - 2, x + 2, stem + y, z, CACTUS);
    builder.addBlock(x - 2, stem + y + 1, z, CACTUS);
    builder.addBlock(x - 2, stem + y + 2, z, CACTUS);
    builder.addBlock(x + 2, stem + y + 1, z, CACTUS);

    builder.build(chunk, preserveTerrain);
}

void makeCactus3(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                 int z, bool preserveTerrain)
{
    StructureBuilder builder;
    int height = rand.intInRange(6, 8);
    builder.makeColumn(x, z, y, height, CACTUS);

    int stem = height / 2;

    builder.makeRowZ(z - 2, z + 2, x, stem + y, CACTUS);
    builder.addBlock(x, stem + y + 1, z - 2, CACTUS);
    builder.addBlock(x, stem + y + 2, z - 2, CACTUS);
    builder.addBlock(x, stem + y + 1, z + 2, CACTUS);

    builder.build(chunk, preserveTerrain);
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
                      int y, int z, bool preserveTerrain)
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
    builder.build(chunk, preserveTerrain);
}

void makeEcologyOakTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x,
                        int y, int z, EcologyTreeShape shape, bool preserveTerrain)
{
    if (shape == EcologyTreeShape::Standard) {
        makeVoxelOakTree(chunk, rand, x, y, z, preserveTerrain);
        return;
    }

    StructureBuilder builder;
    if (shape == EcologyTreeShape::Tall) {
        const int height = rand.intInRange(7, 9);
        addCanopyLayer(builder, x, y + height - 2, z, 1, false);
        addCanopyLayer(builder, x, y + height - 1, z, 2, true);
        addCanopyLayer(builder, x, y + height, z, 2, true);
        addCanopyLayer(builder, x, y + height + 1, z, 1, true);
        builder.makeColumn(x, z, y, height, BlockId::OakBark);
    }
    else {
        const int height = rand.intInRange(4, 5);
        const int offsetX = rand.intInRange(-1, 1);
        const int offsetZ = offsetX == 0 ? rand.intInRange(-1, 1) : 0;
        addCanopyLayer(builder, x, y + height - 2, z, 1, true);
        addCanopyLayer(builder, x, y + height - 1, z, 2, true);
        addCanopyLayer(builder, x + offsetX, y + height, z + offsetZ,
                       2, true);
        addCanopyLayer(builder, x + offsetX, y + height + 1,
                       z + offsetZ, 1, true);
        builder.makeColumn(x, z, y, height, BlockId::OakBark);
    }
    builder.build(chunk, preserveTerrain);
}

void makePalmTree(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                  int z, bool preserveTerrain)
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
    builder.build(chunk, preserveTerrain);
}

void makeCactus(Chunk &chunk, Random<std::minstd_rand> &rand, int x, int y,
                int z, bool preserveTerrain)
{
    int cac = rand.intInRange(0, 2);

    switch (cac) {
        case 0:
            makeCactus1(chunk, rand, x, y, z, preserveTerrain);
            break;

        case 1:
            makeCactus2(chunk, rand, x, y, z, preserveTerrain);
            break;

        case 2:
            makeCactus3(chunk, rand, x, y, z, preserveTerrain);
    }
}
