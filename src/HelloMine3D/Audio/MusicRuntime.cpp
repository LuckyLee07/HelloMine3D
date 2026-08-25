#include "MusicRuntime.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "../Util/ResourcePaths.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace {
constexpr float SilentGain = 0.0001f;

bool truthy(const char *value)
{
    if (value == nullptr) {
        return false;
    }
    const std::string text(value);
    return text == "1" || text == "true" || text == "TRUE" ||
           text == "on" || text == "ON";
}

std::string resolveBaseMusicPath(const std::string &logicalPath)
{
    return ResourcePaths::join(ResourcePaths::projectRoot(), logicalPath);
}

#if defined(_WIN32)
class WindowsMusicStreamBackend final : public IMusicStreamBackend {
  public:
    ~WindowsMusicStreamBackend() override
    {
        stop();
        std::lock_guard<std::mutex> lock(m_waveMutex);
        if (m_output != nullptr) {
            waveOutClose(m_output);
            m_output = nullptr;
        }
    }

    const char *name() const noexcept override
    {
        return "windows-waveout-stream";
    }

    bool isReal() const noexcept override
    {
        return m_output != nullptr;
    }

    bool initialize(std::string &error) noexcept override
    {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = 44100;
        format.wBitsPerSample = 16;
        format.nBlockAlign =
            static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8u);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        std::lock_guard<std::mutex> lock(m_waveMutex);
        const MMRESULT result = waveOutOpen(
            &m_output, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR) {
            m_output = nullptr;
            error = "waveOutOpen failed with code " +
                    std::to_string(static_cast<unsigned>(result));
            return false;
        }
        error.clear();
        return true;
    }

    bool start(const MusicStreamFileInfo &stream,
               std::string &error) noexcept override
    {
        stop();
        if (m_output == nullptr) {
            error = "waveOut stream device is unavailable";
            return false;
        }
        m_stopRequested.store(false);
        m_playing.store(true);
        m_paused.store(false);
        m_workerActive.store(true);
        m_streamedBytes.store(0u);
        setFailure({});
        try {
            m_worker = std::thread(
                [this, stream]() { workerMain(stream); });
        }
        catch (const std::exception &exception) {
            m_playing.store(false);
            m_workerActive.store(false);
            error = exception.what();
            return false;
        }
        catch (...) {
            m_playing.store(false);
            m_workerActive.store(false);
            error = "unable to create music stream worker";
            return false;
        }
        error.clear();
        return true;
    }

    void update(float) noexcept override
    {
        m_wake.notify_all();
    }

    void setGain(float gain) noexcept override
    {
        m_gain.store(std::clamp(gain, 0.f, 1.f));
    }

    void setPaused(bool paused) noexcept override
    {
        if (m_output == nullptr || m_paused.exchange(paused) == paused) {
            return;
        }
        std::lock_guard<std::mutex> lock(m_waveMutex);
        if (paused) {
            waveOutPause(m_output);
        }
        else {
            waveOutRestart(m_output);
        }
        m_wake.notify_all();
    }

    void stop() noexcept override
    {
        m_stopRequested.store(true);
        {
            std::lock_guard<std::mutex> lock(m_waveMutex);
            if (m_output != nullptr) {
                waveOutReset(m_output);
            }
        }
        m_wake.notify_all();
        if (m_worker.joinable()) {
            m_worker.join();
        }
        m_playing.store(false);
        m_paused.store(false);
        m_workerActive.store(false);
    }

    bool isPlaying() const noexcept override
    {
        return m_playing.load();
    }

    bool isPaused() const noexcept override
    {
        return m_paused.load();
    }

    bool workerActive() const noexcept override
    {
        return m_workerActive.load();
    }

    std::size_t streamedBytes() const noexcept override
    {
        return m_streamedBytes.load();
    }

    std::string failureReason() const noexcept override
    {
        std::lock_guard<std::mutex> lock(m_failureMutex);
        return m_failureReason;
    }

  private:
    static constexpr std::size_t BufferCount = 3u;
    static constexpr std::size_t FramesPerBuffer = 4096u;

    struct StreamBuffer {
        std::vector<std::int16_t> samples;
        WAVEHDR header{};
        bool prepared = false;
    };

    void setFailure(std::string reason) noexcept
    {
        std::lock_guard<std::mutex> lock(m_failureMutex);
        m_failureReason = std::move(reason);
    }

    void workerMain(const MusicStreamFileInfo &stream) noexcept
    {
        std::vector<StreamBuffer> buffers(BufferCount);
        try {
            std::ifstream input(stream.path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("unable to open music stream data");
            }
            input.seekg(static_cast<std::streamoff>(stream.dataOffset),
                        std::ios::beg);
            std::size_t remaining = stream.dataBytes;
            std::vector<std::int16_t> mono(FramesPerBuffer);

            while (!m_stopRequested.load()) {
                bool submitted = false;
                bool pending = false;
                for (StreamBuffer &buffer : buffers) {
                    if (buffer.prepared &&
                        (buffer.header.dwFlags & WHDR_DONE) != 0u) {
                        std::lock_guard<std::mutex> lock(m_waveMutex);
                        waveOutUnprepareHeader(
                            m_output, &buffer.header,
                            static_cast<UINT>(sizeof(buffer.header)));
                        buffer.prepared = false;
                    }
                    if (buffer.prepared) {
                        pending = true;
                        continue;
                    }
                    if (remaining == 0u || m_stopRequested.load()) {
                        continue;
                    }

                    const std::size_t sourceBytes = std::min(
                        remaining, FramesPerBuffer * sizeof(std::int16_t));
                    input.read(reinterpret_cast<char *>(mono.data()),
                               static_cast<std::streamsize>(sourceBytes));
                    if (input.gcount() !=
                        static_cast<std::streamsize>(sourceBytes)) {
                        throw std::runtime_error(
                            "music stream ended before its declared data");
                    }
                    const std::size_t frames =
                        sourceBytes / sizeof(std::int16_t);
                    buffer.samples.resize(frames * 2u);
                    const float gain = m_gain.load();
                    for (std::size_t frame = 0; frame < frames; ++frame) {
                        const float scaled =
                            static_cast<float>(mono[frame]) * gain;
                        const auto sample = static_cast<std::int16_t>(
                            std::clamp(scaled, -32768.f, 32767.f));
                        buffer.samples[frame * 2u] = sample;
                        buffer.samples[frame * 2u + 1u] = sample;
                    }
                    buffer.header = {};
                    buffer.header.lpData = reinterpret_cast<LPSTR>(
                        buffer.samples.data());
                    buffer.header.dwBufferLength = static_cast<DWORD>(
                        buffer.samples.size() * sizeof(std::int16_t));
                    {
                        std::lock_guard<std::mutex> lock(m_waveMutex);
                        if (m_stopRequested.load()) {
                            break;
                        }
                        const MMRESULT prepared = waveOutPrepareHeader(
                            m_output, &buffer.header,
                            static_cast<UINT>(sizeof(buffer.header)));
                        if (prepared != MMSYSERR_NOERROR) {
                            throw std::runtime_error(
                                "waveOutPrepareHeader failed");
                        }
                        buffer.prepared = true;
                        const MMRESULT written = waveOutWrite(
                            m_output, &buffer.header,
                            static_cast<UINT>(sizeof(buffer.header)));
                        if (written != MMSYSERR_NOERROR) {
                            waveOutUnprepareHeader(
                                m_output, &buffer.header,
                                static_cast<UINT>(sizeof(buffer.header)));
                            buffer.prepared = false;
                            throw std::runtime_error("waveOutWrite failed");
                        }
                    }
                    remaining -= sourceBytes;
                    m_streamedBytes.fetch_add(sourceBytes);
                    submitted = true;
                    pending = true;
                }

                if (remaining == 0u && !pending) {
                    break;
                }
                if (!submitted) {
                    std::unique_lock<std::mutex> waitLock(m_waitMutex);
                    m_wake.wait_for(waitLock, std::chrono::milliseconds(5));
                }
            }
        }
        catch (const std::exception &exception) {
            setFailure(exception.what());
        }
        catch (...) {
            setFailure("unknown music stream worker failure");
        }

        {
            std::lock_guard<std::mutex> lock(m_waveMutex);
            if (m_output != nullptr) {
                waveOutReset(m_output);
                for (StreamBuffer &buffer : buffers) {
                    if (buffer.prepared) {
                        waveOutUnprepareHeader(
                            m_output, &buffer.header,
                            static_cast<UINT>(sizeof(buffer.header)));
                        buffer.prepared = false;
                    }
                }
            }
        }
        m_playing.store(false);
        m_workerActive.store(false);
    }

    HWAVEOUT m_output = nullptr;
    std::thread m_worker;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_workerActive{false};
    std::atomic<float> m_gain{0.f};
    std::atomic<std::size_t> m_streamedBytes{0u};
    mutable std::mutex m_failureMutex;
    std::string m_failureReason;
    std::mutex m_waveMutex;
    std::mutex m_waitMutex;
    std::condition_variable m_wake;
};
#endif
} // namespace

