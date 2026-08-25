#include "WorldCatalogue.h"
#include "../../Diagnostics/OperationPerformanceTiming.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <system_error>

namespace
{
    namespace fs = std::filesystem;

    constexpr std::int64_t MaximumWorldTimestampUtc = 253402300799;

    const std::set<std::string> KnownMetadataKeys = {
        "actor",          "actor_count",       "alpha_journey_flags",
        "created_utc",    "difficulty_id",      "difficulty_profile_version",
        "generator",      "inventory_count",   "inventory_format",
        "inventory_slot",
        "last_build",     "last_played_utc",   "player_attack_cooldown",
        "player_food_cooldown", "player_health", "player_held", "player_position",
        "player_present", "player_rotation",
        "objective_completed", "objective_completed_count",
        "objective_definition_version", "objective_progress",
        "objective_progress_count",
        "world_outcome_phase", "world_outcome_reward_epoch",
        "world_outcome_claimed_epoch",
        "seed",           "spawn",             "terrain_generation_version",
        "version",
        "world_id",       "world_name",        "world_time"};

    const std::set<std::string> CatalogueKeys = {
        "created_utc", "difficulty_id", "difficulty_profile_version",
        "last_build", "last_played_utc", "seed",
        "version", "world_id", "world_name", "world_outcome_phase",
        "world_outcome_reward_epoch", "world_outcome_claimed_epoch"};

    std::string generic(const fs::path &path)
    {
        return path.generic_string();
    }

    std::string trim(const std::string &value)
    {
        std::size_t begin = 0;
        while (begin < value.size() &&
               (value[begin] == ' ' || value[begin] == '\t' ||
                value[begin] == '\r')) {
            ++begin;
        }
        std::size_t end = value.size();
        while (end > begin &&
               (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                value[end - 1] == '\r')) {
            --end;
        }
        return value.substr(begin, end - begin);
    }

    [[noreturn]] void reject(const fs::path &path,
                             const std::string &reason)
    {
        throw WorldCatalogueError("Invalid world metadata '" + generic(path) +
                                  "': " + reason);
    }

    template <typename Integer>
    Integer parseInteger(const fs::path &path, const std::string &key,
                         const std::string &text)
    {
        Integer value{};
        const char *begin = text.data();
        const char *end = begin + text.size();
        const auto parsed = std::from_chars(begin, end, value);
        if (text.empty() || parsed.ec != std::errc() || parsed.ptr != end) {
            reject(path, "field '" + key + "' must be a canonical integer");
        }
        return value;
    }

    std::string parseName(const fs::path &path, const std::string &text)
    {
        if (text.empty() || text.front() != '"') {
            if (text.find_first_of(" \t\r") != std::string::npos) {
                reject(path,
                       "legacy field 'world_name' must be one token");
            }
            return text;
        }

        std::string name;
        std::istringstream input(text);
        input >> std::quoted(name);
        input >> std::ws;
        if (!input || !input.eof()) {
            reject(path, "field 'world_name' has invalid quoting");
        }
        return name;
    }

    bool readContinuation(unsigned char byte)
    {
        return (byte & 0xc0u) == 0x80u;
    }

    bool validDisplayUtf8(const std::string &value, std::size_t &codepoints)
    {
        codepoints = 0;
        for (std::size_t index = 0; index < value.size();) {
            const unsigned char first =
                static_cast<unsigned char>(value[index]);
            std::uint32_t scalar = 0;
            std::size_t length = 0;
            if (first < 0x80u) {
                scalar = first;
                length = 1;
            }
            else if (first >= 0xc2u && first <= 0xdfu) {
                scalar = first & 0x1fu;
                length = 2;
            }
            else if (first >= 0xe0u && first <= 0xefu) {
                scalar = first & 0x0fu;
                length = 3;
            }
            else if (first >= 0xf0u && first <= 0xf4u) {
                scalar = first & 0x07u;
                length = 4;
            }
            else {
                return false;
            }
            if (index + length > value.size()) {
                return false;
            }
            for (std::size_t offset = 1; offset < length; ++offset) {
                const unsigned char next =
                    static_cast<unsigned char>(value[index + offset]);
                if (!readContinuation(next)) {
                    return false;
                }
                scalar = (scalar << 6u) | (next & 0x3fu);
            }
            if ((length == 3 && scalar < 0x800u) ||
                (length == 4 && scalar < 0x10000u) ||
                scalar > 0x10ffffu ||
                (scalar >= 0xd800u && scalar <= 0xdfffu)) {
                return false;
            }
            if (scalar < 0x20u || scalar == 0x7fu || scalar == '/' ||
                scalar == '\\' || scalar == '"') {
                return false;
            }
            ++codepoints;
            index += length;
        }
        return true;
    }

