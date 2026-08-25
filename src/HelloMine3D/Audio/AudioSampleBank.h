#ifndef AUDIOSAMPLEBANK_H_INCLUDED
#define AUDIOSAMPLEBANK_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AudioDefinitionRegistry.h"

struct AudioSampleData {
    int sampleRate = 0;
    std::vector<std::int16_t> monoSamples;
};

class AudioSampleBank {
  public:
    using PathResolver =
        std::function<std::string(const std::string &logicalPath)>;

    static constexpr int RequiredSampleRate = 44100;
    static constexpr int RequiredChannels = 1;
    static constexpr int RequiredBitsPerSample = 16;
    static constexpr int MinimumDurationMilliseconds = 10;
    static constexpr int MaximumDurationMilliseconds = 3000;
    static constexpr std::size_t MaximumUniqueSamples = 32;
    static constexpr std::size_t MaximumSampleFileBytes = 512u * 1024u;
    static constexpr std::size_t MaximumDecodedBytes = 4u * 1024u * 1024u;

    void freeze(const AudioDefinitionRegistry &definitions,
                const PathResolver &resolvePath);
    bool tryFreeze(const AudioDefinitionRegistry &definitions,
                   const PathResolver &resolvePath,
                   std::string &error) noexcept;

    bool isFrozen() const noexcept;
    const AudioSampleData *find(const std::string &cueId) const noexcept;
    std::size_t cueCount() const noexcept;
    std::size_t uniqueSampleCount() const noexcept;
    std::size_t decodedBytes() const noexcept;

  private:
    using SamplePtr = std::shared_ptr<const AudioSampleData>;

    std::unordered_map<std::string, SamplePtr> m_byCue;
    std::unordered_map<std::string, SamplePtr> m_byPath;
    std::size_t m_decodedBytes = 0;
    bool m_frozen = false;
};

#endif // AUDIOSAMPLEBANK_H_INCLUDED
