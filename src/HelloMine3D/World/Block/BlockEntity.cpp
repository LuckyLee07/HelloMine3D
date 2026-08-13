#include "BlockEntity.h"

#include "../WorldConstants.h"

#include <set>
#include <tuple>

namespace {
bool isNamespaceCharacter(char value) noexcept
{
    return (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '_' || value == '-' ||
           value == '.';
}

bool isPathCharacter(char value) noexcept
{
    return isNamespaceCharacter(value) || value == '/';
}
} // namespace

bool isValidBlockEntityType(const std::string &type) noexcept
{
    if (type.empty() || type.size() > MaxBlockEntityTypeSize) {
        return false;
    }

    const std::size_t separator = type.find(':');
    if (separator == 0 || separator == std::string::npos ||
        separator + 1 >= type.size() ||
        type.find(':', separator + 1) != std::string::npos) {
        return false;
    }

    for (std::size_t index = 0; index < separator; ++index) {
        if (!isNamespaceCharacter(type[index])) {
            return false;
        }
    }
    for (std::size_t index = separator + 1; index < type.size(); ++index) {
        if (!isPathCharacter(type[index])) {
            return false;
        }
    }
    return true;
}

bool validateBlockEntityRecords(
    const std::vector<BlockEntityRecord> &records, std::size_t sectionCount,
    std::string *error)
{
    auto reject = [error](const std::string &reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return false;
    };

    std::set<std::tuple<int, int, int>> positions;
    const std::size_t blockHeight = sectionCount * CHUNK_SIZE;
    for (const BlockEntityRecord &record : records) {
        if (record.position.x < 0 || record.position.x >= CHUNK_SIZE ||
            record.position.z < 0 || record.position.z >= CHUNK_SIZE ||
            record.position.y < 0 ||
            static_cast<std::size_t>(record.position.y) >= blockHeight) {
            return reject("block entity position is outside the chunk");
        }
        if (!isValidBlockEntityType(record.type)) {
            return reject("block entity type is not a valid namespaced id");
        }
        if (record.payload.size() > MaxBlockEntityPayloadSize) {
            return reject("block entity payload exceeds the size limit");
        }

        const auto position = std::make_tuple(
            record.position.x, record.position.y, record.position.z);
        if (!positions.insert(position).second) {
            return reject("duplicate block entity position");
        }
    }

    if (error != nullptr) {
        error->clear();
    }
    return true;
}