    bool isWithin(const fs::path &root, const fs::path &candidate)
    {
        const fs::path relative = candidate.lexically_relative(root);
        if (relative.empty() || relative.is_absolute()) {
            return false;
        }
        for (const fs::path &part : relative) {
            if (part == "..") {
                return false;
            }
        }
        return true;
    }

    WorldCatalogueEntry parseMetadata(const fs::path &metadataPath)
    {
        std::error_code error;
        const auto size = fs::file_size(metadataPath, error);
        if (error) {
            reject(metadataPath, "cannot read file size: " + error.message());
        }
        if (size == 0 || size > WorldCatalogue::MaxMetadataBytes) {
            reject(metadataPath, "file size must be between 1 and " +
                                     std::to_string(
                                         WorldCatalogue::MaxMetadataBytes) +
                                     " bytes");
        }

        std::ifstream input(metadataPath, std::ios::binary);
        if (!input) {
            reject(metadataPath, "file cannot be opened");
        }

        std::map<std::string, std::string> fields;
        std::string line;
        std::size_t lineNumber = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            line = trim(line);
            if (line.empty()) {
                continue;
            }
            const std::size_t separator = line.find_first_of(" \t");
            const std::string key = line.substr(0, separator);
            const std::string value =
                separator == std::string::npos
                    ? std::string()
                    : trim(line.substr(separator + 1));
            if (KnownMetadataKeys.find(key) == KnownMetadataKeys.end()) {
                reject(metadataPath, "unknown field '" + key + "' on line " +
                                         std::to_string(lineNumber));
            }
            if (CatalogueKeys.find(key) == CatalogueKeys.end()) {
                continue;
            }
            if (value.empty() || !fields.emplace(key, value).second) {
                reject(metadataPath, "empty or duplicate field '" + key +
                                         "'");
            }
        }
        if (!input.eof()) {
            reject(metadataPath, "read failed before end of file");
        }

        for (const std::string &required :
             {"version", "world_id", "world_name", "seed"}) {
            if (fields.find(required) == fields.end()) {
                reject(metadataPath, "missing field '" + required + "'");
            }
        }

        WorldCatalogueEntry entry;
        entry.saveFormatVersion =
            parseInteger<int>(metadataPath, "version", fields["version"]);
        if (entry.saveFormatVersion < 1 ||
            entry.saveFormatVersion >
                WorldCatalogue::CurrentSaveFormatVersion) {
            reject(metadataPath, "unsupported save format version " +
                                     fields["version"]);
        }
        entry.id = fields["world_id"];
        if (!WorldCatalogue::isValidWorldId(entry.id)) {
            reject(metadataPath, "field 'world_id' is not canonical");
        }
        entry.displayName = parseName(metadataPath, fields["world_name"]);
        if (!WorldCatalogue::isValidDisplayName(entry.displayName)) {
            reject(metadataPath,
                   "field 'world_name' is not a valid display name");
        }
        entry.seed = parseInteger<int>(metadataPath, "seed", fields["seed"]);

        const bool worldOutcomeFieldsPresent =
            fields.count("world_outcome_phase") != 0 ||
            fields.count("world_outcome_reward_epoch") != 0 ||
            fields.count("world_outcome_claimed_epoch") != 0;
        if (entry.saveFormatVersion < 9 && worldOutcomeFieldsPresent) {
            reject(metadataPath,
                   "world outcome fields require save format version 9");
        }
        const bool difficultyFieldsPresent =
            fields.count("difficulty_profile_version") != 0 ||
            fields.count("difficulty_id") != 0;
        if (entry.saveFormatVersion < 10 && difficultyFieldsPresent) {
            reject(metadataPath,
                   "difficulty fields require save format version 10");
        }

        if (entry.saveFormatVersion < 3) {
            if (fields.count("created_utc") != 0 ||
                fields.count("last_played_utc") != 0 ||
                fields.count("last_build") != 0) {
                reject(metadataPath,
                       "legacy metadata contains partial identity fields");
            }
            entry.createdUtc = LegacyWorldTimestampUtc;
            entry.lastPlayedUtc = LegacyWorldTimestampUtc;
            entry.lastBuildIdentity =
                "legacy-v" + std::to_string(entry.saveFormatVersion);
            entry.legacyMetadata = true;
            return entry;
        }

