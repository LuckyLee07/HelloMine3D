#include "FurnaceContainer.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

#include "../../Item/SmeltingRegistry.h"
#include "../../Player/Player.h"
#include "../../Sandbox/Events/PlayerEvents.h"
#include "../../Sandbox/Events/SmeltingEvents.h"
#include "../World.h"
#include "BlockEntity.h"
#include "BlockId.h"

namespace
{
bool parseInt(const std::string &text, int &value)
{
    if (text.empty()) {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' ||
        parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

InventorySlotState &slot(FurnaceState &state, FurnaceSlot target)
{
    if (target == FurnaceSlot::Input) return state.input;
    if (target == FurnaceSlot::Fuel) return state.fuel;
    return state.output;
}

const InventorySlotState &slot(const FurnaceState &state,
                               FurnaceSlot target)
{
    if (target == FurnaceSlot::Input) return state.input;
    if (target == FurnaceSlot::Fuel) return state.fuel;
    return state.output;
}

bool validStack(const InventorySlotState &stack)
{
    if (stack.amount == 0) {
        return stack.materialId == Material::ID::Nothing &&
               stack.durability == 0;
    }
    if (stack.materialId == Material::ID::Nothing || stack.amount < 0) {
        return false;
    }
    const Material &material = Material::toMaterial(stack.materialId);
    return !material.isTool && stack.amount <= material.maxStackSize &&
           stack.durability == 0;
}

void clearIfEmpty(InventorySlotState &stack)
{
    if (stack.amount <= 0) {
        stack = {};
    }
}

std::optional<FurnaceContainerView>
readFurnace(World &world, const glm::ivec3 &position,
            const SmeltingRegistry &registry)
{
    const auto record = world.getBlockEntity(position);
    if (!record || record->type != FurnaceContainer::BlockEntityType ||
        static_cast<BlockId>(world.getBlock(position.x, position.y,
                                            position.z).id) !=
            BlockId::Furnace) {
        return std::nullopt;
    }
    FurnaceState state;
    if (!FurnaceContainer::deserialize(record->payload, registry, state)) {
        return std::nullopt;
    }
    const SmeltingRecipeDefinition *recipe =
        registry.findRecipe(state.input.materialId);
    return FurnaceContainerView{
        position, state, recipe != nullptr ? recipe->durationTicks : 0};
}

bool outputCanAccept(const FurnaceState &state,
                     const SmeltingRecipeDefinition &recipe)
{
    if (state.output.amount == 0) {
        return recipe.outputAmount <=
               Material::toMaterial(recipe.outputMaterialId).maxStackSize;
    }
    return state.output.materialId == recipe.outputMaterialId &&
           state.output.amount + recipe.outputAmount <=
               Material::toMaterial(recipe.outputMaterialId).maxStackSize;
}
} // namespace

std::string FurnaceContainer::serialize(const FurnaceState &state)
{
    std::ostringstream output;
    output << "v1|" << static_cast<int>(state.input.materialId) << ','
           << state.input.amount << '|'
           << static_cast<int>(state.fuel.materialId) << ','
           << state.fuel.amount << '|'
           << static_cast<int>(state.output.materialId) << ','
           << state.output.amount << '|' << state.progressTicks << '|'
           << state.burnTicksRemaining << '|' << state.burnTicksTotal;
    return output.str();
}

bool FurnaceContainer::deserialize(const std::string &payload,
                                   const SmeltingRegistry &registry,
                                   FurnaceState &state,
                                   std::string *error)
{
    auto reject = [error](const std::string &message) {
        if (error != nullptr) *error = message;
        return false;
    };
    std::array<std::string, 7> parts;
    std::istringstream input(payload);
    std::string part;
    std::size_t count = 0;
    while (std::getline(input, part, '|')) {
        if (count >= parts.size()) return reject("too many furnace fields");
        parts[count++] = part;
    }
    if (count != parts.size() || parts[0] != "v1") {
        return reject("unsupported furnace payload version");
    }
    FurnaceState parsed;
    InventorySlotState *stacks[] = {
        &parsed.input, &parsed.fuel, &parsed.output};
    for (std::size_t index = 0; index < 3; ++index) {
        const std::size_t comma = parts[index + 1].find(',');
        int material = 0;
        if (comma == std::string::npos ||
            parts[index + 1].find(',', comma + 1) != std::string::npos ||
            !parseInt(parts[index + 1].substr(0, comma), material) ||
            !parseInt(parts[index + 1].substr(comma + 1),
                      stacks[index]->amount) ||
            material < static_cast<int>(Material::ID::Nothing) ||
            material >= static_cast<int>(Material::ID::Count)) {
            return reject("invalid furnace stack");
        }
        stacks[index]->materialId = static_cast<Material::ID>(material);
        if (!validStack(*stacks[index])) {
            return reject("furnace stack violates material limits");
        }
    }
    if ((parsed.input.amount > 0 &&
         registry.findRecipe(parsed.input.materialId) == nullptr) ||
        (parsed.fuel.amount > 0 &&
         registry.findFuel(parsed.fuel.materialId) == nullptr)) {
        return reject("furnace input or fuel is not registered");
    }
    bool outputRegistered = parsed.output.amount == 0;
    for (const auto &recipe : registry.recipes()) {
        outputRegistered = outputRegistered ||
                           recipe.outputMaterialId ==
                               parsed.output.materialId;
    }
    if (!outputRegistered) {
        return reject("furnace output is not registered");
    }
    if (!parseInt(parts[4], parsed.progressTicks) ||
        !parseInt(parts[5], parsed.burnTicksRemaining) ||
        !parseInt(parts[6], parsed.burnTicksTotal) ||
        parsed.progressTicks < 0 ||
        parsed.progressTicks >= SmeltingRegistry::MaxTicks ||
        parsed.burnTicksRemaining < 0 ||
        parsed.burnTicksRemaining > SmeltingRegistry::MaxTicks ||
        parsed.burnTicksTotal < 0 ||
        parsed.burnTicksTotal > SmeltingRegistry::MaxTicks ||
        parsed.burnTicksRemaining > parsed.burnTicksTotal ||
        (parsed.burnTicksRemaining > 0 && parsed.burnTicksTotal == 0)) {
        return reject("invalid furnace timer state");
    }
    const auto *recipe = registry.findRecipe(parsed.input.materialId);
    if ((recipe == nullptr && parsed.progressTicks != 0) ||
        (recipe != nullptr &&
         parsed.progressTicks >= recipe->durationTicks)) {
        return reject("furnace progress is inconsistent with its input");
    }
    state = parsed;
    if (error != nullptr) error->clear();
    return true;
}

bool FurnaceContainer::initialize(World &world,
                                  const glm::ivec3 &position)
{
    if (static_cast<BlockId>(world.getBlock(position.x, position.y,
                                            position.z).id) !=
        BlockId::Furnace) {
        return false;
    }
    return world.createBlockEntity(position, BlockEntityType,
                                   serialize(FurnaceState{}));
}

bool FurnaceContainer::open(World &world, Player &player,
                            const glm::ivec3 &position,
                            const SmeltingRegistry &registry)
{
    if (!readFurnace(world, position, registry)) return false;
    player.openContainer(position);
    return true;
}

void FurnaceContainer::close(Player &player) noexcept
{
    player.closeContainer();
}

std::optional<FurnaceContainerView>
FurnaceContainer::view(World &world, const Player &player,
                       const SmeltingRegistry &registry)
{
    if (!player.getOpenContainer()) return std::nullopt;
    return readFurnace(world, *player.getOpenContainer(), registry);
}

bool FurnaceContainer::transferFromPlayer(
    World &world, Player &player, FurnaceSlot target, int playerSlot,
    int amount, const SmeltingRegistry &registry)
{
    auto furnace = view(world, player, registry);
    if (!furnace || target == FurnaceSlot::Output || amount <= 0 ||
        playerSlot < 0 || playerSlot >= player.getInventorySlotCount()) {
        return false;
    }
    const ItemStack &source = player.getInventorySlot(playerSlot);
    if (source.isEmpty() || source.getMaterial().isTool ||
        (target == FurnaceSlot::Input &&
         registry.findRecipe(source.getMaterial().id) == nullptr) ||
        (target == FurnaceSlot::Fuel &&
         registry.findFuel(source.getMaterial().id) == nullptr)) {
        return false;
    }
    const Material &sourceMaterial = source.getMaterial();
    InventorySlotState &destination = slot(furnace->state, target);
    if (destination.amount > 0 &&
        destination.materialId != source.getMaterial().id) {
        return false;
    }
    const int moved = std::min(
        {amount, source.getNumInStack(),
         source.getMaterial().maxStackSize - destination.amount});
    if (moved <= 0) return false;
    const Material::ID previousInput = furnace->state.input.materialId;
    destination.materialId = source.getMaterial().id;
    destination.amount += moved;
    if (target == FurnaceSlot::Input &&
        previousInput != destination.materialId) {
        furnace->state.progressTicks = 0;
    }
    if (player.removeInventoryItem(playerSlot, moved) != moved) {
        return false;
    }
    if (!world.updateBlockEntity(furnace->position,
                                 serialize(furnace->state))) {
        player.addItem(sourceMaterial, moved);
        return false;
    }
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, destination.materialId, -moved,
        target == FurnaceSlot::Input ? "furnace_input" : "furnace_fuel"));
    return true;
}

