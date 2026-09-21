#pragma once
#include "../World/Generation/Ecology/AdventureEcologyPlanner.h"
#include "../World/Generation/Structures/TreeGenerator.h"
#include "../Feedback/BlockSurfaceGeometry.h"

namespace {
void caseAdventureEcologyV18() {
    check("ADVENTURE_ECOLOGY/frozen-v18-identity",AdventureEcologyTerrainGenerationVersion==18 && AdventureWaterTerrainGenerationVersion==17);
    const struct Site {int seed,x,z,region;} sites[]={
        {0,-928,-816,0},
        {0,1816,80,1},
        {0,-32,-956,2},
        {0,1760,-816,3},
        {0,1368,-1908,4},
        {0,1032,-32,5},
        {0,1480,1256,6},
        {0,-704,-1824,7},
        {0,-704,836,8},
        {1,164,1172,0},
        {1,1452,1144,1},
        {1,-1152,-1852,2},
        {1,1844,24,3},
        {1,976,-1096,4},
        {1,-620,668,5},
        {1,1788,1312,6},
        {1,1284,1368,7},
        {1,-200,-452,8},
        {42,1032,640,0},
        {42,52,-1152,1},
        {42,752,-32,2},
        {42,780,-1012,3},
        {42,1396,-1684,4},
        {42,1620,-956,5},
        {42,1648,668,6},
        {42,-1656,24,7},
        {42,-704,1004,8},
        {424,752,1228,0},
        {424,-1516,-1768,1},
        {424,556,-228,2},
        {424,-508,304,3},
        {424,-4,752,4},
        {424,24,1704,5},
        {424,1172,864,6},
        {424,-1404,780,7},
        {424,1060,-844,8},
        {325322,-1208,-648,0},
        {325322,1844,-4,1},
        {325322,-1180,-1432,2},
        {325322,-32,1340,3},
        {325322,-984,1228,4},
        {325322,-508,-1096,5},
        {325322,-1628,640,6},
        {325322,836,-1908,7},
        {325322,444,-172,8},
        {8675309,752,-1404,0},
        {8675309,696,1844,1},
        {8675309,1452,-1348,2},
        {8675309,920,-676,3},
        {8675309,136,-816,4},
        {8675309,-4,1844,5},
        {8675309,24,-1544,6},
        {8675309,1368,-788,7},
        {8675309,-340,444,8},
        {20260807,-1320,-592,0},
        {20260807,976,-4,1},
        {20260807,-1432,-1516,2},
        {20260807,-1040,920,3},
        {20260807,-1040,1564,4},
        {20260807,1844,-1376,5},
        {20260807,-1012,-1824,6},
        {20260807,696,-88,7},
        {20260807,612,136,8},
        {20260809,-1656,-592,0},
        {20260809,-1768,-1152,1},
        {20260809,-4,1312,2},
        {20260809,-1320,1396,3},
        {20260809,-676,-844,4},
        {20260809,-816,808,5},
        {20260809,1564,-1376,6},
        {20260809,-1992,-228,7},
        {20260809,248,-732,8},
    };
    setEnv("HELLOMINE3D_SEED","42");setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig();Camera camera(config);Player owner;
    World world(camera,config,owner,freshSaveDirectory("adventure_ecology_projection"),false,0);
    std::array<int,33> surfaceCounts{};std::array<int,3> logs{},leaves{};std::array<int,4> covers{};
    bool queries=true,order=true,support=true,water=true;int columns=0,wet=0,structures=0,caveColumns=0;
    for(const auto site:sites) {
        AdventureEcologyPlanner ecology(site.seed);ClassicOverWorldGenerator gen(site.seed,18),reverse(site.seed,18);CaveGenerator caves(site.seed,18);
        const auto surface=[&](int x,int z){return gen.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return gen.getBiomeAtWorld(x,z);};
        std::array<std::uint64_t,4> hashes{};int mismatches=0;
        for(int i=0;i<4;++i) {
            const glm::ivec2 location{WorldCoordinates::floorDiv(site.x,16)+i%2,WorldCoordinates::floorDiv(site.z,16)+i/2};
            Chunk chunk(world,location,false),mask(world,location,false);gen.generateTerrainFor(chunk);hashes[i]=TerrainSurvey::blockHash(chunk);
            for(int x=0;x<16;++x)for(int z=0;z<16;++z)mask.setBlock(x,surface(location.x*16+x,location.y*16+z),z,BlockId::Stone);
            caves.carveNaturalEntrances(mask,surface,biome);const auto plans=gen.getStructurePlansForChunk(location.x,location.y);
            for(int x=0;x<16;++x)for(int z=0;z<16;++z) {
                const int wx=location.x*16+x,wz=location.y*16+z;const auto s=ecology.sample(wx,wz);const auto c=s.column;
                queries &= surface(wx,wz)==c.height && biome(wx,wz)==c.biome;
                bool structure=false;for(const auto &p:plans)structure |= p.valid && wx>=p.footprint.minimumX && wx<=p.footprint.maximumX && wz>=p.footprint.minimumZ && wz<=p.footprint.maximumZ;
                if(structure){++structures;continue;}
                const BlockId expected[]={BlockId::Air,BlockId::Grass,BlockId::Dirt,BlockId::Sand,BlockId::Stone,BlockId::Snow,BlockId::Gravel,BlockId::Clay,BlockId::ForestFloor,BlockId::MossStone,BlockId::Silt};
                const auto ground=static_cast<BlockId>(chunk.getBlock(x,c.height,z).id);
                const auto wanted=expected[static_cast<int>(c.surface)];
                const bool cave=mask.getBlock(x,c.height,z)==BlockId::Air;
                if(cave)++caveColumns;
                const bool ore=wanted==BlockId::Stone && (ground==BlockId::CoalOre || ground==BlockId::IronOre);
                const bool matches=cave?ground==BlockId::Air:ground==wanted || ore;
                if(!matches && mismatches<4)std::cout << "[ECOLOGY_MISMATCH] seed=" << site.seed << " x=" << wx << " z=" << wz << " y=" << c.height << " expected=" << static_cast<int>(wanted) << " actual=" << static_cast<int>(ground) << '\n';
                mismatches+=!matches;++columns;++surfaceCounts[static_cast<int>(ground)];
                const auto above=chunk.getBlock(x,c.height+1,z);const auto plant=static_cast<BlockId>(above.id);
                if(plant==BlockId::TallGrass || plant==BlockId::Rose || plant==BlockId::DeadShrub)
                    support &= AdventureEcologyPlanner::supportsPlant(ground,plant);
                if(plant==BlockId::OakBark)support &= c.height>=65 && ground!=BlockId::Air && ground!=BlockId::Water;
                if(c.height<64) {++wet;for(int y=c.height+1;y<=64;++y)water &= chunk.getBlock(x,y,z)==BlockId::Water;}
                for(int y=c.height+1;y<=c.height+16;++y) {
                    const auto b=chunk.getBlock(x,y,z);
                    if(b.id==static_cast<Block_t>(BlockId::OakBark) && b.metadata<3)++logs[b.metadata];
                    if(b.id==static_cast<Block_t>(BlockId::OakLeaf) && b.metadata<3)++leaves[b.metadata];
                    if(b.id==static_cast<Block_t>(BlockId::TallGrass) && b.metadata<4)++covers[b.metadata];
                }
            }
        }
        for(int i=3;i>=0;--i) {Chunk chunk(world,{WorldCoordinates::floorDiv(site.x,16)+i%2,WorldCoordinates::floorDiv(site.z,16)+i/2},false);reverse.generateTerrainFor(chunk);order &= TerrainSurvey::blockHash(chunk)==hashes[i];}
        check("ADVENTURE_ECOLOGY/surface-"+std::to_string(site.seed)+"-"+std::to_string(site.region),mismatches==0,"mismatches="+std::to_string(mismatches));
    }
    check("ADVENTURE_ECOLOGY/shared-surface-and-biome",queries && columns>60000);
    check("ADVENTURE_ECOLOGY/positive-negative-reverse-generation",order);
    check("ADVENTURE_ECOLOGY/plant-roots-and-ground-cover-support",support);
    check("ADVENTURE_ECOLOGY/real-water-preserved",water && wet>2000);
    bool materials=true;for(int i=27;i<=32;++i)materials &= surfaceCounts[i]>20;
    check("ADVENTURE_ECOLOGY/six-materials-in-generated-world",materials);
    check("ADVENTURE_ECOLOGY/three-tree-material-families",logs[0]>40 && logs[1]>40 && logs[2]>40 && leaves[0]>100 && leaves[1]>100 && leaves[2]>100);
    check("ADVENTURE_ECOLOGY/generated-grass-fern-reed",covers[1]>20 && covers[2]>20 && covers[3]>20);
    std::cout << "[ECOLOGY_COUNTS] columns=" << columns << " wet=" << wet << " structures=" << structures << " caves=" << caveColumns << " surfaces=";
    for(int i=27;i<=32;++i)std::cout << surfaceCounts[i] << ',';
    std::cout << " logs=";for(auto n:logs)std::cout << n << ',';std::cout << " cover=";for(auto n:covers)std::cout << n << ',';std::cout << '\n';

    for(const auto kind:{AdventureTreeKind::Spruce,AdventureTreeKind::Birch,AdventureTreeKind::Willow}) {
        Chunk chunk(world,{0,0},false);makeAdventureTree(chunk,42,8,200,8,kind);
        std::array<int,16> levels{};int trunk=0,low=0,high=0;
        for(int y=200;y<216;++y)for(int x=3;x<=13;++x)for(int z=3;z<=13;++z) {
            const auto b=chunk.getBlock(x,y,z);if(b.id==static_cast<Block_t>(BlockId::OakBark))++trunk;
            if(b.id==static_cast<Block_t>(BlockId::OakLeaf)) {++levels[y-200];high=std::max(high,y-200);if(!low)low=y-200;}
        }
        const bool shape=kind==AdventureTreeKind::Spruce?high>=9 && levels[3]>levels[high-1] && levels[high]==1:
            kind==AdventureTreeKind::Birch?low>=4 && high>=7:low<=4 && levels[high-1]>=40;
        check("ADVENTURE_ECOLOGY/distinct-tree-silhouette-"+std::to_string(static_cast<int>(kind)),shape && trunk>=5);
    }
    bool geometry=true;
    const auto &definition=BlockDatabase::get().getDefinition(BlockId::TallGrass);
    for(auto metadata:{BlockMetadata::TallGrass::Fern,BlockMetadata::TallGrass::Reed}) {
        const ChunkBlock block(BlockId::TallGrass,metadata);
        const auto faces=blockSurfaceGeometry(definition,block,{0,200,0},TerrainBiome::Grassland,42);
        geometry &= faces.size()==(metadata==BlockMetadata::TallGrass::Fern?6u:8u) &&
            definition.behavior->getDrop(definition,block)!=Material::Nothing && !definition.behavior->receivesRandomTicks(definition,block);
        for(const auto &face:faces)for(float p:face.positions)geometry &= p>=0 && p<=1;
    }
    check("ADVENTURE_ECOLOGY/fern-reed-geometry-and-harvest",geometry);

    const auto directory=freshSaveDirectory("adventure_ecology_frozen_world");
    check("ADVENTURE_ECOLOGY/initialize-frozen-v18",initializeTerrainIdentity(directory,"adventure-v18",18,42));
    std::uint64_t hash=0;bool saved=false;
    {
        Player player;World actual(camera,config,player,directory,false,0);actual.getChunkManager().loadChunk(0,0);
        actual.setBlock(6,201,8,ChunkBlock(BlockId::OakBark,BlockMetadata::Tree::Birch));
        const auto selection=BlockSelectionSystem::pick(actual,{6.5f,201.5f,11.f},{0,0,0});
        check("ADVENTURE_ECOLOGY/selection-keeps-tree-metadata",selection && selection->blockPosition==glm::ivec3(6,201,8) && selection->metadata==BlockMetadata::Tree::Birch);
        ActionFeedbackTimeline feedback;feedback.attach(actual.getEventBus());
        BlockInteractionSystem::breakBlock(actual,player,{6.5f,201.5f,8.5f});const auto snapshot=feedback.snapshot();
        bool fragments=!snapshot.particles.empty();for(const auto &p:snapshot.particles)fragments &= p.metadata==BlockMetadata::Tree::Birch;
        check("ADVENTURE_ECOLOGY/broken-tree-fragments-keep-metadata",fragments && actual.getBlock(6,201,8)==BlockId::Air);
        feedback.update(.25f);feedback.update(.25f);feedback.update(.25f);BlockSelection hit;hit.blockPosition={6,201,8};hit.placementPosition={6,201,9};hit.blockId=BlockId::OakBark;hit.metadata=BlockMetadata::Tree::Spruce;
        MiningProgressSnapshot mining;mining.active=true;mining.target=hit.blockPosition;mining.blockId=hit.blockId;mining.requiredSeconds=1;mining.elapsedSeconds=.2f;
        feedback.observeMining(&hit,mining);const auto chips=feedback.snapshot();bool matching=!chips.particles.empty();for(const auto &p:chips.particles)matching &= p.metadata==BlockMetadata::Tree::Spruce;
        check("ADVENTURE_ECOLOGY/mining-chips-keep-metadata",matching);
        actual.setBlock(6,201,8,ChunkBlock(BlockId::OakBark,BlockMetadata::Tree::Birch));
        actual.setBlock(7,201,8,ChunkBlock(BlockId::TallGrass,BlockMetadata::TallGrass::Fern));
        actual.setBlock(8,201,8,ChunkBlock(BlockId::TallGrass,BlockMetadata::TallGrass::Reed));
        hash=TerrainSurvey::blockHash(actual.getChunkManager().getChunk(0,0));saved=actual.getChunkManager().getTerrainGenerationVersion()==18 && actual.save();
    }
    {Player player;World actual(camera,config,player,directory,false,0);actual.getChunkManager().loadChunk(0,0);saved &= hash==TerrainSurvey::blockHash(actual.getChunkManager().getChunk(0,0)) && actual.getChunkManager().getTerrainGenerationVersion()==18;}
    check("ADVENTURE_ECOLOGY/frozen-v18-and-metadata-save-reopen",saved);
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
