#pragma once

#include "../Presentation/ItemVisualGeometry.h"
#include "../Presentation/ItemVisualPose.h"

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

    // A thin sword vanished edge-on in the production gallery. Check projected
    // front-face area around the item, from above/below, for its full lifetime.
    bool readable = true, boundedMotion = true, offStable = true;
    const glm::vec3 origin(1,2,3), rotation(29,81,13);
    for (float strength : {0.f, .35f, 1.f})
        for (int heading = 0; heading < 360; heading += 30)
            for (float elevation : {-89.f, -60.f, 0.f, 60.f, 89.f}) {
                const float yaw = glm::radians(static_cast<float>(heading));
                const float pitch = glm::radians(elevation);
                const glm::vec3 camera = origin + 4.f * glm::vec3(
                    std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                    std::cos(yaw) * std::cos(pitch));
                const auto first = ItemVisualPose::drop(origin, rotation, camera, 0, 99, true, strength);
                for (int step = 0; step <= 2400; ++step) {
                    const auto pose = ItemVisualPose::drop(origin, rotation, camera, step * .125f, 99, true, strength);
                    const glm::vec3 normal = pose.orientation * glm::vec3(0,0,1);
                    const float projectedArea = glm::dot(normal, glm::normalize(camera - pose.position));
                    readable &= std::isfinite(projectedArea) && projectedArea >= .80f;
                    boundedMotion &= std::abs(pose.position.y - origin.y - .22f) <= .02501f * strength + .000001f;
                    if (strength == 0.f)
                        offStable &= glm::length(pose.position - first.position) < .000001f &&
                            std::abs(glm::dot(pose.orientation, first.orientation)) > .999999f;
                }
            }
    check("ITEM_VISUAL/thin-drops-keep-readable-projected-face-through-lifetime", readable);
    check("ITEM_VISUAL/drop-bob-respects-feedback-strength", boundedMotion);
    check("ITEM_VISUAL/off-drop-pose-has-no-time-motion", offStable);
    const auto reduced = ItemVisualPose::drop(origin, rotation, {1,2,7}, 1.f, 99, true, .35f);
    const auto full = ItemVisualPose::drop(origin, rotation, {1,2,7}, 1.f, 99, true, 1.f);
    const auto later = ItemVisualPose::drop(origin, rotation, {1,2,7}, 2.f, 99, true, 1.f);
    check("ITEM_VISUAL/full-and-reduced-retain-visible-different-motion",
        std::abs(glm::dot(full.orientation, later.orientation)) < .995f &&
        std::abs(glm::dot(full.orientation, reduced.orientation)) < .999f);
    const auto cubeFirst = ItemVisualPose::drop(origin, {0,0,0}, {1,2,7}, 0, 99, false, 1.f);
    const auto cubeQuarter = ItemVisualPose::drop(origin, {0,0,0}, {1,2,7}, 90.f / 35.f, 99, false, 1.f);
    check("ITEM_VISUAL/cube-still-turns-through-quarter-revolution",
        glm::length(cubeFirst.orientation * glm::vec3(0,0,1) - glm::vec3(0,0,1)) < .00001f &&
        glm::length(cubeQuarter.orientation * glm::vec3(0,0,1) - glm::vec3(1,0,0)) < .00001f);
}
}
