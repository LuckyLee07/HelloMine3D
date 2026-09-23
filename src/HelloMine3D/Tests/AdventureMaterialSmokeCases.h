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

void caseAdventureRegionalBuildRecipes()
{
    RecipeRegistry recipes;
    std::ifstream input(ResourcePaths::media("recipes/Base.recipe"), std::ios::binary);
    std::ostringstream content;
    content << input.rdbuf();
    recipes.freeze({{"base.recipe", content.str()}});

    struct Build {
        const char *id;
        Material::ID output;
        BlockId block;
        const char *entityType;
        std::vector<RecipeIngredient> ingredients;
    };
    const std::array<Build, 3> builds{{
        {"hellomine:woodland_cache", Material::ID::Chest, BlockId::Chest, "hellomine:chest",
         {{Material::ID::MossStone, 2}, {Material::ID::OakPlank, 4},
          {Material::ID::ForestFloor, 1}}},
        {"hellomine:river_kiln", Material::ID::Furnace, BlockId::Furnace, "hellomine:furnace",
         {{Material::ID::Clay, 4}, {Material::ID::Gravel, 4},
          {Material::ID::Silt, 1}}},
        {"hellomine:highland_crusher", Material::ID::Crusher, BlockId::Crusher, "hellomine:crusher",
         {{Material::ID::Gravel, 4}, {Material::ID::Stone, 2},
          {Material::ID::OakPlank, 2}, {Material::ID::IronIngot, 1}}}
    }};

    const auto directory = freshSaveDirectory("adventure_regional_builds");
    setEnv("HELLOMINE3D_SEED", "42");
    setEnv("HELLOMINE3D_PLAYER_POSITION", "8 200 8");
    Config config = makeConfig();
    Camera camera(config);
    Player player;
    bool saved = false;
    {
        World world(camera, config, player, directory, false, 0);
        world.getChunkManager().loadChunk(0, 0);
        for (int x = 3; x <= 9; ++x)
            for (int z = 3; z <= 6; ++z) {
                world.setBlock(x, 198, z, BlockId::Stone);
                world.setBlock(x, 199, z, BlockId::Air);
            }
        player.addItem(Material::WORKBENCH_BLOCK, 1);
        PlayerInputState select;
        select.hotbarSlot = 0;
        player.applyInput(select);
        const bool stationPlaced = BlockInteractionSystem::placeBlock(
            world, player, {4.5f, 199.5f, 4.5f});
        check("ADVENTURE-REGIONAL/workbench-placed-through-player",
              stationPlaced && world.getBlock(4, 199, 4) == BlockId::Workbench);

        for (std::size_t index = 0; index < builds.size(); ++index) {
            const Build &build = builds[index];
            const RecipeDefinition *definition = recipes.find(build.id);
            bool recipeMatches = definition != nullptr &&
                definition->type == RecipeType::Shapeless &&
                definition->outputMaterialId == build.output &&
                definition->outputCount == 1 &&
                definition->ingredients.size() == build.ingredients.size();
            if (recipeMatches) {
                for (const auto &expected : build.ingredients) {
                    recipeMatches &= std::any_of(
                        definition->ingredients.begin(), definition->ingredients.end(),
                        [&](const RecipeIngredient &actual) {
                            return actual.materialId == expected.materialId &&
                                   actual.count == expected.count;
                        });
                }
            }
            const std::string label = "ADVENTURE-REGIONAL/" +
                std::to_string(index);
            check(label + "-formal-regional-recipe", recipeMatches);
            if (!recipeMatches) continue;

            bool supplied = true;
            for (const auto &ingredient : build.ingredients)
                supplied &= player.addItem(Material::toMaterial(
                    ingredient.materialId), ingredient.count) == ingredient.count;
            const bool stationOpened = BlockInteractionSystem::useBlock(
                world, player, {4.5f, 199.5f, 4.5f});
            CraftingSession session(CraftingSession::WorkbenchGridSize);
            const bool loaded = session.loadRecipe(*definition);
            const CraftingPreview preview = player.previewCrafting(session, recipes);
            const CraftingCommitResult result = player.commitCrafting(
                session, recipes, preview, 1);
            player.closeCrafting();
            bool consumed = true;
            for (const auto &ingredient : build.ingredients)
                consumed &= player.getInventoryCount(ingredient.materialId) == 0;
            check(label + "-real-workbench-craft-and-conservation",
                  supplied && stationOpened && loaded && preview.ready() &&
                  preview.recipeId == build.id && result.succeeded() &&
                  result.outputAdded == 1 && consumed &&
                  player.getInventoryCount(build.output) == 1);

            int outputSlot = -1;
            for (int slot = 0; slot < player.getInventorySlotCount(); ++slot)
                if (player.getInventorySlot(slot).getMaterial().id == build.output)
                    outputSlot = slot;
            select.hotbarSlot = outputSlot;
            if (outputSlot >= 0) player.applyInput(select);
            const int x = 6 + static_cast<int>(index);
            const bool placed = outputSlot >= 0 &&
                BlockInteractionSystem::placeBlock(world, player,
                    {float(x) + .5f, 199.5f, 4.5f});
            check(label + "-place-functional-building-block",
                  placed && world.getBlock(x, 199, 4) == build.block &&
                  player.getInventoryCount(build.output) == 0);
            const auto entity = world.getBlockEntity({x, 199, 4});
            const bool used = BlockInteractionSystem::useBlock(
                world, player, {float(x) + .5f, 199.5f, 4.5f});
            const auto opened = player.getOpenContainer();
            check(label + "-placed-block-opens-real-container",
                  entity && entity->type == build.entityType && used &&
                  opened && *opened == glm::ivec3(x, 199, 4));
            player.closeContainer();
        }
        saved = world.save();
    }
    {
        Player reopenedPlayer;
        World reopened(camera, config, reopenedPlayer, directory, false, 0);
        reopened.getChunkManager().loadChunk(0, 0);
        bool allPresent = saved;
        for (std::size_t index = 0; index < builds.size(); ++index)
        {
            const int x = 6 + static_cast<int>(index);
            const auto entity = reopened.getBlockEntity({x, 199, 4});
            allPresent &= reopened.getBlock(x, 199, 4) == builds[index].block &&
                entity && entity->type == builds[index].entityType;
        }
        check("ADVENTURE-REGIONAL/three-built-uses-survive-reopen", allPresent);
    }
    clearDeterministicEnv();
    setEnv("HELLOMINE3D_SEED", "");
}
}
