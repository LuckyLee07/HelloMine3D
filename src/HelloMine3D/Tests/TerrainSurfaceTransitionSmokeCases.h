#pragma once

namespace {
void caseTerrainSurfaceTransitionsV15()
{
    using Surface = TerrainFoundation::Surface;
    check("E9/new-world-surface-version-and-stable-biomes",
          SurfaceTransitionTerrainGenerationVersion == 15 && CurrentTerrainGenerationVersion == 15 &&
          LandmarkArchitectureTerrainGenerationVersion == 14 && static_cast<int>(TerrainBiome::RockPlateau) == 7);
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config=makeConfig(); Camera camera(config); Player owner;
    World world(camera,config,owner,freshSaveDirectory("e9_projection"),false,0);
    int changedColumns=0,matchedColumns=0,excludedStructures=0,excludedCaves=0,oreColumns=0;
    bool queryAgreement=true,ordered=true,supported=true;
    for(const auto site:LandformSites) {
        ClassicOverWorldGenerator generator(site.seed,15),reverse(site.seed,15);
        TerrainFoundation foundation(site.seed);CaveGenerator caves(site.seed,15);
        const auto surface=[&generator](int x,int z){return generator.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&generator](int x,int z){return generator.getBiomeAtWorld(x,z);};
        std::array<std::uint64_t,4> hashes{};int mismatches=0;
        for(int i=0;i<4;++i) {
            const glm::ivec2 location{WorldCoordinates::floorDiv(site.x,CHUNK_SIZE)+i%2,
                                     WorldCoordinates::floorDiv(site.z,CHUNK_SIZE)+i/2};
            Chunk chunk(world,location,false),mask(world,location,false);
            generator.generateTerrainFor(chunk);hashes[i]=TerrainSurvey::blockHash(chunk);
            for(int x=0;x<CHUNK_SIZE;++x) for(int z=0;z<CHUNK_SIZE;++z)
                mask.setBlock(x,surface(location.x*CHUNK_SIZE+x,location.y*CHUNK_SIZE+z),z,BlockId::Stone);
            caves.carveNaturalEntrances(mask,surface,biome);
            const auto plans=generator.getStructurePlansForChunk(location.x,location.y);
            for(int x=0;x<CHUNK_SIZE;++x) for(int z=0;z<CHUNK_SIZE;++z) {
                const int wx=location.x*CHUNK_SIZE+x,wz=location.y*CHUNK_SIZE+z;
                const auto c=foundation.sampleV15(wx,wz),old=foundation.sampleV13(wx,wz);
                queryAgreement &= surface(wx,wz)==c.height && biome(wx,wz)==c.biome &&
                    c.height==old.height && c.biome==old.biome;
                bool structure=false;
                for(const auto &p:plans) structure |= p.valid && wx>=p.footprint.minimumX &&
                    wx<=p.footprint.maximumX && wz>=p.footprint.minimumZ && wz<=p.footprint.maximumZ &&
                    c.height>=p.footprint.minimumY && c.height<=p.footprint.maximumY;
                if(structure){++excludedStructures;continue;}
                const auto actual=static_cast<BlockId>(chunk.getBlock(x,c.height,z).id);
                if(mask.getBlock(x,c.height,z)==BlockId::Air){++excludedCaves;mismatches+=actual!=BlockId::Air;continue;}
                BlockId expected=BlockId::Air;
                switch(c.surface) {
                    case Surface::Grass:expected=BlockId::Grass;break;
                    case Surface::Dirt:expected=BlockId::Dirt;break;
                    case Surface::Sand:expected=BlockId::Sand;break;
                    case Surface::Stone:expected=BlockId::Stone;break;
                    case Surface::Original:break;
                }
                const bool ore=expected==BlockId::Stone && (actual==BlockId::CoalOre || actual==BlockId::IronOre);
                oreColumns+=ore;
                const bool matches=ore || (expected==BlockId::Air
                    ? actual!=BlockId::Air && actual!=BlockId::Water : actual==expected);
                mismatches+=!matches;matchedColumns+=matches;
                changedColumns+=matches && old.surface!=c.surface;
                const auto above=static_cast<BlockId>(chunk.getBlock(x,c.height+1,z).id);
                supported &= (above!=BlockId::TallGrass && above!=BlockId::Rose) || actual==BlockId::Grass;
                supported &= above!=BlockId::DeadShrub || actual==BlockId::Sand;
                supported &= above!=BlockId::OakBark || (c.height>=65 && actual!=BlockId::Air && actual!=BlockId::Water);
            }
        }
        for(int i=3;i>=0;--i) {
            Chunk chunk(world,{WorldCoordinates::floorDiv(site.x,CHUNK_SIZE)+i%2,
                               WorldCoordinates::floorDiv(site.z,CHUNK_SIZE)+i/2},false);
            reverse.generateTerrainFor(chunk);ordered &= hashes[i]==TerrainSurvey::blockHash(chunk);
        }
        check("E9/actual-surface-seed-"+std::to_string(site.seed)+"-biome-"+std::to_string(site.biome),
              mismatches==0,"mismatches="+std::to_string(mismatches));
    }
    check("E9/shared-public-column-identity",queryAgreement);
    check("E9/reverse-order-four-chunks-at-forty-eight-sites",ordered);
    check("E9/changed-materials-reach-actual-chunks",changedColumns>100 && matchedColumns>40000,
          "changed="+std::to_string(changedColumns)+" matched="+std::to_string(matchedColumns)+
          " structure="+std::to_string(excludedStructures)+" cave="+std::to_string(excludedCaves)+" ore="+std::to_string(oreColumns));
    check("E9/vegetation-remains-on-suitable-ground",supported);
    const auto directory=freshSaveDirectory("e9_default_new_world");bool persisted=false;std::uint64_t hash=0;
    {
        Player player;World created(camera,config,player,directory,false,0);
        created.getChunkManager().loadChunk(4,30);
        const int height=created.getChunkManager().getTerrainGenerator().getSurfaceHeightAtWorld(64,480);
        created.setBlock(64,height,480,BlockId::OakPlank);
        hash=TerrainSurvey::blockHash(created.getChunkManager().getChunk(4,30));
        persisted=created.getChunkManager().getTerrainGenerationVersion()==15 && created.save();
    }
    {
        Player player;World reopened(camera,config,player,directory,false,0);
        reopened.getChunkManager().loadChunk(4,30);
        const int height=TerrainFoundation(42).sampleV15(64,480).height;
        persisted &= reopened.getChunkManager().getTerrainGenerationVersion()==15 &&
            reopened.getBlock(64,height,480)==BlockId::OakPlank &&
            hash==TerrainSurvey::blockHash(reopened.getChunkManager().getChunk(4,30));
    }
    check("E9/default-v15-surface-edit-save-reopen",persisted);
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED", "");
}
}
