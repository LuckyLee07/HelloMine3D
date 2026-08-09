#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace Ogre
{
    class RenderWindow;
}

struct OgreRenderCaptureValidation
{
    bool valid = false;
    bool enabled = false;
    std::size_t targetCount = 0;
    std::string message;
};

class OgreRenderCapture
{
  public:
    explicit OgreRenderCapture(Ogre::RenderWindow &window);
    ~OgreRenderCapture();

    OgreRenderCapture(const OgreRenderCapture &) = delete;
    OgreRenderCapture &operator=(const OgreRenderCapture &) = delete;

    void update(float deltaSeconds);
    bool isEnabled() const;
    bool isComplete() const;
    bool shouldCloseWindow() const;

    static OgreRenderCaptureValidation validateConfiguration();

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
