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
    BlockId id = BlockId::Air;
    std::string name;
    glm::ivec2 texTopCoord{0};
    glm::ivec2 texSideCoord{0};
    glm::ivec2 texBottomCoord{0};

    BlockMeshType meshType = BlockMeshType::Cube;
    BlockShaderType shaderType = BlockShaderType::Chunk;

    int light = 0;
    bool isOpaque = false;
    bool isCollidable = false;
};

class BlockData : public NonCopyable {
  public:
    BlockData(const std::string &fileName);
    BlockData(const std::string &fileName,
              const std::string &blockDirectory);

    const BlockDataHolder &getBlockData() const;
    const std::string &getSourcePath() const;

  private:
    void load(const std::string &path);

    BlockDataHolder m_data;
    std::string m_sourcePath;
};

#endif // BLOCKDATA_H_INCLUDED
