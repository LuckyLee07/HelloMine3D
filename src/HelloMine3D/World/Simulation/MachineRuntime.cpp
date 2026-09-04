#include "MachineRuntime.h"

namespace
{
bool validMaterial(Material::ID id) noexcept
{
    return id > Material::ID::Nothing && id < Material::ID::Count;
}

bool validRecipe(const MachineProcessDefinition *recipe) noexcept
{
    return recipe != nullptr && !recipe->id.empty() &&
           validMaterial(recipe->inputMaterialId) &&
           validMaterial(recipe->outputMaterialId) &&
           recipe->inputAmount > 0 && recipe->outputAmount > 0 &&
           recipe->durationTicks > 0;
}

bool outputAccepts(const InventorySlotState &output,
                   const MachineProcessDefinition &recipe) noexcept
{
    const int maximum =
        Material::toMaterial(recipe.outputMaterialId).maxStackSize;
    if (output.amount == 0) {
        return output.materialId == Material::ID::Nothing &&
               recipe.outputAmount <= maximum;
    }
    return output.materialId == recipe.outputMaterialId &&
           output.amount + recipe.outputAmount <= maximum;
}

void clearIfEmpty(InventorySlotState &slot) noexcept
{
    if (slot.amount <= 0) {
        slot = {};
    }
}
} // namespace

const char *machineStatusName(MachineStatus status) noexcept
{
    switch (status) {
        case MachineStatus::Idle:
            return "idle";
        case MachineStatus::MissingInput:
            return "missing_input";
        case MachineStatus::BlockedOutput:
            return "blocked_output";
        case MachineStatus::NoPower:
            return "no_power";
        case MachineStatus::Running:
            return "running";
    }
    return "idle";
}

MachineState MachineRuntime::inspect(
    const InventorySlotState &input,
    const InventorySlotState &output,
    int progressTicks,
    int powerTicksRemaining,
    const MachineProcessDefinition *recipe) noexcept
{
    MachineState state;
    state.progressTicks = progressTicks;
    if (validRecipe(recipe)) {
        state.recipeId = recipe->id;
    }

    if (input.amount == 0 && powerTicksRemaining <= 0) {
        state.status = MachineStatus::Idle;
        return state;
    }
    if (!validRecipe(recipe) ||
        input.materialId != recipe->inputMaterialId ||
        input.amount < recipe->inputAmount || progressTicks < 0 ||
        progressTicks >= recipe->durationTicks) {
        state.status = MachineStatus::MissingInput;
        return state;
    }
    if (!outputAccepts(output, *recipe)) {
        state.status = MachineStatus::BlockedOutput;
        return state;
    }
    if (powerTicksRemaining <= 0) {
        state.status = MachineStatus::NoPower;
        return state;
    }
    state.status = MachineStatus::Running;
    return state;
}

MachineTickResult MachineRuntime::tick(
    InventorySlotState &input,
    InventorySlotState &output,
    int &progressTicks,
    int &powerTicksRemaining,
    const MachineProcessDefinition *recipe) noexcept
{
    MachineTickResult result;
    result.state = inspect(input, output, progressTicks,
                           powerTicksRemaining, recipe);
    if (result.state.status != MachineStatus::Running || recipe == nullptr) {
        return result;
    }

    --powerTicksRemaining;
    ++progressTicks;
    result.changed = true;
    if (progressTicks >= recipe->durationTicks) {
        input.amount -= recipe->inputAmount;
        clearIfEmpty(input);
        if (output.amount == 0) {
            output.materialId = recipe->outputMaterialId;
            output.durability = 0;
        }
        output.amount += recipe->outputAmount;
        progressTicks = 0;
        result.completed = true;
    }
    result.state.progressTicks = progressTicks;
    return result;
}
