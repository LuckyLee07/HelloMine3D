#ifndef PLAYER_H_INCLUDED
#define PLAYER_H_INCLUDED

#include <optional>
#include <vector>

#include "../Entity/Entity.h"
#include "../Item/Inventory.h"
#include "PlayerController.h"

class World;
class CraftingSession;
class RecipeRegistry;
struct CraftingPreview;
struct CraftingCommitResult;

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
    CraftingPreview previewCrafting(
        const CraftingSession &session,
        const RecipeRegistry &recipes) const;
    CraftingCommitResult commitCrafting(
        CraftingSession &session, const RecipeRegistry &recipes,
        const CraftingPreview &expected, int craftCount);
    void openContainer(const glm::ivec3 &containerPosition);
    void closeContainer() noexcept;
    bool hasOpenContainer() const noexcept;
    const std::optional<glm::ivec3> &getOpenContainer() const noexcept;
    void openCrafting(int gridSize,
                      std::optional<glm::ivec3> workbenchPosition = {});
    void closeCrafting() noexcept;
    bool hasOpenCrafting() const noexcept;
    int getCraftingGridSize() const noexcept;
    const std::optional<glm::ivec3> &getOpenWorkbench() const noexcept;
    bool isFlying() const noexcept;
    bool isSneaking() const noexcept;
    glm::vec3 getInterpolatedPosition(float alpha) const noexcept;
    void resetInterpolation() noexcept;
    PlayerSaveState getSaveState() const;
    void applySaveState(const PlayerSaveState &state);

  private:
    bool m_isOnGround = false;
    bool m_isFlying = false;
    bool m_isSneak = false;
    float m_jumpBufferSeconds = 0.f;
    float m_coyoteSeconds = 0.f;

    Inventory m_inventory;
    std::optional<glm::ivec3> m_openContainer;
    std::optional<glm::ivec3> m_openWorkbench;
    int m_craftingGridSize = 0;
    PlayerController m_controller;
    PlayerInputState m_input;
    glm::vec3 m_previousPosition{0.f};
};

#endif // PLAYER_H_INCLUDED
