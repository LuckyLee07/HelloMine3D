#pragma once

#include "../World/Generation/Structures/TreeGenerator.h"
#include <map>
#include <queue>
#include <set>

namespace {
using V24Point = std::array<int,3>;
using V24Blocks = std::map<V24Point,std::uint16_t>;

V24Blocks v24TreeBlocks(World &world,int seed,AdventureTreeKind kind,int stature,
                       int x,int z,bool reverse,bool obstacles=false)
{
    std::vector<glm::ivec2> locations;
    for(int cx=WorldCoordinates::floorDiv(x-6,16);cx<=WorldCoordinates::floorDiv(x+6,16);++cx)
        for(int cz=WorldCoordinates::floorDiv(z-6,16);cz<=WorldCoordinates::floorDiv(z+6,16);++cz)
            locations.push_back({cx,cz});
    if(reverse)std::reverse(locations.begin(),locations.end());
    V24Blocks result;
    for(const auto location:locations) {
        Chunk chunk(world,location,false);
        if(obstacles)for(int lx=0;lx<16;++lx)for(int lz=0;lz<16;++lz) {
            chunk.setBlock(lx,199,lz,BlockId::Stone);
            chunk.setBlock(lx,206,lz,BlockId::Water);
        }
        makePolishedAdventureTree(chunk,seed,x,200,z,kind,stature);
        for(int lx=0;lx<16;++lx)for(int lz=0;lz<16;++lz)for(int by=199;by<220;++by) {
            const auto b=chunk.getBlock(lx,by,lz);
            if(b.id!=static_cast<Block_t>(BlockId::Air))
                result[{location.x*16+lx-x,by-200,location.y*16+lz-z}]=
                    static_cast<std::uint16_t>(b.id)|(static_cast<std::uint16_t>(b.metadata)<<8);
        }
    }
    return result;
}

bool v24Connected(const V24Blocks &blocks)
{
    if(blocks.empty())return false;
    std::set<V24Point> reached;
    std::queue<V24Point> queue;
    queue.push({0,0,0});reached.insert({0,0,0});
    while(!queue.empty()) {
        const auto p=queue.front();queue.pop();
        for(int axis=0;axis<3;++axis)for(int sign:{-1,1}) {
            auto next=p;next[axis]+=sign;
            if(blocks.count(next) && reached.insert(next).second)queue.push(next);
        }
    }
    return reached.size()==blocks.size();
}

void caseVegetationPolishV24()
{
    check("VEGETATION24/version-and-save-boundary",
        AdventureUndergroundTerrainGenerationVersion==23 && VegetationPolishTerrainGenerationVersion==24 &&
        CurrentTerrainGenerationVersion==24 && WorldSaveFormatVersion==12);
    setEnv("HELLOMINE3D_SEED","42");setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig();Camera camera(config);Player owner;
    World world(camera,config,owner,freshSaveDirectory("vegetation24_projection"),false,0);
    bool haloRejected=false;
    try { (void)ClassicOverWorldGenerator(42,24).getStructurePlansForChunk(0,0,13); }
    catch(const std::out_of_range &) { haloRejected=true; }
    check("VEGETATION24/query-halo-remains-bounded",haloRejected);
    bool bounds=true,connected=true,order=true,materials=true,protection=true,clearance=true;
    std::size_t largest=0;
    for(const auto kind:{AdventureTreeKind::Oak,AdventureTreeKind::Birch,AdventureTreeKind::Spruce,
                         AdventureTreeKind::Willow,AdventureTreeKind::Palm,AdventureTreeKind::Cactus}) {
        std::set<std::uint64_t> identities;
        std::set<int> heights,widths;
        int shortHeight=0,tallHeight=0;
        for(int stature=0;stature<3;++stature)for(int seed:{42,20260807,239701883}) {
            const auto blocks=v24TreeBlocks(world,seed,kind,stature,15,-1,false);
            order &= blocks==v24TreeBlocks(world,seed,kind,stature,-17,16,true);
            connected &= v24Connected(blocks);
            largest=std::max(largest,blocks.size());
            int top=0,minX=0,maxX=0;
            std::uint64_t digest=14695981039346656037ull;
            for(const auto &entry:blocks) {
                const auto &p=entry.first;const auto id=entry.second&255,metadata=entry.second>>8;
                bounds &= std::abs(p[0])<=6 && std::abs(p[2])<=6 && p[1]>=0 && p[1]<=16;
                top=std::max(top,p[1]);minX=std::min(minX,p[0]);maxX=std::max(maxX,p[0]);
                if(id==static_cast<int>(BlockId::OakLeaf) || id==static_cast<int>(BlockId::OakBark))
                    materials &= metadata==(kind==AdventureTreeKind::Spruce?BlockMetadata::Tree::Spruce:
                        kind==AdventureTreeKind::Birch?BlockMetadata::Tree::Birch:BlockMetadata::Tree::Oak);
                if(id==static_cast<int>(BlockId::OakLeaf))clearance &= p[1]>=2;
                for(int value:p)TerrainSurvey::hashByte(digest,static_cast<std::uint8_t>(value));
                TerrainSurvey::hashByte(digest,static_cast<std::uint8_t>(entry.second));
            }
            identities.insert(digest);heights.insert(top);widths.insert(maxX-minX);
            if(stature==0)shortHeight+=top;if(stature==2)tallHeight+=top;
        }
        check("VEGETATION24/shape-variation-"+std::to_string(static_cast<int>(kind)),
              identities.size()>=3 && heights.size()>=2 && widths.size()>=2 && tallHeight>shortHeight,
              "shapes="+std::to_string(identities.size())+" heights="+std::to_string(heights.size())+
              " widths="+std::to_string(widths.size()));
        const auto blocked=v24TreeBlocks(world,42,kind,2,15,-1,false,true);
        for(const auto &entry:blocked) {
            if(entry.first[1]==-1)protection &= (entry.second&255)==static_cast<int>(BlockId::Stone);
            if(entry.first[1]==6)protection &= (entry.second&255)==static_cast<int>(BlockId::Water);
        }
    }
    check("VEGETATION24/six-neighbour-connected-to-root",connected);
    check("VEGETATION24/bounded-full-voxel-volume",bounds && largest<=600,"largest="+std::to_string(largest));
    check("VEGETATION24/signed-boundary-order-and-species-metadata",order && materials);
    check("VEGETATION24/water-stone-and-under-crown-clearance",protection && clearance);

    bool columns=true,treeSubset=true,productionOrder=true,terrain=true,support=true;
    std::size_t oldTrees=0,newTrees=0,oldCover=0,newCover=0,changedBlocks=0;
    std::array<int,3> statures{};
    for(int seed:{42,20260807,239701883}) {
        AdventureEcologyPlanner oldPlan(seed,23),plan(seed,24);
        ClassicOverWorldGenerator oldGen(seed,23),gen(seed,24),reverse(seed,24);
        std::map<int,glm::ivec2> sites;
        for(int z=-2048;z<=2048;z+=28)for(int x=-2048;x<=2048;x+=28) {
            const auto a=oldPlan.sample(x,z),b=plan.sample(x,z);
            columns &= a.column.height==b.column.height && a.column.surface==b.column.surface &&
                a.column.biome==b.column.biome && a.grove==b.grove && a.snowLine==b.snowLine;
            if(b.column.height>=64)sites.emplace(static_cast<int>(b.region),glm::ivec2(x,z));
            oldCover+=oldPlan.groundCover(x,z,a).id!=BlockId::Air;
            newCover+=plan.groundCover(x,z,b).id!=BlockId::Air;
            for(int dz=0;dz<7;++dz)for(int dx=0;dx<7;++dx)if(plan.treeAnchor(x+dx,z+dz)) {
                const auto s=plan.sample(x+dx,z+dz);
                const auto before=oldPlan.tree(x+dx,z+dz,s),after=plan.tree(x+dx,z+dz,s);
                oldTrees+=before.kind!=AdventureTreeKind::None;
                if(after.kind!=AdventureTreeKind::None) {
                    ++newTrees;++statures[after.stature];
                    treeSubset &= before.kind==after.kind && before.height==after.height;
                }
            }
        }
        check("VEGETATION24/eight-dry-region-sites-"+std::to_string(seed),sites.size()>=8);
        for(const auto &site:sites) {
            const auto root=site.second;
            std::array<std::uint64_t,4> hashes{};
            for(int i=0;i<4;++i) {
                const glm::ivec2 location{WorldCoordinates::floorDiv(root.x,16)+i%2,
                                         WorldCoordinates::floorDiv(root.y,16)+i/2};
                Chunk oldChunk(world,location,false),chunk(world,location,false);
                oldGen.generateTerrainFor(oldChunk);gen.generateTerrainFor(chunk);hashes[i]=TerrainSurvey::blockHash(chunk);
                for(int x=0;x<16;++x)for(int z=0;z<16;++z) {
                    const int wx=location.x*16+x,wz=location.y*16+z,h=gen.getSurfaceHeightAtWorld(wx,wz);
                    columns &= h==oldGen.getSurfaceHeightAtWorld(wx,wz);
                    for(int by=0;by<256;++by) {
                        const auto a=oldChunk.getBlock(x,by,z),b=chunk.getBlock(x,by,z);
                        const bool same=a.id==b.id && a.metadata==b.metadata;
                        if(by<=h)terrain &= same;
                        changedBlocks+=!same;
                        if(b.id==static_cast<Block_t>(BlockId::TallGrass) || b.id==static_cast<Block_t>(BlockId::Rose))
                            support &= AdventureEcologyPlanner::supportsPlant(
                                static_cast<BlockId>(chunk.getBlock(x,by-1,z).id),static_cast<BlockId>(b.id));
                    }
                }
            }
            for(int i=3;i>=0;--i) {
                Chunk chunk(world,{WorldCoordinates::floorDiv(root.x,16)+i%2,WorldCoordinates::floorDiv(root.y,16)+i/2},false);
                reverse.generateTerrainFor(chunk);productionOrder &= TerrainSurvey::blockHash(chunk)==hashes[i];
            }
        }
    }
    check("VEGETATION24/surface-and-underground-preserved",columns && terrain);
    check("VEGETATION24/glades-and-treeline-retain-resources",treeSubset && newTrees>100 && newTrees<oldTrees);
    check("VEGETATION24/edge-interior-statures-and-cover-patches",statures[0]>20 && statures[1]>20 && statures[2]>20 &&
          newCover>100 && newCover<oldCover);
    check("VEGETATION24/real-regional-blocks-and-reverse-generation",productionOrder && support && changedBlocks>100);
    std::cout << "[VEGETATION24_COUNTS] old_trees=" << oldTrees << " trees=" << newTrees << " old_cover=" << oldCover
              << " cover=" << newCover << " changed_blocks=" << changedBlocks << " statures="
              << statures[0] << ',' << statures[1] << ',' << statures[2] << '\n';
    for(int seed:{42,20260807,239701883}) {
        ClassicOverWorldGenerator generator(seed,24);
        AdventureEcologyPlanner plants(seed,24);CaveGenerator caves(seed,24);
        const auto surface=[&](int x,int z){return generator.getSurfaceHeightAtWorld(x,z);};
        const auto biome=[&](int x,int z){return generator.getBiomeAtWorld(x,z);};
        const auto clearedRoot=[&](int x,int z,const AdventureEcologyPlanner::Tree &tree) {
            const glm::ivec2 location{WorldCoordinates::floorDiv(x,16),WorldCoordinates::floorDiv(z,16)};
            Chunk chunk(world,location,false);generator.generateTerrainFor(chunk);
            const auto id=chunk.getBlock(WorldCoordinates::floorMod(x,16),tree.height,WorldCoordinates::floorMod(z,16)).id;
            return id!=static_cast<Block_t>(BlockId::OakBark) && id!=static_cast<Block_t>(BlockId::Cactus);
        };
        bool entranceFound=false,entranceClear=true,workshopFound=false,workshopClear=true;
        for(int cz=-16;cz<=16 && !entranceFound;++cz)for(int cx=-16;cx<=16 && !entranceFound;++cx) {
            const auto e=caves.getNaturalEntranceForCell(cx,cz,surface,biome);if(!e.valid)continue;
            for(int along=-7;along<=CaveGenerator::EntranceTunnelLength+7 && !entranceFound;++along)
                for(int side=-7;side<=7 && !entranceFound;++side) {
                    // Focus on the added outer clearance, where a six-block
                    // crown would otherwise intrude into the existing lane.
                    if(std::abs(side)<=4)continue;
                    const int x=e.anchorX+e.directionX*along-e.directionZ*side;
                    const int z=e.anchorZ+e.directionZ*along+e.directionX*side;
                    if(!plants.treeAnchor(x,z))continue;
                    const auto tree=plants.tree(x,z,plants.sample(x,z));
                    if(tree.kind==AdventureTreeKind::None)continue;
                    entranceFound=true;entranceClear &= clearedRoot(x,z,tree);
                    std::cout << "[VEGETATION24_ENTRANCE] seed=" << seed << " root=" << x << ',' << z << '\n';
                }
        }
        for(int cz=-12;cz<=12 && !workshopFound;++cz)for(int cx=-12;cx<=12 && !workshopFound;++cx)
            for(const auto type:{StructureType::Ruin,StructureType::RaiderCamp,StructureType::Waystone}) {
                if(workshopFound)break;
                const auto plan=generator.getStructurePlanForCell(type,cx,cz);
                const auto site=LandmarkWorkshop::select(plan,surface,biome);if(!site.valid)continue;
                for(int z=site.minimumZ-6;z<=site.minimumZ+8 && !workshopFound;++z)
                    for(int x=site.minimumX-6;x<=site.minimumX+8 && !workshopFound;++x) {
                        if(LandmarkWorkshop::clearsTreeSource(site,x,z) || !plants.treeAnchor(x,z))continue;
                        if(x>=plan.footprint.minimumX && x<=plan.footprint.maximumX &&
                           z>=plan.footprint.minimumZ && z<=plan.footprint.maximumZ)continue;
                        const auto tree=plants.tree(x,z,plants.sample(x,z));
                        if(tree.kind==AdventureTreeKind::None)continue;
                        workshopFound=true;workshopClear &= clearedRoot(x,z,tree);
                        std::cout << "[VEGETATION24_WORKSHOP] seed=" << seed << " root=" << x << ',' << z << '\n';
                    }
            }
        check("VEGETATION24/expanded-entrance-clearance-"+std::to_string(seed),entranceFound && entranceClear);
        check("VEGETATION24/expanded-workshop-clearance-"+std::to_string(seed),workshopFound && workshopClear);
    }
    for(int version:{23,24}) {
        const auto directory=freshSaveDirectory("vegetation24_save_"+std::to_string(version));
        bool saved=initializeTerrainIdentity(directory,"vegetation-polish",version,42);
        {
            Player player;World actual(camera,config,player,directory,false,0);
            saved &= actual.getChunkManager().getTerrainGenerationVersion()==version && actual.save();
        }
        {
            Player player;World actual(camera,config,player,directory,false,0);
            saved &= actual.getChunkManager().getTerrainGenerationVersion()==version;
        }
        check("VEGETATION24/version-identity-save-reopen-"+std::to_string(version),saved);
    }
}
}
