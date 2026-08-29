#pragma once

#include <optional>
#include <string>

#include "../../Item/Inventory.h"
#include "../../Maths/glm.h"

class Player;
class SmeltingRegistry;
class World;

enum class FurnaceSlot
{
    Input = 0,
    Fuel = 1,
    Output = 2
};

struct FurnaceState
{
    InventorySlotState input;
    InventorySlotState fuel;
    InventorySlotState output;
    int progressTicks = 0;
    int burnTicksRemaining = 0;
    int burnTicksTotal = 0;
};

struct FurnaceContainerView
{
    glm::ivec3 position{0};
    FurnaceState state;
    int recipeDurationTicks = 0;
};

/// Three-slot furnace whose timers advance only on fixed simulation ticks.
class FurnaceContainer
{
  public:
    static constexpr const char *BlockEntityType = "hellomine:furnace";

    static bool initialize(World &world, const glm::ivec3 &position);
    static bool open(World &world, Player &player,
                     const glm::ivec3 &position,
                     const SmeltingRegistry &registry);
    static void close(Player &player) noexcept;
    static std::optional<FurnaceContainerView>
    view(World &world, const Player &player,
         const SmeltingRegistry &registry);

    static bool transferFromPlayer(World &world, Player &player,
                                   FurnaceSlot slot, int playerSlot,
                                   int amount,
                                   const SmeltingRegistry &registry);
    static bool transferToPlayer(World &world, Player &player,
                                 FurnaceSlot slot, int amount,
                                 const SmeltingRegistry &registry);
    static int spillContents(World &world, const glm::ivec3 &position,
                             const SmeltingRegistry &registry);
    static int tickLoaded(World &world,
                          const SmeltingRegistry &registry);
    static bool shouldEmitLight(const FurnaceState &state,
                                const SmeltingRegistry &registry);

    static std::string serialize(const FurnaceState &state);
    static bool deserialize(const std::string &payload,
                            const SmeltingRegistry &registry,
                            FurnaceState &state,
                            std::string *error = nullptr);
};
