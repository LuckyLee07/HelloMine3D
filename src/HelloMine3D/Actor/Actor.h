#ifndef ACTOR_H_INCLUDED
#define ACTOR_H_INCLUDED

#include <string>

#include "../Entity/Entity.h"
#include "ActorTypes.h"

class World;
class SandboxEventBus;

struct ActorSaveState {
    ActorId id = InvalidActorId;
    std::string type;
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 velocity{0.f};
    bool alive = true;
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

    ActorSaveState getSaveState() const;
    ActorSnapshot getSnapshot() const;
    void applySaveState(const ActorSaveState &state);

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
