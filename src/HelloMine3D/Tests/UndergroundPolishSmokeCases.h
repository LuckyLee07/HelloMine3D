#pragma once

#include "../World/Generation/Terrain/UndergroundPolish.h"

void caseUndergroundPolishV28()
{
    check("UNDERGROUND28/appended-generation-save-unchanged",
          LandmarkPolishTerrainGenerationVersion==27 && UndergroundPolishTerrainGenerationVersion==28 &&
          CurrentTerrainGenerationVersion==28 && WorldSaveFormatVersion==12);
    setEnv("HELLOMINE3D_SEED","0"); setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig(); Camera camera(config); Player player;
    World owner(camera,config,player,freshSaveDirectory("underground28_projection"),false,0);
    for (const auto fixture : {glm::ivec3(42,11,2),glm::ivec3(20260807,-5,-5),glm::ivec3(239701883,3,-6)}) {
        const int seed=fixture.x;
        ClassicOverWorldGenerator previous(seed,27),current(seed,28);
        CaveGenerator priorCaves(seed,27),caves(seed,28);
        const auto surface=[&](int x,int z){return current.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return current.getBiomeAtWorld(x,z);};
        const auto plan=caves.getAdventureUndergroundPlanForCell(fixture.y,fixture.z,surface,biome);
        const auto before=priorCaves.getAdventureUndergroundPlanForCell(fixture.y,fixture.z,surface,biome);
        const std::string label="UNDERGROUND28/"+std::to_string(seed);
        check(label+"-current-frozen-cell-plan-valid",plan.valid && before.valid);
        if (!plan.valid || !before.valid) continue;
        check(label+"-planning-identity-preserved",plan.stableKey==before.stableKey && plan.layout==before.layout &&
              plan.destinationX==before.destinationX && plan.destinationZ==before.destinationZ && plan.riftEndAirY==before.riftEndAirY &&
              plan.chamberX==before.chamberX && plan.chamberZ==before.chamberZ && plan.chamberAirY==before.chamberAirY &&
              plan.entrance.anchorX==before.entrance.anchorX && plan.entrance.anchorY==before.entrance.anchorY &&
              plan.entrance.anchorZ==before.entrance.anchorZ);
        std::cout<<"[UNDERGROUND28_PLAN] seed="<<seed<<" layout="<<int(plan.layout)
                 <<" entrance="<<plan.entrance.anchorX<<','<<plan.entrance.anchorY<<','<<plan.entrance.anchorZ
                 <<" direction="<<plan.entrance.directionX<<','<<plan.entrance.directionZ
                 <<" chamber="<<plan.chamberX<<','<<plan.chamberAirY<<','<<plan.chamberZ
                 <<" destination="<<plan.destinationX<<','<<plan.riftEndAirY<<','<<plan.destinationZ
                 <<" rift="<<plan.riftDirectionX<<','<<plan.riftDirectionZ
                 <<" pool="<<plan.poolDirectionX<<','<<plan.poolDirectionZ<<'\n';
        const auto locations=adventureUndergroundLocations(plan);
        const auto old=generateAdventureUndergroundChunks(owner,previous,locations,false);
        const auto generated=generateAdventureUndergroundChunks(owner,current,locations,false);
        const auto reverse=generateAdventureUndergroundChunks(owner,current,locations,true);
        bool deterministic=true,waterSame=true,resourcesSame=true,safe=true,surfaceSame=true;
        std::size_t edits=0,air=0,solid=0;
        for (const auto& entry : generated) {
            const auto* oldChunk=adventureUndergroundChunkAt(old,entry.location.x*16,entry.location.y*16);
            const auto* reversed=adventureUndergroundChunkAt(reverse,entry.location.x*16,entry.location.y*16);
            deterministic &= TerrainSurvey::blockHash(*entry.chunk)==TerrainSurvey::blockHash(*reversed) &&
                adventureUndergroundEntitySignature(*entry.chunk)==adventureUndergroundEntitySignature(*reversed);
            resourcesSame &= adventureUndergroundEntitySignature(*oldChunk)==adventureUndergroundEntitySignature(*entry.chunk);
            for (int x=0;x<16;++x) for (int z=0;z<16;++z) {
                const int wx=entry.location.x*16+x,wz=entry.location.y*16+z;
                surfaceSame &= surface(wx,wz)==previous.getSurfaceHeightAtWorld(wx,wz) && biome(wx,wz)==previous.getBiomeAtWorld(wx,wz);
                for (int y=0;y<256;++y) {
                    const auto a=static_cast<BlockId>(oldChunk->getBlock(x,y,z).id),b=static_cast<BlockId>(entry.chunk->getBlock(x,y,z).id);
                    if (a==b) continue;
                    ++edits; air+=b==BlockId::Air; solid+=b!=BlockId::Air;
                    safe &= y>=8 && y<=surface(wx,wz)-5;
                    waterSame &= a!=BlockId::Water && b!=BlockId::Water;
                    const auto protectedBlock=[](BlockId id){return id==BlockId::Torch || id==BlockId::Chest ||
                        id==BlockId::CoalOre || id==BlockId::IronOre || id==BlockId::WaystoneCore || id==BlockId::Furnace || id==BlockId::Crusher;};
                    resourcesSame &= !protectedBlock(a) && !protectedBlock(b);
                }
            }
        }
        check(label+"-reverse-projection-identical",deterministic);
        check(label+"-surface-and-biome-unchanged",surfaceSame);
        check(label+"-bounded-below-surface-edits",edits>100 && edits<=1500 && safe,
              "edits/air/solid="+std::to_string(edits)+"/"+std::to_string(air)+"/"+std::to_string(solid));
        check(label+"-water-light-sources-resources-and-entities-preserved",waterSame && resourcesSame);
        const auto block=[&](int x,int y,int z){return adventureUndergroundBlock(generated,x,y,z);};
        const auto dryRoute=[&](int x,int y,int z,int dx,int dz) {
            bool valid=true;
            for (int side=-1;side<=1;++side) {
                const int wx=x-dz*side,wz=z+dx*side;
                const auto floor=block(wx,y-1,wz);
                valid &= floor!=BlockId::Air && floor!=BlockId::Water;
                for (int height=0;height<3;++height) valid &= block(wx,y+height,wz)==BlockId::Air;
            }
            return valid;
        };
        bool route=true;
        for (int step=0;step<=29;++step) route &= dryRoute(plan.entrance.anchorX+plan.entrance.directionX*step,
            plan.entrance.anchorY-std::min(step,24)*3/4,plan.entrance.anchorZ+plan.entrance.directionZ*step,
            plan.entrance.directionX,plan.entrance.directionZ);
        for (int step=0;step<=29;++step) route &= dryRoute(plan.chamberX+plan.riftDirectionX*step,
            plan.chamberAirY-(plan.chamberAirY-plan.riftEndAirY)*std::max(0,step-6)/23,
            plan.chamberZ+plan.riftDirectionZ*step,plan.riftDirectionX,plan.riftDirectionZ);
        check(label+"-complete-three-wide-three-high-dry-route",route);
        const int px=-plan.riftDirectionZ,pz=plan.riftDirectionX;
        const bool mine=plan.layout==CaveGenerator::AdventureUndergroundLayout::MinerCache;
        const auto room=[&](int a,int h,int l) {return block(plan.destinationX+plan.riftDirectionX*a+px*l,
            plan.riftEndAirY+h,plan.destinationZ+plan.riftDirectionZ*a+pz*l);};
        std::set<int> ceilingHeights;
        for (int a : {-2,1,2}) for (int l=-4;l<=4;++l) {
            for (int h=3;h<=7;++h) if (room(a,h,l)!=BlockId::Air) {ceilingHeights.insert(h);break;}
        }
        check(label+"-actual-room-has-varied-ceiling",ceilingHeights.size()>=3);
        if (mine) {
            // Flood through actual timber faces from supported posts: diagonal
            // corner contact cannot make an otherwise floating beam pass.
            std::set<std::tuple<int,int,int>> wood,connected;
            std::vector<std::tuple<int,int,int>> pending;
            for (int a=-5;a<=5;++a) for (int l=-5;l<=5;++l) for (int h=0;h<=6;++h) {
                const auto id=room(a,h,l);
                if (id!=BlockId::OakPlank && id!=BlockId::OakBark) continue;
                const auto point=std::make_tuple(a,h,l);wood.insert(point);
                if (h==0 && room(a,-1,l)!=BlockId::Air && room(a,-1,l)!=BlockId::Water) {
                    connected.insert(point);pending.push_back(point);
                }
            }
            for (std::size_t i=0;i<pending.size();++i) {
                const auto [a,h,l]=pending[i];
                for (const auto d : {glm::ivec3(1,0,0),glm::ivec3(-1,0,0),glm::ivec3(0,1,0),
                                    glm::ivec3(0,-1,0),glm::ivec3(0,0,1),glm::ivec3(0,0,-1)}) {
                    const auto n=std::make_tuple(a+d.x,h+d.y,l+d.z);
                    if (wood.count(n) && connected.insert(n).second) pending.push_back(n);
                }
            }
            check(label+"-all-timber-face-connected-to-supported-posts",wood.size()>60 && connected==wood,
                "wood/connected="+std::to_string(wood.size())+"/"+std::to_string(connected.size()));
        } else {
            std::set<std::pair<int,int>> wet,connected;
            std::vector<std::pair<int,int>> pending;
            int oldEdges=0,newEdges=0,exposedOres=0;
            for (int a=-5;a<=5;++a) for (int l=-5;l<=5;++l) {
                if (a*a+l*l>25) continue;
                const auto id=room(a,-1,l);
                if (id==BlockId::Silt || id==BlockId::MossStone) wet.emplace(a,l);
                const int wx=plan.destinationX+plan.riftDirectionX*a+px*l,
                          wz=plan.destinationZ+plan.riftDirectionZ*a+pz*l;
                for (const auto d : {glm::ivec2(1,0),glm::ivec2(0,1)}) {
                    if ((a+d.x)*(a+d.x)+(l+d.y)*(l+d.y)>25) continue;
                    newEdges+=id!=room(a+d.x,-1,l+d.y);
                    oldEdges+=adventureUndergroundBlock(old,wx,plan.riftEndAirY-1,wz)!=
                        adventureUndergroundBlock(old,wx+plan.riftDirectionX*d.x+px*d.y,
                            plan.riftEndAirY-1,wz+plan.riftDirectionZ*d.x+pz*d.y);
                }
            }
            if (!wet.empty()) {pending.push_back(*wet.begin());connected.insert(*wet.begin());}
            for (std::size_t i=0;i<pending.size();++i) {
                const auto [a,l]=pending[i];
                for (const auto d : {glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1),glm::ivec2(0,-1)}) {
                    const auto n=std::make_pair(a+d.x,l+d.y);
                    if (wet.count(n) && connected.insert(n).second) pending.push_back(n);
                }
            }
            check(label+"-damp-floor-connected-with-fewer-tile-boundaries",wet.size()>=15 && connected==wet && newEdges<oldEdges,
                "wet/newEdges/oldEdges="+std::to_string(wet.size())+"/"+std::to_string(newEdges)+"/"+std::to_string(oldEdges));
            for (int a=-2;a<=2;++a) for (int h=1;h<=2;++h) {
                const auto id=room(a,h,5);
                exposedOres+=(id==BlockId::CoalOre || id==BlockId::IronOre) && room(a,h,4)==BlockId::Air;
            }
            check(label+"-all-nine-destination-ores-remain-exposed",exposedOres==9,
                "exposed="+std::to_string(exposedOres));
        }
        checkAdventureUndergroundPlayerReturn(seed,plan,28);
        const auto saveDirectory=freshSaveDirectory("underground28_edit_"+std::to_string(seed));
        bool saved=b7UndergroundWriteWorldIdentity(saveDirectory,seed,28,"polish-edit");
        const glm::ivec3 target{plan.destinationX+plan.riftDirectionX*(mine?2:0)+px*(mine?3:5),
            plan.riftEndAirY+(mine?0:1),plan.destinationZ+plan.riftDirectionZ*(mine?2:0)+pz*(mine?3:5)};
        for (int pass=0;pass<2;++pass) {
            Player editPlayer;World editWorld(camera,config,editPlayer,saveDirectory,false,0);
            editWorld.getChunkManager().loadChunk(WorldCoordinates::floorDiv(target.x,CHUNK_SIZE),
                WorldCoordinates::floorDiv(target.z,CHUNK_SIZE));
            const auto id=editWorld.getBlock(target.x,target.y,target.z);
            if (pass==0) {
                saved &= mine?id==BlockId::Chest:(id==BlockId::CoalOre || id==BlockId::IronOre);
                editWorld.setBlock(target.x,target.y,target.z,BlockId::Air);saved &= editWorld.save();
            } else saved &= id==BlockId::Air && !editWorld.getBlockEntity(target) &&
                editWorld.getChunkManager().getTerrainGenerationVersion()==28;
        }
        check(label+"-resource-removal-save-reopen-no-respawn",saved);
    }
    clearDeterministicEnv();
}