const char *musicPlaybackStateName(MusicPlaybackState state) noexcept
{
    switch (state) {
    case MusicPlaybackState::Stopped: return "stopped";
    case MusicPlaybackState::Waiting: return "waiting";
    case MusicPlaybackState::FadingIn: return "fading-in";
    case MusicPlaybackState::Playing: return "playing";
    case MusicPlaybackState::FadingOut: return "fading-out";
    case MusicPlaybackState::Paused: return "paused";
    case MusicPlaybackState::Degraded: return "degraded";
    }
    return "unknown";
}

const char *DummyMusicStreamBackend::name() const noexcept
{
    return "dummy-music-stream";
}

bool DummyMusicStreamBackend::isReal() const noexcept
{
    return false;
}

bool DummyMusicStreamBackend::initialize(std::string &error) noexcept
{
    error.clear();
    return true;
}

bool DummyMusicStreamBackend::start(
    const MusicStreamFileInfo &stream, std::string &error) noexcept
{
    m_stream = stream;
    m_elapsedSeconds = 0.f;
    m_streamedBytes = 0u;
    m_playing = true;
    m_paused = false;
    ++m_starts;
    error.clear();
    return true;
}

void DummyMusicStreamBackend::update(float deltaSeconds) noexcept
{
    if (!m_playing || m_paused) {
        return;
    }
    m_elapsedSeconds += std::max(0.f, deltaSeconds);
    const float duration =
        static_cast<float>(m_stream.durationMilliseconds) / 1000.f;
    if (duration > 0.f) {
        const float progress = std::clamp(m_elapsedSeconds / duration,
                                          0.f, 1.f);
        m_streamedBytes = static_cast<std::size_t>(
            static_cast<double>(m_stream.dataBytes) * progress);
    }
    if (duration <= 0.f || m_elapsedSeconds >= duration) {
        m_streamedBytes = m_stream.dataBytes;
        m_playing = false;
        m_paused = false;
    }
}

