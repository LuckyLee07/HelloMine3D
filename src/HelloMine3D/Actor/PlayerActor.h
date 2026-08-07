#ifndef PLAYERACTOR_H_INCLUDED
#define PLAYERACTOR_H_INCLUDED

#include "LivingActor.h"

class Player;

class PlayerActor : public LivingActor {
  public:
    explicit PlayerActor(ActorId id = DefaultPlayerActorId);

    void syncFromPlayer(const Player &player);
    void syncToPlayer(Player &player) const;
};

#endif // PLAYERACTOR_H_INCLUDED
