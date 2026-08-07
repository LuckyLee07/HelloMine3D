#include "ItemEntity.h"

#include "../Player/Player.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../World/World.h"

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

    if (m_pickupDelay > 0.f) {
        m_pickupDelay -= dt;
    }

    updatePhysics(world, dt);
    tryPickup(world);
}

Material::ID ItemEntity::getMaterialId() const
{
    return m_materialId;
}

int ItemEntity::getAmount() const
{
    return m_amount;
}

void ItemEntity::setPickupDelay(float seconds)
{
    m_pickupDelay = seconds;
}

void ItemEntity::updatePhysics(World &world, float dt)
{
    velocity.y -= 20.f * dt;
    position += velocity * dt;

    const int x = World::toBlockCoord(position.x);
    const int y = World::toBlockCoord(position.y - 0.05f);
    const int z = World::toBlockCoord(position.z);
    auto block = world.getBlock(x, y, z);
    if (block != 0 && block.getData().isCollidable && velocity.y <= 0.f) {
        position.y = static_cast<float>(y) + 1.05f;
        velocity.y = 0.f;
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
