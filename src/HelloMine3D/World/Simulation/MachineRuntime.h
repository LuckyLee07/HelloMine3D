#pragma once

#include <string>

#include "../../Item/Inventory.h"
#include "../../Item/MachineProcessDefinition.h"

enum class MachineStatus
{
    Idle,
    MissingInput,
    BlockedOutput,
    NoPower,
    Running
};

const char *machineStatusName(MachineStatus status) noexcept;

struct MachineState
{
    MachineStatus status = MachineStatus::Idle;
    std::string recipeId;
    int progressTicks = 0;
};

struct MachineTickResult
{
    MachineState state;
    bool changed = false;
    bool completed = false;
};

/// Small, deterministic state transition shared by the two real C2
/// processors. Persistence, recipe ownership and power admission remain in
/// their concrete adapters.
class MachineRuntime
{
  public:
    static MachineState inspect(
        const InventorySlotState &input,
        const InventorySlotState &output,
        int progressTicks,
        int powerTicksRemaining,
        const MachineProcessDefinition *recipe) noexcept;

    static MachineTickResult tick(
        InventorySlotState &input,
        InventorySlotState &output,
        int &progressTicks,
        int &powerTicksRemaining,
        const MachineProcessDefinition *recipe) noexcept;
};