        for (const std::string &required :
             {"created_utc", "last_played_utc", "last_build"}) {
            if (fields.find(required) == fields.end()) {
                reject(metadataPath, "missing field '" + required + "'");
            }
        }
        entry.createdUtc = parseInteger<std::int64_t>(
            metadataPath, "created_utc", fields["created_utc"]);
        entry.lastPlayedUtc = parseInteger<std::int64_t>(
            metadataPath, "last_played_utc", fields["last_played_utc"]);
        if (!WorldCatalogue::isValidTimestamps(entry.createdUtc,
                                               entry.lastPlayedUtc)) {
            reject(metadataPath,
                   "timestamps are out of range or not monotonic");
        }
        entry.lastBuildIdentity = fields["last_build"];
        if (!WorldCatalogue::isValidBuildIdentity(
                entry.lastBuildIdentity)) {
            reject(metadataPath,
                   "field 'last_build' is not a valid build identity");
        }
        if (entry.saveFormatVersion >= 9) {
            for (const std::string &required :
                 {"world_outcome_phase", "world_outcome_reward_epoch",
                  "world_outcome_claimed_epoch"}) {
                if (fields.find(required) == fields.end()) {
                    reject(metadataPath, "missing field '" + required + "'");
                }
            }
            const int phase = parseInteger<int>(
                metadataPath, "world_outcome_phase",
                fields["world_outcome_phase"]);
            if (!validWorldOutcomePhase(phase)) {
                reject(metadataPath,
                       "field 'world_outcome_phase' is outside its range");
            }
            WorldOutcomeState outcome;
            outcome.phase = static_cast<WorldOutcomePhase>(phase);
            outcome.rewardEpoch = parseInteger<std::uint32_t>(
                metadataPath, "world_outcome_reward_epoch",
                fields["world_outcome_reward_epoch"]);
            outcome.claimedRewardEpoch = parseInteger<std::uint32_t>(
                metadataPath, "world_outcome_claimed_epoch",
                fields["world_outcome_claimed_epoch"]);
            if (!validWorldOutcomeState(outcome)) {
                reject(metadataPath, "world outcome state is inconsistent");
            }
            entry.outcomePhase = outcome.phase;
            entry.completed = worldOutcomeIsVictory(outcome);
        }
        if (entry.saveFormatVersion >= 10) {
            for (const std::string &required :
                 {"difficulty_profile_version", "difficulty_id"}) {
                if (fields.find(required) == fields.end()) {
                    reject(metadataPath, "missing field '" + required + "'");
                }
            }
            entry.difficultyProfileVersion = parseInteger<int>(
                metadataPath, "difficulty_profile_version",
                fields["difficulty_profile_version"]);
            const int difficulty = parseInteger<int>(
                metadataPath, "difficulty_id", fields["difficulty_id"]);
            entry.difficulty = static_cast<WorldDifficulty>(difficulty);
            if (entry.difficultyProfileVersion !=
                    CurrentDifficultyProfileVersion ||
                !validWorldDifficulty(entry.difficulty)) {
                reject(metadataPath,
                       "difficulty profile version or id is outside its range");
            }
        }
        return entry;
    }

    class CatalogueOperationTiming {
      public:
        CatalogueOperationTiming()
            : m_timings(runtimeOperationTimings())
            , m_handle(m_timings.begin(RuntimeOperationKind::Catalogue))
        {
        }

        ~CatalogueOperationTiming() noexcept
        {
            if (m_complete) {
                return;
            }
            try {
                m_timings.complete(m_handle, false);
            }
            catch (...) {
                // Diagnostics must never replace the catalogue exception.
            }
        }

        void succeed(std::size_t entries) noexcept
        {
            if (m_complete) {
                return;
            }
            try {
                m_timings.setCatalogueEntries(m_handle, entries);
                m_timings.complete(m_handle, true);
            }
            catch (...) {
                // Diagnostics must not change successful enumeration.
            }
            m_complete = true;
        }

      private:
        RuntimeOperationTimings &m_timings;
        RuntimeOperationHandle m_handle;
        bool m_complete = false;
    };
}

