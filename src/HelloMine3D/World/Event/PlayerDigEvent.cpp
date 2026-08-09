#include "PlayerDigEvent.h"

#include "../../Player/Player.h"
#include "../Interaction/BlockInteractionSystem.h"
#include "../World.h"

PlayerDigEvent::PlayerDigEvent(PlayerDigAction action,
                               const glm::vec3 &location, Player &player)
    : m_action(action)
    , m_digSpot(location)
    , m_pPlayer(&player)
{
}

void PlayerDigEvent::handle(World &world)
{
    auto chunkLocation = World::getChunkXZ(World::toBlockCoord(m_digSpot.x),
                                           World::toBlockCoord(m_digSpot.z));

    if (world.getChunkManager().chunkLoadedAt(chunkLocation.x,
                                              chunkLocation.z)) {
        dig(world);
    }
}

void PlayerDigEvent::dig(World &world)
{
    if (m_pPlayer == nullptr) {
        return;
    }

    switch (m_action) {
        case PlayerDigAction::Break:
            BlockInteractionSystem::breakBlock(world, *m_pPlayer, m_digSpot);
            break;

        case PlayerDigAction::Place:
            BlockInteractionSystem::placeBlock(world, *m_pPlayer, m_digSpot);
            break;
    }
}
