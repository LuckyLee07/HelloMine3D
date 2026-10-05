#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <imgui.h>

class OgreUserInterface;
class GameApplicationFlow;
struct UserSettings;

// Non-owning UI observation, explicitly attached only in an isolated capture.
// No GL calls, focus fabrication, product window or notification-scroll policy.
class PauseNotificationCapture
{
  public:
    static std::string validateEnvironment(const UserSettings& settings);
    PauseNotificationCapture(std::string output, std::string locale);
    ~PauseNotificationCapture();
    PauseNotificationCapture(const PauseNotificationCapture&) = delete;
    PauseNotificationCapture& operator=(const PauseNotificationCapture&) = delete;

    void drive(OgreUserInterface& ui, GameApplicationFlow& flow,
               bool actualNativeFocused);
    void beginUi(const std::string& status, float statusSeconds,
                 const std::string& captionCue, const std::string& captionText,
                 float captionSeconds, float actualDeltaSeconds);
    void observeButton(const char* key);
    void observeRail();
    void observeStatusText(const ImDrawList& draw, ImFont& font,
                           float fontSize, ImVec2 origin, float wrap,
                           const std::string& text, ImU32 actualColour, int vertexBegin, int indexBegin);
    void afterBackend(const ImDrawData& data, int actualPendingAction);
    bool isFramePending() const noexcept;
    bool isComplete() const noexcept;
    std::size_t frameCount() const noexcept;
    std::string framePngPath() const;
    void finishFrame(); // Called only after actual RenderWindow PNG succeeds.

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
