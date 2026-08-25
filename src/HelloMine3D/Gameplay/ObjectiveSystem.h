#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ObjectiveRegistry.h"
#include "ObjectiveState.h"

class Player;
struct SandboxEvent;
class SandboxEventBus;

struct ObjectiveSnapshot
{
    int definitionVersion = 0;
    std::string currentId;
    std::string nextId;
    std::string title;
    std::string instruction;
    std::string nextTitle;
    std::string completionFeedback;
    std::string completionFeedbackId;
    std::vector<std::string> completedIds;
    std::vector<std::string> completedTitles;
    int progress = 0;
    int required = 0;
    std::size_t completedObjectives = 0;
    std::size_t totalObjectives = 0;
    bool sessionComplete = false;
};

class ObjectiveSystem
{
  public:
    ObjectiveSystem(const ObjectiveRegistry& registry, Player& player,
                    SandboxEventBus& eventBus,
                    const ObjectiveSaveState& savedState,
                    std::uint32_t legacyAlphaFlags,
                    bool restoredWorld);
    ~ObjectiveSystem();

    ObjectiveSystem(const ObjectiveSystem&) = delete;
    ObjectiveSystem& operator=(const ObjectiveSystem&) = delete;

    void update(float deltaSeconds);
    ObjectiveSnapshot snapshot() const;
    ObjectiveSaveState saveState() const;
    std::uint32_t legacyAlphaFlags() const noexcept;
    bool isCompleted(const std::string& id) const noexcept;
    int progress(const std::string& id) const noexcept;

  private:
    bool prerequisiteSatisfied(const ObjectiveDefinition& definition) const;
    void consumeEvent(const SandboxEvent& event);
    void refreshStateObjectives();
    void addProgress(const ObjectiveDefinition& definition, int amount);
    void complete(const ObjectiveDefinition& definition);
    int inventoryCount(Material::ID materialId) const noexcept;
    const ObjectiveDefinition* currentDefinition() const noexcept;

    const ObjectiveRegistry* m_registry = nullptr;
    Player* m_player = nullptr;
    SandboxEventBus* m_eventBus = nullptr;
    std::vector<unsigned> m_subscriptions;
    std::unordered_set<std::string> m_completed;
    std::unordered_set<std::string> m_reopenEligible;
    std::unordered_map<std::string, int> m_progress;
    std::vector<std::string> m_unknownCompleted;
    std::vector<ObjectiveProgressState> m_unknownProgress;
    std::string m_completionFeedback;
    std::string m_completionFeedbackId;
    float m_feedbackSeconds = 0.f;
};
