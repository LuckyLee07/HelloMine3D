#include "CrusherContainer.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>

#include "../../Item/MachineProcessDefinition.h"
#include "../../Player/Player.h"
#include "../../Sandbox/Events/PlayerEvents.h"
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

bool samePosition(const glm::ivec3 &left, const glm::ivec3 &right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool validStack(const InventorySlotState &stack)
{
    if (stack.amount == 0) {
        return stack.materialId == Material::ID::Nothing &&
               stack.durability == 0;
    }
    if (stack.materialId <= Material::ID::Nothing ||
        stack.materialId >= Material::ID::Count || stack.amount < 0) {
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

InventorySlotState &slot(CrusherState &state, CrusherSlot target)
{
    return target == CrusherSlot::Input ? state.input : state.output;
}

std::optional<CrusherContainerView>
readCrusher(World &world, const glm::ivec3 &position)
{
    const auto record = world.getBlockEntity(position);
    if (!record || record->type != CrusherContainer::BlockEntityType ||
        static_cast<BlockId>(world.getBlock(position.x, position.y,
                                            position.z).id) !=
            BlockId::Crusher) {
        return std::nullopt;
    }
    CrusherState state;
    if (!CrusherContainer::deserialize(record->payload, state)) {
        return std::nullopt;
    }
    const MachineProcessDefinition &recipe =
        handCrusherProcessDefinition();
    const MachineProcessDefinition *matched =
        state.input.amount > 0 &&
                state.input.materialId == recipe.inputMaterialId
            ? &recipe
            : nullptr;
    return CrusherContainerView{
        position,
        state,
        MachineRuntime::inspect(state.input, state.output,
                                state.progressTicks,
                                state.crankTicksRemaining, matched)};
}
} // namespace

std::string CrusherContainer::serialize(const CrusherState &state)
{
    std::ostringstream output;
    output << "v1|" << static_cast<int>(state.input.materialId) << ','
           << state.input.amount << '|'
           << static_cast<int>(state.output.materialId) << ','
           << state.output.amount << '|' << state.progressTicks << '|'
           << state.crankTicksRemaining;
    return output.str();
}

bool CrusherContainer::deserialize(const std::string &payload,
                                    CrusherState &state,
                                    std::string *error)
{
    const auto reject = [error](const std::string &message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    };
    std::array<std::string, 5> parts;
    std::istringstream input(payload);
    std::string part;
    std::size_t count = 0;
    while (std::getline(input, part, '|')) {
        if (count >= parts.size()) {
            return reject("too many crusher fields");
        }
        parts[count++] = part;
    }
    if (count != parts.size() || parts[0] != "v1") {
        return reject("unsupported crusher payload version");
    }

    CrusherState parsed;
    InventorySlotState *stacks[] = {&parsed.input, &parsed.output};
    for (std::size_t index = 0; index < 2; ++index) {
        const std::size_t comma = parts[index + 1].find(',');
        int material = 0;
        if (comma == std::string::npos ||
            parts[index + 1].find(',', comma + 1) != std::string::npos ||
            !parseInt(parts[index + 1].substr(0, comma), material) ||
            !parseInt(parts[index + 1].substr(comma + 1),
                      stacks[index]->amount) ||
            material < static_cast<int>(Material::ID::Nothing) ||
            material >= static_cast<int>(Material::ID::Count)) {
            return reject("invalid crusher stack");
        }
        stacks[index]->materialId = static_cast<Material::ID>(material);
        if (!validStack(*stacks[index])) {
            return reject("crusher stack violates material limits");
        }
    }

    const MachineProcessDefinition &recipe = handCrusherProcessDefinition();
    if ((parsed.input.amount > 0 &&
         parsed.input.materialId != recipe.inputMaterialId) ||
        (parsed.output.amount > 0 &&
         parsed.output.materialId != recipe.outputMaterialId) ||
        !parseInt(parts[3], parsed.progressTicks) ||
        !parseInt(parts[4], parsed.crankTicksRemaining) ||
        parsed.progressTicks < 0 ||
        parsed.progressTicks >= recipe.durationTicks ||
        parsed.crankTicksRemaining < 0 ||
        parsed.crankTicksRemaining > MaxCrankTicks ||
        (parsed.input.amount == 0 && parsed.progressTicks != 0)) {
        return reject("crusher contents or timer state is invalid");
    }

    state = parsed;
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

bool CrusherContainer::initialize(World &world,
                                  const glm::ivec3 &position)
{
    if (static_cast<BlockId>(world.getBlock(position.x, position.y,
                                            position.z).id) !=
        BlockId::Crusher) {
        return false;
    }
    return world.createBlockEntity(position, BlockEntityType,
                                   serialize(CrusherState{}));
}

bool CrusherContainer::open(World &world, Player &player,
                            const glm::ivec3 &position)
{
    if (!readCrusher(world, position)) {
        return false;
    }
    player.openContainer(position);
    return true;
}

void CrusherContainer::close(Player &player) noexcept
{
    player.closeContainer();
}

std::optional<CrusherContainerView>
CrusherContainer::view(World &world, const Player &player)
{
    if (!player.getOpenContainer()) {
        return std::nullopt;
    }
    return readCrusher(world, *player.getOpenContainer());
}

bool CrusherContainer::transferFromPlayer(
    World &world, Player &player, CrusherSlot target, int playerSlot,
    int amount)
{
    auto crusher = view(world, player);
    if (!crusher || target != CrusherSlot::Input || amount <= 0 ||
        playerSlot < 0 || playerSlot >= player.getInventorySlotCount()) {
        return false;
    }
    const ItemStack &source = player.getInventorySlot(playerSlot);
    const MachineProcessDefinition &recipe = handCrusherProcessDefinition();
    if (source.isEmpty() ||
        source.getMaterial().id != recipe.inputMaterialId) {
        return false;
    }
    const Material &sourceMaterial = source.getMaterial();
    InventorySlotState &destination = crusher->state.input;
    if (destination.amount > 0 &&
        destination.materialId != source.getMaterial().id) {
        return false;
    }
    const int moved = std::min(
        {amount, source.getNumInStack(),
         source.getMaterial().maxStackSize - destination.amount});
    if (moved <= 0) {
        return false;
    }
    destination.materialId = source.getMaterial().id;
    destination.amount += moved;
    if (player.removeInventoryItem(playerSlot, moved) != moved) {
        return false;
    }
    if (!world.updateBlockEntity(crusher->position,
                                 serialize(crusher->state))) {
        player.addItem(sourceMaterial, moved);
        return false;
    }
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, destination.materialId, -moved,
        "crusher_input"));
    return true;
}

bool CrusherContainer::transferToPlayer(
    World &world, Player &player, CrusherSlot target, int amount)
{
    auto crusher = view(world, player);
    if (!crusher || amount <= 0 ||
        (target != CrusherSlot::Input && target != CrusherSlot::Output)) {
        return false;
    }
    InventorySlotState &source = slot(crusher->state, target);
    if (source.amount <= 0 || source.materialId == Material::ID::Nothing) {
        return false;
    }
    const Material &material = Material::toMaterial(source.materialId);
    const int moved = std::min(
        {amount, source.amount, player.getInventoryCapacity(material)});
    if (moved <= 0) {
        return false;
    }
    const Material::ID materialId = source.materialId;
    source.amount -= moved;
    clearIfEmpty(source);
    if (target == CrusherSlot::Input) {
        crusher->state.progressTicks = 0;
    }
    if (!world.updateBlockEntity(crusher->position,
                                 serialize(crusher->state)) ||
        player.addItem(material, moved) != moved) {
        return false;
    }
    world.getEventBus().publish(PlayerInventoryChangedEvent(
        DefaultPlayerActorId, materialId, moved,
        target == CrusherSlot::Output
            ? "crusher_output"
            : "crusher_input_return"));
    return true;
}

bool CrusherContainer::supplyManualPower(
    World &world, Player &player, const glm::ivec3 &position)
{
    if (!player.getOpenContainer() ||
        !samePosition(*player.getOpenContainer(), position)) {
        return false;
    }
    auto crusher = readCrusher(world, position);
    if (!crusher || crusher->state.crankTicksRemaining >= MaxCrankTicks) {
        return false;
    }
    crusher->state.crankTicksRemaining = std::min(
        MaxCrankTicks,
        crusher->state.crankTicksRemaining + CrankPulseTicks);
    return world.updateBlockEntity(position, serialize(crusher->state));
}

int CrusherContainer::spillContents(World &world,
                                    const glm::ivec3 &position)
{
    const auto removed = world.removeBlockEntity(position);
    if (!removed || removed->type != BlockEntityType) {
        return 0;
    }
    CrusherState state;
    if (!deserialize(removed->payload, state)) {
        return 0;
    }
    const InventorySlotState stacks[] = {state.input, state.output};
    int spawned = 0;
    for (const InventorySlotState &stackValue : stacks) {
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

int CrusherContainer::tickLoaded(World &world)
{
    int completed = 0;
    const MachineProcessDefinition &recipe = handCrusherProcessDefinition();
    for (const glm::ivec3 &position :
         world.collectLoadedBlockEntityPositions(BlockEntityType)) {
        auto crusher = readCrusher(world, position);
        if (!crusher) {
            continue;
        }
        CrusherState &state = crusher->state;
        const MachineProcessDefinition *matched =
            state.input.amount > 0 &&
                    state.input.materialId == recipe.inputMaterialId
                ? &recipe
                : nullptr;
        const MachineTickResult result = MachineRuntime::tick(
            state.input, state.output, state.progressTicks,
            state.crankTicksRemaining, matched);
        if (!result.changed ||
            world.updateBlockEntity(position, serialize(state))) {
            if (result.completed) {
                ++completed;
            }
        }
    }
    return completed;
}
