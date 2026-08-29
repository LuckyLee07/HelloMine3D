#include "ObjectiveSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "../Actor/ActorTypes.h"
#include "../Item/RecipeRegistry.h"
#include "../Player/Player.h"
#include "../Sandbox/Events/BlockEvents.h"
#include "../Sandbox/Events/CraftingEvents.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/FoodEvents.h"
#include "../Sandbox/Events/PlayerEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"
#include "../Sandbox/Events/SmeltingEvents.h"
#include "../Sandbox/Events/WaystoneEvents.h"

namespace
{
    constexpr float FeedbackDurationSeconds = 3.f;

    bool blockMatches(Material::ID materialId, BlockId blockId)
    {
        const Material& material = Material::toMaterial(materialId);
        return material.isBlock && material.toBlockID() == blockId;
    }

    std::string objectiveTrack(const std::string& id)
    {
        const std::size_t separator = id.find('.');
        const std::string prefix = id.substr(0, separator);
        if (prefix == "alpha" || prefix == "progression")
        {
            return "growth";
        }
        return prefix;
    }
}

ObjectiveSystem::ObjectiveSystem(const ObjectiveRegistry& registry,
                                 Player& player,
                                 SandboxEventBus& eventBus,
                                 const ObjectiveSaveState& savedState,
                                 std::uint32_t legacyAlphaFlags,
                                 bool restoredWorld)
    : m_registry(&registry)
    , m_player(&player)
    , m_eventBus(&eventBus)
{
    if (!registry.isFrozen())
    {
        throw std::runtime_error(
            "Objective system requires a frozen registry.");
    }
    m_recipeDiscoveryEnabled = &registry == &runtimeObjectiveRegistry();
    if (m_recipeDiscoveryEnabled)
    {
        ensureRuntimeRecipeRegistry();
    }

    std::unordered_map<std::string, std::string> recipeByToken;
    if (m_recipeDiscoveryEnabled)
    {
        for (const RecipeDefinition& recipe :
             runtimeRecipeRegistry().recipes())
        {
            const std::string token = recipeDiscoveryToken(recipe.id);
            if (!recipeByToken.emplace(token, recipe.id).second)
            {
                throw std::runtime_error(
                    "Recipe discovery token collision for '" + recipe.id +
                    "'.");
            }
        }
    }

    std::vector<std::string> completed = savedState.completedIds;
    const std::vector<std::string> migrated =
        ObjectiveState::completedFromLegacyFlags(legacyAlphaFlags);
    for (const std::string& id : migrated)
    {
        if (std::find(completed.begin(), completed.end(), id) ==
            completed.end())
        {
            completed.push_back(id);
        }
    }

    for (const std::string& id : completed)
    {
        if (registry.find(id) != nullptr)
        {
            m_completed.emplace(id);
        }
        else if (m_recipeDiscoveryEnabled &&
                 recipeByToken.find(id) != recipeByToken.end())
        {
            m_discoveredRecipeIds.emplace(recipeByToken.at(id));
        }
        else
        {
            m_unknownCompleted.push_back(id);
        }
    }
    for (const ObjectiveProgressState& state : savedState.progress)
    {
        const ObjectiveDefinition* definition = registry.find(state.id);
        if (definition != nullptr)
        {
            if (m_completed.find(state.id) == m_completed.end())
            {
                const int progress = std::min(
                    state.value, definition->required - 1);
                if (progress > 0)
                {
                    m_progress[state.id] = progress;
                }
            }
        }
        else
        {
            m_unknownProgress.push_back(state);
        }
    }

    if (m_recipeDiscoveryEnabled)
    {
        for (int slot = 0; slot < player.getInventorySlotCount(); ++slot)
        {
            const ItemStack& stack = player.getInventorySlot(slot);
            if (!stack.isEmpty())
            {
                unlockRecipesForMaterial(stack.getMaterial().id, false);
            }
        }
        for (const ObjectiveDefinition& definition : registry.definitions())
        {
            if (isCompleted(definition.id) &&
                definition.targetMaterial != Material::ID::Nothing)
            {
                unlockRecipesForMaterial(definition.targetMaterial, false);
            }
        }
    }

    if (restoredWorld)
    {
        for (const ObjectiveDefinition& definition : registry.definitions())
        {
            if (definition.type == ObjectiveType::ReopenWorld &&
                prerequisiteSatisfied(definition))
            {
                m_reopenEligible.emplace(definition.id);
            }
        }
    }

    const SandboxEventType eventTypes[] = {
        SandboxEventType::PlayerInventoryChanged,
        SandboxEventType::CraftCompleted,
        SandboxEventType::BlockPlace,
        SandboxEventType::BlockBreak,
        SandboxEventType::EntityDeath,
        SandboxEventType::ItemPickup,
        SandboxEventType::SmeltCompleted,
        SandboxEventType::FoodConsumed,
        SandboxEventType::WaystoneActivated,
        SandboxEventType::VictoryRewardClaimed};
    for (SandboxEventType type : eventTypes)
    {
        m_subscriptions.push_back(eventBus.subscribe(
            type, [this](const SandboxEvent& event)
            {
                consumeEvent(event);
            }));
    }
    refreshStateObjectives();
}

