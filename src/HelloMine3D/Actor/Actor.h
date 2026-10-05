#ifndef ACTOR_H_INCLUDED
#define ACTOR_H_INCLUDED

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "../Entity/Entity.h"
#include "ActorTypes.h"
#include "CombatTypes.h"

class World;
class SandboxEventBus;

enum class ActorSaveKind {
    Generic = 0,
    Mob = 1,
    Item = 2
};

struct ActorSaveState {
    ActorSaveKind kind = ActorSaveKind::Generic;
    ActorId id = InvalidActorId;
    std::string type;
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 velocity{0.f};
    bool alive = true;
    float health = 0.f;
    int materialId = 0;
    int amount = 0;
    float pickupDelay = 0.f;
    float wanderTime = 0.f;
    float wanderSpeed = 0.f;
    int dropMaterialId = 0;
    int dropAmount = 0;
};

// Copied transient facts about successful wildlife movement. These are not
// save data: the renderer follows the actual checked path without world queries.
enum class WildlifeMotionPath {
    None = 0,
    GroundedLevel,
    SupportRise,
    SupportDescent,
    AirborneFall
};

struct WildlifeMotionSegment {
    std::uint64_t sequence = 0;
    glm::vec3 from{0.f};
    glm::vec3 to{0.f};
    float seconds = 0.f;
    WildlifeMotionPath kind = WildlifeMotionPath::None;
};

struct WildlifeMotionHistory {
    static constexpr std::size_t MaximumSegments = 8;
    // Oldest first. Unused entries are neutral and never consumed.
    std::array<WildlifeMotionSegment, MaximumSegments> segments{};
    std::size_t count = 0;
    std::uint64_t newestSequence = 0;
};
static_assert(sizeof(WildlifeMotionHistory) <= 336,
              "Copied animal movement history remains bounded");

struct ActorSnapshot {
    ActorId id = InvalidActorId;
    std::string type;
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 dimensions{0.f};
    // Transient wildlife pose facts. Existing actors publish the neutral values.
    int wildlifeActivity = 0;
    float wildlifeMotionSeconds = 0.f;
    WildlifeMotionHistory wildlifeMotionHistory;
    // Copied item presentation facts; inventory and lifetime remain on ItemEntity.
    int itemMaterialId = 0;
    int itemAmount = 0;
    float itemAgeSeconds = 0.f;
    bool combatant = false;
    EnemyCombatMode combatMode = EnemyCombatMode::Melee;
    MobCombatState combatState = MobCombatState::Idle;
    ActorId combatTargetId = InvalidActorId;
    int combatStateTicksRemaining = 0;
    int combatStateTicksTotal = 0;
    MobCombatTransitionReason combatTransitionReason =
        MobCombatTransitionReason::Spawned;
    float hitFeedback = 0.f;
    bool deathPresentation = false;
    int deathPresentationTicksRemaining = 0;
    int deathPresentationTicksTotal = 0;
};

class Actor : public Entity {
  public:
    Actor(ActorId id, std::string type, const glm::vec3 &position,
          const glm::vec3 &boxDimensions);
    virtual ~Actor() = default;

    void enterWorld(World &world);
    virtual void enterWorld(SandboxEventBus &eventBus);
    virtual void tick(World &world, float dt);

    virtual ActorSaveState getSaveState() const;
    virtual ActorSnapshot getSnapshot() const;
    virtual void applySaveState(const ActorSaveState &state);

    ActorId getId() const;
    const std::string &getType() const;
    bool isAlive() const;
    void kill();
    void setAlive(bool alive);

  private:
    ActorId m_id = InvalidActorId;
    std::string m_type;
    bool m_alive = true;
};

#endif // ACTOR_H_INCLUDED
