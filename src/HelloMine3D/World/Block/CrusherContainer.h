#pragma once

#include <optional>
#include <string>

#include "../../Item/Inventory.h"
#include "../../Maths/glm.h"
#include "../Simulation/MachineRuntime.h"

class Player;
class World;

enum class CrusherSlot
{
    Input = 0,
    Output = 1
};

struct CrusherState
{
    InventorySlotState input;
    InventorySlotState output;
    int progressTicks = 0;
    int crankTicksRemaining = 0;
};

struct CrusherContainerView
{
    glm::ivec3 position{0};
    CrusherState state;
    MachineState machine;
};

class CrusherContainer
{
  public:
    static constexpr const char *BlockEntityType = "hellomine:crusher";
    static constexpr int CrankPulseTicks = 20;
    static constexpr int MaxCrankTicks = 40;

    static bool initialize(World &world, const glm::ivec3 &position);
    static bool open(World &world, Player &player,
                     const glm::ivec3 &position);
    static void close(Player &player) noexcept;
    static std::optional<CrusherContainerView>
    view(World &world, const Player &player);

    static bool transferFromPlayer(World &world, Player &player,
                                   CrusherSlot slot, int playerSlot,
                                   int amount);
    static bool transferToPlayer(World &world, Player &player,
                                 CrusherSlot slot, int amount);
    static bool supplyManualPower(World &world, Player &player,
                                  const glm::ivec3 &position);
    static int spillContents(World &world, const glm::ivec3 &position);
    static bool tickOne(World &world, const glm::ivec3 &position);
    static int tickLoaded(World &world);

    static std::string serialize(const CrusherState &state);
    static bool deserialize(const std::string &payload,
                            CrusherState &state,
                            std::string *error = nullptr);
};
