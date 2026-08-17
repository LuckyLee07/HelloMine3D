#include "AlphaJourney.h"

#include <algorithm>
#include <utility>

#include "../Actor/ActorTypes.h"
#include "../Item/Material.h"
#include "../Player/Player.h"
#include "../Sandbox/Events/BlockEvents.h"
#include "../Sandbox/Events/CraftingEvents.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"
#include "../World/Block/BlockId.h"

namespace {
constexpr float FeedbackDurationSeconds = 3.f;

std::uint32_t stepFlag(AlphaJourneyStep step) noexcept
{
    const unsigned index = static_cast<unsigned>(step);
    return index < AlphaJourney::StepCount ? (1u << index) : 0u;
}
} // namespace

AlphaJourney::AlphaJourney(Player &player, SandboxEventBus &eventBus,
                           std::uint32_t persistedFlags,
                           bool restoredWorld)
    : m_player(&player)
    , m_eventBus(&eventBus)
    , m_flags(persistedFlags & KnownFlags)
{
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::PlayerInventoryChanged,
        [this](const SandboxEvent &) { refreshInventory(); }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::CraftCompleted,
        [this](const SandboxEvent &event) {
            const auto &craft =
                static_cast<const CraftCompletedEvent &>(event);
            switch (craft.outputMaterialId) {
            case Material::ID::Workbench:
                mark(AlphaJourneyStep::CraftWorkbench,
                     "Workbench crafted");
                break;
            case Material::ID::WoodenPickaxe:
                mark(AlphaJourneyStep::CraftWoodenPickaxe,
                     "Wooden Pickaxe crafted");
                break;
            case Material::ID::StonePickaxe:
                mark(AlphaJourneyStep::CraftStonePickaxe,
                     "Stone Pickaxe crafted");
                break;
            default:
                break;
            }
            refreshInventory();
        }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::BlockPlace,
        [this](const SandboxEvent &event) {
            const auto &block = static_cast<const BlockPlaceEvent &>(event);
            if (block.blockId == BlockId::Workbench) {
                mark(AlphaJourneyStep::PlaceWorkbench,
                     "Workbench placed");
            }
        }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::EntityDeath,
        [this](const SandboxEvent &event) {
            const auto &death = static_cast<const EntityDeathEvent &>(event);
            if (death.id != DefaultPlayerActorId &&
                death.killerId == DefaultPlayerActorId) {
                mark(AlphaJourneyStep::DefeatMob, "Enemy defeated");
            }
        }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::ItemPickup,
        [this](const SandboxEvent &event) {
            const auto &pickup = static_cast<const ItemPickupEvent &>(event);
            if (has(AlphaJourneyStep::DefeatMob) && pickup.amount > 0 &&
                pickup.materialId == Material::ID::Dirt) {
                mark(AlphaJourneyStep::CollectMobLoot,
                     "Enemy loot collected");
            }
            refreshInventory();
        }));

    refreshInventory();
    if (restoredWorld && has(AlphaJourneyStep::CollectMobLoot)) {
        mark(AlphaJourneyStep::ReopenWorld, "Alpha journey complete");
    }
}

AlphaJourney::~AlphaJourney()
{
    if (m_eventBus != nullptr) {
        for (unsigned subscription : m_subscriptions) {
            m_eventBus->unsubscribe(subscription);
        }
    }
}

void AlphaJourney::update(float deltaSeconds)
{
    refreshInventory();
    m_feedbackSeconds = std::max(0.f, m_feedbackSeconds -
                                          std::max(0.f, deltaSeconds));
    if (m_feedbackSeconds <= 0.f) {
        m_completionFeedback.clear();
    }
}

