#include "ItemEntity.h"

#include "../Player/Player.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../World/World.h"
#include "../World/Block/BlockGeometry.h"
#include "../World/Block/BlockDatabase.h"

#include <algorithm>
#include <cmath>
#include <limits>

ItemEntity::ItemEntity(ActorId id, Material::ID materialId, int amount,
                       const glm::vec3 &actorPosition)
    : Actor(id, "item", actorPosition, glm::vec3(0.25f, 0.25f, 0.25f))
    , m_materialId(materialId)
    , m_amount(amount)
{
}

void ItemEntity::tick(World &world, float dt)
{
    if (!isAlive()) {
        return;
    }

    if (m_materialId == Material::ID::Nothing || m_amount <= 0) {
        kill();
        return;
    }

    m_ageSeconds += std::max(0.f, dt);
    if (m_ageSeconds >= MaxLifetimeSeconds) {
        kill();
        return;
    }

    if (m_pickupDelay > 0.f) {
        m_pickupDelay -= dt;
    }

    applyPickupAttraction(world, dt);
    updatePhysics(world, dt);
    tryPickup(world);
}

ActorSaveState ItemEntity::getSaveState() const
{
    ActorSaveState state = Actor::getSaveState();
    state.kind = ActorSaveKind::Item;
    state.materialId = static_cast<int>(m_materialId);
    state.amount = m_amount;
    state.pickupDelay = m_pickupDelay;
    // ActorSaveState::wanderTime is kind-specific and was previously zero for
    // item actors. Reusing it preserves save v11 while making lifetime
    // deterministic across save/reload.
    state.wanderTime = m_ageSeconds;
    return state;
}

ActorSnapshot ItemEntity::getSnapshot() const
{
    ActorSnapshot snapshot = Actor::getSnapshot();
    snapshot.itemMaterialId = static_cast<int>(m_materialId);
    snapshot.itemAmount = m_amount;
    snapshot.itemAgeSeconds = m_ageSeconds;
    return snapshot;
}

void ItemEntity::applySaveState(const ActorSaveState &state)
{
    Actor::applySaveState(state);
    m_materialId = static_cast<Material::ID>(state.materialId);
    m_amount = state.amount;
    m_pickupDelay = state.pickupDelay;
    m_ageSeconds = std::clamp(
        state.wanderTime, 0.f, MaxLifetimeSeconds);
    m_groundBounces = 0;
}

Material::ID ItemEntity::getMaterialId() const
{
    return m_materialId;
}

int ItemEntity::getAmount() const
{
    return m_amount;
}

float ItemEntity::getPickupDelay() const
{
    return m_pickupDelay;
}

void ItemEntity::setPickupDelay(float seconds)
{
    m_pickupDelay = seconds;
}

float ItemEntity::getAgeSeconds() const noexcept
{
    return m_ageSeconds;
}

void ItemEntity::applyPickupAttraction(World &world, float dt)
{
    if (m_pickupDelay > 0.f || dt <= 0.f) {
        return;
    }
    Player *player = world.getPlayer();
    if (player == nullptr) {
        return;
    }
    const glm::vec3 destination =
        player->position + glm::vec3(0.f, 0.65f, 0.f);
    const glm::vec3 delta = destination - position;
    const float distance = glm::length(delta);
    if (!std::isfinite(distance) || distance <= m_pickupRadius ||
        distance > PickupAttractionRadius) {
        return;
    }
    velocity += delta / distance * (18.f * dt);
    const float speed = glm::length(velocity);
    if (std::isfinite(speed) && speed > MaxPickupAttractionSpeed) {
        velocity *= MaxPickupAttractionSpeed / speed;
    }
}

void ItemEntity::updatePhysics(World &world, float dt)
{
    const float previousBottom = position.y - 0.05f;
    velocity.y -= 20.f * dt;
    position += velocity * dt;

    const int x = World::toBlockCoord(position.x);
    const int y = World::toBlockCoord(position.y - 0.05f);
    const int z = World::toBlockCoord(position.z);
    const auto block = world.getBlock(x, y, z);
    float support = -std::numeric_limits<float>::infinity();
    if (velocity.y <= 0.f) {
        // Keep the legacy point footprint and padding, but use actual part
        // tops for architectural cells. The bounded vertical sweep also catches
        // a thin rim or half step crossed within this simulation tick.
        const float bottom = position.y - 0.05f;
        for (int cellY = std::max(0, y);
             cellY <= std::min(255, World::toBlockCoord(previousBottom)); ++cellY) {
            const auto candidate = cellY == y ? block : world.getBlock(x, cellY, z);
            if (!BlockGeometry::usesCompound(static_cast<BlockId>(candidate.id)))
                continue;
            const auto &definition = BlockDatabase::get().getDefinition(
                static_cast<BlockId>(candidate.id));
            BlockGeometry::collisionBoxes(definition, candidate, {x, cellY, z},
                [&](const BlockGeometry::Bounds &part) {
                    if (position.x >= part.minimum.x && position.x < part.maximum.x &&
                        position.z >= part.minimum.z && position.z < part.maximum.z &&
                        bottom <= part.maximum.y &&
                        (previousBottom >= part.maximum.y || bottom >= part.minimum.y))
                        support = std::max(support, part.maximum.y);
                });
        }
        if (!BlockGeometry::usesCompound(static_cast<BlockId>(block.id)) &&
            block != 0 && block.getData().isCollidable)
            support = std::max(support, static_cast<float>(y) + 1.f);
    }
    if (std::isfinite(support)) {
        position.y = support + 0.05f;
        if (velocity.y < -1.f && m_groundBounces < MaxGroundBounces) {
            velocity.y = -velocity.y * 0.24f;
            ++m_groundBounces;
        }
        else {
            velocity.y = 0.f;
        }
        velocity.x *= 0.7f;
        velocity.z *= 0.7f;
    }

    if (position.y < 0.f) {
        position.y = 0.f;
        velocity.y = 0.f;
    }

    box.update(position);
}

void ItemEntity::tryPickup(World &world)
{
    if (m_pickupDelay > 0.f) {
        return;
    }

    Player *player = world.getPlayer();
    if (player == nullptr) {
        return;
    }

    const glm::vec3 delta = player->position - position;
    const float distanceSquared =
        delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    if (distanceSquared > m_pickupRadius * m_pickupRadius) {
        return;
    }

    const int pickedAmount =
        player->addItem(Material::toMaterial(m_materialId), m_amount);
    if (pickedAmount <= 0) {
        return;
    }

    m_amount -= pickedAmount;
    world.getEventBus().publish(ItemPickupEvent(
        DefaultPlayerActorId, getId(), m_materialId, pickedAmount, position));
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, m_materialId, pickedAmount, "item_pickup"));

    if (m_amount <= 0) {
        kill();
    }
}