ObjectiveSystem::~ObjectiveSystem()
{
    if (m_eventBus != nullptr)
    {
        for (unsigned subscription : m_subscriptions)
        {
            m_eventBus->unsubscribe(subscription);
        }
    }
}

void ObjectiveSystem::update(float deltaSeconds)
{
    refreshStateObjectives();
    m_feedbackSeconds = std::max(
        0.f, m_feedbackSeconds - std::max(0.f, deltaSeconds));
    if (m_feedbackSeconds <= 0.f)
    {
        m_completionFeedback.clear();
        m_completionFeedbackId.clear();
    }
}

ObjectiveSnapshot ObjectiveSystem::snapshot() const
{
    ObjectiveSnapshot result;
    result.definitionVersion = m_registry->definitionVersion();
    for (const ObjectiveDefinition& definition : m_registry->definitions())
    {
        if (!definition.visible || definition.optional)
        {
            continue;
        }
        ++result.totalObjectives;
        if (isCompleted(definition.id))
        {
            ++result.completedObjectives;
            result.completedIds.push_back(definition.id);
            result.completedTitles.push_back(definition.title);
        }
    }
    result.completionFeedback = m_completionFeedback;
    result.completionFeedbackId = m_completionFeedbackId;

    for (const ObjectiveDefinition& definition : m_registry->definitions())
    {
        if (result.opportunities.size() >= MaxVisibleOpportunities)
        {
            break;
        }
        if (definition.visible && !definition.optional &&
            !isCompleted(definition.id) &&
            prerequisiteSatisfied(definition))
        {
            result.opportunities.push_back(makeOpportunity(definition));
        }
    }

    const ObjectiveDefinition* current = currentDefinition();
    if (current == nullptr)
    {
        result.sessionComplete = true;
        result.title = "First session complete";
        result.instruction =
            "Your world, tools and objective progress are ready to continue.";
        return result;
    }

    const ObjectiveOpportunitySnapshot& primary =
        result.opportunities.front();
    result.currentId = primary.id;
    result.title = primary.title;
    result.instruction = primary.instruction;
    result.required = primary.required;
    result.progress = primary.progress;
    if (result.opportunities.size() > 1)
    {
        result.nextId = result.opportunities[1].id;
        result.nextTitle = result.opportunities[1].title;
    }
    return result;
}