void DummyMusicStreamBackend::setGain(float gain) noexcept
{
    m_gain = std::clamp(gain, 0.f, 1.f);
}

void DummyMusicStreamBackend::setPaused(bool paused) noexcept
{
    if (m_playing) {
        m_paused = paused;
    }
}

void DummyMusicStreamBackend::stop() noexcept
{
    if (m_playing || m_paused) {
        ++m_stops;
    }
    m_playing = false;
    m_paused = false;
}

bool DummyMusicStreamBackend::isPlaying() const noexcept
{
    return m_playing;
}

bool DummyMusicStreamBackend::isPaused() const noexcept
{
    return m_paused;
}

bool DummyMusicStreamBackend::workerActive() const noexcept
{
    return false;
}

std::size_t DummyMusicStreamBackend::streamedBytes() const noexcept
{
    return m_streamedBytes;
}

std::string DummyMusicStreamBackend::failureReason() const noexcept
{
    return {};
}

float DummyMusicStreamBackend::gain() const noexcept
{
    return m_gain;
}

std::size_t DummyMusicStreamBackend::starts() const noexcept
{
    return m_starts;
}

std::size_t DummyMusicStreamBackend::stops() const noexcept
{
    return m_stops;
}

std::unique_ptr<MusicRuntime> MusicRuntime::create(
    MusicDefinitionRegistry definitions, const UserSettings &settings,
    PathResolver resolvePath)
{
    if (!resolvePath) {
        resolvePath = resolveBaseMusicPath;
    }
    MusicStreamFileInfo stream;
    bool streamAvailable = false;
    std::string degradedReason;
    const MusicTrackDefinition *track = definitions.track();
    if (track != nullptr) {
        try {
            stream = inspectMusicStreamFile(resolvePath(track->streamPath));
            if (stream.durationMilliseconds != track->durationMilliseconds) {
                throw std::runtime_error(
                    "music stream duration does not match its definition");
            }
            streamAvailable = true;
        }
        catch (const std::exception &exception) {
            degradedReason = exception.what();
        }
    }
    else {
        degradedReason = "music definitions are unavailable";
    }

    std::unique_ptr<IMusicStreamBackend> backend;
    const char *requestedValue = std::getenv("HELLOMINE3D_MUSIC_BACKEND");
    const std::string requested = requestedValue != nullptr
                                      ? requestedValue
                                      : "auto";
    if (!streamAvailable || requested == "dummy" ||
        truthy(std::getenv("HELLOMINE3D_DISABLE_AUDIO")) ||
        (std::getenv("HELLOMINE3D_AUDIO_BACKEND") != nullptr &&
         std::string(std::getenv("HELLOMINE3D_AUDIO_BACKEND")) == "dummy")) {
        backend = std::make_unique<DummyMusicStreamBackend>();
        if (streamAvailable && degradedReason.empty()) {
            degradedReason = "dummy music backend requested";
        }
    }
    else {
#if defined(_WIN32)
        backend = std::make_unique<WindowsMusicStreamBackend>();
#else
        backend = std::make_unique<DummyMusicStreamBackend>();
        degradedReason = "music streaming backend is not implemented";
#endif
    }
    std::string backendError;
    if (!backend->initialize(backendError)) {
        degradedReason = "music backend unavailable: " + backendError;
        backend = std::make_unique<DummyMusicStreamBackend>();
        std::string ignored;
        backend->initialize(ignored);
    }
    return std::make_unique<MusicRuntime>(
        std::move(definitions), std::move(stream), streamAvailable,
        settings, std::move(backend), std::move(degradedReason));
}

