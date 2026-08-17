#ifndef GAMEAPPLICATIONFLOW_H_INCLUDED
#define GAMEAPPLICATIONFLOW_H_INCLUDED

#include <string>

enum class GameApplicationState {
    MainMenu,
    WorldList,
    Loading,
    Playing,
    Paused
};

const char *gameApplicationStateName(GameApplicationState state) noexcept;

class GameApplicationFlow {
  public:
    GameApplicationState state() const noexcept;
    const std::string &activeWorldId() const noexcept;

    bool showWorldList() noexcept;
    bool beginLoading(const std::string &worldId) noexcept;
    bool completeLoading(bool succeeded) noexcept;
    bool pause() noexcept;
    bool resume() noexcept;
    bool returnToMainMenu() noexcept;

  private:
    GameApplicationState m_state = GameApplicationState::MainMenu;
    std::string m_activeWorldId;
};

#endif // GAMEAPPLICATIONFLOW_H_INCLUDED