ObjectiveSaveState ObjectiveSystem::saveState() const
{
    ObjectiveSaveState state;
    state.definitionVersion = m_registry->definitionVersion();
    for (const ObjectiveDefinition& definition : m_registry->definitions())
    {
        if (isCompleted(definition.id))
        {
            state.completedIds.push_back(definition.id);
        }
        const auto found = m_progress.find(definition.id);
        if (found != m_progress.end() && found->second > 0 &&
            !isCompleted(definition.id))
        {
            state.progress.push_back({definition.id, found->second});
        }
    }
    std::vector<std::string> unknownCompleted = m_unknownCompleted;
    for (const std::string& recipeId : m_discoveredRecipeIds)
    {
        unknownCompleted.push_back(recipeDiscoveryToken(recipeId));
    }
    std::sort(unknownCompleted.begin(), unknownCompleted.end());
    state.completedIds.insert(state.completedIds.end(),
                              unknownCompleted.begin(),
                              unknownCompleted.end());
    std::vector<ObjectiveProgressState> unknownProgress = m_unknownProgress;
    std::sort(unknownProgress.begin(), unknownProgress.end(),
              [](const ObjectiveProgressState& left,
                 const ObjectiveProgressState& right)
              {
                  return left.id < right.id;
              });
    state.progress.insert(state.progress.end(), unknownProgress.begin(),
                          unknownProgress.end());
    return state;
}

std::uint32_t ObjectiveSystem::legacyAlphaFlags() const noexcept
{
    std::uint32_t flags = 0;
    for (std::size_t index = 0;
         index < ObjectiveState::LegacyAlphaIds.size(); ++index)
    {
        if (m_completed.find(ObjectiveState::LegacyAlphaIds[index]) !=
            m_completed.end())
        {
            flags |= 1u << static_cast<unsigned>(index);
        }
    }
    return flags;
}

bool ObjectiveSystem::isCompleted(const std::string& id) const noexcept
{
    return m_completed.find(id) != m_completed.end();
}

int ObjectiveSystem::progress(const std::string& id) const noexcept
{
    const auto found = m_progress.find(id);
    return found == m_progress.end() ? 0 : found->second;
}

RecipeDiscoverySnapshot ObjectiveSystem::recipeDiscoverySnapshot() const
{
    RecipeDiscoverySnapshot result;
    if (!m_recipeDiscoveryEnabled)
    {
        return result;
    }
    const RecipeRegistry& recipes = runtimeRecipeRegistry();
    result.totalRecipes = recipes.recipes().size();
    for (const RecipeDefinition& recipe : recipes.recipes())
    {
        if (isRecipeDiscovered(recipe.id))
        {
            result.discoveredIds.push_back(recipe.id);
        }
    }
    return result;
}

bool ObjectiveSystem::isRecipeDiscovered(
    const std::string& recipeId) const noexcept
{
    return m_discoveredRecipeIds.find(recipeId) !=
           m_discoveredRecipeIds.end();
}

std::string ObjectiveSystem::recipeDiscoveryToken(
    const std::string& recipeId)
{
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char character : recipeId)
    {
        hash ^= static_cast<std::uint64_t>(character);
        hash *= UINT64_C(1099511628211);
    }
    std::ostringstream token;
    token << "recipe." << std::hex << std::setfill('0')
          << std::setw(16) << hash;
    return token.str();
}

bool ObjectiveSystem::prerequisiteSatisfied(
    const ObjectiveDefinition& definition) const
{
    return definition.prerequisite.empty() ||
           isCompleted(definition.prerequisite);
}

