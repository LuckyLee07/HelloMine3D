#include "AudioRuntime.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <list>
#include <utility>

#include "../Sandbox/Events/BlockEvents.h"
#include "../Sandbox/Events/CraftingEvents.h"
#include "../Sandbox/Events/EntityEvents.h"
#include "../Sandbox/Events/SandboxEventBus.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <mmsystem.h>
#endif

namespace {
constexpr std::size_t MaxGlobalVoices = 16;
constexpr float Pi = 3.14159265358979323846f;

glm::vec3 blockCenter(const glm::ivec3 &position)
{
    return glm::vec3(static_cast<float>(position.x) + 0.5f,
                     static_cast<float>(position.y) + 0.5f,
                     static_cast<float>(position.z) + 0.5f);
}

bool truthy(const char *value)
{
    if (value == nullptr) {
        return false;
    }
    const std::string text(value);
    return text == "1" || text == "true" || text == "TRUE" ||
           text == "on" || text == "ON";
}

#if defined(_WIN32)
class WindowsWaveOutBackend final : public IAudioBackend {
  public:
    ~WindowsWaveOutBackend() override
    {
        if (m_output == nullptr) {
            return;
        }
        waveOutReset(m_output);
        for (const auto &voice : m_voices) {
            waveOutUnprepareHeader(m_output, &voice->header,
                                   sizeof(WAVEHDR));
        }
        m_voices.clear();
        waveOutClose(m_output);
        m_output = nullptr;
    }

    bool initialize(std::string &error) noexcept override
    {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = SampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign =
            static_cast<WORD>(format.nChannels *
                              (format.wBitsPerSample / 8));
        format.nAvgBytesPerSec =
            format.nSamplesPerSec * format.nBlockAlign;
        const MMRESULT result = waveOutOpen(
            &m_output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            m_output = nullptr;
            error = "waveOutOpen failed with code " +
                    std::to_string(result);
            return false;
        }
        error.clear();
        return true;
    }