bool FurnaceContainer::transferToPlayer(
    World &world, Player &player, FurnaceSlot target, int amount,
    const SmeltingRegistry &registry)
{
    auto furnace = view(world, player, registry);
    if (!furnace || amount <= 0) return false;
    InventorySlotState &source = slot(furnace->state, target);
    if (source.amount <= 0 || source.materialId == Material::ID::Nothing) {
        return false;
    }
    const Material &material = Material::toMaterial(source.materialId);
    const int moved = std::min(
        {amount, source.amount, player.getInventoryCapacity(material)});
    if (moved <= 0) return false;
    const Material::ID materialId = source.materialId;
    source.amount -= moved;
    clearIfEmpty(source);
    if (target == FurnaceSlot::Input) {
        furnace->state.progressTicks = 0;
    }
    if (!world.updateBlockEntity(furnace->position,
                                 serialize(furnace->state)) ||
        player.addItem(material, moved) != moved) {
        return false;
    }
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, materialId, moved,
        target == FurnaceSlot::Output ? "furnace_output" :
        (target == FurnaceSlot::Input ? "furnace_input_return" :
                                       "furnace_fuel_return")));
    return true;
}

int FurnaceContainer::spillContents(World &world,
                                    const glm::ivec3 &position,
                                    const SmeltingRegistry &registry)
{
    const auto removed = world.removeBlockEntity(position);
    if (!removed || removed->type != BlockEntityType) return 0;
    FurnaceState state;
    if (!deserialize(removed->payload, registry, state)) return 0;
    const InventorySlotState stacks[] = {
        state.input, state.fuel, state.output};
    int spawned = 0;
    for (const auto &stackValue : stacks) {
        if (stackValue.amount > 0 &&
            world.spawnItemEntity(
                stackValue.materialId, stackValue.amount,
                glm::vec3(position) + glm::vec3(0.5f, 0.8f, 0.5f),
                glm::vec3(0.f, 2.5f, 0.f)) != InvalidActorId) {
            ++spawned;
        }
    }
    return spawned;
}

