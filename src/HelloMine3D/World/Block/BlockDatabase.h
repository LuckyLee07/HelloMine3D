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

    // Nonpersistent renderer capability, set only during startup before any
    // World/mesh workers, or synchronous tests. Never hot-switch a live World.
    // False preserves the exact old mesh path for compatible legacy Water VS.
    void setWaterBoundaryPinsAvailable(bool value) noexcept
    { m_waterBoundaryPinsAvailable = value; }
    bool waterBoundaryPinsAvailable() const noexcept
    { return m_waterBoundaryPinsAvailable; }

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
    bool m_waterBoundaryPinsAvailable = false;
};

#endif // BLOCKDATABASE_H_INCLUDED
