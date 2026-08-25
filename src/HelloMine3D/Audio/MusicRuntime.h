#ifndef MUSICRUNTIME_H_INCLUDED
#define MUSICRUNTIME_H_INCLUDED

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "../Config.h"
#include "MusicDefinitionRegistry.h"
#include "MusicStreamFile.h"

enum class MusicPlaybackState {
    Stopped,
    Waiting,
    FadingIn,
    Playing,
    FadingOut,
    Paused,
    Degraded
};

const char *musicPlaybackStateName(MusicPlaybackState state) noexcept;

class IMusicStreamBackend {
  public:
    virtual ~IMusicStreamBackend() = default;

    virtual const char *name() const noexcept = 0;
    virtual bool isReal() const noexcept = 0;
    virtual bool initialize(std::string &error) noexcept = 0;
    virtual bool start(const MusicStreamFileInfo &stream,
                       std::string &error) noexcept = 0;
    virtual void update(float deltaSeconds) noexcept = 0;
    virtual void setGain(float gain) noexcept = 0;
    virtual void setPaused(bool paused) noexcept = 0;
    virtual void stop() noexcept = 0;
    virtual bool isPlaying() const noexcept = 0;
    virtual bool isPaused() const noexcept = 0;
    virtual bool workerActive() const noexcept = 0;
    virtual std::size_t streamedBytes() const noexcept = 0;
    virtual std::string failureReason() const noexcept = 0;
};

class DummyMusicStreamBackend final : public IMusicStreamBackend {
  public:
    const char *name() const noexcept override;
    bool isReal() const noexcept override;
    bool initialize(std::string &error) noexcept override;
    bool start(const MusicStreamFileInfo &stream,
               std::string &error) noexcept override;
    void update(float deltaSeconds) noexcept override;
    void setGain(float gain) noexcept override;
    void setPaused(bool paused) noexcept override;
    void stop() noexcept override;
    bool isPlaying() const noexcept override;
    bool isPaused() const noexcept override;
    bool workerActive() const noexcept override;
    std::size_t streamedBytes() const noexcept override;
    std::string failureReason() const noexcept override;

    float gain() const noexcept;
    std::size_t starts() const noexcept;
    std::size_t stops() const noexcept;

  private:
    MusicStreamFileInfo m_stream;
    float m_elapsedSeconds = 0.f;
    float m_gain = 0.f;
    bool m_playing = false;
    bool m_paused = false;
    std::size_t m_streamedBytes = 0;
    std::size_t m_starts = 0;
    std::size_t m_stops = 0;
};

struct MusicRuntimeStats {
    std::size_t playsStarted = 0;
    std::size_t playsCompleted = 0;
    std::size_t pauses = 0;
    std::size_t stops = 0;
    std::size_t backendFailures = 0;
    std::size_t streamedBytes = 0;
    bool workerActive = false;
};

class MusicRuntime {
  public:
    using PathResolver = std::function<std::string(const std::string &)>;

    static std::unique_ptr<MusicRuntime> create(
        MusicDefinitionRegistry definitions,
        const UserSettings &settings, PathResolver resolvePath = {});
    static std::unique_ptr<MusicRuntime> createDummy(
        MusicDefinitionRegistry definitions,
        const UserSettings &settings, PathResolver resolvePath = {});

    MusicRuntime(MusicDefinitionRegistry definitions,
                 MusicStreamFileInfo stream, bool streamAvailable,
                 const UserSettings &settings,
                 std::unique_ptr<IMusicStreamBackend> backend,
                 std::string degradedReason = {});
    ~MusicRuntime();

    MusicRuntime(const MusicRuntime &) = delete;
    MusicRuntime &operator=(const MusicRuntime &) = delete;

    void update(float deltaSeconds, bool worldActive,
                bool worldPaused) noexcept;
    void setUserSettings(const UserSettings &settings) noexcept;
    void setMuted(bool muted) noexcept;
    void setSuspended(bool suspended) noexcept;
    void stopImmediately() noexcept;

    const MusicDefinitionRegistry &definitions() const noexcept;
    const MusicStreamFileInfo &stream() const noexcept;
    bool streamAvailable() const noexcept;
    MusicPlaybackState state() const noexcept;
    const MusicRuntimeStats &stats() const noexcept;
    const char *backendName() const noexcept;
    bool usesRealBackend() const noexcept;
    const std::string &degradedReason() const noexcept;

  private:
    enum class FadeDestination { None, Pause, Stop };

    bool startTrack() noexcept;
    void updateActive(float deltaSeconds, float targetGain) noexcept;
    void updateFadeOut(float deltaSeconds, float targetGain,
                       FadeDestination destination) noexcept;
    void completeStop(bool resetDelay) noexcept;
    void degrade(const std::string &reason) noexcept;
    float targetGain() const noexcept;
    float nextGapSeconds() noexcept;

    MusicDefinitionRegistry m_definitions;
    MusicStreamFileInfo m_stream;
    bool m_streamAvailable = false;
    UserSettings m_settings;
    std::unique_ptr<IMusicStreamBackend> m_backend;
    std::string m_degradedReason;
    MusicPlaybackState m_state = MusicPlaybackState::Stopped;
    FadeDestination m_fadeDestination = FadeDestination::None;
    float m_currentGain = 0.f;
    float m_fadeStartGain = 0.f;
    float m_waitSeconds = 0.f;
    std::size_t m_gapSequence = 0;
    bool m_muted = false;
    bool m_suspended = false;
    bool m_lastWorldActive = false;
    MusicRuntimeStats m_stats;
};

#endif // MUSICRUNTIME_H_INCLUDED