void ObjectiveSystem::consumeEvent(const SandboxEvent& event)
{
    observeRecipeDiscovery(event);
    for (const ObjectiveDefinition& definition : m_registry->definitions())
    {
        if (isCompleted(definition.id) ||
            !prerequisiteSatisfied(definition))
        {
            continue;
        }

        switch (event.type)
        {
        case SandboxEventType::CraftCompleted:
            if (definition.type == ObjectiveType::CraftItem)
            {
                const auto& craft =
                    static_cast<const CraftCompletedEvent&>(event);
                if (craft.outputMaterialId == definition.targetMaterial &&
                    craft.craftsCompleted > 0)
                {
                    addProgress(definition, craft.craftsCompleted);
                }
            }
            break;
        case SandboxEventType::BlockPlace:
            if (definition.type == ObjectiveType::PlaceBlock)
            {
                const auto& block = static_cast<const BlockPlaceEvent&>(event);
                if (blockMatches(definition.targetMaterial, block.blockId))
                {
                    addProgress(definition, 1);
                }
            }
            break;
        case SandboxEventType::BlockBreak:
            if (definition.type == ObjectiveType::BreakBlock)
            {
                const auto& block = static_cast<const BlockBreakEvent&>(event);
                if (blockMatches(definition.targetMaterial, block.blockId))
                {
                    addProgress(definition, 1);
                }
            }
            break;
        case SandboxEventType::EntityDeath:
            if (definition.type == ObjectiveType::DefeatEnemy)
            {
                const auto& death = static_cast<const EntityDeathEvent&>(event);
                if (death.id != DefaultPlayerActorId &&
                    death.killerId == DefaultPlayerActorId &&
                    (definition.targetActorType.empty() ||
                     definition.targetActorType == death.type))
                {
                    addProgress(definition, 1);
                }
            }
            break;
        case SandboxEventType::WaystoneActivated:
            if (definition.type == ObjectiveType::ActivateWaystone)
            {
                const auto& activated =
                    static_cast<const WaystoneActivatedEvent&>(event);
                if (activated.playerId == DefaultPlayerActorId)
                {
                    addProgress(definition, 1);
                }
            }
            break;
        case SandboxEventType::VictoryRewardClaimed:
            if (definition.type == ObjectiveType::ClaimVictoryReward)
            {
                const auto& claimed =
                    static_cast<const VictoryRewardClaimedEvent&>(event);
                if (claimed.playerId == DefaultPlayerActorId &&
                    claimed.rewardEpoch > 0)
                {
                    addProgress(definition, 1);
                }
            }
            break;
        case SandboxEventType::ItemPickup:
            if (definition.type == ObjectiveType::PickupItem)
            {
                const auto& pickup = static_cast<const ItemPickupEvent&>(event);
                if (pickup.pickerId == DefaultPlayerActorId &&
                    pickup.materialId == definition.targetMaterial &&
                    pickup.amount > 0)
                {
                    addProgress(definition, pickup.amount);
                }
            }
            break;
        case SandboxEventType::SmeltCompleted:
            if (definition.type == ObjectiveType::SmeltItem)
            {
                const auto& smelt =
                    static_cast<const SmeltCompletedEvent&>(event);
                if (smelt.outputMaterialId == definition.targetMaterial &&
                    smelt.amount > 0)
                {
                    addProgress(definition, smelt.amount);
                }
            }
            break;
        case SandboxEventType::FoodConsumed:
            if (definition.type == ObjectiveType::ConsumeItem)
            {
                const auto& consumed =
                    static_cast<const FoodConsumedEvent&>(event);
                if (consumed.playerId == DefaultPlayerActorId &&
                    consumed.materialId == definition.targetMaterial &&
                    consumed.healthRestored > 0.f)
                {
                    addProgress(definition, 1);
                }
            }
            break;
        default:
            break;
        }
    }
    refreshStateObjectives();
}

void ObjectiveSystem::refreshStateObjectives()
{
    bool changed = false;
    do
    {
        changed = false;
        for (const ObjectiveDefinition& definition :
             m_registry->definitions())
        {
            if (isCompleted(definition.id) ||
                !prerequisiteSatisfied(definition))
            {
                continue;
            }
            if (definition.type == ObjectiveType::ObtainItem)
            {
                const int count = inventoryCount(definition.targetMaterial);
                m_progress[definition.id] =
                    std::min(count, definition.required);
                if (count >= definition.required)
                {
                    complete(definition);
                    changed = true;
                }
            }
            else if (definition.type == ObjectiveType::ReachLocation &&
                     m_player != nullptr)
            {
                const glm::vec3 delta = m_player->position -
                                        definition.location;
                if (glm::dot(delta, delta) <=
                    definition.radius * definition.radius)
                {
                    complete(definition);
                    changed = true;
                }
            }
            else if (definition.type == ObjectiveType::ReopenWorld &&
                     m_reopenEligible.find(definition.id) !=
                         m_reopenEligible.end())
            {
                complete(definition);
                changed = true;
            }
        }
    } while (changed);
}

