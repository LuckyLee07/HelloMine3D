#ifndef BLOCKENTITY_H_INCLUDED
#define BLOCKENTITY_H_INCLUDED

#include <string>
#include <vector>

#include "../../Maths/glm.h"

struct BlockEntityRecord {
    glm::ivec3 position{0};
    std::string type;
    std::string payload;
};

constexpr std::size_t MaxBlockEntityTypeSize = 128;
constexpr std::size_t MaxBlockEntityPayloadSize = 64 * 1024;

bool isValidBlockEntityType(const std::string &type) noexcept;
bool validateBlockEntityRecords(
    const std::vector<BlockEntityRecord> &records, std::size_t sectionCount,
    std::string *error = nullptr);

#endif // BLOCKENTITY_H_INCLUDED
