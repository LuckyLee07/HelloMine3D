#include "PlayerController.h"

#include "Player.h"

namespace
{
    constexpr float MovementSpeed = 0.2f;
}

void PlayerController::applyInput(Player &player,
                                  const PlayerInputState &input)
{
    if (input.toggleFlying) {
        player.m_isFlying = !player.m_isFlying;
    }
    if (input.toggleSneaking) {
        player.m_isSneak = !player.m_isSneak;
    }

    float forwardSpeed = MovementSpeed;
    if (input.sprint) {
        forwardSpeed *= 5.f;
    }
    else if (input.descend) {
        forwardSpeed *= 0.35f;
    }

    if (input.moveForward) {
        player.m_acceleration.x +=
            -glm::cos(glm::radians(player.rotation.y + 90)) * forwardSpeed;
        player.m_acceleration.z +=
            -glm::sin(glm::radians(player.rotation.y + 90)) * forwardSpeed;
    }
    if (input.moveBackward) {
        player.m_acceleration.x +=
            glm::cos(glm::radians(player.rotation.y + 90)) * MovementSpeed;
        player.m_acceleration.z +=
            glm::sin(glm::radians(player.rotation.y + 90)) * MovementSpeed;
    }
    if (input.moveLeft) {
        player.m_acceleration.x +=
            -glm::cos(glm::radians(player.rotation.y)) * MovementSpeed;
        player.m_acceleration.z +=
            -glm::sin(glm::radians(player.rotation.y)) * MovementSpeed;
    }
    if (input.moveRight) {
        player.m_acceleration.x +=
            glm::cos(glm::radians(player.rotation.y)) * MovementSpeed;
        player.m_acceleration.z +=
            glm::sin(glm::radians(player.rotation.y)) * MovementSpeed;
    }

    if (input.jump) {
        player.jump();
    }
    else if (input.descend && player.m_isFlying) {
        player.m_acceleration.y -= MovementSpeed * 3;
    }

    if (input.hotbarDelta > 0) {
        player.m_inventory.selectNext();
    }
    else if (input.hotbarDelta < 0) {
        player.m_inventory.selectPrevious();
    }
    if (input.hotbarSlot >= 0) {
        player.m_inventory.setSelectedSlot(input.hotbarSlot);
    }

    static const float Bound = 89.f;
    player.rotation.y += input.lookDelta.x * 0.05f;
    player.rotation.x += input.lookDelta.y * 0.05f;

    if (player.rotation.x > Bound) {
        player.rotation.x = Bound;
    }
    else if (player.rotation.x < -Bound) {
        player.rotation.x = -Bound;
    }

    if (player.rotation.y > 360) {
        player.rotation.y = 0;
    }
    else if (player.rotation.y < 0) {
        player.rotation.y = 360;
    }
}
