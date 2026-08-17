#include "AudioDefinitionRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
constexpr const char *AudioHeader =
    "# HelloMine3D audio definitions v1";

std::string trim(const std::string &value)
{
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool validId(const std::string &id)
{
    if (id.empty() || id.size() > 80 || id.front() == '.' ||
        id.back() == '.') {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](unsigned char value) {
        return std::isalnum(value) || value == '_' || value == '-' ||
               value == '.';
    });
}

[[noreturn]] void fail(const AudioDefinitionSource &source,
                       std::size_t line, const std::string &detail)
{
    throw std::runtime_error("Invalid audio definitions '" + source.name +
                             "' at line " + std::to_string(line) +
                             ": " + detail + ".");
}

AudioCategory parseCategory(const AudioDefinitionSource &source,
                            std::size_t line, const std::string &value)
{
    if (value == "ui") {
        return AudioCategory::Ui;
    }
    if (value == "effects") {
        return AudioCategory::Effects;
    }
    if (value == "ambient") {
        return AudioCategory::Ambient;
    }
    fail(source, line, "unknown category '" + value + "'");
}

AudioWaveform parseWaveform(const AudioDefinitionSource &source,
                            std::size_t line, const std::string &value)
{
    if (value == "sine") {
        return AudioWaveform::Sine;
    }
    if (value == "square") {
        return AudioWaveform::Square;
    }
    if (value == "noise") {
        return AudioWaveform::Noise;
    }
    fail(source, line, "unknown waveform '" + value + "'");
}

void requireCompleteCueSet(
    const std::unordered_map<std::string, std::size_t> &byId)
{
    static const std::vector<std::string> required = {
        "ui.click", "block.break", "block.place", "item.pickup",
        "craft.success", "combat.hit", "ambient.wind"};
    for (const std::string &id : required) {
        if (byId.find(id) == byId.end()) {
            throw std::runtime_error(
                "Audio definitions are missing required cue '" + id +
                "'.");
        }
    }
}
} // namespace

const char *audioCategoryName(AudioCategory category) noexcept
{
    switch (category) {
    case AudioCategory::Ui:
        return "ui";
    case AudioCategory::Effects:
        return "effects";
    case AudioCategory::Ambient:
        return "ambient";
    }
    return "unknown";
}

const char *audioWaveformName(AudioWaveform waveform) noexcept
{
    switch (waveform) {
    case AudioWaveform::Sine:
        return "sine";
    case AudioWaveform::Square:
        return "square";
    case AudioWaveform::Noise:
        return "noise";
    }
    return "unknown";
}

void AudioDefinitionRegistry::freeze(
    const std::vector<AudioDefinitionSource> &sources)
{
    if (m_frozen) {
        throw std::runtime_error(
            "Audio definition registry is already frozen.");
    }
    if (sources.empty()) {
        throw std::runtime_error("No audio definition sources were provided.");
    }

    std::vector<AudioDefinition> parsed;
    std::unordered_map<std::string, std::size_t> byId;
    for (const AudioDefinitionSource &source : sources) {
        std::istringstream input(source.content);
        std::string line;
        std::size_t lineNumber = 0;
        bool headerSeen = false;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (lineNumber == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf) {
                line.erase(0, 3);
            }
            if (!headerSeen) {
                if (trim(line) != AudioHeader) {
                    fail(source, lineNumber,
                         "unsupported or missing version header");
                }
                headerSeen = true;
                continue;
            }

            const std::size_t comment = line.find('#');
            if (comment != std::string::npos) {
                line.erase(comment);
            }
            line = trim(line);
            if (line.empty()) {
                continue;
            }

            std::istringstream values(line);
            std::string directive;
            AudioDefinition definition;
            std::string category;
            std::string mode;
            std::string waveform;
            if (!(values >> directive >> definition.id >> category >> mode >>
                  waveform >> definition.frequency >>
                  definition.durationMilliseconds >> definition.gain >>
                  definition.maxVoices) ||
                directive != "sound") {
                fail(source, lineNumber,
                     "expected sound id category 2d|3d waveform frequency duration_ms gain max_voices");
            }
            values >> std::ws;
            if (!values.eof()) {
                fail(source, lineNumber, "contains trailing data");
            }
            if (!validId(definition.id)) {
                fail(source, lineNumber, "invalid cue id '" +
                                             definition.id + "'");
            }
            definition.category =
                parseCategory(source, lineNumber, category);
            if (mode != "2d" && mode != "3d") {
                fail(source, lineNumber, "mode must be 2d or 3d");
            }
            definition.spatial = mode == "3d";
            definition.waveform =
                parseWaveform(source, lineNumber, waveform);
            if (!std::isfinite(definition.frequency) ||
                definition.frequency < 20.f ||
                definition.frequency > 20000.f) {
                fail(source, lineNumber,
                     "frequency must be between 20 and 20000 Hz");
            }
            if (definition.durationMilliseconds < 10 ||
                definition.durationMilliseconds > 3000) {
                fail(source, lineNumber,
                     "duration must be between 10 and 3000 ms");
            }
            if (!std::isfinite(definition.gain) || definition.gain < 0.f ||
                definition.gain > 1.f) {
                fail(source, lineNumber,
                     "gain must be between 0.0 and 1.0");
            }
            if (definition.maxVoices < 1 || definition.maxVoices > 8) {
                fail(source, lineNumber,
                     "max voices must be between 1 and 8");
            }
            if (!byId.emplace(definition.id, parsed.size()).second) {
                fail(source, lineNumber, "duplicate cue id '" +
                                             definition.id + "'");
            }
            parsed.push_back(std::move(definition));
        }
        if (!headerSeen) {
            fail(source, 1, "missing version header");
        }
    }

    requireCompleteCueSet(byId);
    m_definitions = std::move(parsed);
    m_byId = std::move(byId);
    m_frozen = true;
}

bool AudioDefinitionRegistry::tryFreezeFromFile(
    const std::string &path, std::string &error) noexcept
{
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Unable to read audio definitions '" +
                                     path + "'.");
        }
        std::ostringstream content;
        content << input.rdbuf();
        freeze({{path, content.str()}});
        error.clear();
        return true;
    }
    catch (const std::exception &exception) {
        m_definitions.clear();
        m_byId.clear();
        m_frozen = true;
        error = exception.what();
        return false;
    }
}

bool AudioDefinitionRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

const AudioDefinition *AudioDefinitionRegistry::find(
    const std::string &id) const noexcept
{
    const auto found = m_byId.find(id);
    return found != m_byId.end() ? &m_definitions[found->second] : nullptr;
}

const std::vector<AudioDefinition> &
AudioDefinitionRegistry::definitions() const noexcept
{
    return m_definitions;
}