std::unique_ptr<MusicRuntime> MusicRuntime::createDummy(
    MusicDefinitionRegistry definitions, const UserSettings &settings,
    PathResolver resolvePath)
{
    if (!resolvePath) {
        resolvePath = resolveBaseMusicPath;
    }
    MusicStreamFileInfo stream;
    bool streamAvailable = false;
    std::string degradedReason = "dummy music backend requested";
    const MusicTrackDefinition *track = definitions.track();
    if (track != nullptr) {
        try {
            stream = inspectMusicStreamFile(resolvePath(track->streamPath));
            if (stream.durationMilliseconds != track->durationMilliseconds) {
                throw std::runtime_error(
                    "music stream duration does not match its definition");
            }
            streamAvailable = true;
        }
        catch (const std::exception &exception) {
            degradedReason = exception.what();
        }
    }
    auto backend = std::make_unique<DummyMusicStreamBackend>();
    std::string ignored;
    backend->initialize(ignored);
    return std::make_unique<MusicRuntime>(
        std::move(definitions), std::move(stream), streamAvailable,
        settings, std::move(backend), std::move(degradedReason));
}

MusicRuntime::MusicRuntime(
    MusicDefinitionRegistry definitions, MusicStreamFileInfo stream,
    bool streamAvailable, const UserSettings &settings,
    std::unique_ptr<IMusicStreamBackend> backend,
    std::string degradedReason)
    : m_definitions(std::move(definitions))
    , m_stream(std::move(stream))
    , m_streamAvailable(streamAvailable)
    , m_settings(settings)
    , m_backend(std::move(backend))
    , m_degradedReason(std::move(degradedReason))
{
    const MusicTrackDefinition *track = m_definitions.track();
    m_waitSeconds = track != nullptr
                        ? static_cast<float>(track->initialDelayMilliseconds) /
                              1000.f
                        : 0.f;
    m_state = m_streamAvailable ? MusicPlaybackState::Stopped
                                : MusicPlaybackState::Degraded;
}

MusicRuntime::~MusicRuntime()
{
    stopImmediately();
}

void MusicRuntime::update(float deltaSeconds, bool worldActive,
                          bool worldPaused) noexcept
{
    const float delta = std::max(0.f, deltaSeconds);
    if (m_backend != nullptr) {
        m_backend->update(delta);
        m_stats.streamedBytes = m_backend->streamedBytes();
        m_stats.workerActive = m_backend->workerActive();
        const std::string failure = m_backend->failureReason();
        if (!failure.empty()) {
            degrade("music stream failed: " + failure);
            return;
        }
    }
    if (!m_streamAvailable || m_backend == nullptr ||
        m_state == MusicPlaybackState::Degraded) {
        return;
    }
    if (m_suspended) {
        return;
    }

    if (!worldActive || m_muted || targetGain() <= SilentGain) {
        if (m_backend->isPlaying()) {
            updateFadeOut(delta, targetGain(), FadeDestination::Stop);
        }
        else if (m_state != MusicPlaybackState::Stopped) {
            completeStop(true);
        }
        m_lastWorldActive = worldActive;
        return;
    }

    if (!m_lastWorldActive) {
        const MusicTrackDefinition *track = m_definitions.track();
        m_waitSeconds = truthy(std::getenv(
                            "HELLOMINE3D_MUSIC_IMMEDIATE"))
                            ? 0.f
                            : track != nullptr
                            ? static_cast<float>(
                                  track->initialDelayMilliseconds) /
                                  1000.f
                            : 0.f;
        m_state = MusicPlaybackState::Waiting;
    }
    m_lastWorldActive = true;

    if (worldPaused) {
        if (m_backend->isPlaying() && !m_backend->isPaused()) {
            updateFadeOut(delta, targetGain(), FadeDestination::Pause);
        }
        else {
            m_state = MusicPlaybackState::Paused;
        }
        return;
    }

    if (m_backend->isPaused()) {
        m_backend->setPaused(false);
        m_currentGain = 0.f;
        m_backend->setGain(0.f);
        m_state = MusicPlaybackState::FadingIn;
        m_fadeDestination = FadeDestination::None;
    }
    updateActive(delta, targetGain());
}

