#include "../Actor/ActorManager.h"
#include "../Actor/MobActor.h"
#include "../Sandbox/Events/EntityEvents.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    struct ObservedEvents {
        int spawns = 0;
        int damage = 0;
        int deaths = 0;
        ActorId lastSpawnId = InvalidActorId;
        ActorId lastDamageId = InvalidActorId;
        ActorId lastDeathId = InvalidActorId;
        std::string lastSpawnType;
        float lastHealthAfter = 0.f;
    };

    bool expectTrue(const char *name, bool value)
    {
        if (value) {
            return true;
        }

        std::cerr << name << " failed\n";
        return false;
    }

    template <typename T>
    bool expectEqual(const char *name, const T &actual, const T &expected)
    {
        if (actual == expected) {
            return true;
        }

        std::cerr << name << " failed: expected " << expected << ", got "
                  << actual << "\n";
        return false;
    }
}

int main()
{
    SandboxEventBus eventBus;
    ObservedEvents events;

    eventBus.subscribe(SandboxEventType::EntitySpawn,
                       [&](const SandboxEvent &event) {
                           const auto &spawn =
                               static_cast<const EntitySpawnEvent &>(event);
                           ++events.spawns;
                           events.lastSpawnId = spawn.id;
                           events.lastSpawnType = spawn.type;
                       });
    eventBus.subscribe(SandboxEventType::EntityDamage,
                       [&](const SandboxEvent &event) {
                           const auto &damage =
                               static_cast<const EntityDamageEvent &>(event);
                           ++events.damage;
                           events.lastDamageId = damage.id;
                           events.lastHealthAfter = damage.healthAfter;
                       });
    eventBus.subscribe(SandboxEventType::EntityDeath,
                       [&](const SandboxEvent &event) {
                           const auto &death =
                               static_cast<const EntityDeathEvent &>(event);
                           ++events.deaths;
                           events.lastDeathId = death.id;
                       });

    ActorManager manager;
    const ActorId mobId = manager.allocateActorId();
    manager.addActor(std::make_unique<MobActor>(mobId, "test:mob",
                                                glm::vec3(2.f, 4.f, 6.f)),
                     eventBus);

    auto *mob = dynamic_cast<MobActor *>(manager.findActor(mobId));

    bool ok = true;
    ok &= expectTrue("mob exists", mob != nullptr);
    ok &= expectEqual("spawn count", events.spawns, 1);
    ok &= expectEqual("actor count after spawn", manager.getActorCount(),
                      static_cast<std::size_t>(1));
    ok &= expectEqual("spawn id", events.lastSpawnId, mobId);
    ok &= expectEqual("spawn type", events.lastSpawnType, std::string("test:mob"));

    if (mob == nullptr) {
        return EXIT_FAILURE;
    }

    mob->setWanderSpeed(1.f);
    const glm::vec3 positionBeforeWander = mob->position;
    mob->stepWander(1.f);
    ok &= expectTrue("mob moved during wander",
                     mob->position.x != positionBeforeWander.x ||
                         mob->position.z != positionBeforeWander.z);

    mob->setDrop(Material::ID::Dirt, 2);
    ok &= expectEqual("drop material", mob->getDropMaterialId(),
                      Material::ID::Dirt);
    ok &= expectEqual("drop amount", mob->getDropAmount(), 2);

    ok &= expectTrue("first damage accepted",
                     mob->damage(eventBus, 4.f, DefaultPlayerActorId));
    ok &= expectEqual("damage count after first hit", events.damage, 1);
    ok &= expectEqual("death count before lethal hit", events.deaths, 0);
    ok &= expectEqual("health after first hit", events.lastHealthAfter, 6.f);
    ok &= expectTrue("mob alive after first hit", mob->isAlive());

    ok &= expectTrue("repeat damage rejected during immunity",
                     !mob->damage(eventBus, 6.f, DefaultPlayerActorId));
    ok &= expectEqual("damage count after rejected hit", events.damage, 1);
    ok &= expectEqual("health after rejected hit", mob->getHealth(), 6.f);

    mob->die(eventBus, DefaultPlayerActorId);
    ok &= expectEqual("death count after explicit death", events.deaths, 1);
    ok &= expectEqual("last damage id", events.lastDamageId, mobId);
    ok &= expectEqual("last death id", events.lastDeathId, mobId);
    ok &= expectTrue("mob dead after death event", !mob->isAlive());

    manager.removeDeadActors();
    ok &= expectEqual("actor count after cleanup", manager.getActorCount(),
                      static_cast<std::size_t>(0));

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "Entity lifecycle smoke passed.\n";
    return EXIT_SUCCESS;
}
