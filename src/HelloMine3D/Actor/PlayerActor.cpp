#include "PlayerActor.h"

#include "../Player/Player.h"

PlayerActor::PlayerActor(ActorId id)
    : LivingActor(id, "player", glm::vec3(0.f), glm::vec3(0.3f, 1.f, 0.3f),
                  20.f)
{
}

void PlayerActor::syncFromPlayer(const Player &player)
{
    position = player.position;
    rotation = player.rotation;
    velocity = player.velocity;
    box.update(position);
}

void PlayerActor::syncToPlayer(Player &player) const
{
    player.position = position;
    player.rotation = rotation;
    player.velocity = velocity;
    player.box.update(player.position);
}
