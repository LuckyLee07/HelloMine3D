#pragma once

namespace {
void caseWaterMotionPresentation() {
    struct Climate final : TerrainGenerator {
        TerrainBiome biome=TerrainBiome::Ocean;mutable int queries=0;bool varying=false;
        void generateTerrainFor(Chunk &) override {}
        int getMinimumSpawnHeight() const noexcept override{return 65;}
        int getGenerationVersion() const noexcept override{return 26;}
        int getSurfaceHeightAtWorld(int,int) const noexcept override{return 60;}
        TerrainBiome getBiomeAtWorld(int,int) const noexcept override{return biome;}
        std::array<float,2> getWaterSurfaceVelocityAtWorld(int x,int z) const noexcept override {
            ++queries;if(varying)return {.30f+.06f*std::sin(x*.07f),.22f+.04f*std::sin(z*.09f)};
            return biome==TerrainBiome::River?std::array<float,2>{-.36f,.48f}:
                TerrainGenerator::getWaterSurfaceVelocityAtWorld(x,z);
        }
    } climate;
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig();Camera camera(config);Player player;
    World world(camera,config,player,freshSaveDirectory("water_motion"),false,0);auto &manager=world.getChunkManager();
    constexpr int westCell=-6000,northCell=-6000;
    constexpr int seamX=(westCell+1)*16,seamZ=northCell*16+8;
    for(int z=northCell;z<=northCell+1;++z)for(int x=westCell;x<=westCell+1;++x) {
        auto &c=manager.getOrCreateChunk(x,z);c.transitionDataResidency(ChunkDataResidencyState::Requested);
        c.transitionDataResidency(ChunkDataResidencyState::Loading);
        std::vector<Block_t> ids(CHUNK_VOLUME*5,0);std::vector<BlockMetadata_t> metadata(ids.size(),0);c.loadBlockData(5,ids,metadata);
    }
    auto &west=*manager.findChunk(westCell,northCell),&east=*manager.findChunk(westCell+1,northCell);const auto count=manager.getChunks().size();
    SectionMeshInput dry;dry.capture(*west.findSection(4),climate,42);
    check("WATER_MOTION/dry-snapshot-zero-motion-queries",climate.queries==0 && !dry.containsWater());
    const auto fill=[&](BlockId block) {
        for(int cz=northCell;cz<=northCell+1;++cz)for(int cx=westCell;cx<=westCell+1;++cx)for(int z=0;z<16;++z)for(int x=0;x<16;++x)
            manager.findChunk(cx,cz)->setBlock(x,64,z,block);
    };
    const auto build=[](const SectionMeshInput &input){ChunkMeshCollection meshes;ChunkMeshBuilder(input,meshes).buildMesh();return meshes;};
    const auto at=[](const Mesh &mesh,int x,int y,int z,glm::vec2 expected) {
        int hits=0;bool equal=true;
        for(std::size_t i=0;i<mesh.vertexPositions.size()/3;++i)if(mesh.vertexPositions[i*3]==x && mesh.vertexPositions[i*3+1]==y && mesh.vertexPositions[i*3+2]==z) {
            ++hits;equal &= std::abs(mesh.textureCoords[i*2]-expected.x)<.00001f && std::abs(mesh.textureCoords[i*2+1]-expected.y)<.00001f;
        }
        return hits>0 && equal;
    };
    for(const auto biome:{TerrainBiome::Ocean,TerrainBiome::River,TerrainBiome::Lake,TerrainBiome::Wetland}) {
        climate.biome=biome;fill(BlockId::Water);const auto velocity=climate.getWaterSurfaceVelocityAtWorld(0,0);
        const glm::vec2 expected{velocity[0],velocity[1]};const auto tag="WATER_MOTION/"+std::to_string(int(biome))+"/";
        SectionMeshInput a,b,lower;climate.queries=0;a.capture(*west.findSection(4),climate,42);
        check(tag+"bounded-snapshot-query-count",climate.queries>0 && climate.queries<=18*18);
        b.capture(*east.findSection(4),climate,42);lower.capture(*west.findSection(3),climate,42);
        check(tag+"copied-surface-and-section-halo",a.getWaterSurfaceVelocity(15,0,8)==expected &&
            lower.getWaterSurfaceVelocity(15,16,8)==expected && a.getWaterSurfaceVelocity(-1,0,8)==glm::vec2(0.f));
        const auto am=build(a),bm=build(b);
        check(tag+"negative-chunk-corner-velocity",at(am.waterMesh.getClientMesh(),seamX,65,seamZ,expected) && at(bm.waterMesh.getClientMesh(),seamX,65,seamZ,expected));
        const auto revision=west.findSection(4)->getBlockRevision();
        world.setBlock(seamX-1,64,seamZ-1,BlockId::Sand);world.setBlock(seamX,64,seamZ-1,BlockId::Sand);
        SectionMeshInput changed,changedEast;changed.capture(*west.findSection(4),climate,42);changedEast.capture(*east.findSection(4),climate,42);
        const glm::vec2 along{expected.x*.5f,0.f};
        check(tag+"edited-bank-constrains-shared-flow",west.findSection(4)->getBlockRevision()>revision &&
            at(build(changed).waterMesh.getClientMesh(),seamX,65,seamZ,along) && at(build(changedEast).waterMesh.getClientMesh(),seamX,65,seamZ,along));
        check(tag+"old-snapshot-independent",at(build(a).waterMesh.getClientMesh(),seamX,65,seamZ,expected));
        fill(BlockId::Air);climate.queries=0;changed.capture(*west.findSection(4),climate,42);
        check(tag+"remove-water-clears-motion-without-query",climate.queries==0 && changed.getWaterSurfaceVelocity(15,0,8)==glm::vec2(0.f));
    }
    // Positive flow coordinates can numerically land in a plant-atlas tile.
    // Water UVs carry a physical vector and must never undergo tint encoding.
    climate.varying=true;climate.biome=TerrainBiome::River;fill(BlockId::Water);
    SectionMeshInput varying,varyingEast;varying.capture(*west.findSection(4),climate,42);varyingEast.capture(*east.findSection(4),climate,42);
    const auto vm=build(varying),ve=build(varyingEast);bool raw=true;
    for(int z=1;z<=16;++z)for(int x=1;x<=16;++x) {
        glm::vec2 expected(0.f);
        for(int dz=-1;dz<=0;++dz)for(int dx=-1;dx<=0;++dx)expected+=varying.getWaterSurfaceVelocity(x+dx,0,z+dz)*.25f;
        raw &= at(vm.waterMesh.getClientMesh(),westCell*16+x,65,northCell*16+z,expected);
        if(x==16)raw &= at(ve.waterMesh.getClientMesh(),westCell*16+x,65,northCell*16+z,expected);
    }
    check("WATER_MOTION/atlas-like-vectors-preserved-at-every-corner",raw);
    check("WATER_MOTION/resident-only-no-chunk-load",manager.getChunks().size()==count);
    bool found[4]={};int probes=0;
    for(int seed:{42,20260807,239701883}) {
        ClassicOverWorldGenerator generator(seed,26);AdventureWaterPlanner graph(seed,26);
        for(int z=-2048;z<=2048;z+=16)for(int x=-2048;x<=2048;x+=16) {
            const auto biome=generator.getBiomeAtWorld(x,z);int kind=biome==TerrainBiome::River?0:biome==TerrainBiome::Lake?1:biome==TerrainBiome::Ocean?2:biome==TerrainBiome::Wetland?3:-1;
            if(kind<0 || found[kind] || generator.getSurfaceHeightAtWorld(x,z)>=64)continue;
            const auto actual=generator.getWaterSurfaceVelocityAtWorld(x,z);
            const auto expected=kind==0?graph.surfaceFlow(x,z):kind==1?std::array<float,2>{.16f,.12f}:kind==2?std::array<float,2>{.8f,.6f}:std::array<float,2>{.04f,.03f};
            check("WATER_MOTION/production-biome-"+std::to_string(kind),actual==expected && std::hypot(actual[0],actual[1])>0);
            std::cout<<"[WATER_MOTION_SITE] "<<seed<<' '<<kind<<' '<<x<<' '<<z<<" velocity="<<actual[0]<<','<<actual[1]<<'\n';found[kind]=true;++probes;
        }
    }
    check("WATER_MOTION/all-four-real-water-domains",probes==4);
    clearDeterministicEnv();
}
}
