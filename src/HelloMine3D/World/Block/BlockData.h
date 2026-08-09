#ifndef BLOCKDATA_H_INCLUDED
#define BLOCKDATA_H_INCLUDED

#include "../../Maths/glm.h"
#include "../../Util/NonCopyable.h"
#include "BlockId.h"
#include <string>

/// @brief Allocates meshes to cubes and non-cube entities.
enum class BlockMeshType {
    Cube = 0,
    X = 1,
};

/// @brief Allocates shader behavior to groups of blocks.
enum class BlockShaderType {
    Chunk = 0,
    Liquid = 1,
    Flora = 2,
};

/// @brief Struct designed to hold geometric and tangibility data for each individual block.
struct BlockDataHolder : public NonCopyable {
    BlockId id;
    std::string name;
    glm::ivec2 texTopCoord;
    glm::ivec2 texSideCoord;
    glm::ivec2 texBottomCoord;

    BlockMeshType meshType;
    BlockShaderType shaderType;

    bool isOpaque;
    bool isCollidable;
};

class BlockData : public NonCopyable {
  public:
    BlockData(const std::string &fileName);

    const BlockDataHolder &getBlockData() const;

  private:
    BlockDataHolder m_data;
};

#endif // BLOCKDATA_H_INCLUDED
