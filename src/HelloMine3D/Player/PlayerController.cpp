#include "PlayerController.h"

#include "../Input/Keyboard.h"
#include "Player.h"

namespace
{
    constexpr float MovementSpeed = 0.2f;
}

PlayerController::PlayerController()
    : m_itemDown(sf::Keyboard::Key::Down)
    , m_itemUp(sf::Keyboard::Key::Up)
    , m_flyKey(sf::Keyboard::Key::F)
    , m_num1(sf::Keyboard::Key::Num1)
    , m_num2(sf::Keyboard::Key::Num2)
    , m_num3(sf::Keyboard::Key::Num3)
    , m_num4(sf::Keyboard::Key::Num4)
    , m_num5(sf::Keyboard::Key::Num5)
    , m_slow(sf::Keyboard::Key::LShift)
    , m_useMouseKey(sf::Keyboard::Key::L)
{
}

void PlayerController::handleInput(Player &player, const sf::Window &window,
                                   const Keyboard &keyboard)
{
    applyInput(player, collectInput(window, keyboard));
}

PlayerInputState PlayerController::collectInput(const sf::Window &window,
                                                const Keyboard &keyboard)
{
    PlayerInputState input;
    input.moveForward = keyboard.isKeyDown(sf::Keyboard::Key::W);
    input.moveBackward = keyboard.isKeyDown(sf::Keyboard::Key::S);
    input.moveLeft = keyboard.isKeyDown(sf::Keyboard::Key::A);
    input.moveRight = keyboard.isKeyDown(sf::Keyboard::Key::D);
    input.sprint = keyboard.isKeyDown(sf::Keyboard::Key::LControl);
    input.jump = keyboard.isKeyDown(sf::Keyboard::Key::Space);
    input.descend = keyboard.isKeyDown(sf::Keyboard::Key::LShift) ||
                    keyboard.isKeyDown(sf::Keyboard::Key::RShift);
    input.lookDelta = collectMouseDelta(window);

    if (m_itemDown.isKeyPressed()) {
        input.hotbarDelta = 1;
    }
    else if (m_itemUp.isKeyPressed()) {
        input.hotbarDelta = -1;
    }

    if (m_flyKey.isKeyPressed()) {
        input.toggleFlying = true;
    }

    if (m_num1.isKeyPressed()) {
        input.hotbarSlot = 0;
    }
    if (m_num2.isKeyPressed()) {
        input.hotbarSlot = 1;
    }
    if (m_num3.isKeyPressed()) {
        input.hotbarSlot = 2;
    }
    if (m_num4.isKeyPressed()) {
        input.hotbarSlot = 3;
    }
    if (m_num5.isKeyPressed()) {
        input.hotbarSlot = 4;
    }
    if (m_slow.isKeyPressed()) {
        input.toggleSneaking = true;
    }

    return input;
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

glm::vec2 PlayerController::collectMouseDelta(const sf::Window &window)
{
    if (m_useMouseKey.isKeyPressed()) {
        m_useMouse = !m_useMouse;
    }

    if (!m_useMouse) {
        m_hasLastMousePosition = false;
        return {0.f, 0.f};
    }

    if (!m_hasLastMousePosition) {
        m_lastMousePosition = sf::Mouse::getPosition(window);
        m_hasLastMousePosition = true;
    }

    const auto currentMousePosition = sf::Mouse::getPosition(window);
    const auto change = currentMousePosition - m_lastMousePosition;

    const auto cx = static_cast<int>(window.getSize().x / 2);
    const auto cy = static_cast<int>(window.getSize().y / 2);

    sf::Mouse::setPosition({cx, cy}, window);
    m_lastMousePosition = sf::Mouse::getPosition(window);
    return {static_cast<float>(change.x), static_cast<float>(change.y)};
}