int FurnaceContainer::tickLoaded(World &world,
                                 const SmeltingRegistry &registry)
{
    if (!registry.isFrozen()) return 0;
    int completed = 0;
    for (const glm::ivec3 &position :
         world.collectLoadedBlockEntityPositions(BlockEntityType)) {
        auto furnace = readFurnace(world, position, registry);
        if (!furnace) continue;
        FurnaceState &state = furnace->state;
        const auto *recipe = registry.findRecipe(state.input.materialId);
        if (recipe == nullptr || state.input.amount <= 0 ||
            !outputCanAccept(state, *recipe)) {
            continue;
        }
        bool changed = false;
        if (state.burnTicksRemaining <= 0) {
            const auto *fuel = registry.findFuel(state.fuel.materialId);
            if (fuel == nullptr || state.fuel.amount <= 0) continue;
            --state.fuel.amount;
            clearIfEmpty(state.fuel);
            state.burnTicksRemaining = fuel->burnTicks;
            state.burnTicksTotal = fuel->burnTicks;
            changed = true;
        }
        --state.burnTicksRemaining;
        ++state.progressTicks;
        changed = true;
        bool recipeCompleted = false;
        if (state.progressTicks >= recipe->durationTicks) {
            --state.input.amount;
            clearIfEmpty(state.input);
            if (state.output.amount == 0) {
                state.output.materialId = recipe->outputMaterialId;
            }
            state.output.amount += recipe->outputAmount;
            state.progressTicks = 0;
            recipeCompleted = true;
        }
        if (state.burnTicksRemaining == 0) {
            state.burnTicksTotal = 0;
        }
        if (changed &&
            world.updateBlockEntity(position, serialize(state)) &&
            recipeCompleted) {
            ++completed;
            world.getEventBus().publish(SmeltCompletedEvent(
                recipe->inputMaterialId, recipe->outputMaterialId,
                recipe->outputAmount, position));
        }
    }
    return completed;
}
