#ifndef BLOCKDEFINITION_H_INCLUDED
#define BLOCKDEFINITION_H_INCLUDED

#include <string>

#include "../../Item/Material.h"
#include "BlockData.h"
#include "BlockId.h"

struct BlockRenderInfo {
    sf::Vector2i texTopCoord{0, 0};
    sf::Vector2i texSideCoord{0, 0};
    sf::Vector2i texBottomCoord{0, 0};
    BlockMeshType meshType = BlockMeshType::Cube;
    BlockShaderType shaderType = BlockShaderType::Chunk;
};

struct BlockDefinition {
    BlockId id = BlockId::Air;
    std::string stringId;
    std::string name;
    float hardness = 1.f;
    bool solid = false;
    bool collidable = false;
    bool transparent = true;
    bool liquid = false;
    int light = 0;
    Material::ID defaultDrop = Material::ID::Nothing;
    BlockRenderInfo render;
};

#endif // BLOCKDEFINITION_H_INCLUDED
