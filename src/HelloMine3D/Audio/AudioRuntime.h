#ifndef AUDIORUNTIME_H_INCLUDED
#define AUDIORUNTIME_H_INCLUDED

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Config.h"
#include "../Maths/glm.h"
#include "AudioDefinitionRegistry.h"

class SandboxEventBus;

struct AudioListenerState {
    glm::vec3 position{0.f};
    glm::vec3 forward{0.f, 0.f, -1.f};
};

struct AudioPlaybackEvent {
    std::string cueId;
    bool hasPosition = false;
    glm::vec3 position{0.f};
    float gain = 1.f;
};

enum class AudioBackendPlayResult {
    Played,
    Suppressed,
    Failed
};

class IAudioBackend {
  public:
    virtual ~IAudioBackend() = default;
    virtual bool initialize(std::string &error) noexcept = 0;
    virtual AudioBackendPlayResult play(
        const AudioDefinition &definition,
        const AudioPlaybackEvent &event, float effectiveGain,
        const AudioListenerState &listener) noexcept = 0;
    virtual void update() noexcept = 0;
    virtual void setPaused(bool paused) noexcept = 0;
    virtual std::size_t activeVoices() const noexcept = 0;
    virtual const char *name() const noexcept = 0;
    virtual bool isReal() const noexcept = 0;
};

class DummyAudioBackend final : public IAudioBackend {
  public:
    bool initialize(std::string &error) noexcept override;
    AudioBackendPlayResult play(
        const AudioDefinition &definition,
        const AudioPlaybackEvent &event, float effectiveGain,
        const AudioListenerState &listener) noexcept override;
    void update() noexcept override;
    void setPaused(bool paused) noexcept override;
    std::size_t activeVoices() const noexcept override;
    const char *name() const noexcept override;
    bool isReal() const noexcept override;

    std::size_t acceptedEvents() const noexcept;

  private:
    std::size_t m_acceptedEvents = 0;
    std::size_t m_activeVoices = 0;
    std::unordered_map<std::string, std::size_t> m_activeByCue;
    bool m_paused = false;
};

struct AudioRuntimeStats {
    std::size_t submittedEvents = 0;
    std::size_t playedEvents = 0;
    std::size_t suppressedEvents = 0;
    std::size_t missingDefinitions = 0;
    std::size_t backendFailures = 0;
    std::size_t ambientEvents = 0;
    std::size_t activeVoices = 0;
};

class AudioRuntime {
  public:
    static constexpr float AmbientIntervalSeconds = 8.f;

    static std::unique_ptr<AudioRuntime> create(
        AudioDefinitionRegistry definitions,
        const UserSettings &settings);
    static std::unique_ptr<AudioRuntime> createDummy(
        AudioDefinitionRegistry definitions,
        const UserSettings &settings);

    AudioRuntime(AudioDefinitionRegistry definitions,
                 const UserSettings &settings,
                 std::unique_ptr<IAudioBackend> backend,
                 std::string degradedReason = {});
    ~AudioRuntime();

    AudioRuntime(const AudioRuntime &) = delete;
    AudioRuntime &operator=(const AudioRuntime &) = delete;

    void attach(SandboxEventBus &eventBus);
    void detach() noexcept;
    void submit(AudioPlaybackEvent event) noexcept;
    void emitUiClick() noexcept;
    void update(float deltaSeconds, bool worldSimulationActive,
                const AudioListenerState &listener) noexcept;
    void setUserSettings(const UserSettings &settings) noexcept;
    void setWorldPaused(bool paused) noexcept;
    void setMuted(bool muted) noexcept;
    void setSuspended(bool suspended) noexcept;

    const AudioRuntimeStats &stats() const noexcept;
    const AudioDefinitionRegistry &definitions() const noexcept;
    const char *backendName() const noexcept;
    bool usesRealBackend() const noexcept;
    const std::string &degradedReason() const noexcept;

  private:
    float categoryVolume(AudioCategory category) const noexcept;

    AudioDefinitionRegistry m_definitions;
    UserSettings m_settings;
    std::unique_ptr<IAudioBackend> m_backend;
    SandboxEventBus *m_eventBus = nullptr;
    std::vector<unsigned> m_subscriptions;
    AudioListenerState m_listener;
    AudioRuntimeStats m_stats;
    std::string m_degradedReason;
    float m_ambientElapsedSeconds = 0.f;
    bool m_worldPaused = false;
    bool m_muted = false;
};

#endif // AUDIORUNTIME_H_INCLUDED
