#include "ReferenceVisualScene.h"

#include "../Core/Camera.h"
#include "../Item/Material.h"
#include "../Player/Player.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"
#include "../World/World.h"
#include "../World/Block/ChestContainer.h"
#include "../World/Block/FurnaceContainer.h"
#include "../World/Generation/Structures/TreeGenerator.h"
#include "../World/Generation/Ecology/AdventureEcologyPlanner.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
#include <stdexcept>
#include <string>

namespace {
struct SceneEdit {
    int x0,y0,z0,x1,y1,z1;
    BlockId block;
    BlockMetadata_t yaw;
};
struct SceneView {
    const char* name;
    std::array<float,3> position;
    std::array<float,3> rotation;
};
struct SceneCell {
    int x,y,z;
    BlockId block;
    BlockMetadata_t yaw;
};
struct SceneTree {
    const char* name;
    std::array<int,3> root;
    AdventureTreeKind kind;
    int seed,stature;
};
struct SceneTreeWrite {
    int x,y,z;
    ChunkBlock block;
};
struct PlannedSceneTree {
    const SceneTree* source;
    std::vector<SceneTreeWrite> writes;
    std::size_t uniqueCells=0;
    double planMilliseconds=0;
};
#include "ReferenceVisualSceneData.inc"
constexpr int CentreX=208,FloorY=66,CentreZ=-192;
}

