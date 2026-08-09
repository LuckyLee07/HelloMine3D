#ifndef ACTOR_H_INCLUDED
#define ACTOR_H_INCLUDED

#include <string>

#include "../Entity/Entity.h"
#include "ActorTypes.h"

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

struct ActorSnapshot {
    ActorId id = InvalidActorId;
    std::string type;
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 dimensions{0.f};
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
    ActorSnapshot getSnapshot() const;
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
