#ifndef AUDIODEFINITIONREGISTRY_H_INCLUDED
#define AUDIODEFINITIONREGISTRY_H_INCLUDED

#include <string>
#include <unordered_map>
#include <vector>

enum class AudioCategory {
    Ui,
    Effects,
    Ambient
};

enum class AudioWaveform {
    Sine,
    Square,
    Noise
};

struct AudioDefinition {
    std::string id;
    AudioCategory category = AudioCategory::Effects;
    bool spatial = false;
    AudioWaveform waveform = AudioWaveform::Sine;
    float frequency = 440.f;
    int durationMilliseconds = 100;
    float gain = 1.f;
    int maxVoices = 1;
};

struct AudioDefinitionSource {
    std::string name;
    std::string content;
};

const char *audioCategoryName(AudioCategory category) noexcept;
const char *audioWaveformName(AudioWaveform waveform) noexcept;

class AudioDefinitionRegistry {
  public:
    static constexpr int SupportedFormatVersion = 1;

    void freeze(const std::vector<AudioDefinitionSource> &sources);
    bool tryFreezeFromFile(const std::string &path,
                           std::string &error) noexcept;
    bool isFrozen() const noexcept;
    const AudioDefinition *find(const std::string &id) const noexcept;
    const std::vector<AudioDefinition> &definitions() const noexcept;

  private:
    std::vector<AudioDefinition> m_definitions;
    std::unordered_map<std::string, std::size_t> m_byId;
    bool m_frozen = false;
};

#endif // AUDIODEFINITIONREGISTRY_H_INCLUDED
