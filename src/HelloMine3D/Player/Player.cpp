#include "Player.h"

#include "../Item/CraftingSession.h"
#include "../Item/RecipeRegistry.h"
#include "../Sandbox/Events/CraftingEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

#include "../World/World.h"

Player::Player()
    : Entity({2500, 125, 2500}, {0.f, 0.f, 0.f}, {0.3f, 1.f, 0.3f})
    , m_inventory(5)
    , m_previousPosition(position)
{
}

bool Player::addItem(const Material& material)
{
    return addItem(material, 1) == 1;
}

int Player::addItem(const Material &material, int amount, int durability)
{
    return m_inventory.addItem(material, amount, durability);
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

int Player::getInventoryCount(Material::ID materialId) const noexcept
{
    return m_inventory.count(materialId);
}

std::uint64_t Player::getInventoryRevision() const noexcept
{
    return m_inventory.revision();
}

bool Player::canConsumeInventory(
    const std::vector<InventorySlotState> &consumed) const
{
    return m_inventory.canConsume(consumed);
}

bool Player::consumeInventory(
    const std::vector<InventorySlotState> &consumed,
    std::uint64_t expectedRevision)
{
    return m_inventory.consume(consumed, expectedRevision);
}

int Player::removeInventoryItem(int slot, int amount)
{
    return m_inventory.removeFromSlot(slot, amount);
}

Inventory::ToolDamageResult Player::damageHeldTool(int amount)
{
    return m_inventory.damageSelectedTool(amount);
}

CraftingPreview Player::previewCrafting(
    const CraftingSession &session, const RecipeRegistry &recipes) const
{
    return session.preview(recipes, m_inventory);
}

CraftingCommitResult Player::commitCrafting(
    CraftingSession &session, const RecipeRegistry &recipes,
    const CraftingPreview &expected, int craftCount)
{
    CraftingCommitResult result =
        session.commit(recipes, m_inventory, expected, craftCount);
    if (result.succeeded() && m_eventBus != nullptr) {
        m_eventBus->publish(CraftCompletedEvent(
            result.recipeId, expected.outputMaterialId,
            result.craftsCompleted, result.outputAdded, position));
    }
    return result;
}

void Player::attachEventBus(SandboxEventBus &eventBus) noexcept
{
    m_eventBus = &eventBus;
}

void Player::detachEventBus(const SandboxEventBus &eventBus) noexcept
{
    if (m_eventBus == &eventBus) {
        m_eventBus = nullptr;
    }
}

void Player::openContainer(const glm::ivec3 &containerPosition)
{
    closeCrafting();
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

void Player::openCrafting(
    int gridSize, std::optional<glm::ivec3> workbenchPosition)
{
    closeContainer();
    m_craftingGridSize =
        gridSize == CraftingSession::WorkbenchGridSize
            ? CraftingSession::WorkbenchGridSize
            : CraftingSession::PlayerGridSize;
    m_openWorkbench = std::move(workbenchPosition);
}

void Player::closeCrafting() noexcept
{
    m_craftingGridSize = 0;
    m_openWorkbench.reset();
}

bool Player::hasOpenCrafting() const noexcept
{
    return m_craftingGridSize != 0;
}

int Player::getCraftingGridSize() const noexcept
{
    return m_craftingGridSize;
}

const std::optional<glm::ivec3> &Player::getOpenWorkbench() const noexcept
{
    return m_openWorkbench;
}

bool Player::isFlying() const noexcept
{
    return m_isFlying;
}

bool Player::isSneaking() const noexcept
{
    return m_isSneak;
}

glm::vec3 Player::getInterpolatedPosition(float alpha) const noexcept
{
    const float amount = std::clamp(alpha, 0.f, 1.f);
    return m_previousPosition + (position - m_previousPosition) * amount;
}

void Player::resetInterpolation() noexcept
{
    m_previousPosition = position;
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
    velocity = glm::vec3(0.f);
    m_input = PlayerInputState();
    m_jumpBufferSeconds = 0.f;
    m_coyoteSeconds = 0.f;
    resetInterpolation();
    m_inventory.applySaveState(state.inventory, state.heldItem);
    closeCrafting();
}

void Player::applyInput(const PlayerInputState &input)
{
    m_controller.applyInput(*this, input);
}

void Player::update(float dt, World& world)
{
    m_previousPosition = position;
    m_controller.applyMovement(*this, dt);

    if (!m_isFlying)
    {
        // Apply gravity every tick so resting contact is revalidated. The
        // collision pass below restores m_isOnGround when the floor remains.
        velocity.y -= 40 * dt;
        m_isOnGround = false;
    }

    if (position.y <= 0 && !m_isFlying)
    {
        position.y = 300;
    }

    collide(world, {velocity.x, 0, 0}, dt);

    collide(world, {0, velocity.y, 0}, dt);

    collide(world, {0, 0, velocity.z}, dt);

    box.update(position);
}

void Player::collide(World& world, const glm::vec3& vel, float dt)
{
    const glm::vec3 movement = vel * dt;
    const float distance = std::max(
        {std::abs(movement.x), std::abs(movement.y),
         std::abs(movement.z)});
    if (distance <= 0.f) {
        return;
    }

    // A single fixed tick can cover several blocks during a long fall. Moving
    // in sub-block steps turns the former end-point overlap test into a swept
    // collision test, so a one-block floor cannot be skipped.
    constexpr float MaxCollisionStep = 0.25f;
    constexpr float BoundaryEpsilon = 0.0001f;
    const int stepCount = std::max(
        1, static_cast<int>(std::ceil(distance / MaxCollisionStep)));
    const glm::vec3 step = movement / static_cast<float>(stepCount);

    for (int index = 0; index < stepCount; ++index) {
        position += step;

        // Block cells use half-open bounds. Epsilon keeps a player merely
        // touching a face from colliding with the cell on the other side.
        const int minX = static_cast<int>(
            std::floor(position.x - box.dimensions.x + BoundaryEpsilon));
        const int maxX = static_cast<int>(
            std::floor(position.x + box.dimensions.x - BoundaryEpsilon));
        const int minY = static_cast<int>(
            std::floor(position.y - box.dimensions.y + BoundaryEpsilon));
        const int maxY = static_cast<int>(
            std::floor(position.y + box.dimensions.y - BoundaryEpsilon));
        const int minZ = static_cast<int>(
            std::floor(position.z - box.dimensions.z + BoundaryEpsilon));
        const int maxZ = static_cast<int>(
            std::floor(position.z + box.dimensions.z - BoundaryEpsilon));

        for (int x = minX; x <= maxX; ++x) {
            for (int y = minY; y <= maxY; ++y) {
                for (int z = minZ; z <= maxZ; ++z) {
                    const auto block = world.getBlock(x, y, z);
                    if (block == 0 || !block.getData().isCollidable) {
                        continue;
                    }

                    if (step.y > 0.f) {
                        position.y = y - box.dimensions.y;
                        velocity.y = 0.f;
                    }
                    else if (step.y < 0.f) {
                        m_isOnGround = true;
                        position.y = y + box.dimensions.y + 1.f;
                        velocity.y = 0.f;
                    }
                    else if (step.x > 0.f) {
                        position.x = x - box.dimensions.x;
                        velocity.x = 0.f;
                    }
                    else if (step.x < 0.f) {
                        position.x = x + box.dimensions.x + 1.f;
                        velocity.x = 0.f;
                    }
                    else if (step.z > 0.f) {
                        position.z = z - box.dimensions.z;
                        velocity.z = 0.f;
                    }
                    else if (step.z < 0.f) {
                        position.z = z + box.dimensions.z + 1.f;
                        velocity.z = 0.f;
                    }
                    return;
                }
            }
        }
    }
}
