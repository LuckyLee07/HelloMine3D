#ifndef BLOCKDEFINITION_H_INCLUDED
#define BLOCKDEFINITION_H_INCLUDED

#include <string>

#include "../../Item/Material.h"
#include "BlockData.h"
#include "BlockId.h"

class BlockBehavior;

struct BlockRenderInfo {
    glm::ivec2 texTopCoord{0, 0};
    glm::ivec2 texSideCoord{0, 0};
    glm::ivec2 texBottomCoord{0, 0};
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
    const BlockBehavior *behavior = nullptr;
    BlockRenderInfo render;
};

#endif // BLOCKDEFINITION_H_INCLUDED
