#include "AudioSampleBank.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace {
std::uint16_t readU16(const std::vector<unsigned char> &bytes,
                      std::size_t offset)
{
    if (offset + 2u > bytes.size()) {
        throw std::runtime_error("truncated 16-bit WAVE field");
    }
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1u]) << 8u);
}

std::uint32_t readU32(const std::vector<unsigned char> &bytes,
                      std::size_t offset)
{
    if (offset + 4u > bytes.size()) {
        throw std::runtime_error("truncated 32-bit WAVE field");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
}

bool tagEquals(const std::vector<unsigned char> &bytes,
               std::size_t offset, const char *tag)
{
    return offset + 4u <= bytes.size() &&
           std::equal(tag, tag + 4, bytes.begin() + offset);
}

AudioSampleData loadWave(const std::string &path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("unable to open sample file");
    }
    const std::streamoff fileSize = input.tellg();
    if (fileSize < 44 ||
        static_cast<std::uint64_t>(fileSize) >
            AudioSampleBank::MaximumSampleFileBytes) {
        throw std::runtime_error("sample file size is outside 44..524288 bytes");
    }
    input.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(
        static_cast<std::size_t>(fileSize));
    input.read(reinterpret_cast<char *>(bytes.data()), fileSize);
    if (!input) {
        throw std::runtime_error("unable to read complete sample file");
    }
    if (!tagEquals(bytes, 0, "RIFF") || !tagEquals(bytes, 8, "WAVE") ||
        readU32(bytes, 4) != bytes.size() - 8u) {
        throw std::runtime_error("invalid RIFF/WAVE header or declared size");
    }

    bool formatSeen = false;
    bool dataSeen = false;
    std::size_t dataOffset = 0;
    std::size_t dataBytes = 0;
    std::size_t offset = 12;
    while (offset < bytes.size()) {
        if (offset + 8u > bytes.size()) {
            throw std::runtime_error("truncated WAVE chunk header");
        }
        const std::uint32_t chunkBytes = readU32(bytes, offset + 4u);
        const std::size_t chunkOffset = offset + 8u;
        if (chunkBytes > bytes.size() - chunkOffset) {
            throw std::runtime_error("WAVE chunk escapes the file boundary");
        }
        if (tagEquals(bytes, offset, "fmt ")) {
            if (formatSeen || chunkBytes != 16u) {
                throw std::runtime_error(
                    "WAVE must contain one canonical 16-byte fmt chunk");
            }
            formatSeen = true;
            const std::uint16_t format = readU16(bytes, chunkOffset);
            const std::uint16_t channels = readU16(bytes, chunkOffset + 2u);
            const std::uint32_t sampleRate = readU32(bytes, chunkOffset + 4u);
            const std::uint32_t byteRate = readU32(bytes, chunkOffset + 8u);
            const std::uint16_t blockAlign =
                readU16(bytes, chunkOffset + 12u);
            const std::uint16_t bits = readU16(bytes, chunkOffset + 14u);
            if (format != 1u ||
                channels != AudioSampleBank::RequiredChannels ||
                sampleRate != AudioSampleBank::RequiredSampleRate ||
                bits != AudioSampleBank::RequiredBitsPerSample ||
                blockAlign != 2u || byteRate != sampleRate * blockAlign) {
                throw std::runtime_error(
                    "WAVE must be 44100 Hz mono PCM16 with canonical rates");
            }
        }
        else if (tagEquals(bytes, offset, "data")) {
            if (dataSeen || chunkBytes == 0u || (chunkBytes % 2u) != 0u) {
                throw std::runtime_error(
                    "WAVE must contain one non-empty aligned data chunk");
            }
            dataSeen = true;
            dataOffset = chunkOffset;
            dataBytes = chunkBytes;
        }
        offset = chunkOffset + chunkBytes;
        if ((chunkBytes & 1u) != 0u) {
            if (offset >= bytes.size()) {
                throw std::runtime_error("missing WAVE chunk padding byte");
            }
            ++offset;
        }
    }
    if (!formatSeen || !dataSeen) {
        throw std::runtime_error("WAVE is missing fmt or data chunk");
    }

    const std::size_t frames = dataBytes / 2u;
    const std::size_t minimumFrames =
        static_cast<std::size_t>(AudioSampleBank::RequiredSampleRate) *
        AudioSampleBank::MinimumDurationMilliseconds / 1000u;
    const std::size_t maximumFrames =
        static_cast<std::size_t>(AudioSampleBank::RequiredSampleRate) *
        AudioSampleBank::MaximumDurationMilliseconds / 1000u;
    if (frames < minimumFrames || frames > maximumFrames) {
        throw std::runtime_error("sample duration is outside 10..3000 ms");
    }

    AudioSampleData result;
    result.sampleRate = AudioSampleBank::RequiredSampleRate;
    result.monoSamples.resize(frames);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        result.monoSamples[frame] = static_cast<std::int16_t>(
            readU16(bytes, dataOffset + frame * 2u));
    }
    return result;
}
} // namespace

