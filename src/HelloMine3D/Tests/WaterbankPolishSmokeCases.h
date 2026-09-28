#pragma once

namespace {
void caseWaterbankPolishV26() {
    check("BANK26/version-and-save-boundary",WaterbankPolishTerrainGenerationVersion==26 &&
        LocalReliefTerrainGenerationVersion==25 && CurrentTerrainGenerationVersion==26 && WorldSaveFormatVersion==12);
    setEnv("HELLOMINE3D_SEED","42");setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig();Camera camera(config);Player owner;
    World world(camera,config,owner,freshSaveDirectory("bank26_projection"),false,0);
    const struct Site{int seed,x,z;} sites[]={{42,224,-192},{42,208,-144},{20260807,-464,48},
        {20260807,1248,-352},{239701883,240,-272},{239701883,-224,-336}};
    const BlockId materials[]={BlockId::Air,BlockId::Grass,BlockId::Dirt,BlockId::Sand,BlockId::Stone,
        BlockId::Snow,BlockId::Gravel,BlockId::Clay,BlockId::ForestFloor,BlockId::MossStone,BlockId::Silt};
    bool queries=true,order=true,water=true,plants=true;int columns=0,wet=0,changed=0;
    for(const auto site:sites) {
        ClassicOverWorldGenerator gen(site.seed,26),reverse(site.seed,26);AdventureEcologyPlanner ecology(site.seed,26),old(site.seed,25);
        CaveGenerator caves(site.seed,26);
        const auto surface=[&](int x,int z){return gen.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return gen.getBiomeAtWorld(x,z);};
        std::array<std::uint64_t,4> hashes{};int mismatches=0;
        for(int i=0;i<4;++i) {
            const glm::ivec2 location{WorldCoordinates::floorDiv(site.x,16)+i%2,WorldCoordinates::floorDiv(site.z,16)+i/2};
            Chunk chunk(world,location,false),mask(world,location,false);gen.generateTerrainFor(chunk);hashes[i]=TerrainSurvey::blockHash(chunk);
            for(int x=0;x<16;++x)for(int z=0;z<16;++z)mask.setBlock(x,surface(location.x*16+x,location.y*16+z),z,BlockId::Stone);
            caves.carveNaturalEntrances(mask,surface,biome);const auto plans=gen.getStructurePlansForChunk(location.x,location.y,6);
            for(int x=0;x<16;++x)for(int z=0;z<16;++z) {
                const int wx=location.x*16+x,wz=location.y*16+z;const auto c=ecology.sample(wx,wz).column;
                queries &= surface(wx,wz)==c.height && biome(wx,wz)==c.biome;changed+=old.sampleWaterColumn(wx,wz).height!=c.height;
                bool structure=false;for(const auto &p:plans)structure|=p.valid && wx>=p.footprint.minimumX-6 && wx<=p.footprint.maximumX+6 && wz>=p.footprint.minimumZ-6 && wz<=p.footprint.maximumZ+6;
                if(structure || mask.getBlock(x,c.height,z)==BlockId::Air)continue;
                const auto actual=static_cast<BlockId>(chunk.getBlock(x,c.height,z).id),wanted=materials[int(c.surface)];
                const bool ore=wanted==BlockId::Stone && (actual==BlockId::CoalOre || actual==BlockId::IronOre);
                const bool match=actual==wanted || ore;
                if(!match && mismatches<3)std::cout<<"[BANK26_MISMATCH] "<<site.seed<<' '<<wx<<' '<<c.height<<' '<<wz<<" wanted="<<int(wanted)<<" actual="<<int(actual)<<'\n';
                mismatches+=!match;++columns;
                if(c.height<64) {
                    ++wet;for(int y=c.height+1;y<=64;++y)water &= chunk.getBlock(x,y,z)==BlockId::Water;
                    const auto plant=static_cast<BlockId>(chunk.getBlock(x,65,z).id);
                    plants &= plant!=BlockId::OakBark && plant!=BlockId::TallGrass && plant!=BlockId::Rose && plant!=BlockId::DeadShrub;
                }
            }
        }
        for(int i=3;i>=0;--i) {
            Chunk chunk(world,{WorldCoordinates::floorDiv(site.x,16)+i%2,WorldCoordinates::floorDiv(site.z,16)+i/2},false);
            reverse.generateTerrainFor(chunk);order &= hashes[i]==TerrainSurvey::blockHash(chunk);
        }
        check("BANK26/production-bed-"+std::to_string(site.seed)+"-"+std::to_string(site.x)+"-"+std::to_string(site.z),mismatches==0,"mismatches="+std::to_string(mismatches));
    }
    check("BANK26/shared-shape-and-actual-material",queries && columns>5000 && changed>300);
    check("BANK26/reverse-generation",order);check("BANK26/water-column-to-real-sea-level",water && wet>1000);
    check("BANK26/no-land-plants-in-water",plants);
    std::cout<<"[BANK26_PRODUCTION] columns="<<columns<<" wet="<<wet<<" changed="<<changed<<'\n';
    for(int version:{25,26}) {
        const auto directory=freshSaveDirectory("bank26_edit_"+std::to_string(version));
        check("BANK26/initialize-version-"+std::to_string(version),initializeTerrainIdentity(directory,"waterbank-"+std::to_string(version),version,42));
        bool saved=false;std::uint64_t hash=0;
        {
            Player player;World created(camera,config,player,directory,false,0);auto &manager=created.getChunkManager();manager.loadChunk(14,-12);
            created.setBlock(224,63,-192,BlockId::OakPlank);created.setBlock(225,64,-192,BlockId::Air);
            hash=TerrainSurvey::blockHash(manager.getChunk(14,-12));saved=created.save();
        }
        {
            Player player;World reopened(camera,config,player,directory,false,0);auto &manager=reopened.getChunkManager();manager.loadChunk(14,-12);
            saved &= manager.getTerrainGenerationVersion()==version && reopened.getBlock(224,63,-192)==BlockId::OakPlank &&
                reopened.getBlock(225,64,-192)==BlockId::Air && TerrainSurvey::blockHash(manager.getChunk(14,-12))==hash;
        }
        check("BANK26/edited-bank-save-reopen-"+std::to_string(version),saved);
    }
    clearDeterministicEnv();
    for(int seed:{42,20260807,239701883}) {
        setEnv("HELLOMINE3D_SEED",std::to_string(seed));
        const auto directory=freshSaveDirectory("bank26_spawn_"+std::to_string(seed));
        glm::vec3 spawn{0};bool saved=false;
        {
            Player player;World created(camera,config,player,directory,false,1);
            spawn=created.getPlayerSpawnPoint();
            const int x=World::toBlockCoord(spawn.x),y=World::toBlockCoord(spawn.y),z=World::toBlockCoord(spawn.z);
            const auto floor=created.getBlock(x,y-2,z);
            check("BANK26/normal-new-world-spawn-"+std::to_string(seed),floor.getData().isCollidable && floor!=BlockId::Water &&
                created.getBlock(x,y-1,z)==BlockId::Air && created.getBlock(x,y,z)==BlockId::Air &&
                created.getChunkManager().getTerrainGenerationVersion()==26,vecToString(spawn));
            saved=created.save();
        }
        {
            Player player;World reopened(camera,config,player,directory,false,0);
            saved &= reopened.getChunkManager().getTerrainGenerationVersion()==26 &&
                glm::length(reopened.getPlayerSpawnPoint()-spawn)<.001f && glm::length(player.position-spawn)<.001f;
        }
        check("BANK26/new-world-spawn-save-reopen-"+std::to_string(seed),saved);
    }
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
