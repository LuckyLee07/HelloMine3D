#include "PlayerController.h"

#include <algorithm>
#include <cmath>

#include "Player.h"

namespace
{
    constexpr float WalkSpeed = 4.5f;
    constexpr float SprintSpeed = 7.f;
    constexpr float SneakSpeed = 1.8f;
    constexpr float FlySpeed = 7.f;
    constexpr float FlySprintSpeed = 10.f;
    constexpr float GroundAcceleration = 42.f;
    constexpr float GroundBraking = 52.f;
    constexpr float AirAcceleration = 14.f;
    constexpr float AirBraking = 3.f;
    constexpr float FlyAcceleration = 32.f;
    constexpr float FlyBraking = 40.f;
    constexpr float JumpSpeed = 10.f;
    constexpr float JumpBufferSeconds = 0.12f;
    constexpr float CoyoteSeconds = 0.10f;

    glm::vec2 moveTowards(const glm::vec2 &current,
                          const glm::vec2 &target, float maximumDelta)
    {
        const glm::vec2 offset = target - current;
        const float distance = glm::length(offset);
        if (distance <= maximumDelta || distance <= 0.000001f) {
            return target;
        }
        return current + offset * (maximumDelta / distance);
    }

    float moveTowards(float current, float target, float maximumDelta)
    {
        if (current < target) {
            return std::min(current + maximumDelta, target);
        }
        return std::max(current - maximumDelta, target);
    }
}

void PlayerController::applyInput(Player &player,
                                  const PlayerInputState &input)
{
    if (input.toggleFlying) {
        player.m_isFlying = !player.m_isFlying;
        player.velocity.y = 0.f;
        player.m_jumpBufferSeconds = 0.f;
    }

    const bool jumpPressed = input.jump && !player.m_input.jump;
    player.m_input = input;
    player.m_input.toggleFlying = false;
    player.m_input.hotbarDelta = 0;
    player.m_input.hotbarSlot = -1;
    player.m_input.lookDelta = glm::vec2(0.f);
    if (jumpPressed && !player.m_isFlying) {
        player.m_jumpBufferSeconds = JumpBufferSeconds;
    }
    player.m_isSneak = input.descend && !player.m_isFlying;

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
    player.rotation.y += input.lookDelta.x;
    player.rotation.x += input.lookDelta.y;

    if (player.rotation.x > Bound) {
        player.rotation.x = Bound;
    }
    else if (player.rotation.x < -Bound) {
        player.rotation.x = -Bound;
    }

    player.rotation.y = std::fmod(player.rotation.y, 360.f);
    if (player.rotation.y < 0.f) {
        player.rotation.y += 360.f;
    }
}

void PlayerController::applyMovement(Player &player, float dt)
{
    dt = std::max(0.f, dt);
    const PlayerInputState &input = player.m_input;

    const float forwardAxis =
        (input.moveForward ? 1.f : 0.f) -
        (input.moveBackward ? 1.f : 0.f);
    const float rightAxis =
        (input.moveRight ? 1.f : 0.f) -
        (input.moveLeft ? 1.f : 0.f);
    glm::vec2 localDirection(rightAxis, forwardAxis);
    const float localLength = glm::length(localDirection);
    if (localLength > 1.f) {
        localDirection /= localLength;
    }

    const float yaw = glm::radians(player.rotation.y);
    const glm::vec2 rightDirection(std::cos(yaw), std::sin(yaw));
    const glm::vec2 forwardDirection(std::sin(yaw), -std::cos(yaw));
    glm::vec2 worldDirection =
        rightDirection * localDirection.x +
        forwardDirection * localDirection.y;

    float speed = WalkSpeed;
    if (player.m_isFlying) {
        speed = input.sprint ? FlySprintSpeed : FlySpeed;
    }
    else if (input.descend) {
        speed = SneakSpeed;
    }
    else if (input.sprint) {
        speed = SprintSpeed;
    }
    const glm::vec2 desiredVelocity = worldDirection * speed;
    const glm::vec2 currentVelocity(player.velocity.x, player.velocity.z);
    const bool hasMovement = glm::length(worldDirection) > 0.000001f;
    float response = 0.f;
    if (player.m_isFlying) {
        response = hasMovement ? FlyAcceleration : FlyBraking;
    }
    else if (player.m_isOnGround) {
        response = hasMovement ? GroundAcceleration : GroundBraking;
    }
    else {
        response = hasMovement ? AirAcceleration : AirBraking;
    }
    const glm::vec2 horizontalVelocity = moveTowards(
        currentVelocity, desiredVelocity, response * dt);
    player.velocity.x = horizontalVelocity.x;
    player.velocity.z = horizontalVelocity.y;

    if (player.m_isFlying) {
        player.m_isSneak = false;
        player.m_coyoteSeconds = 0.f;
        player.m_jumpBufferSeconds = 0.f;
        const float verticalAxis =
            (input.jump ? 1.f : 0.f) -
            (input.descend ? 1.f : 0.f);
        const float desiredVerticalVelocity = verticalAxis * speed;
        player.velocity.y = moveTowards(
            player.velocity.y, desiredVerticalVelocity,
            (verticalAxis == 0.f ? FlyBraking : FlyAcceleration) * dt);
        return;
    }

    player.m_isSneak = input.descend;
    if (player.m_isOnGround) {
        player.m_coyoteSeconds = CoyoteSeconds;
    }
    else {
        player.m_coyoteSeconds = std::max(
            0.f, player.m_coyoteSeconds - dt);
    }

    if (player.m_jumpBufferSeconds > 0.f &&
        player.m_coyoteSeconds > 0.f) {
        player.velocity.y = JumpSpeed;
        player.m_isOnGround = false;
        player.m_coyoteSeconds = 0.f;
        player.m_jumpBufferSeconds = 0.f;
    }
    else {
        player.m_jumpBufferSeconds = std::max(
            0.f, player.m_jumpBufferSeconds - dt);
    }
}