void MusicRuntime::setUserSettings(const UserSettings &settings) noexcept
{
    m_settings = settings;
}

void MusicRuntime::setMuted(bool muted) noexcept
{
    m_muted = muted;
}

void MusicRuntime::setSuspended(bool suspended) noexcept
{
    if (m_suspended == suspended) {
        return;
    }
    m_suspended = suspended;
    if (m_backend != nullptr && m_backend->isPlaying()) {
        m_backend->setPaused(
            suspended || m_state == MusicPlaybackState::Paused);
    }
}

void MusicRuntime::stopImmediately() noexcept
{
    if (m_backend != nullptr) {
        m_backend->setGain(0.f);
        m_backend->stop();
    }
    m_currentGain = 0.f;
    m_fadeStartGain = 0.f;
    m_fadeDestination = FadeDestination::None;
    m_stats.workerActive = false;
    m_state = m_streamAvailable ? MusicPlaybackState::Stopped
                                : MusicPlaybackState::Degraded;
}

const MusicDefinitionRegistry &MusicRuntime::definitions() const noexcept
{
    return m_definitions;
}

const MusicStreamFileInfo &MusicRuntime::stream() const noexcept
{
    return m_stream;
}

bool MusicRuntime::streamAvailable() const noexcept
{
    return m_streamAvailable;
}

MusicPlaybackState MusicRuntime::state() const noexcept
{
    return m_state;
}

const MusicRuntimeStats &MusicRuntime::stats() const noexcept
{
    return m_stats;
}

const char *MusicRuntime::backendName() const noexcept
{
    return m_backend != nullptr ? m_backend->name() : "none";
}

bool MusicRuntime::usesRealBackend() const noexcept
{
    return m_backend != nullptr && m_backend->isReal();
}

const std::string &MusicRuntime::degradedReason() const noexcept
{
    return m_degradedReason;
}

bool MusicRuntime::startTrack() noexcept
{
    if (m_backend == nullptr || !m_streamAvailable) {
        return false;
    }
    std::string error;
    m_backend->setGain(0.f);
    if (!m_backend->start(m_stream, error)) {
        degrade("unable to start music stream: " + error);
        return false;
    }
    m_currentGain = 0.f;
    m_fadeStartGain = 0.f;
    m_state = MusicPlaybackState::FadingIn;
    m_fadeDestination = FadeDestination::None;
    ++m_stats.playsStarted;
    return true;
}

void MusicRuntime::updateActive(float deltaSeconds,
                                float desiredGain) noexcept
{
    if (!m_backend->isPlaying()) {
        if (m_state == MusicPlaybackState::FadingIn ||
            m_state == MusicPlaybackState::Playing ||
            m_state == MusicPlaybackState::FadingOut) {
            ++m_stats.playsCompleted;
            m_waitSeconds = nextGapSeconds();
            m_state = MusicPlaybackState::Waiting;
            m_currentGain = 0.f;
        }
        if (m_state == MusicPlaybackState::Paused) {
            m_state = MusicPlaybackState::Waiting;
        }
        if (m_state == MusicPlaybackState::Stopped) {
            const MusicTrackDefinition *track = m_definitions.track();
            m_waitSeconds = track != nullptr
                                ? static_cast<float>(
                                      track->initialDelayMilliseconds) /
                                      1000.f
                                : 0.f;
            m_state = MusicPlaybackState::Waiting;
        }
        m_waitSeconds = std::max(0.f, m_waitSeconds - deltaSeconds);
        if (m_waitSeconds <= 0.f) {
            startTrack();
        }
        return;
    }

    if (m_state == MusicPlaybackState::FadingOut) {
        m_state = MusicPlaybackState::FadingIn;
        m_fadeDestination = FadeDestination::None;
        m_fadeStartGain = 0.f;
    }
    const MusicTrackDefinition *track = m_definitions.track();
    const float fadeSeconds = track != nullptr
                                  ? std::max(0.1f,
                                      static_cast<float>(
                                          track->fadeInMilliseconds) /
                                          1000.f)
                                  : 0.1f;
    const float step = desiredGain * deltaSeconds / fadeSeconds;
    m_currentGain = std::min(desiredGain, m_currentGain + step);
    m_backend->setGain(m_currentGain);
    m_state = m_currentGain + SilentGain >= desiredGain
                  ? MusicPlaybackState::Playing
                  : MusicPlaybackState::FadingIn;
}

