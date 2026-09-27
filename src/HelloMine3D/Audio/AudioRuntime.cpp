#include "AudioRuntime.h"

#include <algorithm>
#include <array>
#include <atomic>
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
#include "../Util/ResourcePaths.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <mmsystem.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)
#include <AudioToolbox/AudioQueue.h>
#endif

namespace {
constexpr std::size_t MaxGlobalVoices = 16;
constexpr std::size_t ReservedFeedbackVoices = 4;

std::size_t voiceAdmissionLimit(const AudioDefinition &definition) noexcept
{
    // Keep headroom for new warnings and menu feedback without stealing a
    // voice that the native callback may still be reading.
    const bool critical = definition.category == AudioCategory::Ui ||
        definition.id == "combat.windup" || definition.id == "combat.hit" ||
        definition.id == "combat.guard";
    return critical ? MaxGlobalVoices
                    : MaxGlobalVoices - ReservedFeedbackVoices;
}

struct StereoGains {
    float left = 0.f;
    float right = 0.f;
};

bool computeStereoGains(const AudioDefinition &definition,
                        const AudioPlaybackEvent &event,
                        float effectiveGain,
                        const AudioListenerState &listener,
                        StereoGains &gains) noexcept
{
    if (!std::isfinite(effectiveGain) || effectiveGain <= 0.0001f) {
        return false;
    }
    gains.left = effectiveGain;
    gains.right = effectiveGain;
    if (!definition.spatial || !event.hasPosition) {
        return true;
    }

    const glm::vec3 relative = event.position - listener.position;
    const float distance = glm::length(relative);
    if (!std::isfinite(distance)) {
        return false;
    }
    const float attenuation =
        std::clamp(1.f - distance / 40.f, 0.f, 1.f);
    if (attenuation <= 0.0001f) {
        return false;
    }
    glm::vec3 forward(listener.forward.x, 0.f, listener.forward.z);
    if (glm::length(forward) < 0.0001f) {
        forward = glm::vec3(0.f, 0.f, -1.f);
    }
    forward = glm::normalize(forward);
    const glm::vec3 right(-forward.z, 0.f, forward.x);
    float pan = 0.f;
    if (distance > 0.0001f) {
        pan = std::clamp(glm::dot(relative / distance, right), -1.f,
                         1.f);
    }
    gains.left *= attenuation * std::sqrt((1.f - pan) * 0.5f);
    gains.right *= attenuation * std::sqrt((1.f + pan) * 0.5f);
    return true;
}

std::int16_t toPcmSample(float value) noexcept
{
    return static_cast<std::int16_t>(
        std::clamp(value, -1.f, 1.f) * 32767.f);
}

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

std::string resolveBaseAudioPath(const std::string &logicalPath)
{
    return ResourcePaths::join(ResourcePaths::projectRoot(), logicalPath);
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
        const AudioSampleData &sample,
        const AudioPlaybackEvent &event, float effectiveGain,
        const AudioListenerState &listener) noexcept override
    {
        update();
        if (m_output == nullptr || m_paused) {
            return AudioBackendPlayResult::Failed;
        }
        StereoGains gains;
        if (!computeStereoGains(definition, event, effectiveGain,
                                listener, gains)) {
            return AudioBackendPlayResult::Suppressed;
        }
        if (m_voices.size() >= voiceAdmissionLimit(definition)) {
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

        auto voice = std::make_unique<Voice>();
        voice->cueId = definition.id;
        const std::size_t frames = sample.monoSamples.size();
        voice->samples.resize(frames * 2u);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const float source = static_cast<float>(
                                     sample.monoSamples[frame]) /
                                 32768.f;
            voice->samples[frame * 2u] =
                toPcmSample(source * gains.left);
            voice->samples[frame * 2u + 1u] =
                toPcmSample(source * gains.right);
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

    void stopAll() noexcept override
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

    HWAVEOUT m_output = nullptr;
    std::list<std::unique_ptr<Voice>> m_voices;
    bool m_paused = false;
};
#endif

#if defined(__APPLE__) && defined(__MACH__)
class MacAudioQueueBackend final : public IAudioBackend {
  public:
    ~MacAudioQueueBackend() override
    {
        shutdown();
    }

    bool initialize(std::string &error) noexcept override
    {
        AudioStreamBasicDescription format{};
        format.mSampleRate = static_cast<Float64>(SampleRate);
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger |
                              kLinearPCMFormatFlagIsPacked;
        format.mBytesPerPacket = BytesPerFrame;
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = BytesPerFrame;
        format.mChannelsPerFrame = ChannelCount;
        format.mBitsPerChannel = BitsPerChannel;

        OSStatus status = AudioQueueNewOutput(
            &format, &MacAudioQueueBackend::outputCallback, this,
            nullptr, nullptr, 0, &m_queue);
        if (status != noErr) {
            error = statusError("AudioQueueNewOutput", status);
            m_queue = nullptr;
            return false;
        }

        for (AudioQueueBufferRef &buffer : m_buffers) {
            status = AudioQueueAllocateBuffer(m_queue, BufferBytes,
                                              &buffer);
            if (status != noErr) {
                error = statusError("AudioQueueAllocateBuffer", status);
                shutdown();
                return false;
            }
            std::fill_n(static_cast<std::int16_t *>(buffer->mAudioData),
                        BufferFrames * ChannelCount,
                        static_cast<std::int16_t>(0));
            buffer->mAudioDataByteSize = BufferBytes;
            status = AudioQueueEnqueueBuffer(m_queue, buffer, 0, nullptr);
            if (status != noErr) {
                error = statusError("AudioQueueEnqueueBuffer", status);
                shutdown();
                return false;
            }
        }

        error.clear();
        return true;
    }

    AudioBackendPlayResult play(
        const AudioDefinition &definition,
        const AudioSampleData &sample,
        const AudioPlaybackEvent &event, float effectiveGain,
        const AudioListenerState &listener) noexcept override
    {
        StereoGains gains;
        if (!computeStereoGains(definition, event, effectiveGain,
                                listener, gains)) {
            return AudioBackendPlayResult::Suppressed;
        }
        if (sample.sampleRate != SampleRate ||
            sample.monoSamples.empty()) {
            return AudioBackendPlayResult::Failed;
        }

        if (m_queue == nullptr ||
            m_failed.load(std::memory_order_acquire) ||
            m_stopping.load(std::memory_order_acquire)) {
            return AudioBackendPlayResult::Failed;
        }
        if (m_paused.load(std::memory_order_acquire)) {
            return AudioBackendPlayResult::Suppressed;
        }
        if (voiceCount() >= voiceAdmissionLimit(definition)) {
            return AudioBackendPlayResult::Suppressed;
        }
        const std::size_t cueVoices =
            static_cast<std::size_t>(std::count_if(
                m_voices.begin(), m_voices.end(),
                [&definition](const Voice &voice) {
                    return voice.state.load(std::memory_order_acquire) !=
                               VoiceState::Free &&
                           voice.definition == &definition;
                }));
        if (cueVoices >=
            static_cast<std::size_t>(definition.maxVoices)) {
            return AudioBackendPlayResult::Suppressed;
        }

        Voice *available = nullptr;
        for (Voice &voice : m_voices) {
            VoiceState expected = VoiceState::Free;
            if (voice.state.compare_exchange_strong(
                    expected, VoiceState::Reserved,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                available = &voice;
                break;
            }
        }
        if (available == nullptr) {
            return AudioBackendPlayResult::Suppressed;
        }
        available->definition = &definition;
        available->sample = &sample;
        available->frame = 0u;
        available->leftGain = gains.left;
        available->rightGain = gains.right;
        available->state.store(VoiceState::Ready,
                               std::memory_order_release);
        m_idleUpdateCount = 0u;
        if (!ensureStarted()) {
            available->state.store(VoiceState::Free,
                                   std::memory_order_release);
            return AudioBackendPlayResult::Failed;
        }
        return AudioBackendPlayResult::Played;
    }

    void update() noexcept override
    {
        if (m_queue == nullptr || !m_started || m_idlePaused ||
            m_paused.load(std::memory_order_acquire) ||
            m_failed.load(std::memory_order_acquire) ||
            m_stopping.load(std::memory_order_acquire)) {
            return;
        }
        if (voiceCount() != 0u) {
            m_idleUpdateCount = 0u;
            return;
        }
        if (++m_idleUpdateCount < IdleUpdatesBeforePause) {
            return;
        }
        if (AudioQueuePause(m_queue) == noErr) {
            m_idlePaused = true;
        }
        else {
            m_failed.store(true, std::memory_order_release);
        }
    }

    void setPaused(bool paused) noexcept override
    {
        if (m_queue == nullptr ||
            m_stopping.load(std::memory_order_acquire) ||
            m_failed.load(std::memory_order_acquire) ||
            paused == m_paused.load(std::memory_order_acquire)) {
            return;
        }
        m_paused.store(paused, std::memory_order_release);

        OSStatus status = noErr;
        if (paused) {
            if (m_started && !m_idlePaused) {
                status = AudioQueuePause(m_queue);
            }
        }
        else if (m_started && !m_idlePaused) {
            status = AudioQueueStart(m_queue, nullptr);
        }
        if (status != noErr) {
            m_failed.store(true, std::memory_order_release);
        }
    }

    void stopAll() noexcept override
    {
        // Do not recycle a slot while the realtime callback may still be
        // reading it. Cancellation becomes Free on the callback thread.
        for (Voice &voice : m_voices) {
            VoiceState expected = VoiceState::Ready;
            voice.state.compare_exchange_strong(
                expected, VoiceState::Cancelled,
                std::memory_order_acq_rel,
                std::memory_order_acquire);
        }
    }

    std::size_t activeVoices() const noexcept override
    {
        return m_failed.load(std::memory_order_acquire) ? 0u
                                                        : voiceCount();
    }

    const char *name() const noexcept override
    {
        return "macos-audioqueue";
    }

    bool isReal() const noexcept override
    {
        return !m_failed.load(std::memory_order_acquire);
    }

  private:
    enum class VoiceState : std::uint8_t {
        Free,
        Reserved,
        Ready,
        Cancelled
    };

    struct Voice {
        std::atomic<VoiceState> state{VoiceState::Free};
        const AudioDefinition *definition = nullptr;
        const AudioSampleData *sample = nullptr;
        std::size_t frame = 0;
        float leftGain = 0.f;
        float rightGain = 0.f;
    };

    static constexpr int SampleRate = AudioSampleBank::RequiredSampleRate;
    static constexpr std::size_t ChannelCount = 2u;
    static constexpr std::size_t BitsPerChannel = 16u;
    static constexpr std::size_t BytesPerFrame =
        ChannelCount * (BitsPerChannel / 8u);
    static constexpr std::size_t BufferFrames = 512u;
    static constexpr UInt32 BufferBytes = static_cast<UInt32>(
        BufferFrames * BytesPerFrame);
    static constexpr std::size_t BufferCount = 3u;
    static constexpr std::size_t IdleUpdatesBeforePause = 30u;

    static void outputCallback(void *userData, AudioQueueRef queue,
                               AudioQueueBufferRef buffer) noexcept
    {
        if (userData == nullptr || buffer == nullptr) {
            return;
        }
        static_cast<MacAudioQueueBackend *>(userData)->renderAndEnqueue(
            queue, buffer);
    }

    void renderAndEnqueue(AudioQueueRef queue,
                          AudioQueueBufferRef buffer) noexcept
    {
        std::array<float, BufferFrames * ChannelCount> mixed{};
        if (m_stopping.load(std::memory_order_acquire) ||
            m_failed.load(std::memory_order_acquire) || queue == nullptr ||
            queue != m_queue) {
            return;
        }

        if (!m_paused.load(std::memory_order_acquire)) {
            for (Voice &voice : m_voices) {
                const VoiceState state =
                    voice.state.load(std::memory_order_acquire);
                if (state == VoiceState::Cancelled) {
                    deactivateVoice(voice);
                    continue;
                }
                if (state != VoiceState::Ready) {
                    continue;
                }
                if (voice.sample == nullptr ||
                    voice.frame >= voice.sample->monoSamples.size()) {
                    deactivateVoice(voice);
                    continue;
                }
                const std::size_t remaining =
                    voice.sample->monoSamples.size() - voice.frame;
                const std::size_t frames =
                    std::min(BufferFrames, remaining);
                for (std::size_t frame = 0; frame < frames; ++frame) {
                    const float source =
                        static_cast<float>(voice.sample->monoSamples[
                            voice.frame + frame]) /
                        32768.f;
                    mixed[frame * ChannelCount] +=
                        source * voice.leftGain;
                    mixed[frame * ChannelCount + 1u] +=
                        source * voice.rightGain;
                }
                voice.frame += frames;
                if (voice.frame >= voice.sample->monoSamples.size()) {
                    deactivateVoice(voice);
                }
            }
        }

        auto *output = static_cast<std::int16_t *>(buffer->mAudioData);
        for (std::size_t index = 0; index < mixed.size(); ++index) {
            output[index] = toPcmSample(mixed[index]);
        }
        buffer->mAudioDataByteSize = BufferBytes;
        const OSStatus status =
            AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
        if (status != noErr) {
            m_failed.store(true, std::memory_order_release);
        }
    }

    void deactivateVoice(Voice &voice) noexcept
    {
        voice.state.store(VoiceState::Free, std::memory_order_release);
    }

    void clearVoices() noexcept
    {
        for (Voice &voice : m_voices) {
            deactivateVoice(voice);
        }
    }

    std::size_t voiceCount() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            m_voices.begin(), m_voices.end(), [](const Voice &voice) {
                return voice.state.load(std::memory_order_acquire) !=
                       VoiceState::Free;
            }));
    }