void AudioSampleBank::freeze(const AudioDefinitionRegistry &definitions,
                             const PathResolver &resolvePath)
{
    if (m_frozen) {
        throw std::runtime_error("Audio sample bank is already frozen.");
    }
    if (!definitions.isFrozen() || definitions.definitions().empty()) {
        throw std::runtime_error(
            "Audio sample bank requires frozen non-empty definitions.");
    }
    if (!resolvePath) {
        throw std::runtime_error("Audio sample path resolver is unavailable.");
    }

    std::unordered_map<std::string, SamplePtr> byCue;
    std::unordered_map<std::string, SamplePtr> byPath;
    std::size_t decodedBytes = 0;
    for (const AudioDefinition &definition : definitions.definitions()) {
        SamplePtr sample;
        const auto cached = byPath.find(definition.samplePath);
        if (cached != byPath.end()) {
            sample = cached->second;
        }
        else {
            if (byPath.size() >= MaximumUniqueSamples) {
                throw std::runtime_error(
                    "Audio sample bank exceeds the 32-sample cache limit.");
            }
            const std::string physicalPath = resolvePath(definition.samplePath);
            try {
                auto loaded = std::make_shared<AudioSampleData>(
                    loadWave(physicalPath));
                const std::size_t bytes =
                    loaded->monoSamples.size() * sizeof(std::int16_t);
                if (bytes > MaximumDecodedBytes - decodedBytes) {
                    throw std::runtime_error(
                        "decoded cache exceeds the 4 MiB limit");
                }
                decodedBytes += bytes;
                sample = std::move(loaded);
                byPath.emplace(definition.samplePath, sample);
            }
            catch (const std::exception &error) {
                throw std::runtime_error(
                    "Unable to load audio sample '" + definition.samplePath +
                    "' for cue '" + definition.id + "' from '" +
                    physicalPath + "': " + error.what());
            }
        }
        byCue.emplace(definition.id, std::move(sample));
    }
    m_byCue = std::move(byCue);
    m_byPath = std::move(byPath);
    m_decodedBytes = decodedBytes;
    m_frozen = true;
}

bool AudioSampleBank::tryFreeze(const AudioDefinitionRegistry &definitions,
                                const PathResolver &resolvePath,
                                std::string &error) noexcept
{
    try {
        freeze(definitions, resolvePath);
        error.clear();
        return true;
    }
    catch (const std::exception &exception) {
        m_byCue.clear();
        m_byPath.clear();
        m_decodedBytes = 0;
        m_frozen = true;
        error = exception.what();
        return false;
    }
}

bool AudioSampleBank::isFrozen() const noexcept
{
    return m_frozen;
}

const AudioSampleData *AudioSampleBank::find(
    const std::string &cueId) const noexcept
{
    const auto found = m_byCue.find(cueId);
    return found != m_byCue.end() ? found->second.get() : nullptr;
}

std::size_t AudioSampleBank::cueCount() const noexcept
{
    return m_byCue.size();
}

std::size_t AudioSampleBank::uniqueSampleCount() const noexcept
{
    return m_byPath.size();
}

std::size_t AudioSampleBank::decodedBytes() const noexcept
{
    return m_decodedBytes;
}