void caseUndergroundPolishGeometry()
{
    const auto root=freshSaveDirectory("underground28_geometry");
    for (const auto fixture : {glm::ivec3(42,11,2),glm::ivec3(20260807,-5,-5),glm::ivec3(239701883,3,-6)}) {
        ClassicOverWorldGenerator generator(fixture.x,28);CaveGenerator caves(fixture.x,28);
        const auto plan=caves.getAdventureUndergroundPlanForCell(fixture.y,fixture.z,
            [&](int x,int z){return generator.getSurfaceHeightAtWorld(x,z);},
            [&](int x,int z){return generator.getBiomeAtWorld(x,z);});
        check("UNDERGROUND28_GEOMETRY/plan-"+std::to_string(fixture.x),plan.valid);
        if (!plan.valid) continue;
        const auto locations=adventureUndergroundLocations(plan);
        const auto before=b7UndergroundCollectMeshMetrics(root,fixture.x,27,locations);
        const auto after=b7UndergroundCollectMeshMetrics(root,fixture.x,28,locations);
        std::cout<<"[UNDERGROUND28_GEOMETRY] seed="<<fixture.x<<" chunks="<<locations.size()
            <<" vertices="<<before.vertices<<'/'<<after.vertices<<" indices="<<before.indices<<'/'<<after.indices
            <<" renderables="<<before.renderables<<'/'<<after.renderables
            <<" buffers="<<before.bufferBytes<<'/'<<after.bufferBytes<<'\n';
        check("UNDERGROUND28_GEOMETRY/bounded-"+std::to_string(fixture.x),
            before.targetChunks==after.targetChunks && before.haloChunks==after.haloChunks &&
            before.sections==after.sections && after.bufferBytes<=before.bufferBytes*11/10 &&
            after.renderables<=before.renderables+3 && after.waterFaces==before.waterFaces);
    }
    clearDeterministicEnv();
}