    bool ensureStarted() noexcept
    {
        if (m_started && !m_idlePaused) {
            return true;
        }
        if (m_queue == nullptr ||
            AudioQueueStart(m_queue, nullptr) != noErr) {
            m_failed.store(true, std::memory_order_release);
            return false;
        }
        m_started = true;
        m_idlePaused = false;
        return true;
    }

    void shutdown() noexcept
    {
        if (m_queue == nullptr) {
            return;
        }
        m_stopping.store(true, std::memory_order_release);
        if (m_started) {
            AudioQueueStop(m_queue, true);
        }
        AudioQueueDispose(m_queue, true);
        clearVoices();
        m_queue = nullptr;
        m_started = false;
        for (AudioQueueBufferRef &buffer : m_buffers) {
            buffer = nullptr;
        }
    }

    static std::string statusError(const char *operation,
                                   OSStatus status)
    {
        return std::string(operation) + " failed with code " +
               std::to_string(static_cast<long long>(status));
    }

    AudioQueueRef m_queue = nullptr;
    std::array<AudioQueueBufferRef, BufferCount> m_buffers{};
    std::array<Voice, MaxGlobalVoices> m_voices{};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_failed{false};
    std::size_t m_idleUpdateCount = 0u;
    bool m_started = false;
    bool m_idlePaused = false;
};
#endif

