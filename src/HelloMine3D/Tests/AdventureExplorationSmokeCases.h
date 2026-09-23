#pragma once
#include "../World/Block/ForestFernGeometry.h"
#include <queue>

namespace {
void caseAdventureExplorationV19() {
    check("ADVENTURE_EXPLORE/v19-remains-supported",
        CurrentTerrainGenerationVersion >= 19 &&
        AdventureExplorationTerrainGenerationVersion == 19 &&
        AdventureEcologyTerrainGenerationVersion == 18);
    clearDeterministicEnv();Config config=makeConfig();Camera camera(config);
    const auto evidence=freshSaveDirectory("adventure_exploration_routes");
    std::ofstream summary(std::filesystem::path(evidence)/"summary.csv");
    summary << "seed,spawn_x,spawn_y,spawn_z,wood,stone,visited,core_regions,water_route_m,waystone_route_m,ruin_route_m,camp_route_m\n";
    for(int seed:{0,1,42,424,20260807,20260809,8675309,325322}) {
        setEnv("HELLOMINE3D_SEED",std::to_string(seed));Player player;
        World world(camera,config,player,freshSaveDirectory("adventure_exploration_"+std::to_string(seed)),false,1);
        auto &manager=world.getChunkManager();auto &generator=manager.getTerrainGenerator();const auto spawn=world.getPlayerSpawnPoint();
        const int sx=World::toBlockCoord(spawn.x),sy=World::toBlockCoord(spawn.y),sz=World::toBlockCoord(spawn.z);
        const auto floor=world.getBlock(sx,sy-2,sz);
        check("ADVENTURE_EXPLORE/safe-normal-spawn-"+std::to_string(seed),floor.getData().isCollidable && floor!=BlockId::Water && world.getBlock(sx,sy-1,sz)==BlockId::Air && world.getBlock(sx,sy,sz)==BlockId::Air);
        int wood=0,stone=0;
        for(int cx=WorldCoordinates::floorDiv(sx,16)-2;cx<=WorldCoordinates::floorDiv(sx,16)+2;++cx)
            for(int cz=WorldCoordinates::floorDiv(sz,16)-2;cz<=WorldCoordinates::floorDiv(sz,16)+2;++cz) {
                manager.loadChunk(cx,cz);const auto &chunk=manager.getChunk(cx,cz);
                for(int x=0;x<16;++x)for(int z=0;z<16;++z) {
                    const int wx=cx*16+x,wz=cz*16+z,top=chunk.getHeightAt(x,z);
                    if(std::abs(wx-sx)>32 || std::abs(wz-sz)>32)continue;
                    for(int y=std::max(1,top-14);y<=top;++y) {
                        const auto id=static_cast<BlockId>(chunk.getBlock(x,y,z).id);
                        wood+=id==BlockId::OakBark;stone+=id==BlockId::Stone || id==BlockId::MossStone;
                    }
                }
            }
        check("ADVENTURE_EXPLORE/nearby-real-wood-and-stone-"+std::to_string(seed),wood>=11 && stone>0,"wood="+std::to_string(wood)+" stone="+std::to_string(stone));
        // A bounded land-route survey. Every four-metre edge checks all four
        // intervening columns and allows only one-block steps, not jumps across
        // cliffs. This is planning evidence, not actual player collision/input.
        constexpr int Step=4,Radius=512,N=Radius*2/Step+1;
        const int ox=sx-Radius,oz=sz-Radius;
        std::vector<int> height(N*N),distance(N*N,-1),parent(N*N,-1);
        const auto index=[](int x,int z){return z*N+x;};
        for(int z=0;z<N;++z)for(int x=0;x<N;++x)height[index(x,z)]=generator.getSurfaceHeightAtWorld(ox+x*Step,oz+z*Step);
        std::queue<int> queue;const int start=index(Radius/Step,Radius/Step);distance[start]=0;queue.push(start);
        int waterDistance=-1,waterIndex=-1,visited=0;std::array<int,7> regions{};std::array<int,7> regionNodes{};regionNodes.fill(-1);
        AdventureTerrainPlanner regionsPlanner(seed);
        while(!queue.empty()) {
            const int from=queue.front();queue.pop();++visited;const int x=from%N,z=from/N,wx=ox+x*Step,wz=oz+z*Step;
            if(distance[from]<=768 && (x%4==0 && z%4==0)) {
                const auto region=regionsPlanner.sample(wx,wz);const int r=static_cast<int>(region.region);
                if(r<7 && region.weights[r]>.65) {++regions[r];if(regionNodes[r]<0)regionNodes[r]=from;}
            }
            for(const auto offset:{glm::ivec2{1,0},{-1,0},{0,1},{0,-1}}) {
                const int nx=x+offset.x,nz=z+offset.y;if(nx<0 || nz<0 || nx>=N || nz>=N)continue;
                const int to=index(nx,nz);if(height[to]<64) {
                    if(waterDistance<0){waterDistance=distance[from];waterIndex=from;}continue;
                }
                if(distance[to]>=0)continue;
                int previous=height[from];bool walkable=true;
                for(int n=1;n<=Step;++n) {const int h=generator.getSurfaceHeightAtWorld(wx+offset.x*n,wz+offset.y*n);walkable &= h>=64 && std::abs(h-previous)<=1;previous=h;}
                if(walkable) {distance[to]=distance[from]+Step;parent[to]=from;queue.push(to);}
            }
        }
        int cores=0;for(int count:regions)cores+=count>=12; // repeated 16 m samples, not one mixed edge label
        check("ADVENTURE_EXPLORE/connected-regional-cores-"+std::to_string(seed),cores>=2,"cores="+std::to_string(cores)+" nodes="+std::to_string(visited));
        check("ADVENTURE_EXPLORE/water-by-land-route-"+std::to_string(seed),waterDistance>=0 && waterDistance<=768,"metres="+std::to_string(waterDistance));
        ClassicOverWorldGenerator sites(seed,19);std::array<int,3> siteDistance{{-1,-1,-1}},siteNode{{-1,-1,-1}};
        const std::array<StructureType,3> types{{StructureType::Waystone,StructureType::Ruin,StructureType::RaiderCamp}};
        for(int cx=WorldCoordinates::floorDiv(sx-Radius,64);cx<=WorldCoordinates::floorDiv(sx+Radius,64);++cx)
            for(int cz=WorldCoordinates::floorDiv(sz-Radius,64);cz<=WorldCoordinates::floorDiv(sz+Radius,64);++cz)
                for(int t=0;t<3;++t) {
                    const auto p=sites.getStructurePlanForCell(types[t],cx,cz);if(!p.valid)continue;
                    const int px=WorldCoordinates::floorDiv(p.anchor.x-ox,Step),pz=WorldCoordinates::floorDiv(p.anchor.z-oz,Step);
                    for(int dz=-3;dz<=3;++dz)for(int dx=-3;dx<=3;++dx) {
                        if(std::abs(dx)<2 && std::abs(dz)<2)continue;
                        const int x=px+dx,z=pz+dz;if(x<0 || z<0 || x>=N || z>=N)continue;
                        const int at=index(x,z),d=distance[at];
                        if(d>=0 && (siteDistance[t]<0 || d<siteDistance[t])){siteDistance[t]=d;siteNode[t]=at;}
                    }
                }
        bool destinations=true;for(int d:siteDistance)destinations &= d>=0 && d<=768;
        check("ADVENTURE_EXPLORE/three-destination-approach-routes-"+std::to_string(seed),destinations,
            "waystone/ruin/camp="+std::to_string(siteDistance[0])+"/"+std::to_string(siteDistance[1])+"/"+std::to_string(siteDistance[2]));
        std::ofstream route(std::filesystem::path(evidence)/("seed-"+std::to_string(seed)+"-routes.csv"));route << "target,step,x,y,z\n";
        const auto writeRoute=[&](const std::string &name,int end) {
            if(end<0)return;std::vector<int> path;for(int at=end;at>=0;at=parent[at])path.push_back(at);std::reverse(path.begin(),path.end());
            for(std::size_t i=0;i<path.size();++i){const int at=path[i];route<<name<<','<<i<<','<<ox+(at%N)*Step<<','<<height[at]+2<<','<<oz+(at/N)*Step<<'\n';}
        };
        writeRoute("water",waterIndex);for(int r=0;r<7;++r)if(regions[r]>=12)writeRoute("region-"+std::to_string(r),regionNodes[r]);
        for(int t=0;t<3;++t)writeRoute("site-"+std::to_string(t),siteNode[t]);
        summary<<seed<<','<<sx<<','<<sy<<','<<sz<<','<<wood<<','<<stone<<','<<visited<<','<<cores<<','<<waterDistance;
        for(int d:siteDistance)summary<<','<<d;summary<<'\n';
        std::cout<<"[EXPLORATION_ROUTE] seed="<<seed<<" spawn="<<vecToString(spawn)<<" cores="<<cores<<" water="<<waterDistance<<" destinations="<<siteDistance[0]<<','<<siteDistance[1]<<','<<siteDistance[2]<<'\n';
    }
    setEnv("HELLOMINE3D_SEED","42");setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");Player owner;
    World world(camera,config,owner,freshSaveDirectory("adventure_v19_projection"),false,0);
    ClassicOverWorldGenerator old(42,18),current(42,19);AdventureEcologyPlanner ecology(42);
    bool snow=true,unchanged=true,ordered=true;int snowColumns=0;std::array<std::uint64_t,4> hashes{};
    for(int i=0;i<4;++i) {
        const glm::ivec2 loc{103+i%2,41+i/2};Chunk before(world,loc,false),after(world,loc,false);old.generateTerrainFor(before);current.generateTerrainFor(after);hashes[i]=TerrainSurvey::blockHash(after);
        for(int x=0;x<16;++x)for(int z=0;z<16;++z) {
            const int wx=loc.x*16+x,wz=loc.y*16+z,h=current.getSurfaceHeightAtWorld(wx,wz);
            unchanged &= h==old.getSurfaceHeightAtWorld(wx,wz) && current.getBiomeAtWorld(wx,wz)==old.getBiomeAtWorld(wx,wz);
            if(before.getBlock(x,h,z)==BlockId::Snow && before.getBlock(x,h-1,z)==BlockId::Stone && before.getBlock(x,h-2,z)==BlockId::Stone) {
                ++snowColumns;for(int depth=0;depth<3;++depth)snow &= after.getBlock(x,h-depth,z)==before.getBlock(x,h-depth,z);
            }
        }
    }
    for(int i=3;i>=0;--i){Chunk chunk(world,{103+i%2,41+i/2},false);current.generateTerrainFor(chunk);ordered &= hashes[i]==TerrainSurvey::blockHash(chunk);}
    check("ADVENTURE_EXPLORE/snow-geology-and-height-preserved",snow && unchanged && snowColumns>100,"columns="+std::to_string(snowColumns));
    check("ADVENTURE_EXPLORE/v19-reverse-snow-generation",ordered);
    int clearedRoots=0;bool entranceClear=true,entranceOrder=true;
    for(int seed:{0,1,42,424,20260807,20260809,8675309,325322}) {
        ClassicOverWorldGenerator beforeGen(seed,18),afterGen(seed,19);CaveGenerator cavePlanner(seed,19);AdventureEcologyPlanner vegetation(seed);
        const auto surface=[&](int x,int z){return afterGen.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return afterGen.getBiomeAtWorld(x,z);};
        bool found=false;
        for(int cz=-24;cz<=24 && !found;++cz)for(int cx=-24;cx<=24 && !found;++cx) {
            const auto entrance=cavePlanner.getNaturalEntranceForCell(cx,cz,surface,biome);if(!entrance.valid)continue;
            for(int along=-4;along<=CaveGenerator::EntranceTunnelLength+4 && !found;++along)
                for(int lateral=-4;lateral<=4 && !found;++lateral) {
                    const int x=entrance.anchorX+entrance.directionX*along-entrance.directionZ*lateral;
                    const int z=entrance.anchorZ+entrance.directionZ*along+entrance.directionX*lateral;
                    if(!vegetation.treeAnchor(x,z))continue;
                    const auto sample=vegetation.sample(x,z);const auto tree=vegetation.tree(x,z,sample);
                    if(tree.kind==AdventureTreeKind::None)continue;
                    const glm::ivec2 loc{WorldCoordinates::floorDiv(x,16),WorldCoordinates::floorDiv(z,16)};
                    const int lx=WorldCoordinates::floorMod(x,16),lz=WorldCoordinates::floorMod(z,16);
                    Chunk before(world,loc,false),after(world,loc,false);beforeGen.generateTerrainFor(before);
                    if(before.getBlock(lx,tree.height,lz).id!=static_cast<Block_t>(BlockId::OakBark))continue;
                    afterGen.generateTerrainFor(after);++clearedRoots;found=true;
                    entranceClear &= after.getBlock(lx,tree.height,lz).id!=static_cast<Block_t>(BlockId::OakBark);
                    const auto hash=TerrainSurvey::blockHash(after);
                    Chunk neighbour(world,{loc.x+1,loc.y-1},false),again(world,loc,false);afterGen.generateTerrainFor(neighbour);afterGen.generateTerrainFor(again);
                    entranceOrder &= hash==TerrainSurvey::blockHash(again);
                    std::cout << "[ENTRANCE_CLEARANCE] seed=" << seed << " root=" << x << ',' << tree.height << ',' << z << " mouth=" << entrance.anchorX << ',' << entrance.anchorY << ',' << entrance.anchorZ << '\n';
                }
        }
        check("ADVENTURE_EXPLORE/entrance-root-clearance-"+std::to_string(seed),found && entranceClear);
    }
    check("ADVENTURE_EXPLORE/entrance-clearance-reverse-generation",entranceOrder && clearedRoots==8);
    const auto &grass=BlockDatabase::get().getDefinition(BlockId::TallGrass);
    auto custom=grass.render.shape;custom.faces[0][0]=.15f;
    check("ADVENTURE_EXPLORE/custom-cross-keeps-resource-geometry",ForestFernGeometry::applies(ChunkBlock(BlockId::TallGrass,2),grass.render.shape) && !ForestFernGeometry::applies(ChunkBlock(BlockId::TallGrass,2),custom));
    // An explicit v19 world retains its original generation and edits after
    // the default version advances; this is a compatibility assertion.
    const auto directory=freshSaveDirectory("adventure_v19_save");
    bool saved=initializeTerrainIdentity(directory,"adventure-v19",19,42);
    std::uint64_t hash=0;
    {Player player;World actual(camera,config,player,directory,false,0);actual.getChunkManager().loadChunk(103,41);actual.setBlock(1650,140,660,BlockId::OakPlank);hash=TerrainSurvey::blockHash(actual.getChunkManager().getChunk(103,41));saved &= actual.save() && actual.getChunkManager().getTerrainGenerationVersion()==19;}
    {Player player;World actual(camera,config,player,directory,false,0);actual.getChunkManager().loadChunk(103,41);saved &= actual.getChunkManager().getTerrainGenerationVersion()==19 && actual.getBlock(1650,140,660)==BlockId::OakPlank && hash==TerrainSurvey::blockHash(actual.getChunkManager().getChunk(103,41));}
    check("ADVENTURE_EXPLORE/old-v19-save-reopen",saved);
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
