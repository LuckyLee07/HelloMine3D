#pragma once
#include "../World/Block/BlockGeometry.h"
#include "../Feedback/BlockSurfaceGeometry.h"
#include "../Presentation/ItemVisualGeometry.h"

namespace {
// Real CPU propagation, immutable section capture, production mesher and the
// upload vertex packer are all exercised together. Fixture light edits below
// only isolate the conservative self-occlusion case; they do not replace the
// actual outside/closed-room/Lantern propagation checks.
void checkArchitecturalMeshLighting(World &world)
{
    auto &chunk = world.getChunkManager().getChunk(0,0);
    const auto buildAt = [&](int y) {
        SectionMeshInput input;
        auto *section = chunk.findSection(y/CHUNK_SIZE);
        if (section) section->captureMeshInput(input);
        ChunkMeshCollection result;
        if (section) ChunkMeshBuilder(input,result).buildMesh();
        return result;
    };
    const auto faceValues = [](const ChunkMeshCollection &meshes,
                               const glm::ivec3 &cell,int axis,bool positive,float plane) {
        std::vector<glm::vec3> values;
        const auto &mesh = meshes.solidMesh.getClientMesh();
        const auto &light = meshes.solidMesh.getLight();
        const auto &sources = meshes.solidMesh.getLightSources();
        for (std::size_t f=0;f<mesh.indices.size()/6;++f) {
            const auto quad = indexedQuad(mesh,f);
            if (quad.count != 4) continue;
            std::array<glm::vec3,4> points{};
            bool matches = true;
            for (int k=0;k<4;++k) {
                const auto at = quad.vertices[k]*3;
                points[k] = {mesh.vertexPositions[at],mesh.vertexPositions[at+1],mesh.vertexPositions[at+2]};
                for (int a=0;a<3;++a) matches &= points[k][a]>=cell[a]-.0001f && points[k][a]<=cell[a]+1.0001f;
                matches &= std::abs(points[k][axis]-plane)<.0001f;
            }
            const auto normal = glm::cross(points[1]-points[0],points[2]-points[0]);
            matches &= positive ? normal[axis]>0.f : normal[axis]<0.f;
            if (matches) for (int k=0;k<4;++k) {
                const auto vertex = quad.vertices[k];
                values.emplace_back(light[vertex],sources[vertex].x,sources[vertex].y);
            }
        }
        return values;
    };
    const auto allValues = [](const std::vector<glm::vec3> &values,float light,float sky,float local) {
        return !values.empty() && std::all_of(values.begin(),values.end(),[&](const glm::vec3 &v) {
            return std::abs(v.x-light)<.001f && std::abs(v.y-sky)<.001f && std::abs(v.z-local)<.001f;
        });
    };
    const auto packedSources = [](const ChunkMeshCollection &meshes,int sectionY) {
        const auto &source = meshes.solidMesh;
        const auto packed = packTerrainRenderBatch({{{0,sectionY,0},&source}},{0,sectionY,0});
        bool same = packed.vertices.size()==source.getLight().size();
        for (std::size_t k=0;k<packed.vertices.size() && same;++k) {
            const auto &v = packed.vertices[k]; const auto s = source.getLightSources()[k];
            // uv2.z uses block+1 to distinguish source-aware vertices from
            // legacy FLOAT1 meshes, exactly as the production shader decodes.
            same &= v.light==source.getLight()[k] && v.skySource==s.x && v.blockSource==s.y+1.f &&
                std::isfinite(v.light) && s.x>=0.f && s.x<=1.f && s.y>=0.f && s.y<=1.f &&
                v.blockSource>=1.f && v.blockSource<=2.f;
        }
        return same && !packed.vertices.empty();
    };
    const auto loadedBefore = world.collectDebugStats().chunks.loadedChunks;
    // Closed StoneBrick shell: full source voxels stay dark, whereas the
    // outward wall's actual air neighbour has sky=15. The interior must never
    // borrow that opposite outward light, regardless of metadata/yaw.
    for (int y=256;y<=264;++y) for (int z=3;z<=12;++z) for (int x=3;x<=12;++x)
        if (y==256 || y==264 || x==3 || x==12 || z==3 || z==12)
            world.setBlock(x,y,z,BlockId::StoneBrick);
    const glm::ivec3 wall(3,260,7);
    for (int yaw=0;yaw<4;++yaw) {
        world.setBlock(wall.x,wall.y,wall.z,ChunkBlock(BlockId::StoneBrick,yaw));
        const auto meshes = buildAt(wall.y);
        check("B-LIGHT/opaque-wall-authoritative-dark-cell-yaw-"+std::to_string(yaw),
            world.getSunlight(wall.x,wall.y,wall.z)==0 && world.getSunlight(2,260,7)==15 &&
            world.getSunlight(4,260,7)==0 && world.getBlockLight(4,260,7)==0);
        check("B-LIGHT/opaque-wall-outside-bright-yaw-"+std::to_string(yaw),
            allValues(faceValues(meshes,wall,0,false,3.f),.8f,1.f,0.f));
        check("B-LIGHT/opaque-wall-inside-dark-yaw-"+std::to_string(yaw),
            allValues(faceValues(meshes,wall,0,true,4.f),.8f*.15f,0.f,0.f));
        check("B-LIGHT/opaque-wall-upload-vertex-sources-yaw-"+std::to_string(yaw),packedSources(meshes,wall.y/16));
    }
    world.setBlock(6,260,7,BlockId::Lantern);
    const auto lit = buildAt(wall.y);
    const auto inside = faceValues(lit,wall,0,true,4.f);
    check("B-LIGHT/closed-room-real-lantern-lights-inner-wall",
        world.getSunlight(4,260,7)==0 && world.getBlockLight(4,260,7)>0 && !inside.empty() &&
        std::all_of(inside.begin(),inside.end(),[](const glm::vec3 &v){return v.y==0.f && v.z>.4f && v.x>.4f;}));
    check("B-LIGHT/closed-room-lantern-does-not-leak-through-full-wall",
        world.getBlockLight(2,260,7)==0 && allValues(faceValues(lit,wall,0,false,3.f),.8f,1.f,0.f));
    world.setBlock(6,260,7,BlockId::Air);
    check("B-LIGHT/closed-room-removal-clears-mesh-local-source",
        allValues(faceValues(buildAt(wall.y),wall,0,true,4.f),.8f*.15f,0.f,0.f));
    // Every blocking part and yaw: top and exposed side vertices must use the
    // face's air light, even though propagation deliberately stores zero in
    // the shape's cell. Non-blocking parts retain their capability flags.
    const glm::ivec3 isolated(8,288,8);
    for (int part=0;part<12;++part) {
        const auto id = static_cast<BlockId>(33+part);
        const auto &definition = BlockDatabase::get().getDefinition(id);
        if (!definition.blocksLight) continue;
        for (int yaw=0;yaw<4;++yaw) {
            world.setBlock(isolated.x,isolated.y,isolated.z,ChunkBlock(id,yaw));
            const auto meshes = buildAt(isolated.y);
            const auto &mesh = meshes.solidMesh.getClientMesh();
            const auto &sources = meshes.solidMesh.getLightSources();
            const auto &lights = meshes.solidMesh.getLight();
            bool top=false,side=false,cardinal=true;
            for (std::size_t f=0;f<mesh.indices.size()/6;++f) {
                const auto q = indexedQuad(mesh,f);
                if (q.count!=4) {cardinal=false;continue;}
                const auto point = [&](int k){const auto a=q.vertices[k]*3;return glm::vec3(mesh.vertexPositions[a],mesh.vertexPositions[a+1],mesh.vertexPositions[a+2]);};
                const auto n = glm::cross(point(1)-point(0),point(2)-point(0));
                for (int k=0;k<4;++k) {
                    const auto v=q.vertices[k];
                    top |= n.y>0.f && sources[v].x>.99f && lights[v]>.99f;
                    side |= n.y==0.f && sources[v].x>.99f && lights[v]>.59f;
                    if (sources[v].x>.99f) {
                        const float expected=n.y>0?1.f:n.y<0?.4f:n.x!=0?.8f:.6f;
                        cardinal &= std::abs(lights[v]-expected)<.001f;
                    }
                }
            }
            const std::string label="B-LIGHT/"+definition.name+"-yaw-"+std::to_string(yaw);
            check(label+"-blocking-cell-retains-zero",world.getSunlight(8,288,8)==0 && world.getBlockLight(8,288,8)==0);
            check(label+"-exposed-top-and-side-readable",top && side && meshes.solidMesh.faces==int(definition.render.shape.variants[yaw].surfaces.size()));
            check(label+"-normal-specific-cardinal",cardinal);
            check(label+"-sources-reach-upload-vertices",packedSources(meshes,isolated.y/16));
        }
    }
    world.setBlock(8,288,8,BlockId::Air);
    // A lit cardinal neighbour behind another box in the same v2 shape is
    // deliberately unusable. Inject a dark source cell into a real section
    // to isolate this geometry guard independently of coarse propagation.
    for (const auto id:{BlockId::TimberRailing,BlockId::StoneWindowFrame}) for (int yaw=0;yaw<4;++yaw) {
        const glm::ivec3 cell(8,344,8);
        world.setBlock(cell.x,cell.y,cell.z,ChunkBlock(id,yaw));
        auto *section=chunk.findSection(cell.y/16);
        if (section) {section->setSunlight(8,8,8,0);section->setBlockLight(8,8,8,0);}
        const auto meshes=buildAt(cell.y);
        const float plane=cell.y+(id==BlockId::TimberRailing?.25f:.125f);
        check("B-LIGHT/own-upper-box-blocks-interior-top-"+std::to_string(int(id))+"-yaw-"+std::to_string(yaw),
            section && world.getSunlight(8,345,8)==15 &&
            allValues(faceValues(meshes,cell,1,true,plane),.15f,0.f,0.f));
    }
    world.setBlock(8,344,8,BlockId::Air);
    check("B-LIGHT/mesh-lighting-does-not-load-chunks",world.collectDebugStats().chunks.loadedChunks==loadedBefore);
}

void caseArchitecturalKit()
{
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","42");
    setEnv("HELLOMINE3D_PLAYER_POSITION","8 220 8");
    const auto directory=freshSaveDirectory("architectural_kit");
    Config config=makeConfig();Camera camera(config);
    bool persisted=false;
    const auto cellFor=[](int part,int yaw){return glm::ivec3(2+yaw*3,part<6?210:214,2+(part%6)*2);};
    {
        Player player;World world(camera,config,player,directory,false,0);
        for(const auto &chunk:{glm::ivec2(0,0),glm::ivec2(1,0),glm::ivec2(-1,0),glm::ivec2(0,1)})
            world.getChunkManager().loadChunk(chunk.x,chunk.y);
        check("B-KIT/append-only-twelve-identities",int(BlockId::StoneStep)==33 && int(BlockId::StoneWindowFrame)==34 &&
            int(BlockId::StoneBrick)==35 && int(BlockId::Lantern)==44 && int(BlockId::NUM_TYPES)==45 &&
            int(Material::StoneStep)==49 && int(Material::StoneWindowFrame)==50 && int(Material::StoneBrick)==51 && int(Material::Lantern)==60 && int(Material::Count)==61);
        for(int part=0;part<12;++part) {
            const auto id=static_cast<BlockId>(33+part);const auto &definition=BlockDatabase::get().getDefinition(id);
            const auto &material=Material::toMaterial(id);const std::string label="B-KIT/"+definition.name;
            const bool full=id==BlockId::StoneBrick;
            check(label+"-registered-geometry-and-identity",material.id==static_cast<Material::ID>(49+part) && material.toBlockID()==id &&
                definition.defaultDrop==material.id && definition.render.shape.isCompound() && definition.collidable && !definition.transparent &&
                definition.occludesFaces==full && definition.aoOccluder==full && definition.fullCellSolid==full &&
                definition.render.shape.variants[0].boxes.size()<=8);
            for(int yaw=0;yaw<4;++yaw) {
                const auto cell=cellFor(part,yaw);world.setBlock(cell.x,cell.y,cell.z,BlockId::Air);
                player.position={8,220,8};player.rotation.y=yaw*90.f;player.getHeldItems()=ItemStack(material,2);
                const bool placed=BlockInteractionSystem::placeBlock(world,player,glm::vec3(cell)+glm::vec3(.5f));
                check(label+"-actual-place-"+std::to_string(yaw),placed && world.getBlock(cell.x,cell.y,cell.z)==ChunkBlock(id,yaw) && player.getHeldItems().getNumInStack()==1);
                const auto selected=BlockSelectionSystem::pick(world,glm::vec3(cell)+glm::vec3(.5f,2.f,.5f),{89,0,0});
                check(label+"-actual-ray-"+std::to_string(yaw),selected && selected->blockPosition==cell && selected->metadata==yaw && selected->blockId==id);
                const auto feedback=blockSurfaceGeometry(definition,ChunkBlock(id,yaw),cell,TerrainBiome::TemperateForest,42);
                const auto item=ItemVisualGeometry::compound(definition.render.shape,definition.render.texTopCoord,definition.render.texSideCoord,definition.render.texBottomCoord,yaw);
                const auto &faces=definition.render.shape.variants[yaw].surfaces;
                bool same=feedback.size()==faces.size() && item.size()==faces.size();
                for(std::size_t i=0;i<faces.size() && same;++i) {
                    const auto tile=faces[i].material==0?definition.render.texTopCoord:faces[i].material==2?definition.render.texBottomCoord:definition.render.texSideCoord;
                    same &= feedback[i].positions==faces[i].positions && feedback[i].repeat==faces[i].repeat && feedback[i].tile==tile && item[i].tile==glm::vec2(tile);
                    for(int k=0;k<4;++k) same &= item[i].positions[k]+glm::vec3(.5f)==glm::vec3(faces[i].positions[k*3],faces[i].positions[k*3+1],faces[i].positions[k*3+2]) &&
                        item[i].uv[k]==glm::vec2(faces[i].repeat[k*2],faces[i].repeat[k*2+1]);
                }
                check(label+"-world-feedback-item-material-uv-parity-"+std::to_string(yaw),same);
            }
            const auto cell=cellFor(part,0);bool rejected=false;
            try {world.setBlock(cell.x,cell.y,cell.z,ChunkBlock(id,4));}catch(const std::invalid_argument &){rejected=true;}
            check(label+"-invalid-metadata-write-is-atomic",rejected && world.getBlock(cell.x,cell.y,cell.z)==ChunkBlock(id,0));
            for(int slot=0;slot<player.getInventorySlotCount();++slot)
                player.removeInventoryItem(slot,player.getInventorySlot(slot).getNumInStack());
            player.position={8,220,8};player.getHeldItems()=ItemStack(Material::WOODEN_PICKAXE,1);
            const int before=player.getInventoryCount(material.id);
            const bool broken=BlockInteractionSystem::breakBlock(world,player,glm::vec3(cell)+glm::vec3(.5f));
            check(label+"-actual-break-pickup-retains-identity",broken && world.getBlock(cell.x,cell.y,cell.z)==BlockId::Air && player.getInventoryCount(material.id)==before+1);
            world.setBlock(cell.x,cell.y,cell.z,ChunkBlock(id,0));
        }
        world.setBlock(8,220,6,ChunkBlock(BlockId::TimberRailing,0));world.setBlock(8,220,5,BlockId::StoneBrick);
        const auto gap=BlockSelectionSystem::pick(world,{8.5f,220.5f,8.5f},{0,0,0});
        const auto rail=BlockSelectionSystem::pick(world,{8.5f,220.9f,8.5f},{0,0,0});
        check("B-KIT/normal-railing-gap-picks-behind",gap && gap->blockPosition==glm::ivec3(8,220,5) && rail && rail->blockPosition==glm::ivec3(8,220,6));
        for(const BlockId stair:{BlockId::StoneSlab,BlockId::ClayTileStep}) {
            world.setBlock(8,200,8,BlockId::StoneBrick);world.setBlock(8,201,9,ChunkBlock(stair,0));
            player.position={8.5f,202,8.5f};player.velocity={0,0,1};player.collide(world,{0,0,1},.35f);
            const bool half=std::abs(player.position.y-202.5f)<.001f;
            if(stair==BlockId::ClayTileStep) player.collide(world,{0,0,1},.5f);
            check("B-KIT/real-half-step-height-"+std::to_string(int(stair)),half &&
                (stair!=BlockId::ClayTileStep || std::abs(player.position.y-203.f)<.001f));
        }
        player.position={8,220,8};
        const auto dropRest=[&](Material::ID material,const glm::vec3 &position) {
            const auto actor=world.spawnItemEntity(material,1,position);
            auto *item=dynamic_cast<ItemEntity *>(world.getActorManager().findActor(actor));
            if(!item) return -1.f;
            item->setPickupDelay(100.f);
            for(int tick=0;tick<80;++tick) item->tick(world,.025f);
            const float height=item->position.y;item->kill();return height;
        };
        world.setBlock(13,200,10,ChunkBlock(BlockId::StoneSlab,0));
        world.setBlock(14,200,10,ChunkBlock(BlockId::ClayTileStep,0));
        world.setBlock(13,200,12,ChunkBlock(BlockId::StonePlanter,0));
        world.setBlock(14,200,12,ChunkBlock(BlockId::TimberBeam,0));
        world.setBlock(13,200,14,ChunkBlock(BlockId::TimberRailing,0));
        check("B-KIT/drop-real-slab-half-height",std::abs(dropRest(Material::StoneSlab,{13.5f,201.2f,10.5f})-200.55f)<.001f);
        check("B-KIT/drop-real-clay-low-half",std::abs(dropRest(Material::ClayTileStep,{14.5f,200.8f,10.1f})-200.55f)<.001f);
        check("B-KIT/drop-real-clay-upper-half",std::abs(dropRest(Material::ClayTileStep,{14.5f,201.3f,10.8f})-201.05f)<.001f);
        check("B-KIT/drop-real-planter-soil-bed",std::abs(dropRest(Material::StonePlanter,{13.5f,200.8f,12.5f})-200.425f)<.001f);
        check("B-KIT/drop-real-beam-top",std::abs(dropRest(Material::TimberBeam,{14.5f,201.f,12.5f})-200.675f)<.001f);
        check("B-KIT/drop-through-railing-gap-to-lower-rail",std::abs(dropRest(Material::TimberRailing,{13.5f,200.5f,14.5f})-200.3f)<.001f);
        glm::vec3 settled(0);bool grounded=false;WildlifeMotionPath path=WildlifeMotionPath::None;
        const auto wildlife=[&](const glm::vec3 &from,const glm::vec3 &to) {
            return world.tryWildlifeStep(from,to,{.1f,.3f,.1f},settled,&grounded,&path)==World::WildlifeStepResult::Allowed && grounded;
        };
        check("B-KIT/real-world-navigation-slab-level",wildlife({13.5f,200.5f,10.5f},{13.55f,200.5f,10.5f}) &&
            path==WildlifeMotionPath::GroundedLevel && std::abs(settled.y-200.5f)<.001f);
        check("B-KIT/real-world-navigation-clay-rise",wildlife({14.5f,200.5f,10.25f},{14.5f,200.5f,10.65f}) &&
            path==WildlifeMotionPath::SupportRise && std::abs(settled.y-201.f)<.001f);
        // Production mesh consumes fixed cached neighbour coverage, preserving
        // the visible half of a cube next to a slab conservatively.
        const auto meshFaces=[&](BlockId first,BlockId second) {
            world.setBlock(4,240,4,ChunkBlock(first,0));world.setBlock(5,240,4,ChunkBlock(second,0));
            auto *section=world.getChunkManager().getChunk(0,0).findSection(15);
            if(!section) return std::size_t(0);
            SectionMeshInput input;section->captureMeshInput(input);ChunkMeshCollection meshes;ChunkMeshBuilder(input,meshes).buildMesh();
            return meshes.solidMesh.getClientMesh().vertexPositions.size()/12;
        };
        check("B-KIT/full-brick-neighbour-boundary-culls",meshFaces(BlockId::StoneBrick,BlockId::StoneBrick)==10);
        const auto slabFaces=meshFaces(BlockId::StoneSlab,BlockId::StoneSlab);
        check("B-KIT/equal-half-slab-boundary-culls",slabFaces==10,"faces="+std::to_string(slabFaces));
        check("B-KIT/partial-slab-never-hides-whole-brick-face",meshFaces(BlockId::StoneBrick,BlockId::StoneSlab)==11);
        checkArchitecturalMeshLighting(world);
        // Existing Furnace and Workbench pathways are exercised with bounded
        // supplied engineering inputs; ordinary gathering remains a UI route.
        const auto clearInventory=[&]() {for(int slot=0;slot<player.getInventorySlotCount();++slot) player.removeInventoryItem(slot,player.getInventorySlot(slot).getNumInStack());};
        const auto slotFor=[&](Material::ID material){for(int slot=0;slot<player.getInventorySlotCount();++slot) if(player.getInventorySlot(slot).getMaterial().id==material) return slot;return -1;};
        clearInventory();player.position={4.5f,203,6.5f};player.getHeldItems()=ItemStack(Material::FURNACE_BLOCK,1);
        const glm::ivec3 furnaceCell(4,202,4);world.setBlock(4,202,4,BlockId::Air);
        const bool furnacePlaced=BlockInteractionSystem::placeBlock(world,player,glm::vec3(furnaceCell)+glm::vec3(.5f));
        const bool furnaceOpened=BlockInteractionSystem::useBlock(world,player,glm::vec3(furnaceCell)+glm::vec3(.5f));
        player.addItem(Material::CLAY_BLOCK,2);player.addItem(Material::COAL_ORE_BLOCK,1);
        const bool loaded=FurnaceContainer::transferFromPlayer(world,player,FurnaceSlot::Input,slotFor(Material::Clay),2,runtimeSmeltingRegistry()) &&
            FurnaceContainer::transferFromPlayer(world,player,FurnaceSlot::Fuel,slotFor(Material::CoalOre),1,runtimeSmeltingRegistry());
        FurnaceContainer::tickOne(world,furnaceCell,runtimeSmeltingRegistry());
        const auto furnaceLight=world.observeLocalLights(glm::vec3(furnaceCell)+glm::vec3(.5f));
        const auto containsLight=[](const LocalLightSnapshot &snapshot,const glm::vec3 &position){for(std::size_t i=0;i<snapshot.count;++i) if(snapshot.sources[i].position==position) return true;return false;};
        check("B-KIT/furnace-active-metadata-is-indexed-light",furnacePlaced && furnaceOpened && loaded && containsLight(furnaceLight,glm::vec3(furnaceCell)+glm::vec3(.5f)));
        for(int tick=1;tick<160;++tick) FurnaceContainer::tickOne(world,furnaceCell,runtimeSmeltingRegistry());
        const auto furnace=FurnaceContainer::view(world,player,runtimeSmeltingRegistry());
        const bool fired=furnace && furnace->state.input.amount==0 && furnace->state.fuel.amount==0 && furnace->state.output.materialId==Material::ClayTileStep &&
            furnace->state.output.amount==2 && furnace->state.burnTicksRemaining==0 && FurnaceContainer::transferToPlayer(world,player,FurnaceSlot::Output,2,runtimeSmeltingRegistry());
        check("B-KIT/clay-real-furnace-fuel-output-and-recipe-discovery",fired && player.getInventoryCount(Material::ClayTileStep)==2 && world.isRecipeDiscovered("hellomine:clay_tile_eave"));
        check("B-KIT/furnace-extinguished-source-is-removed",!containsLight(world.observeLocalLights(glm::vec3(furnaceCell)+glm::vec3(.5f)),glm::vec3(furnaceCell)+glm::vec3(.5f)));
        FurnaceContainer::close(player);clearInventory();player.position={8.5f,203,10.5f};player.getHeldItems()=ItemStack(Material::WORKBENCH_BLOCK,1);
        world.setBlock(8,202,8,BlockId::Air);
        const bool workbench=BlockInteractionSystem::placeBlock(world,player,{8.5f,202.5f,8.5f}) && BlockInteractionSystem::useBlock(world,player,{8.5f,202.5f,8.5f});
        check("B-KIT/normal-workbench-opens-three-by-three",workbench && player.getCraftingGridSize()==3);
        const std::array<const char*,11> recipes={{"stone_brick","stone_slab","stone_step","stone_cornice","stone_window_frame","stone_window_sill","clay_tile_eave","timber_beam","timber_railing","stone_planter","lantern"}};
        for(const char *part:recipes) {
            clearInventory();const auto *recipe=runtimeRecipeRegistry().find("hellomine:"+std::string(part));
            bool crafted=recipe!=nullptr;
            if(recipe) {
                for(const auto &ingredient:recipe->ingredients) crafted &= player.addItem(Material::toMaterial(ingredient.materialId),ingredient.count)==ingredient.count;
                CraftingSession session(CraftingSession::WorkbenchGridSize);crafted &= session.loadRecipe(*recipe);
                const auto preview=player.previewCrafting(session,runtimeRecipeRegistry());
                const auto result=player.commitCrafting(session,runtimeRecipeRegistry(),preview,1);
                crafted &= preview.ready() && result.succeeded() && result.outputAdded==recipe->outputCount && player.getInventoryCount(recipe->outputMaterialId)==recipe->outputCount && world.isRecipeDiscovered(recipe->id);
                for(const auto &ingredient:recipe->ingredients) crafted &= player.getInventoryCount(ingredient.materialId)==0;
            }
            check("B-KIT/actual-workbench-conservation-and-discovery-"+std::string(part),crafted);
        }
        player.closeCrafting();
        // Local lighting uses resident indexed cells and immutable bounded
        // copies; yaw edits neither duplicate a source nor cause streaming.
        const auto loadedBefore=world.collectDebugStats().chunks.loadedChunks;
        const glm::vec3 eye(8.5f,230.5f,8.5f);std::uint64_t revision=0;
        for(int yaw=0;yaw<4;++yaw) {
            world.setBlock(8,230,8,ChunkBlock(BlockId::Lantern,yaw));
            const auto snapshot=world.observeLocalLights(eye);revision=snapshot.revision;
            check("B-KIT/lantern-single-indexed-source-yaw-"+std::to_string(yaw),snapshot.count==1 && snapshot.candidates==1 &&
                snapshot.sources[0].position==eye && snapshot.sources[0].energy>0 && snapshot.inspectedSections<=27 && snapshot.inspectedCells<=27*4096 &&
                world.collectDebugStats().chunks.loadedChunks==loadedBefore && world.getBlockLight(9,230,8)==13);
        }
        world.setBlock(8,230,8,BlockId::Air);const auto removed=world.observeLocalLights(eye);
        check("B-KIT/lantern-removal-clears-index-and-propagated-light",removed.count==0 && removed.revision>revision && world.getBlockLight(9,230,8)==0);
        for(int x=0;x<10;++x) world.setBlock(x,230,9,ChunkBlock(BlockId::Lantern,x%4));
        const auto capped=world.observeLocalLights({4.5f,230.5f,9.5f});
        const int expectedX[8]={4,3,5,2,6,1,7,0};bool ordered=capped.count==8 && capped.candidates==10;
        for(int i=0;i<8 && ordered;++i) ordered &= capped.sources[i].position==glm::vec3(expectedX[i]+.5f,230.5f,9.5f);
        check("B-KIT/nearest-eight-source-cap-and-stable-ties",ordered && capped.inspectedSections<=27 && capped.inspectedCells<=27*4096 && world.collectDebugStats().chunks.loadedChunks==loadedBefore);
        world.setBlock(-1,230,8,ChunkBlock(BlockId::Lantern,3));const glm::vec3 negativeEye(-.5f,230.5f,8.5f);
        const auto negative=world.observeLocalLights(negativeEye),repeated=world.observeLocalLights(negativeEye);
        bool stable=negative.count==8 && negative.sources[0].position==negativeEye && negative.revision==repeated.revision && negative.count==repeated.count;
        for(std::size_t i=0;i<negative.count && stable;++i) stable &= negative.sources[i].position==repeated.sources[i].position;
        check("B-KIT/negative-coordinate-light-index-is-stable",stable);
        const bool savedChunk=world.getChunkManager().saveChunk(world.getChunkManager().getChunk(-1,0));
        const bool unloaded=world.getChunkManager().unloadChunk(-1,0);const auto absent=world.observeLocalLights(negativeEye);
        check("B-KIT/unloaded-source-is-not-observed-or-reloaded",savedChunk && unloaded && !containsLight(absent,negativeEye) && world.collectDebugStats().chunks.loadedChunks+1==loadedBefore);
        world.getChunkManager().loadChunk(-1,0);
        check("B-KIT/reloaded-source-index-is-rebuilt",containsLight(world.observeLocalLights(negativeEye),negativeEye) && world.getBlock(-1,230,8)==ChunkBlock(BlockId::Lantern,3));
        world.getChunkManager().getChunk(0,0).setBlock(12,230,9,ChunkBlock(BlockId::Lantern,2));
        check("B-KIT/direct-chunk-edit-updates-emitter-index",containsLight(world.observeLocalLights({12.5f,230.5f,9.5f}),{12.5f,230.5f,9.5f}));
        world.setBlock(-1,212,3,ChunkBlock(BlockId::StonePlanter,2));world.setBlock(16,212,3,ChunkBlock(BlockId::Lantern,1));
        persisted=world.save();
    }
    {
        Player player;World world(camera,config,player,directory,false,0);
        for(const auto &chunk:{glm::ivec2(0,0),glm::ivec2(1,0),glm::ivec2(-1,0)}) world.getChunkManager().loadChunk(chunk.x,chunk.y);
        for(int part=0;part<12;++part) for(int yaw=0;yaw<4;++yaw) {
            const auto cell=cellFor(part,yaw);persisted &= world.getBlock(cell.x,cell.y,cell.z)==ChunkBlock(static_cast<BlockId>(33+part),yaw);
        }
        persisted &= world.getBlock(-1,212,3)==ChunkBlock(BlockId::StonePlanter,2) && world.getBlock(16,212,3)==ChunkBlock(BlockId::Lantern,1);
        persisted &= world.isRecipeDiscovered("hellomine:lantern") && world.isRecipeDiscovered("hellomine:clay_tile_eave");
        const auto light=world.observeLocalLights({-.5f,230.5f,8.5f});
        persisted &= light.count>0 && light.sources[0].position==glm::vec3(-.5f,230.5f,8.5f);
    }
    check("B-KIT/save-reopen-all-twelve-four-yaws-boundaries-recipes-and-lights",persisted);
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
