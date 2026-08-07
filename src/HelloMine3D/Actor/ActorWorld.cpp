#include "Actor.h"

#include "../World/World.h"

void Actor::enterWorld(World &world)
{
    enterWorld(world.getEventBus());
}
