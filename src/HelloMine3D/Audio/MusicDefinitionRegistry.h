#ifndef MUSICDEFINITIONREGISTRY_H_INCLUDED
#define MUSICDEFINITIONREGISTRY_H_INCLUDED

#include <cstddef>
#include <string>
#include <vector>

struct MusicTrackDefinition {
    std::string id;
    std::string streamPath;
    float gain = 1.f;
    int durationMilliseconds = 0;
    int fadeInMilliseconds = 0;
    int fadeOutMilliseconds = 0;
    int initialDelayMilliseconds = 0;
    int minimumGapMilliseconds = 0;
    int maximumGapMilliseconds = 0;
};

struct MusicDefinitionSource {
    std::string name;
    std::string content;
};

class MusicDefinitionRegistry {
  public:
    static constexpr int SupportedFormatVersion = 1;

    void freeze(const std::vector<MusicDefinitionSource> &sources);
    bool tryFreezeFromFile(const std::string &path,
                           std::string &error) noexcept;

    bool isFrozen() const noexcept;
    const MusicTrackDefinition *track() const noexcept;
    const std::vector<MusicTrackDefinition> &tracks() const noexcept;

  private:
    bool m_frozen = false;
    std::vector<MusicTrackDefinition> m_tracks;
};

#endif // MUSICDEFINITIONREGISTRY_H_INCLUDED
