#include "Player.h"

#include "../Item/CraftingSession.h"
#include "../Item/RecipeRegistry.h"
#include "../Sandbox/Events/CraftingEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

#include "../World/World.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockGeometry.h"

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

bool Player::isOnGround() const noexcept
{
    return m_isOnGround;
}

glm::vec3 Player::getInterpolatedPosition(float alpha) const noexcept
{
    const float amount = std::clamp(alpha, 0.f, 1.f);
    return m_previousPosition + (position - m_previousPosition) * amount;
}

std::uint64_t Player::getInterpolationEpoch() const noexcept
{
    return m_interpolationEpoch;
}

void Player::resetInterpolation() noexcept
{
    m_previousPosition = position;
    ++m_interpolationEpoch;
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
        const glm::vec3 source = position;
        position.y = 300;
        velocity = glm::vec3(0.f);
        resetInterpolation();
        if (m_eventBus != nullptr)
        {
            m_eventBus->publish(PlayerTeleportEvent(
                DefaultPlayerActorId, 0, 0, source, position));
        }
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

                    const glm::vec3 before=position-step;
                    const auto &definition=BlockDatabase::get().getDefinition(static_cast<BlockId>(block.id));
                    bool hit=false;float resolved=0.f;int axis=step.y!=0?1:step.x!=0?0:2;
                    const auto clearAt=[&](const glm::vec3 &candidate) {
                        const BlockGeometry::Bounds body{candidate-box.dimensions,candidate+box.dimensions};
                        for(int cx=int(std::floor(body.minimum.x+BoundaryEpsilon));cx<=int(std::floor(body.maximum.x-BoundaryEpsilon));++cx)
                        for(int cy=int(std::floor(body.minimum.y+BoundaryEpsilon));cy<=int(std::floor(body.maximum.y-BoundaryEpsilon));++cy)
                        for(int cz=int(std::floor(body.minimum.z+BoundaryEpsilon));cz<=int(std::floor(body.maximum.z-BoundaryEpsilon));++cz) {
                            const auto neighbour=world.getBlock(cx,cy,cz);
                            if(BlockGeometry::collides(BlockDatabase::get().getDefinition(static_cast<BlockId>(neighbour.id)),
                                neighbour,{cx,cy,cz},body)) return false;
                        }
                        return true;
                    };
                    BlockGeometry::collisionBoxes(definition,block,{x,y,z},[&](const BlockGeometry::Bounds &part) {
                        const BlockGeometry::Bounds body{position-box.dimensions,position+box.dimensions};
                        if(!BlockGeometry::intersects(part,body)) return;
                        const float rise=part.maximum.y-(before.y-box.dimensions.y);
                        if(block.id==static_cast<Block_t>(BlockId::StoneStep) && axis!=1 &&
                           velocity.y<=0.f && rise>BoundaryEpsilon && rise<=.5f+BoundaryEpsilon) {
                            glm::vec3 raised=position;raised.y=part.maximum.y+box.dimensions.y;
                            if(clearAt(raised)) {position=raised;m_isOnGround=true;return;}
                        }
                        const float contact=step[axis]>0?part.minimum[axis]-box.dimensions[axis]:part.maximum[axis]+box.dimensions[axis];
                        if(!hit) resolved=contact;
                        else resolved=step[axis]>0?std::min(resolved,contact):std::max(resolved,contact);
                        hit=true;
                    });
                    if(hit) {
                        position[axis]=resolved;velocity[axis]=0.f;
                        if(axis==1 && step.y<0) m_isOnGround=true;
                        return;
                    }
                }
            }
        }
    }
}
