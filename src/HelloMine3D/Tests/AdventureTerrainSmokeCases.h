#pragma once

namespace {
void caseAdventureTerrainV16()
{
    check("ADVENTURE/default-v16-and-frozen-v15-identity",
        CurrentTerrainGenerationVersion == 16 && AdventureRegionTerrainGenerationVersion == 16 &&
        SurfaceTransitionTerrainGenerationVersion == 15 && static_cast<int>(TerrainBiome::RockPlateau) == 7);
    // Largest core components selected before visual capture. Full selection
    // provenance lives in tools/fixtures/terrain/adventure-regions-v16.json.
    const struct Site { int seed, x, z, region; } sites[] = {
        {0,-960,-832,0},{0,-1568,-1728,1},{0,-32,-992,2},{0,1792,-832,3},
        {0,1344,-1952,4},{0,1024,0,5},{0,1600,1184,6},{0,-736,-1824,7},{0,-704,832,8},
        {42,1024,576,0},{42,96,-1152,1},{42,768,-32,2},{42,800,-992,3},
        {42,1376,-1664,4},{42,1664,-928,5},{42,1440,32,6},{42,-1728,-128,7},{42,-704,992,8},
        {20260807,-1312,-576,0},{20260807,992,32,1},{20260807,-1440,-1536,2},
        {20260807,-864,704,3},{20260807,-1504,1824,4},{20260807,1888,-1376,5},
        {20260807,-992,-1824,6},{20260807,672,-192,7},{20260807,640,128,8}
    };
    setEnv("HELLOMINE3D_SEED","42");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig(); Camera camera(config); Player owner;
    World world(camera,config,owner,freshSaveDirectory("adventure_projection"),false,0);
    bool queries=true,ordered=true,plants=true,water=true;
    int matched=0,excludedStructures=0,excludedCaves=0,waterColumns=0;
    for(const auto site:sites) {
        ClassicOverWorldGenerator generator(site.seed,16), reverse(site.seed,16);
        AdventureTerrainPlanner planner(site.seed); CaveGenerator caves(site.seed,16);
        const auto surface=[&](int x,int z){return generator.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return generator.getBiomeAtWorld(x,z);};
        queries &= static_cast<int>(planner.sample(site.x,site.z).region)==site.region;
        std::array<std::uint64_t,4> hashes{};int mismatches=0;
        for(int i=0;i<4;++i) {
            const glm::ivec2 location{WorldCoordinates::floorDiv(site.x,CHUNK_SIZE)+i%2,
                                     WorldCoordinates::floorDiv(site.z,CHUNK_SIZE)+i/2};
            Chunk chunk(world,location,false),mask(world,location,false);
            generator.generateTerrainFor(chunk); hashes[i]=TerrainSurvey::blockHash(chunk);
            for(int x=0;x<CHUNK_SIZE;++x) for(int z=0;z<CHUNK_SIZE;++z)
                mask.setBlock(x,surface(location.x*CHUNK_SIZE+x,location.y*CHUNK_SIZE+z),z,BlockId::Stone);
            caves.carveNaturalEntrances(mask,surface,biome);
            const auto plans=generator.getStructurePlansForChunk(location.x,location.y);
            for(int x=0;x<CHUNK_SIZE;++x)for(int z=0;z<CHUNK_SIZE;++z) {
                const int wx=location.x*CHUNK_SIZE+x,wz=location.y*CHUNK_SIZE+z;
                const auto c=planner.sample(wx,wz).column;
                queries &= surface(wx,wz)==c.height && biome(wx,wz)==c.biome;
                bool structure=false;
                for(const auto &p:plans)structure |= p.valid && wx>=p.footprint.minimumX &&
                    wx<=p.footprint.maximumX && wz>=p.footprint.minimumZ && wz<=p.footprint.maximumZ;
                if(structure){++excludedStructures;continue;}
                const auto actual=static_cast<BlockId>(chunk.getBlock(x,c.height,z).id);
                if(mask.getBlock(x,c.height,z)==BlockId::Air){++excludedCaves;mismatches+=actual!=BlockId::Air;continue;}
                BlockId expected=BlockId::Grass;
                switch(c.surface) {
                    case TerrainFoundation::Surface::Sand:expected=BlockId::Sand;break;
                    case TerrainFoundation::Surface::Dirt:expected=BlockId::Dirt;break;
                    case TerrainFoundation::Surface::Stone:expected=BlockId::Stone;break;
                    default:break;
                }
                const bool ore=expected==BlockId::Stone && (actual==BlockId::CoalOre || actual==BlockId::IronOre);
                mismatches+=!(actual==expected || ore);++matched;
                const auto above=static_cast<BlockId>(chunk.getBlock(x,c.height+1,z).id);
                plants &= (above!=BlockId::TallGrass && above!=BlockId::Rose) || actual==BlockId::Grass;
                plants &= above!=BlockId::DeadShrub || actual==BlockId::Sand;
                plants &= above!=BlockId::OakBark || (c.height>=65 && actual!=BlockId::Water && actual!=BlockId::Air);
                if(c.height<64) {
                    ++waterColumns;
                    for(int y=c.height+1;y<=64;++y)water &= chunk.getBlock(x,y,z)==BlockId::Water;
                }
            }
        }
        for(int i=3;i>=0;--i) {
            Chunk chunk(world,{WorldCoordinates::floorDiv(site.x,CHUNK_SIZE)+i%2,
                               WorldCoordinates::floorDiv(site.z,CHUNK_SIZE)+i/2},false);
            reverse.generateTerrainFor(chunk);ordered &= hashes[i]==TerrainSurvey::blockHash(chunk);
        }
        check("ADVENTURE/actual-surface-"+std::to_string(site.seed)+"-region-"+std::to_string(site.region),
              mismatches==0,"mismatches="+std::to_string(mismatches));
    }
    check("ADVENTURE/shared-production-column",queries && matched>20000,
        "matched="+std::to_string(matched)+" structures="+std::to_string(excludedStructures)+" caves="+std::to_string(excludedCaves));
    check("ADVENTURE/reverse-chunk-generation",ordered);
    check("ADVENTURE/plants-supported",plants);
    check("ADVENTURE/real-water-columns",water && waterColumns>2000,"columns="+std::to_string(waterColumns));
    const auto directory=freshSaveDirectory("adventure_default_world");
    std::uint64_t savedHash=0;int height=0;bool persisted=false;
    {
        Player player;World created(camera,config,player,directory,false,0);
        created.getChunkManager().loadChunk(4,30);
        height=created.getChunkManager().getTerrainGenerator().getSurfaceHeightAtWorld(64,480);
        created.setBlock(64,height,480,BlockId::OakPlank);
        savedHash=TerrainSurvey::blockHash(created.getChunkManager().getChunk(4,30));
        persisted=created.getChunkManager().getTerrainGenerationVersion()==16 && created.save();
    }
    {
        Player player;World reopened(camera,config,player,directory,false,0);
        reopened.getChunkManager().loadChunk(4,30);
        persisted &= reopened.getChunkManager().getTerrainGenerationVersion()==16 &&
            reopened.getBlock(64,height,480)==BlockId::OakPlank &&
            savedHash==TerrainSurvey::blockHash(reopened.getChunkManager().getChunk(4,30));
    }
    check("ADVENTURE/default-world-edit-save-reopen",persisted);
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
