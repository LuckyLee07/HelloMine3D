#include "MusicDefinitionRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {
constexpr const char *MusicHeader =
    "# HelloMine3D music definitions v1";
constexpr const char *RequiredTrackId = "overworld.quiet";

std::string trim(const std::string &value)
{
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1u);
}

bool validId(const std::string &id)
{
    return !id.empty() && id.size() <= 48u &&
           std::all_of(id.begin(), id.end(), [](unsigned char value) {
               return std::islower(value) || std::isdigit(value) ||
                      value == '.' || value == '-' || value == '_';
           });
}

bool validStreamPath(const std::string &path)
{
    constexpr const char *Prefix = "media/music/tracks/";
    const std::size_t prefixLength =
        std::char_traits<char>::length(Prefix);
    if (path.size() <= prefixLength + 4u ||
        path.compare(0, prefixLength, Prefix) != 0 ||
        path.compare(path.size() - 4u, 4u, ".wav") != 0 ||
        path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos) {
        return false;
    }
    std::size_t begin = 0;
    while (begin < path.size()) {
        const std::size_t end = path.find('/', begin);
        const std::string part = path.substr(
            begin, end == std::string::npos ? std::string::npos
                                             : end - begin);
        if (part.empty() || part == "." || part == ".." ||
            !std::all_of(part.begin(), part.end(), [](unsigned char value) {
                return std::isalnum(value) || value == '_' || value == '-' ||
                       value == '.';
            })) {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1u;
    }
    return true;
}

[[noreturn]] void fail(const MusicDefinitionSource &source,
                       std::size_t line, const std::string &detail)
{
    throw std::runtime_error(
        "Invalid music definition '" + source.name + "' at line " +
        std::to_string(line) + ": " + detail + ".");
}
} // namespace

void MusicDefinitionRegistry::freeze(
    const std::vector<MusicDefinitionSource> &sources)
{
    if (m_frozen) {
        throw std::runtime_error("Music definition registry is already frozen.");
    }
    if (sources.size() != 1u) {
        throw std::runtime_error(
            "Music definition registry requires exactly one base source.");
    }

    std::vector<MusicTrackDefinition> parsed;
    std::unordered_set<std::string> ids;
    for (const MusicDefinitionSource &source : sources) {
        std::istringstream input(source.content);
        std::string line;
        std::size_t lineNumber = 0;
        bool headerSeen = false;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (!headerSeen) {
                const std::string first = trim(line);
                if (first.empty()) {
                    continue;
                }
                if (first != MusicHeader) {
                    fail(source, lineNumber,
                         "expected the v1 music header");
                }
                headerSeen = true;
                continue;
            }

            const std::size_t comment = line.find('#');
            if (comment != std::string::npos) {
                line.erase(comment);
            }
            if (trim(line).empty()) {
                continue;
            }

            MusicTrackDefinition definition;
            std::string directive;
            std::istringstream values(line);
            if (!(values >> directive >> definition.id >>
                  std::quoted(definition.streamPath) >> definition.gain >>
                  definition.durationMilliseconds >>
                  definition.fadeInMilliseconds >>
                  definition.fadeOutMilliseconds >>
                  definition.initialDelayMilliseconds >>
                  definition.minimumGapMilliseconds >>
                  definition.maximumGapMilliseconds) ||
                directive != "track") {
                fail(source, lineNumber,
                     "expected track id logical_path gain duration_ms fade_in_ms fade_out_ms initial_delay_ms min_gap_ms max_gap_ms");
            }
            values >> std::ws;
            if (!values.eof()) {
                fail(source, lineNumber, "contains trailing data");
            }
            if (!validId(definition.id)) {
                fail(source, lineNumber, "track id is not canonical");
            }
            if (!ids.insert(definition.id).second) {
                fail(source, lineNumber, "duplicate track id");
            }
            if (!validStreamPath(definition.streamPath)) {
                fail(source, lineNumber,
                     "stream path must be a canonical media/music/tracks/*.wav path");
            }
            if (!std::isfinite(definition.gain) || definition.gain < 0.f ||
                definition.gain > 1.f) {
                fail(source, lineNumber, "gain must be between 0 and 1");
            }
            if (definition.durationMilliseconds < 10000 ||
                definition.durationMilliseconds > 180000) {
                fail(source, lineNumber,
                     "duration must be between 10000 and 180000 ms");
            }
            if (definition.fadeInMilliseconds < 100 ||
                definition.fadeInMilliseconds > 10000 ||
                definition.fadeOutMilliseconds < 100 ||
                definition.fadeOutMilliseconds > 10000 ||
                definition.fadeInMilliseconds >=
                    definition.durationMilliseconds ||
                definition.fadeOutMilliseconds >=
                    definition.durationMilliseconds) {
                fail(source, lineNumber,
                     "fade times must be 100..10000 ms and shorter than the track");
            }
            if (definition.initialDelayMilliseconds < 0 ||
                definition.initialDelayMilliseconds > 60000 ||
                definition.minimumGapMilliseconds < 10000 ||
                definition.maximumGapMilliseconds <
                    definition.minimumGapMilliseconds ||
                definition.maximumGapMilliseconds > 300000) {
                fail(source, lineNumber,
                     "initial delay or low-density gap is outside its bound");
            }
            parsed.push_back(std::move(definition));
        }
        if (!headerSeen) {
            fail(source, 1, "music header is missing");
        }
    }
    if (parsed.size() != 1u || parsed.front().id != RequiredTrackId) {
        throw std::runtime_error(
            "Music definition registry requires exactly overworld.quiet.");
    }
    m_tracks = std::move(parsed);
    m_frozen = true;
}

bool MusicDefinitionRegistry::tryFreezeFromFile(
    const std::string &path, std::string &error) noexcept
{
    try {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) {
            throw std::runtime_error("Unable to open music definition file.");
        }
        const std::streamoff size = input.tellg();
        if (size <= 0 || size > 64 * 1024) {
            throw std::runtime_error(
                "Music definition file size is outside 1..65536 bytes.");
        }
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        input.read(&content[0], size);
        if (!input) {
            throw std::runtime_error(
                "Unable to read complete music definition file.");
        }
        freeze({{path, std::move(content)}});
        error.clear();
        return true;
    }
    catch (const std::exception &exception) {
        m_tracks.clear();
        m_frozen = true;
        error = exception.what();
        return false;
    }
}

bool MusicDefinitionRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

const MusicTrackDefinition *MusicDefinitionRegistry::track() const noexcept
{
    return m_tracks.empty() ? nullptr : &m_tracks.front();
}

const std::vector<MusicTrackDefinition> &
MusicDefinitionRegistry::tracks() const noexcept
{
    return m_tracks;
}
