#pragma once

namespace {
void caseAdventureWaterV17()
{
    check("ADVENTURE_WATER/version-and-appended-biomes",
          AdventureWaterTerrainGenerationVersion == 17 && AdventureRegionTerrainGenerationVersion == 16 &&
          static_cast<int>(TerrainBiome::River) == 8 && static_cast<int>(TerrainBiome::Lake) == 9);
    // Nearest samples to the origin, selected before capture (fixture stores provenance).
    const struct Site { int seed, x, z, biome; } sites[] = {
        {0,352,-160,8},{0,608,-192,9},{1,0,-416,8},{1,-704,352,9},
        {42,224,-192,8},{42,224,-128,9},{424,-416,0,8},{424,832,-1888,9},
        {20260807,-448,96,8},{20260807,1248,-352,9},
        {20260809,448,160,8},{20260809,128,736,9},
        {8675309,704,-64,8},{8675309,192,-1248,9},
        {325322,0,544,8},{325322,-352,1184,9}
    };
    setEnv("HELLOMINE3D_SEED","42");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig(); Camera camera(config); Player owner;
    World world(camera,config,owner,freshSaveDirectory("adventure_water_projection"),false,0);
    bool queries=true,ordered=true,water=true,plants=true;
    int waterColumns=0,surfaceColumns=0,excludedStructures=0,excludedCaves=0;
    for(const auto site:sites) {
        AdventureWaterPlanner planner(site.seed);
        ClassicOverWorldGenerator generator(site.seed,17), reverse(site.seed,17);
        CaveGenerator caves(site.seed,17);
        const auto surface=[&](int x,int z){return generator.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return generator.getBiomeAtWorld(x,z);};
        queries &= static_cast<int>(biome(site.x,site.z))==site.biome;
        std::array<std::uint64_t,4> hashes{}; int mismatches=0;
        for(int i=0;i<4;++i) {
            const glm::ivec2 location{WorldCoordinates::floorDiv(site.x,CHUNK_SIZE)+i%2,
                                     WorldCoordinates::floorDiv(site.z,CHUNK_SIZE)+i/2};
            Chunk chunk(world,location,false),mask(world,location,false);
            generator.generateTerrainFor(chunk); hashes[i]=TerrainSurvey::blockHash(chunk);
            for(int x=0;x<CHUNK_SIZE;++x)for(int z=0;z<CHUNK_SIZE;++z)
                mask.setBlock(x,surface(location.x*CHUNK_SIZE+x,location.y*CHUNK_SIZE+z),z,BlockId::Stone);
            caves.carveNaturalEntrances(mask,surface,biome);
            const auto plans=generator.getStructurePlansForChunk(location.x,location.y);
            for(int x=0;x<CHUNK_SIZE;++x)for(int z=0;z<CHUNK_SIZE;++z) {
                const int wx=location.x*CHUNK_SIZE+x,wz=location.y*CHUNK_SIZE+z;
                const auto column=planner.sample(wx,wz).column;
                queries &= surface(wx,wz)==column.height && biome(wx,wz)==column.biome;
                bool structure=false;
                for(const auto &p:plans)structure |= p.valid && wx>=p.footprint.minimumX &&
                    wx<=p.footprint.maximumX && wz>=p.footprint.minimumZ && wz<=p.footprint.maximumZ;
                if(structure){++excludedStructures;continue;}
                if(mask.getBlock(x,column.height,z)==BlockId::Air){++excludedCaves;continue;}
                BlockId expected=BlockId::Grass;
                switch(column.surface) {
                    case TerrainFoundation::Surface::Sand:expected=BlockId::Sand;break;
                    case TerrainFoundation::Surface::Dirt:expected=BlockId::Dirt;break;
                    case TerrainFoundation::Surface::Stone:expected=BlockId::Stone;break;
                    default:break;
                }
                const auto actual=static_cast<BlockId>(chunk.getBlock(x,column.height,z).id);
                const bool ore=expected==BlockId::Stone && (actual==BlockId::CoalOre || actual==BlockId::IronOre);
                if(actual!=expected && !ore)std::cout << "[WATER_MISMATCH] seed=" << site.seed << " x=" << wx << " z=" << wz
                    << " y=" << column.height << " expected=" << static_cast<int>(expected) << " actual=" << static_cast<int>(actual) << '\n';
                mismatches+=actual!=expected && !ore; ++surfaceColumns;
                if(column.height<64) {
                    ++waterColumns;
                    for(int y=column.height+1;y<=64;++y)water &= chunk.getBlock(x,y,z)==BlockId::Water;
                    const auto above=static_cast<BlockId>(chunk.getBlock(x,65,z).id);
                    plants &= above!=BlockId::OakBark && above!=BlockId::TallGrass && above!=BlockId::Rose && above!=BlockId::DeadShrub;
                }
            }
        }
        for(int i=3;i>=0;--i) {
            Chunk chunk(world,{WorldCoordinates::floorDiv(site.x,CHUNK_SIZE)+i%2,
                               WorldCoordinates::floorDiv(site.z,CHUNK_SIZE)+i/2},false);
            reverse.generateTerrainFor(chunk); ordered &= hashes[i]==TerrainSurvey::blockHash(chunk);
        }
        check("ADVENTURE_WATER/actual-bed-"+std::to_string(site.seed)+"-"+std::to_string(site.biome),
              mismatches==0,"mismatches="+std::to_string(mismatches));
    }
    check("ADVENTURE_WATER/shared-column-query",queries && surfaceColumns>12000,
          "columns="+std::to_string(surfaceColumns)+" structures="+std::to_string(excludedStructures)+" caves="+std::to_string(excludedCaves));
    check("ADVENTURE_WATER/reverse-chunk-generation",ordered);
    check("ADVENTURE_WATER/actual-continuous-water-columns",water && waterColumns>1000,
          "columns="+std::to_string(waterColumns));
    check("ADVENTURE_WATER/no-terrestrial-plants-in-water",plants);
    const auto directory=freshSaveDirectory("adventure_water_default_world");
    check("ADVENTURE_WATER/initialize-frozen-v17-world",initializeTerrainIdentity(directory,"adventure-v17",17,42));
    std::uint64_t savedHash=0; bool persisted=false;
    {
        Player player; World created(camera,config,player,directory,false,0);
        created.getChunkManager().loadChunk(14,-8);
        created.setBlock(224,64,-128,BlockId::OakPlank);
        savedHash=TerrainSurvey::blockHash(created.getChunkManager().getChunk(14,-8));
        persisted=created.getChunkManager().getTerrainGenerationVersion()==17 && created.save();
    }
    {
        Player player; World reopened(camera,config,player,directory,false,0);
        reopened.getChunkManager().loadChunk(14,-8);
        persisted &= reopened.getChunkManager().getTerrainGenerationVersion()==17 &&
            reopened.getBlock(224,64,-128)==BlockId::OakPlank &&
            savedHash==TerrainSurvey::blockHash(reopened.getChunkManager().getChunk(14,-8));
    }
    check("ADVENTURE_WATER/frozen-v17-water-edit-save-reopen",persisted);
    clearDeterministicEnv(); setEnv("HELLOMINE3D_SEED","");
}
}
