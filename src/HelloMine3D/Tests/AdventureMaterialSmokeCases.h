#pragma once

namespace {
void caseAdventureMaterials()
{
    check("ADVENTURE_MATERIAL/append-only-identities",
          static_cast<int>(BlockId::Crusher)==26 && static_cast<int>(BlockId::Snow)==27 &&
          static_cast<int>(BlockId::Silt)==32 && static_cast<int>(Material::Crusher)==42 &&
          static_cast<int>(Material::Snow)==43 && static_cast<int>(Material::Silt)==48 &&
          BlockMetadata::Tree::Oak==0);
    const BlockId blocks[]={BlockId::Snow,BlockId::Gravel,BlockId::Clay,
        BlockId::ForestFloor,BlockId::MossStone,BlockId::Silt};
    const auto directory=freshSaveDirectory("adventure_materials");
    setEnv("HELLOMINE3D_SEED","42");setEnv("HELLOMINE3D_PLAYER_POSITION","8 200 8");
    Config config=makeConfig();Camera camera(config);bool persisted=false;std::uint64_t hash=0;
    {
        Player player;World world(camera,config,player,directory,false,0);
        world.getChunkManager().loadChunk(0,0);
        // A support platform also makes this saved fixture useful for a real
        // renderer material inspection without a falling diagnostic camera.
        for(int x=0;x<CHUNK_SIZE;++x)for(int z=0;z<CHUNK_SIZE;++z)
            world.setBlock(x,198,z,BlockId::Stone);
        for(int i=0;i<6;++i) {
            const auto id=blocks[i];const auto &material=Material::toMaterial(id);
            const auto &definition=BlockDatabase::get().getDefinition(id);
            Material::ID parsed=Material::Nothing;
            check("ADVENTURE_MATERIAL/registry-"+std::to_string(i),
                  material.toBlockID()==id && material.isBlock && material.maxStackSize==99 &&
                  Material::tryParseStringId(Material::toStringId(material.id),parsed) && parsed==material.id &&
                  definition.solid && definition.collidable && !definition.transparent && !definition.liquid &&
                  definition.behavior->getDrop(definition,ChunkBlock(id))==material.id &&
                  Material::iconCoordinate(material.id).x==6+i && Material::iconCoordinate(material.id).y==8);
            const auto &tool=id==BlockId::MossStone?Material::WOODEN_PICKAXE:Material::WOODEN_SHOVEL;
            const auto evaluation=BlockInteractionSystem::evaluateMining(id,ItemStack(tool,1));
            check("ADVENTURE_MATERIAL/tool-and-drop-"+std::to_string(i),
                  evaluation.matchingClass && evaluation.meetsTier && evaluation.dropAllowed &&
                  evaluation.requiredSeconds<definition.hardness);
            PlayerInputState selectTool;selectTool.hotbarSlot=0;player.applyInput(selectTool);
            world.setBlock(2+i,200,5,id);player.getHeldItems()=ItemStack(tool,1);
            const int before=player.getInventoryCount(material.id);
            const bool broken=BlockInteractionSystem::breakBlock(world,player,{2.5f+i,200.5f,5.5f});
            const bool obtained=player.getInventoryCount(material.id)==before+1;
            PlayerInputState selectDrop;
            for(int slot=0;slot<player.getInventorySlotCount();++slot)
                if(player.getInventorySlot(slot).getMaterial().id==material.id)selectDrop.hotbarSlot=slot;
            player.applyInput(selectDrop);
            const bool placed=BlockInteractionSystem::placeBlock(world,player,{2.5f+i,200.5f,5.5f});
            check("ADVENTURE_MATERIAL/real-harvest-and-place-"+std::to_string(i),
                  broken && obtained && placed && world.getBlock(2+i,200,5)==id && player.getHeldItems().isEmpty());
        }
        bool treeTiles=true;
        for(int kind=0;kind<3;++kind)for(int leaf=0;leaf<2;++leaf) {
            const auto block=leaf?BlockId::OakLeaf:BlockId::OakBark;
            const auto &definition=BlockDatabase::get().getDefinition(block);
            world.setBlock(2+kind*3,202,8+leaf,ChunkBlock(block,static_cast<BlockMetadata_t>(kind)));
            const auto selection=TerrainAppearance::select(block,TerrainFaceKind::Side,
                definition.render.texSideCoord,TerrainBiome::TemperateForest,42,{2+kind*3,202,8+leaf},kind);
            if(kind)treeTiles &= selection.coordinates==glm::ivec2((kind-1)*3+(leaf?2:0),8) && !selection.ecologyTinted;
            else treeTiles &= selection.coordinates.y!=8;
            treeTiles &= definition.behavior->getDrop(definition,ChunkBlock(block,kind))==Material::toMaterial(block).id;
        }
        check("ADVENTURE_MATERIAL/tree-species-tiles-and-common-wood",treeTiles);
        auto *section=world.getChunkManager().getChunk(0,0).findSection(12);
        bool realTiles=section!=nullptr;std::array<bool,12> slots{};
        if(section) {
            SectionMeshInput input;section->captureMeshInput(input);ChunkMeshCollection meshes;
            ChunkMeshBuilder(input,meshes).buildMesh();
            const auto &uv=meshes.solidMesh.getClientMesh().textureCoords;
            for(std::size_t i=0;i<uv.size();i+=2)if(std::floor(uv[i+1]*16)==8) {
                const int x=static_cast<int>(std::floor(uv[i]*16));if(x>=0 && x<12)slots[x]=true;
            }
            for(bool present:slots)realTiles &= present;
        }
        check("ADVENTURE_MATERIAL/real-mesh-all-twelve-slots",realTiles);
        hash=TerrainSurvey::blockHash(world.getChunkManager().getChunk(0,0));persisted=world.save();
    }
    {
        Player player;World world(camera,config,player,directory,false,0);world.getChunkManager().loadChunk(0,0);
        persisted &= hash==TerrainSurvey::blockHash(world.getChunkManager().getChunk(0,0));
        for(int i=0;i<6;++i)persisted &= world.getBlock(2+i,200,5)==blocks[i];
        for(int kind=0;kind<3;++kind)for(int leaf=0;leaf<2;++leaf)
            persisted &= world.getBlock(2+kind*3,202,8+leaf)==ChunkBlock(leaf?BlockId::OakLeaf:BlockId::OakBark,kind);
    }
    check("ADVENTURE_MATERIAL/blocks-and-tree-metadata-save-reopen",persisted);
    clearDeterministicEnv();setEnv("HELLOMINE3D_SEED","");
}
}
