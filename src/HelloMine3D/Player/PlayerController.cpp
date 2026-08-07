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
    keyboardInput(player, keyboard);
    mouseInput(player, window);

    if (m_itemDown.isKeyPressed()) {
        player.m_inventory.selectNext();
    }
    else if (m_itemUp.isKeyPressed()) {
        player.m_inventory.selectPrevious();
    }

    if (m_flyKey.isKeyPressed()) {
        player.m_isFlying = !player.m_isFlying;
    }

    if (m_num1.isKeyPressed()) {
        player.m_inventory.setSelectedSlot(0);
    }
    if (m_num2.isKeyPressed()) {
        player.m_inventory.setSelectedSlot(1);
    }
    if (m_num3.isKeyPressed()) {
        player.m_inventory.setSelectedSlot(2);
    }
    if (m_num4.isKeyPressed()) {
        player.m_inventory.setSelectedSlot(3);
    }
    if (m_num5.isKeyPressed()) {
        player.m_inventory.setSelectedSlot(4);
    }
    if (m_slow.isKeyPressed()) {
        player.m_isSneak = !player.m_isSneak;
    }
}

void PlayerController::keyboardInput(Player &player, const Keyboard &keyboard)
{
    if (keyboard.isKeyDown(sf::Keyboard::Key::W)) {
        float speed = MovementSpeed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)) {
            speed *= 5;
        }
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift) ||
                 sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)) {
            speed *= 0.35f;
        }

        player.m_acceleration.x +=
            -glm::cos(glm::radians(player.rotation.y + 90)) * speed;
        player.m_acceleration.z +=
            -glm::sin(glm::radians(player.rotation.y + 90)) * speed;
    }
    if (keyboard.isKeyDown(sf::Keyboard::Key::S)) {
        player.m_acceleration.x +=
            glm::cos(glm::radians(player.rotation.y + 90)) * MovementSpeed;
        player.m_acceleration.z +=
            glm::sin(glm::radians(player.rotation.y + 90)) * MovementSpeed;
    }
    if (keyboard.isKeyDown(sf::Keyboard::Key::A)) {
        player.m_acceleration.x +=
            -glm::cos(glm::radians(player.rotation.y)) * MovementSpeed;
        player.m_acceleration.z +=
            -glm::sin(glm::radians(player.rotation.y)) * MovementSpeed;
    }
    if (keyboard.isKeyDown(sf::Keyboard::Key::D)) {
        player.m_acceleration.x +=
            glm::cos(glm::radians(player.rotation.y)) * MovementSpeed;
        player.m_acceleration.z +=
            glm::sin(glm::radians(player.rotation.y)) * MovementSpeed;
    }

    if (keyboard.isKeyDown(sf::Keyboard::Key::Space)) {
        player.jump();
    }
    else if (keyboard.isKeyDown(sf::Keyboard::Key::LShift) &&
             player.m_isFlying) {
        player.m_acceleration.y -= MovementSpeed * 3;
    }
}

void PlayerController::mouseInput(Player &player, const sf::Window &window)
{
    if (m_useMouseKey.isKeyPressed()) {
        m_useMouse = !m_useMouse;
    }

    if (!m_useMouse) {
        m_hasLastMousePosition = false;
        return;
    }

    if (!m_hasLastMousePosition) {
        m_lastMousePosition = sf::Mouse::getPosition(window);
        m_hasLastMousePosition = true;
    }

    static const float Bound = 89.f;
    const auto currentMousePosition = sf::Mouse::getPosition(window);
    const auto change = currentMousePosition - m_lastMousePosition;

    player.rotation.y += change.x * 0.05f;
    player.rotation.x += change.y * 0.05f;

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

    const auto cx = static_cast<int>(window.getSize().x / 2);
    const auto cy = static_cast<int>(window.getSize().y / 2);

    sf::Mouse::setPosition({cx, cy}, window);
    m_lastMousePosition = sf::Mouse::getPosition(window);
}