AlphaJourneySnapshot AlphaJourney::snapshot() const
{
    AlphaJourneySnapshot result;
    result.step = currentStep();
    result.totalSteps = StepCount;
    for (std::size_t index = 0; index < StepCount; ++index) {
        if ((m_flags & (1u << static_cast<unsigned>(index))) == 0u) {
            break;
        }
        ++result.completedSteps;
    }
    result.completionFeedback = m_completionFeedback;

    switch (result.step) {
    case AlphaJourneyStep::GatherWood:
        result.title = "Start with wood";
        result.progress = inventoryCount(
            static_cast<int>(Material::ID::OakBark));
        result.required = RequiredOakBark;
        result.instruction = "Collect Oak Bark (11 needed for tools).";
        break;
    case AlphaJourneyStep::CraftWorkbench:
        result.title = "Craft a Workbench";
        result.instruction = "Press E and fill the 2x2 grid with Oak Bark.";
        break;
    case AlphaJourneyStep::PlaceWorkbench:
        result.title = "Place the Workbench";
        result.instruction = "Select the Workbench and place it nearby.";
        break;
    case AlphaJourneyStep::CraftWoodenPickaxe:
        result.title = "Craft a Wooden Pickaxe";
        result.instruction = "Use the Workbench: 3 Oak Bark over 2 handles.";
        break;
    case AlphaJourneyStep::GatherStone:
        result.title = "Mine Stone";
        result.progress = inventoryCount(
            static_cast<int>(Material::ID::Stone));
        result.required = RequiredStone;
        result.instruction = "Use the Wooden Pickaxe to collect 3 Stone.";
        break;
    case AlphaJourneyStep::CraftStonePickaxe:
        result.title = "Craft a Stone Pickaxe";
        result.instruction = "Use 3 Stone and 2 Oak Bark at the Workbench.";
        break;
    case AlphaJourneyStep::GatherIronOre:
        result.title = "Mine Iron Ore";
        result.progress = inventoryCount(
            static_cast<int>(Material::ID::IronOre));
        result.required = 1;
        result.instruction = "Iron drops only when mined with the Stone Pickaxe.";
        break;
    case AlphaJourneyStep::DefeatMob:
        result.title = "Defeat an enemy";
        result.instruction = "Find a roaming Mob and attack until it drops loot.";
        break;
    case AlphaJourneyStep::CollectMobLoot:
        result.title = "Collect the loot";
        result.instruction = "Move close to the dropped item to pick it up.";
        break;
    case AlphaJourneyStep::ReopenWorld:
        result.title = "Save and continue";
        result.instruction =
            "Pause, save to the main menu, then reopen this world.";
        break;
    case AlphaJourneyStep::Complete:
        result.title = "Playable Alpha complete";
        result.instruction = "Your world, tools and progress are ready to continue.";
        break;
    }
    return result;
}

std::uint32_t AlphaJourney::flags() const noexcept
{
    return m_flags;
}

void AlphaJourney::refreshInventory()
{
    if (m_player == nullptr) {
        return;
    }
    if (inventoryCount(static_cast<int>(Material::ID::OakBark)) >=
        RequiredOakBark) {
        mark(AlphaJourneyStep::GatherWood, "Enough Oak Bark collected");
    }
    if (inventoryCount(static_cast<int>(Material::ID::Workbench)) > 0) {
        mark(AlphaJourneyStep::CraftWorkbench, "Workbench ready");
    }
    if (inventoryCount(static_cast<int>(Material::ID::WoodenPickaxe)) > 0) {
        mark(AlphaJourneyStep::CraftWoodenPickaxe,
             "Wooden Pickaxe ready");
    }
    if (inventoryCount(static_cast<int>(Material::ID::Stone)) >=
        RequiredStone) {
        mark(AlphaJourneyStep::GatherStone, "Enough Stone collected");
    }
    if (inventoryCount(static_cast<int>(Material::ID::StonePickaxe)) > 0) {
        mark(AlphaJourneyStep::CraftStonePickaxe, "Stone Pickaxe ready");
    }
    if (inventoryCount(static_cast<int>(Material::ID::IronOre)) > 0) {
        mark(AlphaJourneyStep::GatherIronOre, "Iron Ore acquired");
    }
}

void AlphaJourney::mark(AlphaJourneyStep step, const char *feedback)
{
    const std::uint32_t flag = stepFlag(step);
    if (flag == 0u || (m_flags & flag) != 0u) {
        return;
    }
    m_flags |= flag;
    m_completionFeedback = feedback != nullptr ? feedback : "Step complete";
    m_feedbackSeconds = FeedbackDurationSeconds;
}

bool AlphaJourney::has(AlphaJourneyStep step) const noexcept
{
    const std::uint32_t flag = stepFlag(step);
    return flag != 0u && (m_flags & flag) != 0u;
}

int AlphaJourney::inventoryCount(int materialId) const noexcept
{
    if (m_player == nullptr || materialId <= 0) {
        return 0;
    }
    int total = 0;
    for (int slot = 0; slot < m_player->getInventorySlotCount(); ++slot) {
        const ItemStack &stack = m_player->getInventorySlot(slot);
        if (static_cast<int>(stack.getMaterial().id) == materialId) {
            total += stack.getNumInStack();
        }
    }
    return total;
}

AlphaJourneyStep AlphaJourney::currentStep() const noexcept
{
    for (std::size_t index = 0; index < StepCount; ++index) {
        if ((m_flags & (1u << static_cast<unsigned>(index))) == 0u) {
            return static_cast<AlphaJourneyStep>(index);
        }
    }
    return AlphaJourneyStep::Complete;
}