std::unique_ptr<IAudioBackend> platformBackend()
{
#if defined(_WIN32)
    return std::make_unique<WindowsWaveOutBackend>();
#elif defined(__APPLE__) && defined(__MACH__)
    return std::make_unique<MacAudioQueueBackend>();
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
    const AudioDefinition &definition, const AudioSampleData &,
    const AudioPlaybackEvent &,
    float effectiveGain,
    const AudioListenerState &) noexcept
{
    if (m_paused || effectiveGain <= 0.0001f) {
        return AudioBackendPlayResult::Suppressed;
    }
    const std::size_t cueVoices = m_activeByCue[definition.id];
    if (m_activeVoices >= voiceAdmissionLimit(definition) ||
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

void DummyAudioBackend::stopAll() noexcept
{
    m_activeVoices = 0;
    m_activeByCue.clear();
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
    AudioDefinitionRegistry definitions, const UserSettings &settings,
    AudioSampleBank::PathResolver resolvePath)
{
    const char *requested = std::getenv("HELLOMINE3D_AUDIO_BACKEND");
    const std::string requestedName = requested != nullptr
                                          ? requested
                                          : "auto";
    std::string degradedReason;
    AudioSampleBank samples;
    bool samplesAvailable = false;
    if (!definitions.definitions().empty()) {
        if (!resolvePath) {
            resolvePath = resolveBaseAudioPath;
        }
        std::string sampleError;
        samplesAvailable = samples.tryFreeze(
            definitions, resolvePath, sampleError);
        if (!samplesAvailable) {
            degradedReason = "audio samples are unavailable: " + sampleError;
        }
    }
    std::unique_ptr<IAudioBackend> backend;
    if (definitions.definitions().empty()) {
        backend = std::make_unique<DummyAudioBackend>();
        degradedReason = "audio definitions are unavailable";
    }
    else if (!samplesAvailable) {
        backend = std::make_unique<DummyAudioBackend>();
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
        std::move(definitions), std::move(samples), settings,
        std::move(backend),
        std::move(degradedReason));
}

std::unique_ptr<AudioRuntime> AudioRuntime::createDummy(
    AudioDefinitionRegistry definitions, const UserSettings &settings,
    AudioSampleBank::PathResolver resolvePath)
{
    AudioSampleBank samples;
    std::string degradedReason = "dummy backend requested";
    if (!definitions.definitions().empty()) {
        if (!resolvePath) {
            resolvePath = resolveBaseAudioPath;
        }
        std::string sampleError;
        if (!samples.tryFreeze(definitions, resolvePath, sampleError)) {
            degradedReason = "audio samples are unavailable: " + sampleError;
        }
    }
    auto backend = std::make_unique<DummyAudioBackend>();
    std::string ignored;
    backend->initialize(ignored);
    return std::make_unique<AudioRuntime>(
        std::move(definitions), std::move(samples), settings,
        std::move(backend), std::move(degradedReason));
}

AudioRuntime::AudioRuntime(AudioDefinitionRegistry definitions,
                           AudioSampleBank samples,
                           const UserSettings &settings,
                           std::unique_ptr<IAudioBackend> backend,
                           std::string degradedReason)
    : m_definitions(std::move(definitions))
    , m_samples(std::move(samples))
    , m_settings(settings)
    , m_backend(std::move(backend))
    , m_degradedReason(std::move(degradedReason))
{
    m_backendStartedReal = m_backend != nullptr && m_backend->isReal();
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
            submit({"block.break", true, blockCenter(block.position),
                    nextFeedbackGain()});
        }, SandboxEventSubscriptionOptions::observer("AudioRuntime")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::BlockPlace, [this](const SandboxEvent &event) {
            const auto &block = static_cast<const BlockPlaceEvent &>(event);
            submit({"block.place", true, blockCenter(block.position),
                    nextFeedbackGain()});
        }, SandboxEventSubscriptionOptions::observer("AudioRuntime")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::ItemPickup, [this](const SandboxEvent &event) {
            const auto &pickup = static_cast<const ItemPickupEvent &>(event);
            submit({"item.pickup", true, pickup.position,
                    nextFeedbackGain()});
        }, SandboxEventSubscriptionOptions::observer("AudioRuntime")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::EntityDamage, [this](const SandboxEvent &event) {
            const auto &damage = static_cast<const EntityDamageEvent &>(event);
            submit({"combat.hit", true, damage.position,
                    nextFeedbackGain()});
        }, SandboxEventSubscriptionOptions::observer("AudioRuntime")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::CombatWindup, [this](const SandboxEvent &event) {
            const auto &windup =
                static_cast<const CombatWindupEvent &>(event);
            submit({"combat.windup", true, windup.position,
                    nextFeedbackGain()});
        }, SandboxEventSubscriptionOptions::observer("AudioRuntime")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::CombatGuard, [this](const SandboxEvent &event) {
            const auto &guard = static_cast<const CombatGuardEvent &>(event);
            submit({"combat.guard", true, guard.position,
                    nextFeedbackGain()});
        }, SandboxEventSubscriptionOptions::observer("AudioRuntime")));
    m_subscriptions.push_back(eventBus.subscribe(
        SandboxEventType::CraftCompleted,
        [this](const SandboxEvent &event) {
            const auto &craft =
                static_cast<const CraftCompletedEvent &>(event);
            submit({"craft.success", true, craft.position, 1.f});
        }, SandboxEventSubscriptionOptions::observer("AudioRuntime")));
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
    if (m_backend != nullptr) {
        m_backend->stopAll();
    }
    m_stats.activeVoices = 0;
}

