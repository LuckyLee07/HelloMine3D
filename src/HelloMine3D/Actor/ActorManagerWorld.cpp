#include "ActorManager.h"

#include "../World/World.h"

ActorId ActorManager::addActor(std::unique_ptr<Actor> actor, World &world)
{
    return addActor(std::move(actor), world.getEventBus());
}
