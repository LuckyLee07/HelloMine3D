#include "BlockSelection.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../Actor/LivingActor.h"
#include "../../Maths/Ray.h"
#include "../World.h"

namespace {
glm::ivec3 toBlockPosition(const glm::vec3 &position)
{
    return {World::toBlockCoord(position.x), World::toBlockCoord(position.y),
            World::toBlockCoord(position.z)};
}

bool rayIntersectsActor(const glm::vec3 &origin,
                        const glm::vec3 &direction,
                        const ActorSnapshot &snapshot,
                        float maxDistance, float &distance)
{
    const glm::vec3 minimum = snapshot.position - snapshot.dimensions;
    const glm::vec3 maximum = snapshot.position + snapshot.dimensions;
    float nearDistance = 0.f;
    float farDistance = maxDistance;

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 0.000001f) {
            if (origin[axis] < minimum[axis] ||
                origin[axis] > maximum[axis]) {
                return false;
            }
            continue;
        }

        float first = (minimum[axis] - origin[axis]) / direction[axis];
        float second = (maximum[axis] - origin[axis]) / direction[axis];
        if (first > second) {
            std::swap(first, second);
        }
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) {
            return false;
        }
    }

    distance = nearDistance;
    return nearDistance <= maxDistance && farDistance >= 0.f;
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

std::optional<ActorSelection>
ActorSelectionSystem::pick(World &world, const glm::vec3 &origin,
                           const glm::vec3 &rotation, float maxDistance,
                           float stepSize)
{
    if (maxDistance <= 0.f || stepSize <= 0.f) {
        return std::nullopt;
    }

    const float yaw = glm::radians(rotation.y + 90.f);
    const float pitch = glm::radians(rotation.x);
    const glm::vec3 direction = glm::normalize(glm::vec3(
        -std::cos(yaw), -std::tan(pitch), -std::sin(yaw)));

    ActorSelection nearest;
    nearest.distance = std::numeric_limits<float>::max();
    for (const ActorSnapshot &snapshot : world.collectActorSnapshots()) {
        const Actor *actor = world.getActorManager().findActor(snapshot.id);
        float distance = 0.f;
        if (dynamic_cast<const LivingActor *>(actor) == nullptr ||
            !rayIntersectsActor(origin, direction, snapshot,
                                maxDistance, distance) ||
            distance >= nearest.distance) {
            continue;
        }
        nearest = ActorSelection{snapshot.id,
                                 origin + direction * distance,
                                 distance};
    }
    if (nearest.actorId != InvalidActorId) {
        return nearest;
    }
    return std::nullopt;
}

PlayerTargetSelection
PlayerTargetSelectionSystem::pick(World &world, const glm::vec3 &origin,
                                  const glm::vec3 &rotation,
                                  float maxDistance, float stepSize)
{
    PlayerTargetSelection selection;
    selection.block = BlockSelectionSystem::pick(
        world, origin, rotation, maxDistance, stepSize);
    selection.actor = ActorSelectionSystem::pick(
        world, origin, rotation, maxDistance, stepSize);

    if (!selection.block || !selection.actor) {
        return selection;
    }

    const float blockDistance =
        glm::distance(origin, selection.block->hitPoint);
    if (selection.actor->distance <= blockDistance) {
        selection.block.reset();
    }
    else {
        selection.actor.reset();
    }
    return selection;
}
