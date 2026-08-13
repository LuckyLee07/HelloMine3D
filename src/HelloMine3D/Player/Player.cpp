#include "Player.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#include "../World/World.h"

Player::Player()
    : Entity({2500, 125, 2500}, {0.f, 0.f, 0.f}, {0.3f, 1.f, 0.3f})
    , m_inventory(5)
    , m_acceleration(glm::vec3(0.f))
{
}

bool Player::addItem(const Material& material)
{
    return addItem(material, 1) == 1;
}

int Player::addItem(const Material &material, int amount)
{
    return m_inventory.addItem(material, amount);
}

bool Player::removeHeldItem(int amount)
{
    return m_inventory.removeFromSelected(amount);
}

ItemStack& Player::getHeldItems()
{
    return m_inventory.getSelectedStack();
}

const ItemStack &Player::getInventorySlot(int index) const
{
    return m_inventory.getSlot(index);
}

int Player::getInventorySlotCount() const
{
    return m_inventory.getSlotCount();
}

int Player::getInventoryCapacity(const Material &material) const
{
    return m_inventory.capacityFor(material);
}

int Player::removeInventoryItem(int slot, int amount)
{
    return m_inventory.removeFromSlot(slot, amount);
}

void Player::openContainer(const glm::ivec3 &containerPosition)
{
    m_openContainer = containerPosition;
}

void Player::closeContainer() noexcept
{
    m_openContainer.reset();
}

bool Player::hasOpenContainer() const noexcept
{
    return m_openContainer.has_value();
}

const std::optional<glm::ivec3> &Player::getOpenContainer() const noexcept
{
    return m_openContainer;
}

bool Player::isFlying() const noexcept
{
    return m_isFlying;
}

bool Player::isSneaking() const noexcept
{
    return m_isSneak;
}

PlayerSaveState Player::getSaveState() const
{
    PlayerSaveState state;
    state.position = position;
    state.rotation = rotation;
    state.heldItem = m_inventory.getSelectedSlot();
    state.inventory = m_inventory.getSaveState();

    return state;
}

void Player::applySaveState(const PlayerSaveState &state)
{
    closeContainer();
    position = state.position;
    rotation = state.rotation;
    m_inventory.applySaveState(state.inventory, state.heldItem);
}

void Player::applyInput(const PlayerInputState &input)
{
    m_controller.applyInput(*this, input);
}

void Player::update(float dt, World& world)
{
    velocity += m_acceleration;
    m_acceleration = {0, 0, 0};

    if (!m_isFlying)
    {
        if (!m_isOnGround)
        {
            velocity.y -= 40 * dt;
        }
        m_isOnGround = false;
    }

    if (position.y <= 0 && !m_isFlying)
    {
        position.y = 300;
    }

    position.x += velocity.x * dt;
    collide(world, {velocity.x, 0, 0}, dt);

    position.y += velocity.y * dt;
    collide(world, {0, velocity.y, 0}, dt);

    position.z += velocity.z * dt;
    collide(world, {0, 0, velocity.z}, dt);

    box.update(position);
    velocity.x *= 0.95f;
    velocity.z *= 0.95f;
    if (m_isFlying)
    {
        velocity.y *= 0.95f;
    }
}

void Player::collide(World& world, const glm::vec3& vel, float dt)
{
    (void)dt;

    const int minX = static_cast<int>(position.x - box.dimensions.x);
    const int maxX = static_cast<int>(position.x + box.dimensions.x);
    const int minY = static_cast<int>(position.y - box.dimensions.y);
    const int maxY = static_cast<int>(position.y + 0.7f);
    const int minZ = static_cast<int>(position.z - box.dimensions.z);
    const int maxZ = static_cast<int>(position.z + box.dimensions.z);

    for (int x = minX; x < maxX; x++)
        for (int y = minY; y < maxY; y++)
            for (int z = minZ; z < maxZ; z++)
            {
                auto block = world.getBlock(x, y, z);

                if (block != 0 && block.getData().isCollidable)
                {
                    if (vel.y > 0)
                    {
                        position.y = y - box.dimensions.y;
                        velocity.y = 0;
                    }
                    else if (vel.y < 0)
                    {
                        m_isOnGround = true;
                        position.y = y + box.dimensions.y + 1;
                        velocity.y = 0;
                    }

                    if (vel.x > 0)
                    {
                        position.x = x - box.dimensions.x;
                    }
                    else if (vel.x < 0)
                    {
                        position.x = x + box.dimensions.x + 1;
                    }

                    if (vel.z > 0)
                    {
                        position.z = z - box.dimensions.z;
                    }
                    else if (vel.z < 0)
                    {
                        position.z = z + box.dimensions.z + 1;
                    }
                }
            }
}

void Player::jump()
{
    if (!m_isFlying)
    {
        if (m_isOnGround)
        {

            m_isOnGround = false;
            m_acceleration.y += 0.2f * 50;
        }
    }
    else
    {
        m_acceleration.y += 0.2f * 3;
    }
}
