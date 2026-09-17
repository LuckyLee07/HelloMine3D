#pragma once

#include "../Presentation/ItemVisualGeometry.h"

namespace {
void caseItemVisualPresentation()
{
    using namespace ItemVisualGeometry;
    const auto block = cube({0,0}, {1,0}, {2,0});
    check("ITEM_VISUAL/cube-uses-three-material-faces", block.size() == 6 &&
        block[0].tile == glm::vec2(1,0) && block[4].tile == glm::vec2(0,0) && block[5].tile == glm::vec2(2,0));
    Mask mask{};
    check("ITEM_VISUAL/empty-alpha-has-no-mesh", extrudedIcon({2,2}, mask).empty());
    mask[8 * 16 + 8] = true;
    const auto pixel = extrudedIcon({2,2}, mask);
    check("ITEM_VISUAL/single-pixel-closed-thickness", pixel.size() == 6 &&
        pixel[0].positions[0].z > 0.f && pixel[1].positions[0].z < 0.f);
    mask.fill(true);
    const auto solid = extrudedIcon({2,2}, mask);
    check("ITEM_VISUAL/internal-pixel-faces-culled", solid.size() == 66);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) mask[y * 16 + x] = (x + y) % 2 == 0;
    const auto worst = extrudedIcon({2,2}, mask);
    check("ITEM_VISUAL/checkerboard-bound", worst.size() == 514);
    bool bounded = true;
    for (const auto* mesh : {&block, &pixel, &solid, &worst})
        for (const auto& face : *mesh) {
            bounded &= std::abs(glm::length(face.normal) - 1.f) < .0001f;
            for (int index = 0; index < 4; ++index) {
                bounded &= glm::all(glm::lessThanEqual(glm::abs(face.positions[index]), glm::vec3(.5f)));
                bounded &= glm::all(glm::greaterThanEqual(face.uv[index], glm::vec2(0.f))) &&
                    glm::all(glm::lessThanEqual(face.uv[index], glm::vec2(1.f)));
            }
        }
    check("ITEM_VISUAL/finite-normalized-bounded-mesh", bounded);
    ItemEntity item(99, Material::WoodenPickaxe, 3, {1,2,3});
    const auto snapshot = item.getSnapshot();
    check("ITEM_VISUAL/drop-snapshot-retains-identity", snapshot.itemMaterialId == Material::WoodenPickaxe &&
        snapshot.itemAmount == 3 && snapshot.itemAgeSeconds == 0.f && item.getAmount() == 3);
    item.applySaveState(ItemEntity(100, Material::IronIngot, 7, {4,5,6}).getSaveState());
    check("ITEM_VISUAL/copied-drop-identity-is-stable", snapshot.itemMaterialId == Material::WoodenPickaxe &&
        snapshot.itemAmount == 3 && item.getSnapshot().itemMaterialId == Material::IronIngot);
}
}