void ObjectiveSystem::addProgress(const ObjectiveDefinition& definition,
                                  int amount)
{
    if (amount <= 0 || isCompleted(definition.id))
    {
        return;
    }
    int& value = m_progress[definition.id];
    value = std::min(definition.required,
                     value > definition.required - amount
                         ? definition.required
                         : value + amount);
    if (value >= definition.required)
    {
        complete(definition);
    }
}

void ObjectiveSystem::complete(const ObjectiveDefinition& definition)
{
    if (!m_completed.emplace(definition.id).second)
    {
        return;
    }
    m_progress.erase(definition.id);
    if (definition.visible)
    {
        m_completionFeedback = definition.feedback;
        m_completionFeedbackId = definition.id;
        m_feedbackSeconds = FeedbackDurationSeconds;
    }
}

void ObjectiveSystem::observeRecipeDiscovery(const SandboxEvent& event)
{
    if (!m_recipeDiscoveryEnabled)
    {
        return;
    }
    if (event.type == SandboxEventType::PlayerInventoryChanged)
    {
        const auto& changed =
            static_cast<const PlayerInventoryChangedEvent&>(event);
        if (changed.playerId == DefaultPlayerActorId &&
            changed.amountDelta > 0)
        {
            unlockRecipesForMaterial(changed.materialId, true);
        }
    }
    else if (event.type == SandboxEventType::CraftCompleted)
    {
        const auto& craft = static_cast<const CraftCompletedEvent&>(event);
        if (craft.craftsCompleted > 0 && craft.outputAdded > 0)
        {
            unlockRecipe(craft.recipeId, true);
            unlockRecipesForMaterial(craft.outputMaterialId, true);
        }
    }
}

void ObjectiveSystem::unlockRecipesForMaterial(Material::ID materialId,
                                               bool announce)
{
    if (!m_recipeDiscoveryEnabled || materialId == Material::ID::Nothing)
    {
        return;
    }
    for (const RecipeDefinition& recipe : runtimeRecipeRegistry().recipes())
    {
        bool related = recipe.outputMaterialId == materialId;
        for (const RecipeIngredient& ingredient : recipe.ingredients)
        {
            related = related || ingredient.materialId == materialId;
        }
        if (related)
        {
            unlockRecipe(recipe.id, announce);
        }
    }
}

void ObjectiveSystem::unlockRecipe(const std::string& recipeId,
                                   bool announce)
{
    (void)announce;
    if (!recipeId.empty() &&
        runtimeRecipeRegistry().find(recipeId) != nullptr)
    {
        m_discoveredRecipeIds.emplace(recipeId);
    }
}

ObjectiveOpportunitySnapshot ObjectiveSystem::makeOpportunity(
    const ObjectiveDefinition& definition) const
{
    ObjectiveOpportunitySnapshot result;
    result.id = definition.id;
    result.track = objectiveTrack(definition.id);
    result.title = definition.title;
    result.instruction = definition.instruction;
    result.progress = definition.type == ObjectiveType::ObtainItem
                          ? inventoryCount(definition.targetMaterial)
                          : progress(definition.id);
    result.required = definition.required;
    return result;
}

int ObjectiveSystem::inventoryCount(Material::ID materialId) const noexcept
{
    if (m_player == nullptr || materialId == Material::ID::Nothing)
    {
        return 0;
    }
    int total = 0;
    for (int slot = 0; slot < m_player->getInventorySlotCount(); ++slot)
    {
        const ItemStack& stack = m_player->getInventorySlot(slot);
        if (stack.getMaterial().id == materialId)
        {
            total += stack.getNumInStack();
        }
    }
    return total;
}

const ObjectiveDefinition* ObjectiveSystem::currentDefinition() const noexcept
{
    for (const ObjectiveDefinition& definition : m_registry->definitions())
    {
        if (definition.visible && !definition.optional &&
            !isCompleted(definition.id) && prerequisiteSatisfied(definition))
        {
            return &definition;
        }
    }
    return nullptr;
}