    AudioBackendPlayResult play(
        const AudioDefinition &definition,
        const AudioPlaybackEvent &event, float effectiveGain,
        const AudioListenerState &listener) noexcept override
    {
        update();
        if (m_output == nullptr || m_paused) {
            return AudioBackendPlayResult::Failed;
        }
        if (effectiveGain <= 0.0001f) {
            return AudioBackendPlayResult::Suppressed;
        }
        if (m_voices.size() >= MaxGlobalVoices) {
            return AudioBackendPlayResult::Suppressed;
        }
        const std::size_t cueVoices =
            static_cast<std::size_t>(std::count_if(
                m_voices.begin(), m_voices.end(),
                [&definition](const std::unique_ptr<Voice> &voice) {
                    return voice->cueId == definition.id;
                }));
        if (cueVoices >=
            static_cast<std::size_t>(definition.maxVoices)) {
            return AudioBackendPlayResult::Suppressed;
        }

        float leftGain = effectiveGain;
        float rightGain = effectiveGain;
        if (definition.spatial && event.hasPosition) {
            const glm::vec3 relative = event.position - listener.position;
            const float distance = glm::length(relative);
            const float attenuation =
                std::clamp(1.f - distance / 40.f, 0.f, 1.f);
            if (attenuation <= 0.0001f) {
                return AudioBackendPlayResult::Suppressed;
            }
            glm::vec3 forward(listener.forward.x, 0.f, listener.forward.z);
            if (glm::length(forward) < 0.0001f) {
                forward = glm::vec3(0.f, 0.f, -1.f);
            }
            forward = glm::normalize(forward);
            const glm::vec3 right(-forward.z, 0.f, forward.x);
            float pan = 0.f;
            if (distance > 0.0001f) {
                pan = std::clamp(glm::dot(relative / distance, right),
                                 -1.f, 1.f);
            }
            leftGain *= attenuation * std::sqrt((1.f - pan) * 0.5f);
            rightGain *= attenuation * std::sqrt((1.f + pan) * 0.5f);
        }

        auto voice = std::make_unique<Voice>();
        voice->cueId = definition.id;
        const std::size_t frames = static_cast<std::size_t>(
            (static_cast<std::uint64_t>(SampleRate) *
             static_cast<std::uint64_t>(definition.durationMilliseconds)) /
            1000u);
        voice->samples.resize(frames * 2u);
        std::uint32_t noise = static_cast<std::uint32_t>(
            std::hash<std::string>{}(definition.id) ^ m_sequence++);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const float time = static_cast<float>(frame) /
                               static_cast<float>(SampleRate);
            const float phase = 2.f * Pi * definition.frequency * time;
            float sample = 0.f;
            switch (definition.waveform) {
            case AudioWaveform::Sine:
                sample = std::sin(phase);
                break;
            case AudioWaveform::Square:
                sample = std::sin(phase) >= 0.f ? 1.f : -1.f;
                break;
            case AudioWaveform::Noise:
                noise ^= noise << 13;
                noise ^= noise >> 17;
                noise ^= noise << 5;
                sample = static_cast<float>(noise & 0xffffu) / 32767.5f -
                         1.f;
                break;
            }
            const float normalized =
                frames > 1 ? static_cast<float>(frame) /
                                 static_cast<float>(frames - 1)
                           : 1.f;
            const float envelope = std::min(
                {1.f, normalized * 24.f, (1.f - normalized) * 12.f});
            const float shaped = sample * std::max(0.f, envelope);
            voice->samples[frame * 2u] = toSample(shaped * leftGain);
            voice->samples[frame * 2u + 1u] =
                toSample(shaped * rightGain);
        }
        voice->header.lpData = reinterpret_cast<LPSTR>(
            voice->samples.data());
        voice->header.dwBufferLength = static_cast<DWORD>(
            voice->samples.size() * sizeof(std::int16_t));
        if (waveOutPrepareHeader(m_output, &voice->header,
                                 sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            return AudioBackendPlayResult::Failed;
        }
        if (waveOutWrite(m_output, &voice->header,
                         sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            waveOutUnprepareHeader(m_output, &voice->header,
                                   sizeof(WAVEHDR));
            return AudioBackendPlayResult::Failed;
        }
        m_voices.push_back(std::move(voice));
        return AudioBackendPlayResult::Played;
    }

    void update() noexcept override
    {
        if (m_output == nullptr) {
            return;
        }
        for (auto iterator = m_voices.begin(); iterator != m_voices.end();) {
            if (((*iterator)->header.dwFlags & WHDR_DONE) == 0) {
                ++iterator;
                continue;
            }
            if (waveOutUnprepareHeader(m_output, &(*iterator)->header,
                                       sizeof(WAVEHDR)) ==
                WAVERR_STILLPLAYING) {
                ++iterator;
                continue;
            }
            iterator = m_voices.erase(iterator);
        }
    }

    void setPaused(bool paused) noexcept override
    {
        if (m_output == nullptr || paused == m_paused) {
            return;
        }
        m_paused = paused;
        if (paused) {
            waveOutPause(m_output);
        }
        else {
            waveOutRestart(m_output);
        }
    }

    std::size_t activeVoices() const noexcept override
    {
        return m_voices.size();
    }

    const char *name() const noexcept override
    {
        return "windows-waveout";
    }

    bool isReal() const noexcept override
    {
        return true;
    }

  private:
    struct Voice {
        std::string cueId;
        std::vector<std::int16_t> samples;
        WAVEHDR header{};
    };

    static constexpr int SampleRate = 44100;

    static std::int16_t toSample(float value) noexcept
    {
        return static_cast<std::int16_t>(
            std::clamp(value, -1.f, 1.f) * 32767.f);
    }

    HWAVEOUT m_output = nullptr;
    std::list<std::unique_ptr<Voice>> m_voices;
    std::uint32_t m_sequence = 1;
    bool m_paused = false;
};
#endif

std::unique_ptr<IAudioBackend> platformBackend()
{
#if defined(_WIN32)
    return std::make_unique<WindowsWaveOutBackend>();
#else
    return nullptr;
#endif
}
} // namespace

bool DummyAudioBackend::initialize(std::string &error) noexcept
{
    error.clear();
    return true;
}

AudioBackendPlayResult DummyAudioBackend::play(
    const AudioDefinition &definition, const AudioPlaybackEvent &,
    float effectiveGain,
    const AudioListenerState &) noexcept
{
    if (m_paused || effectiveGain <= 0.0001f) {
        return AudioBackendPlayResult::Suppressed;
    }
    const std::size_t cueVoices = m_activeByCue[definition.id];
    if (m_activeVoices >= MaxGlobalVoices ||
        cueVoices >= static_cast<std::size_t>(definition.maxVoices)) {
        return AudioBackendPlayResult::Suppressed;
    }
    ++m_acceptedEvents;
    ++m_activeVoices;
    ++m_activeByCue[definition.id];
    return AudioBackendPlayResult::Played;
}

void DummyAudioBackend::update() noexcept
{
    m_activeVoices = 0;
    m_activeByCue.clear();
}

void DummyAudioBackend::setPaused(bool paused) noexcept
{
    m_paused = paused;
}

std::size_t DummyAudioBackend::activeVoices() const noexcept
{
    return m_activeVoices;
}

const char *DummyAudioBackend::name() const noexcept
{
    return "dummy";
}

bool DummyAudioBackend::isReal() const noexcept
{
    return false;
}

std::size_t DummyAudioBackend::acceptedEvents() const noexcept
{
    return m_acceptedEvents;
}

std::unique_ptr<AudioRuntime> AudioRuntime::create(
    AudioDefinitionRegistry definitions, const UserSettings &settings)
{
    const char *requested = std::getenv("HELLOMINE3D_AUDIO_BACKEND");
    const std::string requestedName = requested != nullptr
                                          ? requested
                                          : "auto";
    std::string degradedReason;
    std::unique_ptr<IAudioBackend> backend;
    if (definitions.definitions().empty()) {
        backend = std::make_unique<DummyAudioBackend>();
        degradedReason = "audio definitions are unavailable";
    }
    else if (requestedName == "dummy" || truthy(std::getenv(
                                        "HELLOMINE3D_DISABLE_AUDIO"))) {
        backend = std::make_unique<DummyAudioBackend>();
        degradedReason = "dummy backend requested";
    }
    else {
        backend = platformBackend();
        if (backend == nullptr) {
            degradedReason = "no native audio backend on this platform";
        }
    }

    std::string error;
    if (backend == nullptr || !backend->initialize(error)) {
        if (!error.empty()) {
            degradedReason = error;
        }
        backend = std::make_unique<DummyAudioBackend>();
        std::string ignored;
        backend->initialize(ignored);
    }
    return std::make_unique<AudioRuntime>(
        std::move(definitions), settings, std::move(backend),
        std::move(degradedReason));
}

std::unique_ptr<AudioRuntime> AudioRuntime::createDummy(
    AudioDefinitionRegistry definitions, const UserSettings &settings)
{
    auto backend = std::make_unique<DummyAudioBackend>();
    std::string ignored;
    backend->initialize(ignored);
    return std::make_unique<AudioRuntime>(
        std::move(definitions), settings, std::move(backend),
        "dummy backend requested");
}

AudioRuntime::AudioRuntime(AudioDefinitionRegistry definitions,
                           const UserSettings &settings,
                           std::unique_ptr<IAudioBackend> backend,
                           std::string degradedReason)
    : m_definitions(std::move(definitions))
    , m_settings(settings)
    , m_backend(std::move(backend))
    , m_degradedReason(std::move(degradedReason))
{
}

AudioRuntime::~AudioRuntime()
{
    detach();
}

void AudioRuntime::attach(SandboxEventBus &eventBus)
{
    detach();
    m_eventBus = &eventBus;
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::BlockBreak, [this](const SandboxEvent &event) {
            const auto &block = static_cast<const BlockBreakEvent &>(event);
            submit({"block.break", true, blockCenter(block.position), 1.f});
        }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::BlockPlace, [this](const SandboxEvent &event) {
            const auto &block = static_cast<const BlockPlaceEvent &>(event);
            submit({"block.place", true, blockCenter(block.position), 1.f});
        }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::ItemPickup, [this](const SandboxEvent &event) {
            const auto &pickup = static_cast<const ItemPickupEvent &>(event);
            submit({"item.pickup", true, pickup.position, 1.f});
        }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::EntityDamage, [this](const SandboxEvent &event) {
            const auto &damage = static_cast<const EntityDamageEvent &>(event);
            submit({"combat.hit", true, damage.position, 1.f});
        }));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::CraftCompleted,
        [this](const SandboxEvent &event) {
            const auto &craft =
                static_cast<const CraftCompletedEvent &>(event);
            submit({"craft.success", true, craft.position, 1.f});
        }));
}

