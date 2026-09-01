#include "PlayerBlockInteractionCommand.h"

#include "../../Player/Player.h"
#include "../Interaction/BlockInteractionSystem.h"
#include "../World.h"

PlayerBlockInteractionCommand::PlayerBlockInteractionCommand(
    PlayerBlockInteractionAction action, const glm::vec3 &location,
    Player &player)
    : m_action(action)
    , m_blockPosition(location)
    , m_player(&player)
{
}

void PlayerBlockInteractionCommand::execute(World &world)
{
    const VectorXZ chunkLocation = World::getChunkXZ(
        World::toBlockCoord(m_blockPosition.x),
        World::toBlockCoord(m_blockPosition.z));

    if (world.getChunkManager().chunkLoadedAt(chunkLocation.x,
                                              chunkLocation.z)) {
        interact(world);
    }
}

void PlayerBlockInteractionCommand::interact(World &world)
{
    if (m_player == nullptr) {
        return;
    }

    switch (m_action) {
        case PlayerBlockInteractionAction::Break:
            BlockInteractionSystem::breakBlock(
                world, *m_player, m_blockPosition);
            break;

        case PlayerBlockInteractionAction::Place:
            BlockInteractionSystem::placeBlock(
                world, *m_player, m_blockPosition);
            break;

        case PlayerBlockInteractionAction::Use:
            BlockInteractionSystem::useBlock(
                world, *m_player, m_blockPosition);
            break;
    }
}
