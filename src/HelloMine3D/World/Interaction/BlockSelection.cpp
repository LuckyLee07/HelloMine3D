#include "BlockSelection.h"

#include "../../Maths/Ray.h"
#include "../World.h"

namespace {
glm::ivec3 toBlockPosition(const glm::vec3 &position)
{
    return {World::toBlockCoord(position.x), World::toBlockCoord(position.y),
            World::toBlockCoord(position.z)};
}
} // namespace

std::optional<BlockSelection>
BlockSelectionSystem::pick(World &world, const glm::vec3 &origin,
                           const glm::vec3 &rotation, float maxDistance,
                           float stepSize)
{
    if (maxDistance <= 0.f || stepSize <= 0.f) {
        return std::nullopt;
    }

    glm::ivec3 previousPosition = toBlockPosition(origin);
    glm::ivec3 testedPosition = previousPosition + glm::ivec3(1, 1, 1);

    for (Ray ray(origin, rotation); ray.getLength() < maxDistance;
         ray.step(stepSize)) {
        const glm::ivec3 blockPosition = toBlockPosition(ray.getEnd());
        if (blockPosition == testedPosition) {
            continue;
        }
        testedPosition = blockPosition;

        const auto block = world.getBlock(blockPosition.x, blockPosition.y,
                                          blockPosition.z);
        const auto blockId = static_cast<BlockId>(block.id);
        if (blockId != BlockId::Air && blockId != BlockId::Water) {
            return BlockSelection{blockPosition, previousPosition,
                                  ray.getEnd(), blockId};
        }

        previousPosition = blockPosition;
    }

    return std::nullopt;
}
