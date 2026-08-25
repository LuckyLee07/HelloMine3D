#include "MusicStreamFile.h"

#include <array>
#include <fstream>
#include <stdexcept>

namespace {
constexpr std::size_t CanonicalWaveHeaderBytes = 44u;
constexpr std::size_t MaximumMusicFileBytes = 32u * 1024u * 1024u;
constexpr std::uint32_t RequiredSampleRate = 44100u;
constexpr std::uint16_t RequiredChannels = 1u;
constexpr std::uint16_t RequiredBitsPerSample = 16u;

std::uint16_t readU16(const std::array<unsigned char, 44> &bytes,
                      std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1u]) << 8u);
}

std::uint32_t readU32(const std::array<unsigned char, 44> &bytes,
                      std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

bool tagEquals(const std::array<unsigned char, 44> &bytes,
               std::size_t offset, const char *tag)
{
    for (std::size_t index = 0; index < 4u; ++index) {
        if (bytes[offset + index] !=
            static_cast<unsigned char>(tag[index])) {
            return false;
        }
    }
    return true;
}
} // namespace

MusicStreamFileInfo inspectMusicStreamFile(const std::string &path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Unable to open music stream file '" +
                                 path + "'.");
    }
    const std::streamoff size = input.tellg();
    if (size < static_cast<std::streamoff>(CanonicalWaveHeaderBytes) ||
        static_cast<std::uint64_t>(size) > MaximumMusicFileBytes) {
        throw std::runtime_error(
            "Music stream file size is outside 44..33554432 bytes.");
    }
    input.seekg(0, std::ios::beg);
    std::array<unsigned char, CanonicalWaveHeaderBytes> header{};
    input.read(reinterpret_cast<char *>(header.data()),
               static_cast<std::streamsize>(header.size()));
    if (!input) {
        throw std::runtime_error("Unable to read music WAVE header.");
    }

    const std::uint32_t declaredRiffBytes = readU32(header, 4u);
    const std::uint32_t formatBytes = readU32(header, 16u);
    const std::uint16_t format = readU16(header, 20u);
    const std::uint16_t channels = readU16(header, 22u);
    const std::uint32_t sampleRate = readU32(header, 24u);
    const std::uint32_t byteRate = readU32(header, 28u);
    const std::uint16_t blockAlign = readU16(header, 32u);
    const std::uint16_t bits = readU16(header, 34u);
    const std::uint32_t dataBytes = readU32(header, 40u);
    if (!tagEquals(header, 0u, "RIFF") ||
        !tagEquals(header, 8u, "WAVE") ||
        !tagEquals(header, 12u, "fmt ") ||
        !tagEquals(header, 36u, "data") || formatBytes != 16u ||
        format != 1u || channels != RequiredChannels ||
        sampleRate != RequiredSampleRate ||
        bits != RequiredBitsPerSample || blockAlign != 2u ||
        byteRate != sampleRate * blockAlign || dataBytes == 0u ||
        (dataBytes % blockAlign) != 0u ||
        declaredRiffBytes != static_cast<std::uint64_t>(size) - 8u ||
        dataBytes != static_cast<std::uint64_t>(size) -
                         CanonicalWaveHeaderBytes) {
        throw std::runtime_error(
            "Music stream must be a canonical 44100 Hz mono PCM16 WAVE.");
    }

    const std::uint64_t frames = dataBytes / blockAlign;
    const std::uint64_t durationMilliseconds =
        frames * 1000u / RequiredSampleRate;
    if (durationMilliseconds < 10000u ||
        durationMilliseconds > 180000u) {
        throw std::runtime_error(
            "Music stream duration is outside 10000..180000 ms.");
    }

    MusicStreamFileInfo info;
    info.path = path;
    info.fileBytes = static_cast<std::size_t>(size);
    info.dataOffset = CanonicalWaveHeaderBytes;
    info.dataBytes = dataBytes;
    info.sampleRate = sampleRate;
    info.channels = channels;
    info.bitsPerSample = bits;
    info.durationMilliseconds =
        static_cast<int>(durationMilliseconds);
    return info;
}
