#pragma once

#include "../World/Generation/Structures/LandmarkPolish.h"

namespace {
using LandmarkPoint = std::tuple<int, int, int>;

std::set<LandmarkPoint> landmarkWalkableCells(
    const std::vector<GeneratedStructureChunkSample>& chunks,
    const StructurePlanSnapshot& plan)
{
    const auto& f = plan.footprint;
    const auto open = [&](int x, int y, int z) {
        return generatedLandmarkBlock(chunks,x,y,z) == BlockId::Air;
    };
    std::set<LandmarkPoint> walkable;
    for(int x=f.minimumX;x<=f.maximumX;++x)
        for(int z=f.minimumZ;z<=f.maximumZ;++z)
            for(int y=plan.anchor.y;y<=f.maximumY;++y) {
                const auto floor=generatedLandmarkBlock(chunks,x,y,z);
                if(floor!=BlockId::Air && floor!=BlockId::Water && floor!=BlockId::NUM_TYPES &&
                   open(x,y+1,z) && open(x,y+2,z)) walkable.emplace(x,y,z);
            }
    std::set<LandmarkPoint> reached;
    std::queue<LandmarkPoint> pending;
    for(const auto& point:walkable)
        if(std::get<0>(point)==plan.anchor.x && std::get<2>(point)==f.minimumZ) {
            reached.insert(point);pending.push(point);
        }
    while(!pending.empty()) {
        const auto [x,y,z]=pending.front();pending.pop();
        for(const glm::ivec2 d:{glm::ivec2(1,0),{-1,0},{0,1},{0,-1}})
            for(int rise=-1;rise<=1;++rise) {
                LandmarkPoint next{x+d.x,y+rise,z+d.y};
                // A one-block rise is a jump route, with head clearance at
                // the higher transition plane; do not call it flat walking.
                const int clearance=y+std::max(0,rise)+2;
                if(walkable.count(next) && open(x,clearance,z) &&
                   open(x+d.x,clearance,z+d.y) && reached.insert(next).second)
                    pending.push(next);
            }
    }
    return reached;
}

void caseLandmarkPolishV27()
{
    check("LANDMARK27/appended-version-and-save-boundary",
        WaterbankPolishTerrainGenerationVersion==26 && LandmarkPolishTerrainGenerationVersion==27 &&
        CurrentTerrainGenerationVersion>=27 && WorldSaveFormatVersion==12);
    setEnv("HELLOMINE3D_SEED","0");setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig();Camera camera(config);Player player;
    World world(camera,config,player,freshSaveDirectory("landmark27_projection"),false,0);
    for(int seed:{42,20260807,239701883}) {
        ClassicOverWorldGenerator current(seed,27),previous(seed,26);
        std::array<std::array<StructurePlanSnapshot,2>,3> layouts{};
        bool all=false;
        for(int radius=0;radius<=24 && !all;++radius) {
            for(int cx=-radius;cx<=radius;++cx)for(int cz=-radius;cz<=radius;++cz) {
                if(std::max(std::abs(cx),std::abs(cz))!=radius)continue;
                for(int type=0;type<3;++type) {
                    const auto plan=current.getStructurePlanForCell(static_cast<StructureType>(type),cx,cz);
                    if(!plan.valid)continue;
                    auto& selected=layouts[type][LandmarkExpedition::layoutFor(plan)];
                    if(selected.valid)continue;
                    const auto plans=current.getStructurePlansForChunk(WorldCoordinates::floorDiv(plan.anchor.x,16),WorldCoordinates::floorDiv(plan.anchor.z,16));
                    if(std::any_of(plans.begin(),plans.end(),[&](const auto& p){return p.key==plan.key;}))selected=plan;
                }
            }
            all=true;for(const auto& pair:layouts)all &= pair[0].valid && pair[1].valid;
        }
        check("LANDMARK27/six-real-layouts-"+std::to_string(seed),all);
        if(!all)continue;
        for(int type=0;type<3;++type)for(int layout=0;layout<2;++layout) {
            const auto& plan=layouts[type][layout];const auto& f=plan.footprint;
            const auto prior=previous.getStructurePlanForCell(plan.key.type,plan.key.cellX,plan.key.cellZ);
            const auto loot=structureLootForPlan(plan,ExplorationRewards::CurrentVersion);
            const std::string label="LANDMARK27/"+std::to_string(seed)+"-"+std::to_string(type)+"-"+std::to_string(layout);
            check(label+"-same-candidate-footprint-and-reward",prior.valid && prior.anchor==plan.anchor &&
                prior.selectionHash==plan.selectionHash && prior.key==plan.key && prior.plannedBlockCount==plan.plannedBlockCount &&
                prior.footprint.minimumX==f.minimumX && prior.footprint.maximumX==f.maximumX &&
                prior.footprint.minimumY==f.minimumY && prior.footprint.maximumY==f.maximumY &&
                prior.footprint.minimumZ==f.minimumZ && prior.footprint.maximumZ==f.maximumZ &&
                structureLootForPlan(prior,ExplorationRewards::CurrentVersion).entries==loot.entries);
            const auto forward=sampleStructureChunks(world,current,plan,false);
            const auto reverse=sampleStructureChunks(world,current,plan,true);
            bool exact=sameGeneratedStructureSamples(forward,reverse);
            int solidCount=0,oldSolidCount=0,changed=0,writes=0;
            std::array<int,static_cast<int>(BlockId::NUM_TYPES)> counts{};
            std::set<LandmarkPoint> solids,supported;
            std::queue<LandmarkPoint> pending;
            for(int x=f.minimumX;x<=f.maximumX;++x)for(int z=f.minimumZ;z<=f.maximumZ;++z)
                for(int y=f.minimumY;y<=f.maximumY;++y) {
                    const auto b=generatedLandmarkBlock(forward,x,y,z);
                    const auto old=LandmarkExpedition::blockAt(plan,x-plan.anchor.x,y-plan.anchor.y,z-plan.anchor.z);
                    exact &= b==LandmarkPolish::blockAt(plan,x-plan.anchor.x,y-plan.anchor.y,z-plan.anchor.z);
                    ++writes;changed+=b!=old;oldSolidCount+=old!=BlockId::Air;
                    if(b<BlockId::NUM_TYPES)++counts[static_cast<int>(b)];
                    if(b!=BlockId::Air && b<BlockId::NUM_TYPES) {
                        ++solidCount;LandmarkPoint point{x,y,z};solids.insert(point);
                        if(y==f.minimumY){supported.insert(point);pending.push(point);}
                    }
                }
            while(!pending.empty()) {
                const auto [x,y,z]=pending.front();pending.pop();
                for(const glm::ivec3 d:{glm::ivec3(1,0,0),{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}) {
                    LandmarkPoint next{x+d.x,y+d.y,z+d.z};
                    if(solids.count(next) && supported.insert(next).second)pending.push(next);
                }
            }
            check(label+"-actual-projection-and-order",exact);
            check(label+"-supported-bounded-change",supported.size()==solids.size() &&
                changed>=5 && solidCount<=oldSolidCount+64 && writes<=static_cast<int>(plan.plannedBlockCount));
            check(label+"-unchanged-resources-and-interactions",
                counts[int(BlockId::Chest)]==(type==0?0:1) && counts[int(BlockId::WaystoneCore)]==(type==0?1:0) &&
                counts[int(BlockId::IronOre)]==(type==0?1:type==1?4:0) && counts[int(BlockId::CoalOre)]==(type==0?4:type==2?1:0) &&
                counts[int(BlockId::Furnace)]==0 && counts[int(BlockId::Crusher)]==0);
            if(plan.hasChest)check(label+"-actual-chest-payload",generatedChestMatchesLoot(forward,plan,loot));
            if(type==2) {
                bool floor=true;
                for(int x=f.minimumX;x<=f.maximumX;++x)for(int z=f.minimumZ;z<=f.maximumZ;++z)
                    if(std::abs(x-plan.anchor.x)>1)
                        floor &= generatedLandmarkBlock(forward,x,plan.anchor.y+1,z)==
                            LandmarkExpedition::blockAt(plan,x-plan.anchor.x,1,z-plan.anchor.z);
                check(label+"-original-side-floor-retained",floor);
            }
            const auto reached=landmarkWalkableCells(forward,plan);
            const int targetZ=plan.anchor.z+(type==2?0:-1);
            check(label+"-entrance-reaches-interaction",reached.count({plan.anchor.x,plan.anchor.y+1,targetZ})!=0);
            if(seed==42 && type==2 && layout==0) {
                // Faults affect copies of actual production chunks. Verify the
                // path oracle rejects a sealed entrance and missing headroom.
                auto faulty=forward;
                const auto put=[&](int x,int y,int z,BlockId block) {
                    for(auto& chunk:faulty) {
                        if(chunk.x!=WorldCoordinates::floorDiv(x,CHUNK_SIZE) ||
                           chunk.z!=WorldCoordinates::floorDiv(z,CHUNK_SIZE))continue;
                        const std::size_t index=std::size_t(y/CHUNK_SIZE)*CHUNK_VOLUME+
                            std::size_t(y%CHUNK_SIZE)*CHUNK_SIZE*CHUNK_SIZE+
                            std::size_t(WorldCoordinates::floorMod(z,CHUNK_SIZE))*CHUNK_SIZE+
                            std::size_t(WorldCoordinates::floorMod(x,CHUNK_SIZE));
                        chunk.blocks.at(index)=static_cast<Block_t>(block);
                    }
                };
                put(plan.anchor.x,plan.anchor.y+3,targetZ,BlockId::Stone);
                check("LANDMARK27/reject-one-block-headroom",
                    landmarkWalkableCells(faulty,plan).count({plan.anchor.x,plan.anchor.y+1,targetZ})==0);
                faulty=forward;
                for(int x=f.minimumX;x<=f.maximumX;++x)
                    for(int y=plan.anchor.y;y<=f.maximumY+2;++y)
                        put(x,y,f.minimumZ,BlockId::Stone);
                check("LANDMARK27/reject-sealed-entrance",landmarkWalkableCells(faulty,plan).empty());
            }
            if(type==2 && layout==1) {
                const int side=(plan.selectionHash&1ull)?-1:1;
                check(label+"-original-lookout-route",reached.count({plan.anchor.x+side*4,plan.anchor.y+4,plan.anchor.z-1})!=0);
            }
            if(type==1) {
                bool ores=true;
                for(int ox:{-2,2})for(int oz:{-2,2}) {
                    bool near=false;
                    for(const auto& pt:reached)near |= std::abs(std::get<0>(pt)-(plan.anchor.x+ox))+std::abs(std::get<2>(pt)-(plan.anchor.z+oz))<=1 && std::abs(std::get<1>(pt)-(plan.anchor.y+1))<=1;
                    ores &= near;
                }
                check(label+"-courtyard-reaches-all-four-ores",ores);
            }
            std::cout<<"[LANDMARK27_SITE] seed="<<seed<<" type="<<type<<" layout="<<layout
                <<" cell="<<plan.key.cellX<<','<<plan.key.cellZ<<" anchor="<<plan.anchor.x<<','<<plan.anchor.y<<','<<plan.anchor.z
                <<" blocks="<<solidCount<<" old="<<oldSolidCount<<" changed="<<changed<<" reached="<<reached.size()<<'\n';
            for(int dz:{-12,-6,-5,-3}) {
                const int x=plan.anchor.x,z=plan.anchor.z+dz;
                std::cout<<"[LANDMARK27_VIEW] seed="<<seed<<" type="<<type<<" layout="<<layout<<" x="<<x<<" z="<<z
                    <<" ground="<<current.getSurfaceHeightAtWorld(x,z)<<" old="<<previous.getSurfaceHeightAtWorld(x,z)<<'\n';
            }
        }
    }
    const auto directory=freshSaveDirectory("landmark27_edit_reopen");
    check("LANDMARK27/initialize-save",initializeTerrainIdentity(directory,"landmark27-edit",27,42));
    bool saved=false;std::uint64_t hash=0;
    {
        Player owner;World created(camera,config,owner,directory,false,0);auto& manager=created.getChunkManager();manager.loadChunk(1,7);
        // Actual seed42 camp chest: removal and an adjacent player addition
        // must survive; the new generator cannot refill a reward on reload.
        check("LANDMARK27/edit-removes-real-generated-chest",created.getBlock(23,117,113)==BlockId::Chest);
        created.setBlock(23,117,113,BlockId::Air);created.setBlock(24,117,113,BlockId::Glass);
        hash=TerrainSurvey::blockHash(manager.getChunk(1,7));saved=created.save();
    }
    {
        Player owner;World reopened(camera,config,owner,directory,false,0);auto& manager=reopened.getChunkManager();manager.loadChunk(1,7);
        saved &= manager.getTerrainGenerationVersion()==27 && reopened.getBlock(23,117,113)==BlockId::Air &&
            reopened.getBlock(24,117,113)==BlockId::Glass && hash==TerrainSurvey::blockHash(manager.getChunk(1,7));
    }
    check("LANDMARK27/removal-and-player-edit-survive-reopen",saved);
    clearDeterministicEnv();
}
}
