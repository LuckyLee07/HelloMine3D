#include "BlockSelection.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../../Actor/LivingActor.h"
#include "../../Maths/Ray.h"
#include "../World.h"
#include "../Block/BlockDatabase.h"
#include "../Block/BlockGeometry.h"

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

    Ray directionRay(origin,rotation);directionRay.step(1.f);
    const glm::vec3 rawDirection=directionRay.getEnd()-origin;
    const float directionLength=glm::length(rawDirection);
    if(!std::isfinite(rawDirection.x)||!std::isfinite(rawDirection.y)||!std::isfinite(rawDirection.z) ||
       !std::isfinite(directionLength) || directionLength<0.000001f)
        return std::nullopt;
    const glm::vec3 direction=rawDirection/directionLength;
    glm::ivec3 previousPosition = toBlockPosition(origin);
    glm::ivec3 testedPosition = previousPosition + glm::ivec3(1, 1, 1);

    for (Ray ray(origin, rotation); ray.getLength() < maxDistance;
         // Ray::step measures horizontal travel. Bound each sample by actual
         // world distance so a steep look cannot skip a whole voxel cell.
         ray.step(stepSize/directionLength)) {
        const glm::ivec3 blockPosition = toBlockPosition(ray.getEnd());
        if (blockPosition == testedPosition) {
            continue;
        }
        testedPosition = blockPosition;

        const auto block = world.getBlock(blockPosition.x, blockPosition.y,
                                          blockPosition.z);
        const auto blockId = static_cast<BlockId>(block.id);
        if (blockId != BlockId::Air && blockId != BlockId::Water) {
            if(BlockGeometry::usesCompound(blockId)) {
                float distance=0;glm::ivec3 normal(0);
                if(BlockGeometry::pick(BlockDatabase::get().getDefinition(blockId),block,blockPosition,
                        origin,direction,maxDistance,distance,normal)) {
                    return BlockSelection{blockPosition,blockPosition+normal,
                        origin+direction*distance,blockId,block.metadata};
                }
                previousPosition=blockPosition;continue;
            }
            return BlockSelection{blockPosition, previousPosition,
                                  ray.getEnd(), blockId, block.metadata};
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
