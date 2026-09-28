#pragma once

namespace {
void caseLocalReliefV25() {
    check("LOCAL25/version-and-save-boundary",LocalReliefTerrainGenerationVersion==25 &&
          VegetationPolishTerrainGenerationVersion==24 && CurrentTerrainGenerationVersion==25 && WorldSaveFormatVersion==12);
    setEnv("HELLOMINE3D_SEED","42");setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig();Camera camera(config);Player owner;
    World world(camera,config,owner,freshSaveDirectory("local_relief_projection"),false,0);
    bool queries=true,order=true,support=true;int columns=0,modified=0;
    const BlockId materials[]={BlockId::Air,BlockId::Grass,BlockId::Dirt,BlockId::Sand,BlockId::Stone,
        BlockId::Snow,BlockId::Gravel,BlockId::Clay,BlockId::ForestFloor,BlockId::MossStone,BlockId::Silt};
    for(int seed:{42,20260807,239701883})for(const auto site:{glm::ivec2{32,48},{52,-1152},{752,-32},{1396,-1684},{-17,-17}}) {
        ClassicOverWorldGenerator gen(seed,25),reverse(seed,25);AdventureEcologyPlanner ecology(seed,25),old(seed,24);
        CaveGenerator caves(seed,25);
        const auto surface=[&](int x,int z){return gen.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return gen.getBiomeAtWorld(x,z);};
        std::array<std::uint64_t,4> hashes{};int mismatches=0;
        for(int i=0;i<4;++i) {
            const glm::ivec2 location{WorldCoordinates::floorDiv(site.x,16)+i%2,WorldCoordinates::floorDiv(site.y,16)+i/2};
            Chunk chunk(world,location,false),mask(world,location,false);gen.generateTerrainFor(chunk);hashes[i]=TerrainSurvey::blockHash(chunk);
            for(int x=0;x<16;++x)for(int z=0;z<16;++z)mask.setBlock(x,surface(location.x*16+x,location.y*16+z),z,BlockId::Stone);
            caves.carveNaturalEntrances(mask,surface,biome);
            const auto plans=gen.getStructurePlansForChunk(location.x,location.y,6);
            for(int x=0;x<16;++x)for(int z=0;z<16;++z) {
                const int wx=location.x*16+x,wz=location.y*16+z;const auto c=ecology.sample(wx,wz).column;
                queries &= surface(wx,wz)==c.height && biome(wx,wz)==c.biome;
                modified+=old.sample(wx,wz).column.height!=c.height;
                bool structure=false;for(const auto &p:plans)structure|=p.valid && wx>=p.footprint.minimumX-6 &&
                    wx<=p.footprint.maximumX+6 && wz>=p.footprint.minimumZ-6 && wz<=p.footprint.maximumZ+6;
                if(structure)continue; // Architecture/approach projection has its own block contract.
                const auto actual=static_cast<BlockId>(chunk.getBlock(x,c.height,z).id),wanted=materials[int(c.surface)];
                const bool cave=mask.getBlock(x,c.height,z)==BlockId::Air;
                const bool ore=wanted==BlockId::Stone && (actual==BlockId::CoalOre || actual==BlockId::IronOre);
                const bool match=cave?actual==BlockId::Air:actual==wanted || ore;
                if(!match && mismatches<3)std::cout<<"[LOCAL25_MISMATCH] "<<seed<<' '<<wx<<' '<<c.height<<' '<<wz<<" wanted="<<int(wanted)<<" actual="<<int(actual)<<'\n';
                mismatches+=!match;++columns;
                const auto plant=static_cast<BlockId>(chunk.getBlock(x,c.height+1,z).id);
                if(plant==BlockId::TallGrass || plant==BlockId::Rose || plant==BlockId::DeadShrub)
                    support &= AdventureEcologyPlanner::supportsPlant(actual,plant);
                if(plant==BlockId::OakBark)support &= actual!=BlockId::Air && actual!=BlockId::Water;
            }
        }
        for(int i=3;i>=0;--i) {
            Chunk chunk(world,{WorldCoordinates::floorDiv(site.x,16)+i%2,WorldCoordinates::floorDiv(site.y,16)+i/2},false);
            reverse.generateTerrainFor(chunk);order &= hashes[i]==TerrainSurvey::blockHash(chunk);
        }
        check("LOCAL25/production-columns-"+std::to_string(seed)+"-"+std::to_string(site.x)+"-"+std::to_string(site.y),mismatches==0,"mismatches="+std::to_string(mismatches));
    }
    check("LOCAL25/shared-production-shape",queries && columns>10000 && modified>100);
    check("LOCAL25/reverse-chunk-order",order);
    check("LOCAL25/plant-support",support);
    std::cout<<"[LOCAL25_PRODUCTION] columns="<<columns<<" modified="<<modified<<'\n';
    clearDeterministicEnv();
    for(int seed:{42,20260807,239701883}) {
        setEnv("HELLOMINE3D_SEED",std::to_string(seed));const auto directory=freshSaveDirectory("local25_save_"+std::to_string(seed));
        glm::ivec3 edit{0};bool persisted=false;std::uint64_t hash=0;
        {
            Player player;World created(camera,config,player,directory,false,1);
            const auto spawn=created.getPlayerSpawnPoint();const int x=World::toBlockCoord(spawn.x),y=World::toBlockCoord(spawn.y),z=World::toBlockCoord(spawn.z);
            const auto floor=created.getBlock(x,y-2,z);
            check("LOCAL25/normal-spawn-"+std::to_string(seed),floor.getData().isCollidable && floor!=BlockId::Water &&
                created.getBlock(x,y-1,z)==BlockId::Air && created.getBlock(x,y,z)==BlockId::Air,vecToString(spawn));
            edit={x,y-2,z};created.setBlock(edit.x,edit.y,edit.z,BlockId::OakPlank);
            auto &manager=created.getChunkManager();hash=TerrainSurvey::blockHash(manager.getChunk(WorldCoordinates::floorDiv(x,16),WorldCoordinates::floorDiv(z,16)));
            persisted=manager.getTerrainGenerationVersion()==25 && created.save();
        }
        {
            Player player;World reopened(camera,config,player,directory,false,0);auto &manager=reopened.getChunkManager();
            const int cx=WorldCoordinates::floorDiv(edit.x,16),cz=WorldCoordinates::floorDiv(edit.z,16);manager.loadChunk(cx,cz);
            persisted &= manager.getTerrainGenerationVersion()==25 && reopened.getBlock(edit.x,edit.y,edit.z)==BlockId::OakPlank &&
                TerrainSurvey::blockHash(manager.getChunk(cx,cz))==hash;
        }
        check("LOCAL25/edited-ground-save-reopen-"+std::to_string(seed),persisted);
    }
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
