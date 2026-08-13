#ifndef PLAYER_H_INCLUDED
#define PLAYER_H_INCLUDED

#include <optional>
#include <vector>

#include "../Entity/Entity.h"
#include "../Item/Inventory.h"
#include "PlayerController.h"

class World;

using PlayerInventorySlot = InventorySlotState;

struct PlayerSaveState {
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    int heldItem = 0;
    std::vector<PlayerInventorySlot> inventory;
};

/// @brief Player character, including player movements and world interactions.
class Player : public Entity {
    friend class PlayerController;

  public:
    Player();

    void applyInput(const PlayerInputState &input);

    void update(float dt, World &wolrd);
    void collide(World &world, const glm::vec3 &vel, float dt);

    bool addItem(const Material &material);
    int addItem(const Material &material, int amount);
    bool removeHeldItem(int amount = 1);

    ItemStack &getHeldItems();
    const ItemStack &getInventorySlot(int index) const;
    int getInventorySlotCount() const;
    int getInventoryCapacity(const Material &material) const;
    int removeInventoryItem(int slot, int amount);
    void openContainer(const glm::ivec3 &containerPosition);
    void closeContainer() noexcept;
    bool hasOpenContainer() const noexcept;
    const std::optional<glm::ivec3> &getOpenContainer() const noexcept;
    bool isFlying() const noexcept;
    bool isSneaking() const noexcept;
    PlayerSaveState getSaveState() const;
    void applySaveState(const PlayerSaveState &state);

  private:
    void jump();

    bool m_isOnGround = false;
    bool m_isFlying = false;
    bool m_isSneak = false;

    Inventory m_inventory;
    std::optional<glm::ivec3> m_openContainer;
    PlayerController m_controller;
    glm::vec3 m_acceleration;
};

#endif // PLAYER_H_INCLUDED
