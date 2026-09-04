#ifndef BLOCKDEFINITION_H_INCLUDED
#define BLOCKDEFINITION_H_INCLUDED

#include <string>

#include "../../Item/Material.h"
#include "BlockData.h"
#include "BlockCapability.h"
#include "BlockId.h"

class BlockBehavior;

struct BlockRenderInfo {
    glm::ivec2 texTopCoord{0, 0};
    glm::ivec2 texSideCoord{0, 0};
    glm::ivec2 texBottomCoord{0, 0};
    BlockMeshType meshType = BlockMeshType::Cube;
    BlockShaderType shaderType = BlockShaderType::Chunk;
    BlockShape shape;
};

struct BlockDefinition {
    BlockId id = BlockId::Air;
    std::string stringId;
    std::string name;
    float hardness = 0.25f;
    MiningClass miningClass = MiningClass::None;
    int requiredToolTier = 0;
    bool wrongToolDrops = true;
    bool solid = false;
    bool collidable = false;
    bool transparent = true;
    bool liquid = false;
    int light = 0;
    Material::ID defaultDrop = Material::ID::Nothing;
    const BlockBehavior *behavior = nullptr;
    BlockCapabilityDefinition capabilities;
    BlockRenderInfo render;
};

#endif // BLOCKDEFINITION_H_INCLUDED