void AudioRuntime::detach() noexcept
{
    if (m_eventBus != nullptr) {
        for (unsigned subscription : m_subscriptions) {
            m_eventBus->unsubscribe(subscription);
        }
    }
    m_subscriptions.clear();
    m_eventBus = nullptr;
}

void AudioRuntime::submit(AudioPlaybackEvent event) noexcept
{
    ++m_stats.submittedEvents;
    const AudioDefinition *definition = m_definitions.find(event.cueId);
    if (definition == nullptr) {
        ++m_stats.missingDefinitions;
        return;
    }
    if (m_worldPaused && definition->category != AudioCategory::Ui) {
        ++m_stats.suppressedEvents;
        return;
    }
    if (m_settings.audioCaptions && m_captionSink &&
        !definition->caption.empty()) {
        try {
            m_captionSink(definition->caption);
        }
        catch (...) {
        }
    }
    if (m_muted) {
        ++m_stats.suppressedEvents;
        return;
    }
    const float effectiveGain =
        std::clamp(event.gain, 0.f, 1.f) * definition->gain *
        std::clamp(m_settings.masterVolume, 0.f, 1.f) *
        categoryVolume(definition->category);
    if (effectiveGain <= 0.0001f) {
        ++m_stats.suppressedEvents;
        return;
    }
    if (m_backend == nullptr) {
        ++m_stats.backendFailures;
        return;
    }
    switch (m_backend->play(*definition, event, effectiveGain, m_listener)) {
    case AudioBackendPlayResult::Played:
        ++m_stats.playedEvents;
        break;
    case AudioBackendPlayResult::Suppressed:
        ++m_stats.suppressedEvents;
        break;
    case AudioBackendPlayResult::Failed:
        ++m_stats.backendFailures;
        break;
    }
}