float AudioRuntime::nextFeedbackGain() noexcept
{
    return ActionFeedbackTimeline::audioGainVariant(
        ++m_feedbackVariantEpoch);
}

void AudioRuntime::submit(AudioPlaybackEvent event) noexcept
{
    ++m_stats.submittedEvents;
    const AudioDefinition *definition = m_definitions.find(event.cueId);
    if (definition == nullptr) {
        ++m_stats.missingDefinitions;
        return;
    }
    if (definition->id.rfind("ambient.", 0) == 0) {
        ++m_stats.ambientEvents;
    }
    if (m_worldPaused && definition->category != AudioCategory::Ui) {
        ++m_stats.suppressedEvents;
        return;
    }
    if (event.caption && m_settings.audioCaptions && m_captionSink &&
        !definition->caption.empty()) {
        try {
            m_captionSink(definition->id, definition->caption);
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
    const AudioSampleData *sample = m_samples.find(definition->id);
    if (sample == nullptr) {
        ++m_stats.missingSamples;
        return;
    }
    if (m_backend == nullptr) {
        ++m_stats.backendFailures;
        return;
    }
    switch (m_backend->play(
        *definition, *sample, event, effectiveGain, m_listener)) {
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
    m_stats.activeVoices = m_backend->activeVoices();
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
        if (m_backendStartedReal && !m_backendFailureObserved &&
            !m_backend->isReal()) {
            ++m_stats.backendFailures;
            m_backendFailureObserved = true;
            m_degradedReason =
                "native audio backend failed during playback";
        }
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
    submit({"ambient.wind", false, glm::vec3(0.f), 1.f});
}

void AudioRuntime::setUserSettings(const UserSettings &settings) noexcept
{
    m_settings = settings;
}

void AudioRuntime::setCaptionSink(
    std::function<void(std::string, std::string)> sink) noexcept
{
    m_captionSink = std::move(sink);
}

void AudioRuntime::setWorldPaused(bool paused) noexcept
{
    if (paused && !m_worldPaused && m_backend != nullptr) {
        // Cut already-buffered world feedback at the pause boundary while
        // leaving the backend available for menu UI cues.
        m_backend->stopAll();
    }
    m_worldPaused = paused;
}

void AudioRuntime::setMuted(bool muted) noexcept
{
    if (muted && !m_muted && m_backend != nullptr) {
        m_backend->stopAll();
    }
    m_muted = muted;
}

void AudioRuntime::setSuspended(bool suspended) noexcept
{
    if (m_backend != nullptr) {
        m_backend->setPaused(suspended);
    }
}

void AudioRuntime::stopAllPlayback() noexcept
{
    if (m_backend != nullptr) {
        m_backend->stopAll();
    }
    m_stats.activeVoices = 0;
}

const AudioRuntimeStats &AudioRuntime::stats() const noexcept
{
    return m_stats;
}

const AudioDefinitionRegistry &AudioRuntime::definitions() const noexcept
{
    return m_definitions;
}

const AudioSampleBank &AudioRuntime::samples() const noexcept
{
    return m_samples;
}

const char *AudioRuntime::backendName() const noexcept
{
    return m_backend != nullptr ? m_backend->name() : "none";
}

bool AudioRuntime::usesRealBackend() const noexcept
{
    return m_backend != nullptr && m_backend->isReal();
}

bool AudioRuntime::captionsEnabled() const noexcept
{
    return m_settings.audioCaptions;
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