bool buildReferenceVisualScene(World& world, Player& player, Camera& camera)
{
    const char* requested=std::getenv("HELLOMINE3D_REFERENCE_VISUAL_SCENE");
    if(requested==nullptr || std::string(requested)!="1") return false;
    const auto started=std::chrono::steady_clock::now();
    const char* viewValue=std::getenv("HELLOMINE3D_REFERENCE_VISUAL_VIEW");
    const std::string name=viewValue==nullptr?"street":viewValue;
    const SceneView* view=nullptr;
    for(const auto& candidate:kSceneViews) if(name==candidate.name) view=&candidate;
    if(!view) throw std::runtime_error("Reference visual view must be street, interior, details, water, second or upstairs.");

    player.position={CentreX+.5f,FloorY+2.02f,CentreZ+22.5f};
    player.velocity={0.f,0.f,0.f};player.box.update(player.position);
    // Four radius-one preloads cover the exact 4x4 chunk site, including its
    // negative-z edge. World retains locking and streaming ownership.
    for(int z:{-8,8}) for(int x:{-8,8})
        world.preloadAround({CentreX+float(x),FloorY+2.f,CentreZ+float(z)});
    world.preloadAround({CentreX+.5f,FloorY+2.f,CentreZ+.5f});
    std::size_t residentSceneChunks=0;
    for(int z=World::floorDiv(CentreZ-24,16);z<=World::floorDiv(CentreZ+23,16);++z)
    for(int x=World::floorDiv(CentreX-24,16);x<=World::floorDiv(CentreX+23,16);++x) {
        const auto* chunk=world.getChunkManager().findChunk(x,z);
        if(chunk && chunk->hasLoaded()) ++residentSceneChunks;
    }
    if(residentSceneChunks!=16u) throw std::runtime_error("Reference scene needs all sixteen resident site chunks before editing.");

    // Native columns in the two preserved groves are absent from generated
    // edits, including their soil and ownership tags. The town parcel is
    // cleared before construction, keeping overhangs out of rooms and paths.
    std::size_t nativeGroveTrunks=0;
    for(int z=-24;z<=-20;++z) for(int x=-24;x<=23;++x)
        if(x<=-20 || x>=19) for(int y=1;y<=28;++y)
            nativeGroveTrunks+=world.getBlock(CentreX+x,FloorY+y,CentreZ+z)==BlockId::OakBark;

    // Plan the production tree sequence against the completed scene before
    // the first edit. No tree geometry or owner rules are duplicated here.
    const auto futureBlock=[&](int x,int y,int z) {
        ChunkBlock result=world.getBlock(CentreX+x,FloorY+y,CentreZ+z);
        for(const auto& edit:kSceneEdits)
            if(x>=edit.x0 && x<=edit.x1 && y>=edit.y0 && y<=edit.y1 && z>=edit.z0 && z<=edit.z1)
                result=ChunkBlock(edit.block,edit.yaw);
        return result;
    };
    const auto replaceable=[](ChunkBlock block) {
        const auto id=static_cast<BlockId>(block.id);
        return id==BlockId::Air || id==BlockId::OakLeaf || id==BlockId::TallGrass ||
               id==BlockId::Rose || id==BlockId::DeadShrub;
    };
    std::vector<PlannedSceneTree> trees;
    std::map<std::array<int,3>,ChunkBlock> treeBefore,treeFinal;
    std::size_t treePlannedAttempts=0;
    for(const auto& tree:kSceneTrees) {
        const auto planStarted=std::chrono::steady_clock::now();
        PlannedSceneTree plan{&tree,{},0,0};
        const auto support=futureBlock(tree.root[0],tree.root[1]-1,tree.root[2]);
        if(support!=BlockId::Grass && support!=BlockId::Dirt && support!=BlockId::ForestFloor)
            throw std::runtime_error("Reference planted tree has no authored soil support.");
        visitPolishedAdventureTreeBlocks(tree.seed,tree.root[0],tree.root[1],tree.root[2],tree.kind,tree.stature,
            [&](int x,int y,int z,ChunkBlock block) {
                if(plan.writes.size()>=PolishedTreeMaximumPlannedBlocks)
                    throw std::runtime_error("Reference planted tree exceeded 1024 production plan writes.");
                if(x<-24 || x>23 || z<-24 || z>23 || y<1 || y>20 ||
                   (static_cast<BlockId>(block.id)!=BlockId::OakBark && static_cast<BlockId>(block.id)!=BlockId::OakLeaf))
                    throw std::runtime_error("Reference planted tree escaped its scene or material bounds.");
                const auto* chunk=world.getChunkManager().findChunk(World::floorDiv(CentreX+x,16),World::floorDiv(CentreZ+z,16));
                if(!chunk || !chunk->hasLoaded())
                    throw std::runtime_error("Reference planted tree requires already resident site chunks.");
                // Low trunks/leaves may not occupy streets or either bank's
                // walking aisle. Higher canopy over water is real geometry.
                if(y<=3 && ((x>=-2 && x<=3) || (z>=3 && z<=8) || (z>=15 && z<=16)))
                    throw std::runtime_error("Reference planted tree would obstruct a walking aisle.");
                for(const auto& cell:kSceneClearanceCells)
                    if(x==cell.x && y==cell.y && z==cell.z)
                        throw std::runtime_error("Reference planted tree would obstruct required clearance.");
                for(const auto& candidate:kSceneViews) {
                    const auto& p=candidate.position;
                    if(p[0]+.3f>x && p[0]-.3f<x+1 && p[1]+1.f>y && p[1]-1.f<y+1 && p[2]+.3f>z && p[2]-.3f<z+1)
                        throw std::runtime_error("Reference planted tree would obstruct a fixed view body.");
                }
                plan.writes.push_back({x,y,z,block});
            });
        if(plan.writes.empty()) throw std::runtime_error("Reference planted tree emitted an empty plan.");
        treePlannedAttempts+=plan.writes.size();
        if(kSceneBaseEditAttempts+treePlannedAttempts>kSceneEditBudget)
            throw std::runtime_error("Reference scene and tree plans exceeded their joint edit budget.");
        const auto previousCells=treeFinal.size();
        for(const auto& write:plan.writes) {
            const std::array<int,3> key{write.x,write.y,write.z};
            const auto found=treeFinal.find(key);
            const bool own=found!=treeFinal.end();
            const ChunkBlock existing=own?found->second:futureBlock(write.x,write.y,write.z);
            if(!own) treeBefore.emplace(key,existing);
            // Mirror the production vegetation-only sequence: later leaves
            // and duplicate branches never replace an earlier planned trunk.
            if(own && static_cast<BlockId>(existing.id)==BlockId::OakBark) continue;
            if(!replaceable(existing))
                throw std::runtime_error("Reference planted tree would replace a building, water or existing trunk.");
            treeFinal[key]=write.block;
        }
        plan.uniqueCells=treeFinal.size()-previousCells;
        plan.planMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-planStarted).count();
        trees.push_back(std::move(plan));
    }

    std::size_t attempts=0,changed=0;
    for(const auto& edit:kSceneEdits)
    for(int y=edit.y0;y<=edit.y1;++y)
    for(int z=edit.z0;z<=edit.z1;++z)
    for(int x=edit.x0;x<=edit.x1;++x) {
        if(++attempts>kSceneEditBudget) throw std::runtime_error("Reference visual scene exceeded its edit budget.");
        const ChunkBlock block(edit.block,edit.yaw);
        if(world.getBlock(CentreX+x,FloorY+y,CentreZ+z)==block) continue;
        world.setBlock(CentreX+x,FloorY+y,CentreZ+z,block);++changed;
    }
    if(attempts!=kSceneBaseEditAttempts)
        throw std::runtime_error("Reference scene base attempt count drifted from its deterministic export.");
    // All completed background cells must match the preflight before the
    // first tree write. Then use normal World edits, relighting and dirtying.
    for(const auto& before:treeBefore)
        if(world.getBlock(CentreX+before.first[0],FloorY+before.first[1],CentreZ+before.first[2])!=before.second)
            throw std::runtime_error("Reference tree background changed after preflight.");
    std::size_t plantedChanges=0;
    for(const auto& tree:trees) {
        const auto editStarted=std::chrono::steady_clock::now();
        std::size_t treeChanges=0;
        for(const auto& write:tree.writes) {
            if(++attempts>kSceneEditBudget)
                throw std::runtime_error("Reference scene and planted trees exceeded their edit budget.");
            const auto existing=world.getBlock(CentreX+write.x,FloorY+write.y,CentreZ+write.z);
            if(existing==write.block || static_cast<BlockId>(existing.id)==BlockId::OakBark) continue;
            if(!replaceable(existing)) throw std::runtime_error("Reference tree edit encountered an unexpected protected block.");
            world.setBlock(CentreX+write.x,FloorY+write.y,CentreZ+write.z,write.block);
            ++changed;++treeChanges;++plantedChanges;
        }
        const auto editMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-editStarted).count();
        std::cout<<"[REFERENCE_VISUAL_TREE] name="<<tree.source->name<<" kind="<<int(tree.source->kind)
                 <<" seed="<<tree.source->seed<<" stature="<<tree.source->stature
                 <<" root="<<CentreX+tree.source->root[0]<<','<<FloorY+tree.source->root[1]<<','<<CentreZ+tree.source->root[2]
                 <<" attempts="<<tree.writes.size()<<" changed="<<treeChanges<<" final_cells="<<tree.uniqueCells
                 <<" plan_ms="<<tree.planMilliseconds<<" edit_ms="<<editMilliseconds
                 <<" source=ordinary_planted owner_override=0 world_edits=1\n";
    }
    for(const auto& final:treeFinal)
        if(world.getBlock(CentreX+final.first[0],FloorY+final.first[1],CentreZ+final.first[2])!=final.second)
            throw std::runtime_error("Reference tree production plan did not reach authoritative World.");
    const auto verify=[&](const SceneCell& cell) {
        return world.getBlock(CentreX+cell.x,FloorY+cell.y,CentreZ+cell.z)==ChunkBlock(cell.block,cell.yaw);
    };
    for(const auto& cell:kSceneRequiredCells) if(!verify(cell))
        throw std::runtime_error("Reference scene edits did not reach authoritative World.");
    for(const auto& cell:kSceneClearanceCells) if(!verify(cell))
        throw std::runtime_error("Reference scene doorway, stair or bridge clearance was obstructed.");
    std::size_t weatherChecks=0;
    for(const auto& region:kSceneWeatherShellRegions)
    for(int y=region.y0;y<=region.y1;++y)
    for(int z=region.z0;z<=region.z1;++z)
    for(int x=region.x0;x<=region.x1;++x) {
        ++weatherChecks;
        if(!verify({x,y,z,region.block,region.yaw}))
            throw std::runtime_error("Reference scene weather wall or ceiling was not closed in World.");
    }
    for(const auto& cell:kSceneWeatherRoofCells) if(!verify(cell))
        throw std::runtime_error("Reference scene flat roof did not reach its wall-top connection.");

    // The direct World construction path must create the same empty block
    // entities as ordinary placement; otherwise a visible furnace/chest cannot
    // be opened through normal Use. Reopening never runs this fresh-only path.
    for(const auto& cell:kSceneRequiredCells) {
        const glm::ivec3 position(CentreX+cell.x,FloorY+cell.y,CentreZ+cell.z);
        if(cell.block==BlockId::Chest && !ChestContainer::initialize(world,position))
            throw std::runtime_error("Reference scene chest initialization failed.");
        if(cell.block==BlockId::Furnace && !FurnaceContainer::initialize(world,position))
            throw std::runtime_error("Reference scene furnace initialization failed.");
    }

    // Explicit sample starter inputs. Recipes remain the production registry,
    // and the normal Workbench/Furnace/Chest are usable World block entities.
    // One stone/bark stack is consumed completely by its first formal recipe.
    const std::array<std::pair<const Material*,int>,5> starter={{
        {&Material::STONE_PICKAXE,1},{&Material::STONE_BLOCK,1},
        {&Material::OAK_BARK_BLOCK,1},{&Material::CLAY_BLOCK,2},
        {&Material::COAL_ORE_BLOCK,1}}};
    for(const auto& item:starter) {
        if(player.addItem(*item.first,item.second)!=item.second)
            throw std::runtime_error("Reference scene starter inputs exceeded normal inventory capacity.");
        world.getEventBus().publish(PlayerInventoryChangedEvent(
            DefaultPlayerActorId,item.first->id,item.second,"reference_scene_starter"));
    }
    player.position={CentreX+view->position[0],FloorY+view->position[1],CentreZ+view->position[2]};
    player.rotation={view->rotation[0],view->rotation[1],view->rotation[2]};
    player.box.update(player.position);player.resetInterpolation();
    camera.hookEntity(player);camera.update();
    if(!world.save()) throw std::runtime_error("Reference scene initial save failed.");
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    std::cout<<"[REFERENCE_VISUAL_SCENE] version=2 edits="<<attempts<<" changed="<<changed
             <<" regions="<<kSceneRegionCount<<" budget="<<kSceneEditBudget
             <<" scene_chunks="<<residentSceneChunks<<" native_grove_trunks="<<nativeGroveTrunks
             <<" kit_parts=12 layouts=two_storey_balcony,L_kiln_gallery starter_inputs=5"
             <<" planted_trees="<<trees.size()<<" planted_tree_attempts="<<treePlannedAttempts
             <<" planted_tree_cells="<<treeFinal.size()<<" planted_tree_changed="<<plantedChanges<<" tree_owner_override=0"
             <<" weather_shell_checks="<<weatherChecks<<" weather_roof_checks="<<std::size(kSceneWeatherRoofCells)
             <<" origin="<<CentreX<<','<<FloorY<<','<<CentreZ<<" water_mean_y=66.9 view="<<name
             <<" generation_ms="<<elapsed<<" saved=1 normal_world=1\n";
    return true;
}