void AudioRuntime::emitUiClick() noexcept
{
    submit({"ui.click", false, glm::vec3(0.f), 1.f});
}

void AudioRuntime::update(float deltaSeconds, bool worldSimulationActive,
                          const AudioListenerState &listener) noexcept
{
    m_listener = listener;
    if (m_backend != nullptr) {
        m_backend->update();
        m_stats.activeVoices = m_backend->activeVoices();
    }
    if (!worldSimulationActive || m_worldPaused || m_muted) {
        return;
    }
    m_ambientElapsedSeconds += std::max(0.f, deltaSeconds);
    if (m_ambientElapsedSeconds < AmbientIntervalSeconds) {
        return;
    }
    m_ambientElapsedSeconds =
        std::fmod(m_ambientElapsedSeconds, AmbientIntervalSeconds);
    ++m_stats.ambientEvents;
    submit({"ambient.wind", false, glm::vec3(0.f), 1.f});
}

void AudioRuntime::setUserSettings(const UserSettings &settings) noexcept
{
    m_settings = settings;
}

void AudioRuntime::setCaptionSink(
    std::function<void(std::string)> sink) noexcept
{
    m_captionSink = std::move(sink);
}

void AudioRuntime::setWorldPaused(bool paused) noexcept
{
    m_worldPaused = paused;
}

void AudioRuntime::setMuted(bool muted) noexcept
{
    m_muted = muted;
}

void AudioRuntime::setSuspended(bool suspended) noexcept
{
    if (m_backend != nullptr) {
        m_backend->setPaused(suspended);
    }
}

const AudioRuntimeStats &AudioRuntime::stats() const noexcept
{
    return m_stats;
}

const AudioDefinitionRegistry &AudioRuntime::definitions() const noexcept
{
    return m_definitions;
}

const char *AudioRuntime::backendName() const noexcept
{
    return m_backend != nullptr ? m_backend->name() : "none";
}

bool AudioRuntime::usesRealBackend() const noexcept
{
    return m_backend != nullptr && m_backend->isReal();
}

const std::string &AudioRuntime::degradedReason() const noexcept
{
    return m_degradedReason;
}

float AudioRuntime::categoryVolume(AudioCategory category) const noexcept
{
    switch (category) {
    case AudioCategory::Ui:
        return std::clamp(m_settings.uiVolume, 0.f, 1.f);
    case AudioCategory::Effects:
        return std::clamp(m_settings.effectsVolume, 0.f, 1.f);
    case AudioCategory::Ambient:
        return std::clamp(m_settings.ambientVolume, 0.f, 1.f);
    }
    return 0.f;
}