void MusicRuntime::updateFadeOut(
    float deltaSeconds, float desiredGain,
    FadeDestination destination) noexcept
{
    if (m_state != MusicPlaybackState::FadingOut ||
        m_fadeDestination != destination) {
        m_fadeStartGain = std::max(desiredGain, m_currentGain);
    }
    m_state = MusicPlaybackState::FadingOut;
    m_fadeDestination = destination;
    const MusicTrackDefinition *track = m_definitions.track();
    const float fadeSeconds = track != nullptr
                                  ? std::max(0.1f,
                                      static_cast<float>(
                                          track->fadeOutMilliseconds) /
                                          1000.f)
                                  : 0.1f;
    m_currentGain = std::max(
        0.f, m_currentGain - m_fadeStartGain * deltaSeconds / fadeSeconds);
    m_backend->setGain(m_currentGain);
    if (m_currentGain > SilentGain) {
        return;
    }
    m_currentGain = 0.f;
    m_backend->setGain(0.f);
    if (destination == FadeDestination::Pause) {
        m_backend->setPaused(true);
        m_state = MusicPlaybackState::Paused;
        ++m_stats.pauses;
    }
    else {
        completeStop(true);
    }
    m_fadeDestination = FadeDestination::None;
    m_fadeStartGain = 0.f;
}

void MusicRuntime::completeStop(bool resetDelay) noexcept
{
    const bool wasActive = m_backend != nullptr &&
                           (m_backend->isPlaying() ||
                            m_backend->isPaused());
    if (m_backend != nullptr) {
        m_backend->stop();
    }
    if (wasActive) {
        ++m_stats.stops;
    }
    m_currentGain = 0.f;
    m_fadeStartGain = 0.f;
    m_state = MusicPlaybackState::Stopped;
    m_fadeDestination = FadeDestination::None;
    if (resetDelay) {
        const MusicTrackDefinition *track = m_definitions.track();
        m_waitSeconds = track != nullptr
                            ? static_cast<float>(
                                  track->initialDelayMilliseconds) /
                                  1000.f
                            : 0.f;
    }
}

void MusicRuntime::degrade(const std::string &reason) noexcept
{
    if (m_backend != nullptr) {
        m_backend->stop();
    }
    m_degradedReason = reason;
    m_state = MusicPlaybackState::Degraded;
    m_streamAvailable = false;
    m_currentGain = 0.f;
    m_fadeStartGain = 0.f;
    m_fadeDestination = FadeDestination::None;
    m_stats.workerActive = false;
    ++m_stats.backendFailures;
}

float MusicRuntime::targetGain() const noexcept
{
    const MusicTrackDefinition *track = m_definitions.track();
    if (track == nullptr) {
        return 0.f;
    }
    return std::clamp(m_settings.masterVolume, 0.f, 1.f) *
           std::clamp(m_settings.musicVolume, 0.f, 1.f) *
           std::clamp(track->gain, 0.f, 1.f);
}

float MusicRuntime::nextGapSeconds() noexcept
{
    const MusicTrackDefinition *track = m_definitions.track();
    if (track == nullptr) {
        return 0.f;
    }
    const int range = track->maximumGapMilliseconds -
                      track->minimumGapMilliseconds;
    const std::size_t sequence = m_gapSequence++;
    const int offset = range > 0
                           ? static_cast<int>((sequence * 7919u) %
                                              static_cast<std::size_t>(
                                                  range + 1))
                           : 0;
    return static_cast<float>(track->minimumGapMilliseconds + offset) /
           1000.f;
}
