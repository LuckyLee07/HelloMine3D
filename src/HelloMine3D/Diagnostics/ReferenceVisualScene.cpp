#include "ReferenceVisualScene.h"

#include "../Core/Camera.h"
#include "../Item/Material.h"
#include "../Player/Player.h"
#include "../World/World.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

bool buildReferenceVisualScene(World& world, Player& player, Camera& camera)
{
    const char* requested = std::getenv("HELLOMINE3D_REFERENCE_VISUAL_SCENE");
    if (requested == nullptr || std::string(requested) != "1") return false;
    const char* viewValue = std::getenv("HELLOMINE3D_REFERENCE_VISUAL_VIEW");
    const std::string view = viewValue == nullptr ? "street" : viewValue;
    if (view != "street" && view != "interior" && view != "details")
        throw std::runtime_error("Reference visual view must be street, interior or details.");

    constexpr int centreX = 208, floorY = 66, centreZ = -192;
    player.position = {centreX + 3.5f, floorY + 2.02f, centreZ + 14.5f};
    player.velocity = {0.f, 0.f, 0.f};
    player.box.update(player.position);
    world.preloadAround(player.position);
    std::size_t writes = 0;
    const auto place = [&](int x, int y, int z, ChunkBlock block) {
        if (++writes > 24000u)
            throw std::runtime_error("Reference visual scene exceeded its edit budget.");
        world.setBlock(centreX + x, floorY + y, centreZ + z, block);
    };

    // Finite courtyard: ordinary World edits own every visible surface.
    for (int z = -16; z <= 16; ++z)
    for (int x = -16; x <= 16; ++x) {
        place(x, -2, z, BlockId::Stone);
        place(x, -1, z, BlockId::Stone);
        place(x, 0, z, (x < -13 || x > 12 || z < -10) ?
              BlockId::Grass : BlockId::Cobblestone);
        for (int y = 1; y <= 13; ++y) place(x, y, z, BlockId::Air);
    }

    // Canal and one bridge. Existing water depth, motion and material paths.
    for (int z = 7; z <= 10; ++z)
    for (int x = -16; x <= 16; ++x) {
        place(x, -2, z, BlockId::Silt);
        place(x, -1, z, BlockId::Water);
        place(x, 0, z, BlockId::Air);
        if (x >= -1 && x <= 1) place(x, 0, z, BlockId::OakPlank);
    }
    for (int x = -16; x <= 16; ++x) {
        // Low banks expose the water from an ordinary standing eye height.
        place(x, 0, 6, x < -1 || x > 1 ?
              ChunkBlock(BlockId::StoneStep, 2) : ChunkBlock(BlockId::Stone));
        place(x, 0, 11, x < -1 || x > 1 ?
              ChunkBlock(BlockId::StoneStep, 0) : ChunkBlock(BlockId::Stone));
    }

    // Raised small workshop, with a two-cell doorway and real window holes.
    for (int z = -6; z <= 3; ++z)
    for (int x = -12; x <= -2; ++x) {
        place(x, 1, z, BlockId::OakPlank);
        for (int y = 2; y <= 7; ++y)
            if (x == -12 || x == -2 || z == -6 || z == 3)
                place(x, y, z, BlockId::Stone);
    }
    for (int y = 2; y <= 4; ++y)
    for (int x = -8; x <= -7; ++x) place(x, y, 3, BlockId::Air);
    for (const int x : {-11, -4})
    for (int y = 3; y <= 4; ++y) {
        place(x, y, 3, ChunkBlock(BlockId::StoneWindowFrame, 0));
        place(x, y, 2, BlockId::GlassBorderless);
    }
    for (int x = -8; x <= -7; ++x)
        place(x, 1, 4, ChunkBlock(BlockId::StoneStep, 2));
    for (int z = -7; z <= 4; ++z) {
        const int rise = std::min(z + 7, 4 - z) / 2;
        for (int x = -13; x <= -1; ++x) place(x, 8 + rise, z, BlockId::OakPlank);
        if (z >= -6 && z <= 3)
            for (int y = 8; y < 8 + rise; ++y) {
                place(-12, y, z, BlockId::Stone);
                place(-2, y, z, BlockId::Stone);
            }
    }
    place(-10, 2, -4, BlockId::Workbench);
    place(-4, 2, -4, BlockId::Furnace);
    place(-10, 2, 0, BlockId::Chest);
    place(-11, 4, -4, BlockId::Torch);
    place(-3, 4, -4, BlockId::Torch);
    place(-11, 4, 1, BlockId::Torch);
    place(-3, 4, 1, BlockId::Torch);

    // Porch and flower boxes use existing, editable assets.
    for (const int x : {-12, -2}) {
        for (int y = 1; y <= 4; ++y) place(x, y, 5, BlockId::OakBark);
        place(x + (x == -12 ? 1 : -1), 1, 5, BlockId::OakPlank);
        place(x + (x == -12 ? 1 : -1), 2, 5, BlockId::Rose);
    }
    for (int x = -12; x <= -2; ++x) place(x, 5, 5, BlockId::OakPlank);
    for (int x = 3; x <= 9; ++x) {
        place(x, 0, -4, BlockId::OakPlank);
        if (x == 3 || x == 9)
            for (int y = 1; y <= 4; ++y) place(x, y, -4, BlockId::OakBark);
        place(x, 5, -4, BlockId::OakLeaf);
        place(x, 5, -3, BlockId::OakLeaf);
    }

    // Four directions remain visible and can be mined or used normally.
    for (int yaw = 0; yaw < 4; ++yaw) {
        place(3 + yaw * 2, 1, 2, ChunkBlock(BlockId::StoneStep, yaw));
        place(3 + yaw * 2, 2, 2, ChunkBlock(BlockId::StoneWindowFrame, yaw));
    }
    if (world.getBlock(centreX + 3, floorY + 1, centreZ + 2) !=
        ChunkBlock(BlockId::StoneStep, 0) ||
        world.getBlock(centreX + 9, floorY + 2, centreZ + 2) !=
        ChunkBlock(BlockId::StoneWindowFrame, 3))
        throw std::runtime_error("Reference scene edits did not reach authoritative World.");

    player.addItem(Material::STONE_STEP_BLOCK, 32);
    player.addItem(Material::STONE_WINDOW_FRAME_BLOCK, 32);
    player.addItem(Material::STONE_PICKAXE, 1);
    player.addItem(Material::TORCH, 16);
    player.addItem(Material::OAK_PLANK_BLOCK, 32);
    if (view == "interior") {
        player.position = {centreX - 6.5f, floorY + 3.02f, centreZ + 0.5f};
        player.rotation = {-6.f, 0.f, 0.f};
    } else if (view == "details") {
        player.position = {centreX + 6.5f, floorY + 2.02f, centreZ + 5.5f};
        player.rotation = {5.f, 0.f, 0.f};
    } else {
        player.rotation = {8.f, -30.f, 0.f};
    }
    player.box.update(player.position);
    player.resetInterpolation();
    camera.hookEntity(player);
    camera.update();
    if (!world.save()) throw std::runtime_error("Reference scene initial save failed.");
    std::cout << "[REFERENCE_VISUAL_SCENE] version=1 edits=" << writes
              << " origin=" << centreX << ',' << floorY << ',' << centreZ
              << " view=" << view << " saved=1 normal_world=1\n";
    return true;
}
