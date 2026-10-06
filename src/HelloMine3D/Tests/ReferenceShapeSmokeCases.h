#pragma once
#include "../World/Block/BlockGeometry.h"
#include "../Feedback/BlockSurfaceGeometry.h"
#include "../Presentation/ItemVisualGeometry.h"

namespace {
void caseReferenceShapes()
{
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","42");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 220 8");
    const auto directory=freshSaveDirectory("reference_shapes");
    Config config=makeConfig();Camera camera(config);
    bool persisted=false;
    {
        Player player;World world(camera,config,player,directory,false,0);
        world.getChunkManager().loadChunk(0,0);world.getChunkManager().loadChunk(1,0);
        world.getChunkManager().loadChunk(-1,0);
        check("REFERENCE_SHAPE/append-only-identities",static_cast<int>(BlockId::Silt)==32 &&
            static_cast<int>(BlockId::StoneStep)==33 && static_cast<int>(BlockId::StoneWindowFrame)==34 &&
            static_cast<int>(Material::Silt)==48 && static_cast<int>(Material::StoneStep)==49 &&
            static_cast<int>(Material::StoneWindowFrame)==50);
        for(const BlockId id:{BlockId::StoneStep,BlockId::StoneWindowFrame}) {
            const auto &definition=BlockDatabase::get().getDefinition(id);
            const auto &material=Material::toMaterial(id);
            check("REFERENCE_SHAPE/registered-"+std::to_string(int(id)),material.isBlock && material.toBlockID()==id &&
                definition.render.shape.isCompound() && definition.collidable && !definition.occludesFaces &&
                !definition.fullCellSolid && !definition.aoOccluder && definition.blocksLight==(id==BlockId::StoneStep) &&
                definition.defaultDrop==material.id);
            for(int yaw=0;yaw<4;++yaw) {
                const glm::ivec3 cell(2+yaw*3,210,id==BlockId::StoneStep?2:5);
                world.setBlock(cell.x,cell.y,cell.z,BlockId::Air);
                player.position={8,220,8};player.rotation.y=yaw*90.f;
                player.getHeldItems()=ItemStack(material,2);
                const bool placed=BlockInteractionSystem::placeBlock(world,player,glm::vec3(cell)+glm::vec3(.5f));
                check("REFERENCE_SHAPE/real-place-yaw-"+std::to_string(int(id))+"-"+std::to_string(yaw),placed &&
                    world.getBlock(cell.x,cell.y,cell.z)==ChunkBlock(id,yaw) && player.getHeldItems().getNumInStack()==1);
                const auto feedback=blockSurfaceGeometry(definition,ChunkBlock(id,yaw),cell,TerrainBiome::TemperateForest,42);
                const auto &surfaces=definition.render.shape.variants[yaw].surfaces;
                bool matches=feedback.size()==surfaces.size();
                for(std::size_t i=0;i<feedback.size() && matches;++i)
                    matches &= feedback[i].positions==surfaces[i].positions && feedback[i].repeat==surfaces[i].repeat && feedback[i].tile==glm::ivec2(0,9);
                const auto item=ItemVisualGeometry::compound(definition.render.shape,definition.render.texTopCoord,
                    definition.render.texSideCoord,definition.render.texBottomCoord,yaw);
                matches &= item.size()==surfaces.size();
                for(std::size_t i=0;i<item.size() && matches;++i) for(int k=0;k<4;++k)
                    matches &= item[i].positions[k]+glm::vec3(.5f)==glm::vec3(surfaces[i].positions[k*3],surfaces[i].positions[k*3+1],surfaces[i].positions[k*3+2]);
                check("REFERENCE_SHAPE/feedback-item-world-parity-"+std::to_string(int(id))+"-"+std::to_string(yaw),matches);
            }
        }
        // Precise ray picking must look through a window and hit its rim.
        world.setBlock(8,215,6,ChunkBlock(BlockId::StoneWindowFrame,0));world.setBlock(8,215,5,BlockId::Stone);
        const auto hole=BlockSelectionSystem::pick(world,{8.5f,215.5f,8.5f},{0,0,0});
        const auto rim=BlockSelectionSystem::pick(world,{8.95f,215.5f,8.5f},{0,0,0});
        check("REFERENCE_SHAPE/window-hole-selects-behind",hole && hole->blockPosition==glm::ivec3(8,215,5));
        check("REFERENCE_SHAPE/window-rim-and-placement-face",rim && rim->blockPosition==glm::ivec3(8,215,6) &&
            rim->placementPosition==glm::ivec3(8,215,7) && std::abs(rim->hitPoint.z-6.625f)<.0001f);
        // Final-orientation collision rejection is atomic with inventory.
        player.position={8.5f,215.5f,7.5f};player.rotation.y=0;
        player.getHeldItems()=ItemStack(Material::STONE_STEP_BLOCK,2);world.setBlock(8,215,7,BlockId::Air);
        const bool rejected=!BlockInteractionSystem::placeBlock(world,player,{8.5f,215.5f,7.1f});
        check("REFERENCE_SHAPE/placement-player-overlap-atomic",rejected && world.getBlock(8,215,7)==BlockId::Air && player.getHeldItems().getNumInStack()==2);
        // A fall onto the low half rests at y+.5, not at the invisible cell top.
        world.setBlock(8,200,9,ChunkBlock(BlockId::StoneStep,0));
        player.position={8.5f,205,9.1f};player.velocity={0,-100,0};player.collide(world,{0,-100,0},.05f);
        check("REFERENCE_SHAPE/fall-rests-on-low-half",std::abs(player.position.y-201.5f)<.0001f);
        // The actual horizontal collision path climbs each half step.
        world.setBlock(8,200,8,BlockId::Stone);world.setBlock(8,201,9,ChunkBlock(BlockId::StoneStep,0));
        player.position={8.5f,202,8.5f};player.velocity={0,0,1};player.collide(world,{0,0,1},.35f);
        const bool first=std::abs(player.position.y-202.5f)<.0001f;
        player.collide(world,{0,0,1},.5f);
        check("REFERENCE_SHAPE/player-climbs-half-step-parts",first && std::abs(player.position.y-203.f)<.0001f && player.position.z>9.f);
        // Actual item actors settle on each exposed part and fall through the
        // empty window centre onto its bottom rim, keeping legacy .05 padding.
        player.position={8,220,8};
        const auto dropRest=[&](const glm::vec3 &position) {
            const auto id=world.spawnItemEntity(Material::StoneStep,1,position);
            auto *item=dynamic_cast<ItemEntity *>(world.getActorManager().findActor(id));
            if(!item) return -1.f;
            item->setPickupDelay(100.f);
            for(int i=0;i<80;++i) item->tick(world,.025f);
            const float height=item->position.y;item->kill();return height;
        };
        world.setBlock(14,200,9,ChunkBlock(BlockId::StoneStep,0));
        check("REFERENCE_SHAPE/drop-rests-on-low-half",std::abs(dropRest({14.5f,200.8f,9.1f})-200.55f)<.001f);
        check("REFERENCE_SHAPE/drop-rests-on-upper-half",std::abs(dropRest({14.5f,201.3f,9.8f})-201.05f)<.001f);
        check("REFERENCE_SHAPE/drop-falls-inside-window-to-rim",std::abs(dropRest({8.5f,215.5f,6.5f})-215.175f)<.001f);
        world.setBlock(12,200,12,ChunkBlock(BlockId::StoneStep,0));
        glm::vec3 settled(0);bool grounded=false;WildlifeMotionPath path=WildlifeMotionPath::None;
        const auto wildlife=[&](const glm::vec3 &from,const glm::vec3 &to) {
            return world.tryWildlifeStep(from,to,{.1f,.3f,.1f},settled,&grounded,&path)==World::WildlifeStepResult::Allowed && grounded;
        };
        check("REFERENCE_SHAPE/wildlife-low-half-level-path",wildlife({12.5f,200.5f,12.2f},{12.5f,200.5f,12.25f}) &&
            path==WildlifeMotionPath::GroundedLevel && std::abs(settled.y-200.5f)<.001f);
        check("REFERENCE_SHAPE/wildlife-half-step-rise-path",wildlife({12.5f,200.5f,12.25f},{12.5f,200.5f,12.65f}) &&
            path==WildlifeMotionPath::SupportRise && std::abs(settled.y-201.f)<.001f);
        check("REFERENCE_SHAPE/wildlife-half-step-descent-path",wildlife({12.5f,201.f,12.8f},{12.5f,201.f,12.25f}) &&
            path==WildlifeMotionPath::SupportDescent && std::abs(settled.y-200.5f)<.001f);
        // Isolated high section: actual mesh consumes the same bounded surfaces.
        world.setBlock(4,240,4,ChunkBlock(BlockId::StoneStep,0));world.setBlock(10,240,10,ChunkBlock(BlockId::StoneWindowFrame,0));
        auto *section=world.getChunkManager().getChunk(0,0).findSection(15);
        bool meshMatches=section!=nullptr;
        if(section) {
            SectionMeshInput input;section->captureMeshInput(input);ChunkMeshCollection meshes;ChunkMeshBuilder(input,meshes).buildMesh();
            const auto &positions=meshes.solidMesh.getClientMesh().vertexPositions;
            std::vector<float> expected;
            for(const auto &pair:{std::pair<BlockId,glm::ivec3>{BlockId::StoneStep,{4,240,4}}, {BlockId::StoneWindowFrame,{10,240,10}}})
                for(const auto &face:BlockDatabase::get().getDefinition(pair.first).render.shape.variants[0].surfaces)
                    for(int k=0;k<4;++k) for(int a=0;a<3;++a) expected.push_back(face.positions[k*3+a]+pair.second[a]);
            meshMatches &= positions==expected;
            const auto revision=section->getBlockRevision();world.setBlock(4,240,4,ChunkBlock(BlockId::StoneStep,3));
            check("REFERENCE_SHAPE/yaw-edit-invalidates-revision",section->getBlockRevision()>revision && (section->getMeshState()==ChunkMeshState::Dirty || section->getMeshState()==ChunkMeshState::Queued));
            bool rejectedMetadata=false;try {world.setBlock(4,240,4,ChunkBlock(BlockId::StoneStep,4));}catch(const std::invalid_argument &) {rejectedMetadata=true;}
            check("REFERENCE_SHAPE/invalid-metadata-write-rejected",rejectedMetadata && world.getBlock(4,240,4)==ChunkBlock(BlockId::StoneStep,3));
        }
        check("REFERENCE_SHAPE/actual-section-mesh-surfaces",meshMatches);
        // Break and retrieve a new part through the same normal inventory path.
        player.position={8,220,8};player.getHeldItems()=ItemStack(Material::WOODEN_PICKAXE,1);
        const int before=player.getInventoryCount(Material::StoneWindowFrame);
        const bool broken=BlockInteractionSystem::breakBlock(world,player,{8.5f,215.5f,6.5f});
        check("REFERENCE_SHAPE/real-break-retrieves-part",broken && world.getBlock(8,215,6)==BlockId::Air && player.getInventoryCount(Material::StoneWindowFrame)==before+1);
        // Saved boundary parts are restored across ordinary chunk unloading.
        world.setBlock(-1,212,3,ChunkBlock(BlockId::StoneStep,2));world.setBlock(16,212,3,ChunkBlock(BlockId::StoneWindowFrame,1));
        const bool savedChunk=world.getChunkManager().saveChunk(world.getChunkManager().getChunk(-1,0));
        const bool unloaded=world.getChunkManager().unloadChunk(-1,0);world.getChunkManager().loadChunk(-1,0);
        check("REFERENCE_SHAPE/unload-reload-negative-coordinate",savedChunk && unloaded && world.getBlock(-1,212,3)==ChunkBlock(BlockId::StoneStep,2));
        persisted=world.save();
    }
    {
        Player player;World world(camera,config,player,directory,false,0);
        world.getChunkManager().loadChunk(0,0);world.getChunkManager().loadChunk(1,0);world.getChunkManager().loadChunk(-1,0);
        for(int yaw=0;yaw<4;++yaw) {
            persisted &= world.getBlock(2+yaw*3,210,2)==ChunkBlock(BlockId::StoneStep,yaw);
            persisted &= world.getBlock(2+yaw*3,210,5)==ChunkBlock(BlockId::StoneWindowFrame,yaw);
        }
        persisted &= world.getBlock(-1,212,3)==ChunkBlock(BlockId::StoneStep,2) && world.getBlock(16,212,3)==ChunkBlock(BlockId::StoneWindowFrame,1);
        persisted &= world.getBlock(4,240,4)==ChunkBlock(BlockId::StoneStep,3);
    }
    check("REFERENCE_SHAPE/save-reopen-four-yaws-and-boundaries",persisted);
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
} // namespace
