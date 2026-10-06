#pragma once

#include <Ogre.h>
#include <array>
#include <cstddef>
#include <fstream>
#include <iterator>
#include "../Presentation/ItemVisualGeometry.h"
#include "../Item/Material.h"
#include "../Util/ResourcePackResolver.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockDefinition.h"
#include "../World/Block/BlockTextureCoordinates.h"

inline glm::vec2 itemVisualTileOrigin(const glm::ivec2& tile)
{
    // Keep manual meshes inside the same frozen semantic cell as world meshes.
    // The half-pixel inset also avoids rounding below an integer cell boundary
    // when tilesPerRow is not a power of two.
    const auto coordinates = BlockTextureCoordinates::get(tile.x, tile.y);
    return {coordinates[2], coordinates[5]};
}

inline bool itemVisualUsesCube(Material::ID id)
{
    if (id <= Material::Nothing || id >= Material::Count) return false;
    const auto& material = Material::toMaterial(id);
    if(!material.isBlock) return false;
    const auto &render=BlockDatabase::get().getDefinition(material.toBlockID()).render;
    // Existing callers use this flag for block grip/pose (versus a thin icon).
    return render.meshType==BlockMeshType::Cube || render.shape.isCompound();
}

// One bounded, resource-pack-aware CPU cache shared by hand and world drops.
// No Ogre resources survive in the cache after the source alpha has been read.
inline const ItemVisualGeometry::Mesh& itemVisualGeometry(Material::ID id)
{
    struct Cache {
        std::array<ItemVisualGeometry::Mesh, Material::Count> meshes;
        std::array<bool, Material::Count> ready{};
        std::array<ItemVisualGeometry::Mask, Material::Count> masks{};
        Cache() {
            Ogre::Image atlas;
            std::ifstream input(runtimeResourcePackResolver().resolve("media/textures/DefaultPack.png"),
                                std::ios::binary);
            std::vector<char> bytes{std::istreambuf_iterator<char>(input), {}};
            Ogre::DataStreamPtr stream(new Ogre::MemoryDataStream(bytes.data(), bytes.size(), false, true));
            atlas.load(stream, "png");
            const auto tileSize = static_cast<std::size_t>(
                runtimeTerrainMaterialProfile().parameters().tilePixels);
            for (int value = 1; value < Material::Count; ++value) {
                const auto icon = Material::iconCoordinate(static_cast<Material::ID>(value));
                if (!icon.available()) continue;
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x)
                        masks[value][y * 16 + x] = atlas.getColourAt(
                            icon.x * tileSize + (x * tileSize + tileSize / 2) / 16,
                            icon.y * tileSize + (y * tileSize + tileSize / 2) / 16, 0).a >= .5f;
            }
        }
    };
    static Cache cache;
    static const ItemVisualGeometry::Mesh empty;
    if (id <= Material::Nothing || id >= Material::Count) return empty;
    if (!cache.ready[id]) {
        const auto& material = Material::toMaterial(id);
        const auto icon = Material::iconCoordinate(id);
        if (!icon.available()) return empty;
        if(material.isBlock) {
            const auto &render=BlockDatabase::get().getDefinition(material.toBlockID()).render;
            if(render.shape.isCompound())
                cache.meshes[id]=ItemVisualGeometry::compound(render.shape,render.texTopCoord,render.texSideCoord,render.texBottomCoord);
        }
        if (itemVisualUsesCube(id) && cache.meshes[id].empty()) {
            const auto& render = BlockDatabase::get().getDefinition(material.toBlockID()).render;
            cache.meshes[id] = ItemVisualGeometry::cube(render.texTopCoord, render.texSideCoord, render.texBottomCoord);
        }
        if (cache.meshes[id].empty())
            cache.meshes[id] = ItemVisualGeometry::extrudedIcon({icon.x, icon.y}, cache.masks[id]);
        cache.ready[id] = true;
    }
    return cache.meshes[id];
}
