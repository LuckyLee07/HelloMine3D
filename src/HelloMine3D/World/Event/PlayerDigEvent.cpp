#include "PlayerDigEvent.h"

#include "../../Player/Player.h"
#include "../Interaction/BlockInteractionSystem.h"
#include "../World.h"

PlayerDigEvent::PlayerDigEvent(sf::Mouse::Button button,
                               const glm::vec3 &location, Player &player)
    : m_buttonPress(button)
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

    switch (m_buttonPress) {
        case sf::Mouse::Button::Left:
            BlockInteractionSystem::breakBlock(world, *m_pPlayer, m_digSpot);
            break;

        case sf::Mouse::Button::Right:
            BlockInteractionSystem::placeBlock(world, *m_pPlayer, m_digSpot);
            break;

        default:
            break;
    }
}