std::vector<WorldCatalogueEntry>
WorldCatalogue::enumerate(const std::string &catalogueRoot)
{
    CatalogueOperationTiming timing;
    if (catalogueRoot.empty()) {
        throw WorldCatalogueError("World catalogue root must not be empty.");
    }

    const fs::path requestedRoot(catalogueRoot);
    std::error_code error;
    const fs::file_status rootStatus = fs::symlink_status(requestedRoot, error);
    if (error == std::errc::no_such_file_or_directory ||
        rootStatus.type() == fs::file_type::not_found) {
        timing.succeed(0);
        return {};
    }
    if (error) {
        throw WorldCatalogueError("Cannot inspect world catalogue root '" +
                                  generic(requestedRoot) + "': " +
                                  error.message());
    }
    if (fs::is_symlink(rootStatus) || !fs::is_directory(rootStatus)) {
        throw WorldCatalogueError(
            "World catalogue root must be a real directory: '" +
            generic(requestedRoot) + "'.");
    }

    const fs::path canonicalRoot = fs::canonical(requestedRoot, error);
    if (error) {
        throw WorldCatalogueError("Cannot canonicalize world catalogue root '" +
                                  generic(requestedRoot) + "': " +
                                  error.message());
    }

    std::vector<fs::path> candidates;
    fs::directory_iterator iterator(canonicalRoot, error);
    const fs::directory_iterator end;
    while (!error && iterator != end) {
        const fs::directory_entry candidate = *iterator;
        const fs::file_status status = candidate.symlink_status(error);
        if (error) {
            break;
        }
        if (fs::is_symlink(status)) {
            throw WorldCatalogueError(
                "World catalogue entries must not be symlinks: '" +
                generic(candidate.path()) + "'.");
        }
        if (fs::is_directory(status)) {
            candidates.push_back(candidate.path());
        }
        iterator.increment(error);
    }
    if (error) {
        throw WorldCatalogueError("Cannot enumerate world catalogue root '" +
                                  generic(canonicalRoot) + "': " +
                                  error.message());
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const fs::path &left, const fs::path &right) {
                  return generic(left.filename()) < generic(right.filename());
              });

    std::map<std::string, std::string> idOwners;
    std::vector<WorldCatalogueEntry> result;
    result.reserve(candidates.size());
    for (const fs::path &candidate : candidates) {
        const fs::path canonicalCandidate = fs::canonical(candidate, error);
        if (error || !isWithin(canonicalRoot, canonicalCandidate)) {
            throw WorldCatalogueError(
                "World directory escapes catalogue root: '" +
                generic(candidate) + "'.");
        }

        const fs::path metadataPath = canonicalCandidate / "world.meta";
        const fs::file_status metadataStatus =
            fs::symlink_status(metadataPath, error);
        if (error == std::errc::no_such_file_or_directory ||
            metadataStatus.type() == fs::file_type::not_found) {
            throw WorldCatalogueError("Missing world metadata: expected '" +
                                      generic(metadataPath) + "'.");
        }
        if (error) {
            throw WorldCatalogueError("Cannot inspect world metadata '" +
                                      generic(metadataPath) + "': " +
                                      error.message());
        }
        if (fs::is_symlink(metadataStatus) ||
            !fs::is_regular_file(metadataStatus)) {
            throw WorldCatalogueError(
                "World metadata must be a real regular file: '" +
                generic(metadataPath) + "'.");
        }

        WorldCatalogueEntry entry = parseMetadata(metadataPath);
        entry.directoryName = generic(candidate.filename());
        entry.directoryPath = generic(canonicalCandidate);
        const auto owner = idOwners.find(entry.id);
        if (owner != idOwners.end()) {
            throw WorldCatalogueError(
                "Duplicate world id '" + entry.id + "' in directories '" +
                owner->second + "' and '" + entry.directoryName + "'.");
        }
        idOwners.emplace(entry.id, entry.directoryName);
        result.push_back(std::move(entry));
    }

    std::sort(result.begin(), result.end(),
              [](const WorldCatalogueEntry &left,
                 const WorldCatalogueEntry &right) {
                  if (left.lastPlayedUtc != right.lastPlayedUtc) {
                      return left.lastPlayedUtc > right.lastPlayedUtc;
                  }
                  if (left.createdUtc != right.createdUtc) {
                      return left.createdUtc > right.createdUtc;
                  }
                  return left.id < right.id;
              });
    timing.succeed(result.size());
    return result;
}

bool WorldCatalogue::isValidWorldId(const std::string &value) noexcept
{
    if (value.empty() || value.size() > MaxWorldIdBytes) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    if (!((first >= 'a' && first <= 'z') ||
          (first >= '0' && first <= '9'))) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') || character == '-' ||
               character == '_';
    });
}

bool WorldCatalogue::isValidDisplayName(const std::string &value) noexcept
{
    if (value.empty() || value.size() > MaxDisplayNameCodepoints * 4 ||
        value.front() == ' ' || value.back() == ' ' || value == "." ||
        value == "..") {
        return false;
    }
    std::size_t codepoints = 0;
    return validDisplayUtf8(value, codepoints) && codepoints > 0 &&
           codepoints <= MaxDisplayNameCodepoints;
}

bool WorldCatalogue::isValidBuildIdentity(const std::string &value) noexcept
{
    if (value.empty() || value.size() > MaxBuildIdentityBytes) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '-' ||
               character == '_' || character == '.' || character == '+';
    });
}

bool WorldCatalogue::isValidTimestamps(std::int64_t createdUtc,
                                       std::int64_t lastPlayedUtc) noexcept
{
    return createdUtc >= LegacyWorldTimestampUtc &&
           createdUtc <= MaximumWorldTimestampUtc &&
           lastPlayedUtc >= createdUtc &&
           lastPlayedUtc <= MaximumWorldTimestampUtc;
}
