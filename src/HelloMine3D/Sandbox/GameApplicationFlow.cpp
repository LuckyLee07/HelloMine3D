#include "GameApplicationFlow.h"

const char *gameApplicationStateName(GameApplicationState state) noexcept
{
    switch (state) {
    case GameApplicationState::MainMenu:
        return "main-menu";
    case GameApplicationState::WorldList:
        return "world-list";
    case GameApplicationState::Loading:
        return "loading";
    case GameApplicationState::Playing:
        return "playing";
    case GameApplicationState::Paused:
        return "paused";
    }
    return "unknown";
}

GameApplicationState GameApplicationFlow::state() const noexcept
{
    return m_state;
}

const std::string &GameApplicationFlow::activeWorldId() const noexcept
{
    return m_activeWorldId;
}

bool GameApplicationFlow::showWorldList() noexcept
{
    if (m_state != GameApplicationState::MainMenu) {
        return false;
    }
    m_state = GameApplicationState::WorldList;
    return true;
}

bool GameApplicationFlow::beginLoading(const std::string &worldId) noexcept
{
    if ((m_state != GameApplicationState::MainMenu &&
         m_state != GameApplicationState::WorldList) ||
        worldId.empty()) {
        return false;
    }
    m_activeWorldId = worldId;
    m_state = GameApplicationState::Loading;
    return true;
}

bool GameApplicationFlow::completeLoading(bool succeeded) noexcept
{
    if (m_state != GameApplicationState::Loading) {
        return false;
    }
    if (succeeded) {
        m_state = GameApplicationState::Playing;
    }
    else {
        m_activeWorldId.clear();
        m_state = GameApplicationState::WorldList;
    }
    return true;
}

bool GameApplicationFlow::pause() noexcept
{
    if (m_state != GameApplicationState::Playing) {
        return false;
    }
    m_state = GameApplicationState::Paused;
    return true;
}

bool GameApplicationFlow::resume() noexcept
{
    if (m_state != GameApplicationState::Paused) {
        return false;
    }
    m_state = GameApplicationState::Playing;
    return true;
}

bool GameApplicationFlow::returnToMainMenu() noexcept
{
    if (m_state == GameApplicationState::Loading) {
        return false;
    }
    m_activeWorldId.clear();
    m_state = GameApplicationState::MainMenu;
    return true;
}
