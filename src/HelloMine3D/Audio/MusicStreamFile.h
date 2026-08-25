#ifndef MUSICSTREAMFILE_H_INCLUDED
#define MUSICSTREAMFILE_H_INCLUDED

#include <cstddef>
#include <cstdint>
#include <string>

struct MusicStreamFileInfo {
    std::string path;
    std::size_t fileBytes = 0;
    std::size_t dataOffset = 0;
    std::size_t dataBytes = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bitsPerSample = 0;
    int durationMilliseconds = 0;
};

MusicStreamFileInfo inspectMusicStreamFile(const std::string &path);

#endif // MUSICSTREAMFILE_H_INCLUDED
