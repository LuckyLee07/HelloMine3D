#ifndef BLOCKDATABASE_H_INCLUDED
#define BLOCKDATABASE_H_INCLUDED

#include <array>
#include <memory>
#include <string>

#include "../../Util/Singleton.h"

#include "BlockId.h"
#include "BlockBehavior.h"
#include "BlockDefinition.h"
#include "BlockTypes/BlockType.h"

class BlockIdUniquenessValidator {
  public:
    void add(BlockId id, const std::string &sourcePath);

  private:
    std::array<std::string, static_cast<unsigned>(BlockId::NUM_TYPES)>
        m_sources;
};

/// @brief Singleton class that determines status and ID of blocks as a whole.
class BlockDatabase : public Singleton {
  public:
    static BlockDatabase &get();

    const BlockType &getBlock(BlockId id) const;
    const BlockData &getData(BlockId id) const;
    const BlockDefinition &getDefinition(BlockId id) const;

  private:
    BlockDatabase();
    void addBlock(BlockId id, const std::string &fileName,
                  std::unique_ptr<BlockBehavior> behavior = nullptr,
                  BlockCapabilityDefinition capabilities = {});

    std::array<std::unique_ptr<BlockType>, (unsigned)BlockId::NUM_TYPES>
        m_blocks;
    std::array<BlockDefinition, (unsigned)BlockId::NUM_TYPES> m_definitions;
    std::array<std::unique_ptr<BlockBehavior>,
               (unsigned)BlockId::NUM_TYPES>
        m_behaviors;
    BlockIdUniquenessValidator m_idValidator;
};

#endif // BLOCKDATABASE_H_INCLUDED
