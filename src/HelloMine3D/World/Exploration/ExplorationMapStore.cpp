#include "ExplorationMapStore.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;

constexpr std::array<char, 8> Magic{{'H', 'M', '3', 'D', 'M', 'A', 'P', '1'}};
constexpr std::size_t KnownMaskBytes =
    ExplorationAtlas::CellsPerTile * ExplorationAtlas::CellsPerTile / 8;
constexpr std::size_t TileBytes = 8 + KnownMaskBytes +
    ExplorationAtlas::CellsPerTile * ExplorationAtlas::CellsPerTile * 2;
constexpr std::uint64_t HashBasis = 14695981039346656037ull;
constexpr std::uint64_t HashPrime = 1099511628211ull;

bool validWorldId(const std::string& id)
{
    if (id.empty() || id.size() > 64 ||
        !((id.front() >= 'a' && id.front() <= 'z') ||
          (id.front() >= '0' && id.front() <= '9'))) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '-' || c == '_';
    });
}

bool validIdentity(const ExplorationMapStore::Identity& identity)
{
    return validWorldId(identity.worldId) &&
           identity.terrainGenerationVersion > 0 &&
           identity.terrainGenerationVersion <= 1000000;
}

void append8(std::vector<char>& bytes, std::uint8_t value)
{
    bytes.push_back(static_cast<char>(value));
}

void append32(std::vector<char>& bytes, std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        append8(bytes, static_cast<std::uint8_t>(value >> shift));
    }
}

void append64(std::vector<char>& bytes, std::uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8) {
        append8(bytes, static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint64_t hashPrefix(const std::vector<char>& bytes,
                         std::size_t length)
{
    std::uint64_t result = HashBasis;
    for (std::size_t index = 0; index < length; ++index) {
        result ^= static_cast<unsigned char>(bytes[index]);
        result *= HashPrime;
    }
    return result;
}

class Reader {
  public:
    explicit Reader(const std::vector<char>& bytes) : m_bytes(bytes) {}

    bool read8(std::uint8_t& value)
    {
        if (m_offset >= m_bytes.size()) {
            return false;
        }
        value = static_cast<unsigned char>(m_bytes[m_offset++]);
        return true;
    }

    bool read32(std::uint32_t& value)
    {
        value = 0;
        for (int shift = 0; shift < 32; shift += 8) {
            std::uint8_t byte = 0;
            if (!read8(byte)) {
                return false;
            }
            value |= std::uint32_t(byte) << shift;
        }
        return true;
    }

    bool read64(std::uint64_t& value)
    {
        value = 0;
        for (int shift = 0; shift < 64; shift += 8) {
            std::uint8_t byte = 0;
            if (!read8(byte)) {
                return false;
            }
            value |= std::uint64_t(byte) << shift;
        }
        return true;
    }

    std::size_t offset() const noexcept { return m_offset; }

  private:
    const std::vector<char>& m_bytes;
    std::size_t m_offset = 0;
};

std::int32_t signed32(std::uint32_t value)
{
    const std::int64_t signedValue =
        value <= std::uint32_t(std::numeric_limits<std::int32_t>::max())
            ? std::int64_t(value) : std::int64_t(value) - 0x100000000ll;
    return static_cast<std::int32_t>(signedValue);
}

bool readFileBytes(const std::string& path, std::vector<char>& bytes,
                   std::string& error)
{
    std::error_code statusError;
    const fs::file_status status = fs::symlink_status(path, statusError);
    if (statusError || !fs::is_regular_file(status) ||
        fs::is_symlink(status)) {
        error = "map path is not a regular file";
        return false;
    }
    std::error_code sizeError;
    const std::uintmax_t size = fs::file_size(path, sizeError);
    if (sizeError || size < Magic.size() + 4 + 1 + 4 * 4 + 8 ||
        size > ExplorationMapStore::MaxFileBytes) {
        error = "map file size is outside limit";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "map file cannot be opened";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) {
        error = "map file cannot be read completely";
        return false;
    }
    return true;
}

}

bool ExplorationMapStore::parseFile(const std::string& path,
                                    Identity& identity,
                                    ExplorationAtlas& atlas,
                                    ExplorationMarkers& markers,
                                    std::optional<KnownSite>& knownWaystone,
                                    std::optional<KnownSite>& boundWaystone,
                                    std::string& error)
{
    std::vector<char> bytes;
    if (!readFileBytes(path, bytes, error)) {
        return false;
    }
    if (!std::equal(Magic.begin(), Magic.end(), bytes.begin())) {
        error = "map magic differs";
        return false;
    }
    Reader reader(bytes);
    std::uint8_t byte = 0;
    for (std::size_t index = 0; index < Magic.size(); ++index) {
        reader.read8(byte);
    }
    std::uint32_t version = 0;
    std::uint8_t idLength = 0;
    if (!reader.read32(version) ||
        (version < 1 || version > FormatVersion) ||
        !reader.read8(idLength) || idLength == 0 || idLength > 64 ||
        reader.offset() + idLength > bytes.size()) {
        error = "map version or identity length differs";
        return false;
    }
    identity.worldId.clear();
    for (std::size_t index = 0; index < idLength; ++index) {
        reader.read8(byte);
        identity.worldId.push_back(static_cast<char>(byte));
    }
    std::uint32_t seed = 0, terrain = 0, pageCount = 0, markerCount = 0;
    if (!reader.read32(seed) || !reader.read32(terrain) ||
        !reader.read32(pageCount) || !reader.read32(markerCount)) {
        error = "map header is truncated";
        return false;
    }
    identity.seed = signed32(seed);
    identity.terrainGenerationVersion = signed32(terrain);
    const std::size_t fixedBytes = reader.offset() +
        std::size_t(pageCount) * TileBytes + 8;
    if (!validIdentity(identity) ||
        pageCount > ExplorationAtlas::MaxTiles ||
        markerCount > ExplorationMarkers::Capacity ||
        (version == 1 && (markerCount != 0 ||
                          fixedBytes != bytes.size())) ||
        (version >= 2 &&
         fixedBytes + 8 + std::size_t(markerCount) * 14 +
             (version >= 3 ? 1 : 0) + (version >= 4 ? 1 : 0) > bytes.size())) {
        error = "map identity, count or length is invalid";
        return false;
    }
    std::uint64_t storedHash = 0;
    Reader hashReader(bytes);
    // The trailing checksum is read without trusting any page payload.
    for (std::size_t index = 0; index < bytes.size() - 8; ++index) {
        hashReader.read8(byte);
    }
    if (!hashReader.read64(storedHash) ||
        storedHash != hashPrefix(bytes, bytes.size() - 8)) {
        error = "map checksum differs";
        return false;
    }

    ExplorationAtlas parsed;
    std::pair<int, int> previous;
    bool hasPrevious = false;
    for (std::uint32_t page = 0; page < pageCount; ++page) {
        std::uint32_t rawX = 0, rawZ = 0;
        if (!reader.read32(rawX) || !reader.read32(rawZ)) {
            error = "map page position is truncated";
            return false;
        }
        const auto key = std::make_pair(int(signed32(rawX)),
                                        int(signed32(rawZ)));
        if (hasPrevious && !(previous < key)) {
            error = "map pages are repeated or unsorted";
            return false;
        }
        previous = key;
        hasPrevious = true;
        std::array<std::uint8_t, KnownMaskBytes> known{};
        for (std::uint8_t& value : known) {
            if (!reader.read8(value)) {
                error = "map known mask is truncated";
                return false;
            }
        }
        auto& tile = parsed.m_tiles.try_emplace(key).first->second;
        std::size_t pageKnown = 0;
        for (std::size_t index = 0; index < tile.cells.size(); ++index) {
            std::uint8_t height = 0, material = 0;
            if (!reader.read8(height) || !reader.read8(material)) {
                error = "map surface page is truncated";
                return false;
            }
            const bool occupied =
                (known[index / 8] & (1u << (index % 8))) != 0;
            if ((occupied && material >=
                     static_cast<std::uint8_t>(BlockId::NUM_TYPES)) ||
                (!occupied && (height != 0 || material != 0))) {
                error = "map surface value is invalid";
                return false;
            }
            tile.cells[index] = {static_cast<std::uint8_t>(occupied),
                                 height, material};
            pageKnown += occupied;
        }
        if (pageKnown == 0) {
            error = "map contains an empty page";
            return false;
        }
        parsed.m_knownCells += pageKnown;
    }
    ExplorationMarkers parsedMarkers;
    if (version >= 2) {
        std::uint32_t nextId = 0, trackedId = 0;
        if (!reader.read32(nextId) || !reader.read32(trackedId)) {
            error = "map marker header is truncated";
            return false;
        }
        parsedMarkers.m_nextId = nextId;
        parsedMarkers.m_trackedId = trackedId;
        for (std::uint32_t index = 0; index < markerCount; ++index) {
            std::uint32_t id = 0, rawX = 0, rawZ = 0;
            std::uint8_t kind = 0, nameLength = 0;
            if (!reader.read32(id) || !reader.read32(rawX) ||
                !reader.read32(rawZ) || !reader.read8(kind) ||
                !reader.read8(nameLength) ||
                nameLength == 0 ||
                nameLength > ExplorationMarkers::MaxNameBytes ||
                reader.offset() + nameLength > bytes.size() - 8) {
                error = "map marker record is truncated or oversized";
                return false;
            }
            std::string name;
            name.reserve(nameLength);
            for (std::uint8_t offset = 0; offset < nameLength; ++offset) {
                reader.read8(byte);
                name.push_back(static_cast<char>(byte));
            }
            parsedMarkers.m_markers.push_back({
                id, signed32(rawX), signed32(rawZ), std::move(name),
                static_cast<ExplorationMarkers::Kind>(kind)});
        }
        if (!validateMarkers(parsed, parsedMarkers, error)) {
            return false;
        }
    }
    std::optional<KnownSite> parsedSite, parsedBinding;
    const auto readSite = [&](std::optional<KnownSite>& site) {
        std::uint8_t present = 0;
        if (!reader.read8(present) || present > 1) {
            error = "map known site flag is invalid";
            return false;
        }
        if (present == 1) {
            std::uint32_t rawX = 0, rawY = 0, rawZ = 0;
            if (!reader.read32(rawX) || !reader.read32(rawY) ||
                !reader.read32(rawZ)) {
                error = "map known site is truncated";
                return false;
            }
            const int y = signed32(rawY);
            if (y < 1) {
                error = "map known site height is invalid";
                return false;
            }
            site = KnownSite{signed32(rawX), y, signed32(rawZ)};
        }
        return true;
    };
    if ((version >= 3 && !readSite(parsedSite)) ||
        (version >= 4 && !readSite(parsedBinding))) return false;
    if (reader.offset() != bytes.size() - 8) {
        error = "map page length differs";
        return false;
    }
    atlas = std::move(parsed);
    markers = std::move(parsedMarkers);
    knownWaystone = parsedSite;
    boundWaystone = parsedBinding;
    return true;
}

bool ExplorationMapStore::validateMarkers(
    const ExplorationAtlas& atlas, const ExplorationMarkers& markers,
    std::string& error)
{
    if (markers.m_markers.size() > ExplorationMarkers::Capacity) {
        error = "map marker count exceeds limit";
        return false;
    }
    std::uint32_t previousId = 0;
    std::size_t homeCount = 0;
    bool trackedFound = markers.m_trackedId == 0;
    for (const auto& marker : markers.m_markers) {
        if (marker.id == 0 || marker.id <= previousId ||
            !ExplorationMarkers::validKind(marker.kind) ||
            !ExplorationMarkers::validName(marker.name) ||
            !atlas.surfaceAt(marker.worldX, marker.worldZ).has_value()) {
            error = "map marker id, value or explored position is invalid";
            return false;
        }
        previousId = marker.id;
        homeCount += marker.kind == ExplorationMarkers::Kind::Home;
        trackedFound |= marker.id == markers.m_trackedId;
    }
    if (homeCount > 1 || !trackedFound ||
        (markers.m_nextId != 0 && markers.m_nextId <= previousId)) {
        error = "map marker home, tracking or next id is invalid";
        return false;
    }
    return true;
}

ExplorationMapStore::ExplorationMapStore(std::string worldDirectory)
    : m_worldDirectory(std::move(worldDirectory))
{
    if (m_worldDirectory.empty()) {
        throw std::invalid_argument("map store requires a world directory");
    }
}

std::string ExplorationMapStore::filePath() const
{
    return (fs::path(m_worldDirectory) / "exploration.hmap").string();
}

ExplorationMapStore::LoadStatus ExplorationMapStore::load(
    const Identity& expected, ExplorationAtlas& atlas,
    std::string* error) const
{
    ExplorationMarkers ignored;
    return load(expected, atlas, ignored, error);
}

ExplorationMapStore::LoadStatus ExplorationMapStore::load(
    const Identity& expected, ExplorationAtlas& atlas,
    ExplorationMarkers& markers, std::string* error) const
{
    std::optional<KnownSite> ignored;
    return load(expected, atlas, markers, ignored, error);
}

ExplorationMapStore::LoadStatus ExplorationMapStore::load(
    const Identity& expected, ExplorationAtlas& atlas,
    ExplorationMarkers& markers,
    std::optional<KnownSite>& knownWaystone, std::string* error) const
{
    std::optional<KnownSite> ignored;
    return load(expected, atlas, markers, knownWaystone, ignored, error);
}

ExplorationMapStore::LoadStatus ExplorationMapStore::load(
    const Identity& expected, ExplorationAtlas& atlas,
    ExplorationMarkers& markers, std::optional<KnownSite>& knownWaystone,
    std::optional<KnownSite>& boundWaystone, std::string* error) const
{
    atlas.clear();
    markers.clear();
    knownWaystone.reset();
    boundWaystone.reset();
    if (error != nullptr) {
        error->clear();
    }
    std::error_code statusError;
    const fs::file_status status =
        fs::symlink_status(filePath(), statusError);
    if (statusError == std::errc::no_such_file_or_directory) {
        return LoadStatus::Absent;
    }
    if (statusError || !fs::is_regular_file(status) ||
        fs::is_symlink(status)) {
        if (error != nullptr) {
            *error = "map path is not a regular file";
        }
        return LoadStatus::Corrupt;
    }
    Identity loadedIdentity;
    std::string localError;
    if (!parseFile(filePath(), loadedIdentity, atlas, markers,
                   knownWaystone, boundWaystone,
                   localError)) {
        if (error != nullptr) {
            *error = localError;
        }
        return LoadStatus::Corrupt;
    }
    if (!validIdentity(expected) ||
        loadedIdentity.worldId != expected.worldId ||
        loadedIdentity.seed != expected.seed ||
        loadedIdentity.terrainGenerationVersion !=
            expected.terrainGenerationVersion) {
        atlas.clear();
        markers.clear();
        knownWaystone.reset();
        boundWaystone.reset();
        if (error != nullptr) {
            *error = "map belongs to another world identity";
        }
        return LoadStatus::IdentityMismatch;
    }
    return LoadStatus::Loaded;
}

bool ExplorationMapStore::save(
    const Identity& identity, const ExplorationAtlas& atlas,
    const ExplorationMarkers& markers,
    const std::optional<KnownSite>& knownWaystone,
    const std::optional<KnownSite>& boundWaystone,
    const StorageTransactionOptions& options,
    StorageTransactionMetrics* metrics) const
{
    std::string markerError;
    const bool siteValid = (!knownWaystone || knownWaystone->worldY > 0) &&
        (!boundWaystone || boundWaystone->worldY > 0);
    if (!validIdentity(identity) || atlas.tileCount() >
            ExplorationAtlas::MaxTiles ||
        !validateMarkers(atlas, markers, markerError) ||
        !siteValid) {
        if (metrics != nullptr) {
            *metrics = {};
            metrics->error = !siteValid ? "map known site height is invalid"
                : markerError.empty() ? "map identity or page count is invalid"
                                      : markerError;
        }
        return false;
    }
    std::error_code statusError;
    const fs::file_status status =
        fs::symlink_status(filePath(), statusError);
    if (statusError != std::errc::no_such_file_or_directory) {
        ExplorationAtlas existing;
        std::string loadError;
        if (statusError || !fs::is_regular_file(status) ||
            fs::is_symlink(status) ||
            load(identity, existing, &loadError) != LoadStatus::Loaded) {
            if (metrics != nullptr) {
                *metrics = {};
                metrics->error = "existing exploration map must be "
                    "quarantined before replacement: " + loadError;
            }
            return false;
        }
    }
    std::vector<char> bytes;
    bytes.reserve(64 + atlas.tileCount() * TileBytes +
                  markers.size() * (14 + ExplorationMarkers::MaxNameBytes) +
                  26);
    bytes.insert(bytes.end(), Magic.begin(), Magic.end());
    append32(bytes, FormatVersion);
    append8(bytes, static_cast<std::uint8_t>(identity.worldId.size()));
    bytes.insert(bytes.end(), identity.worldId.begin(),
                 identity.worldId.end());
    append32(bytes, static_cast<std::uint32_t>(identity.seed));
    append32(bytes, static_cast<std::uint32_t>(
                        identity.terrainGenerationVersion));
    append32(bytes, static_cast<std::uint32_t>(atlas.tileCount()));
    append32(bytes, static_cast<std::uint32_t>(markers.size()));
    for (const auto& entry : atlas.m_tiles) {
        append32(bytes, static_cast<std::uint32_t>(entry.first.first));
        append32(bytes, static_cast<std::uint32_t>(entry.first.second));
        std::array<std::uint8_t, KnownMaskBytes> known{};
        std::size_t pageKnown = 0;
        for (std::size_t index = 0; index < entry.second.cells.size();
             ++index) {
            if (entry.second.cells[index].known != 0) {
                known[index / 8] |= std::uint8_t(1u << (index % 8));
                ++pageKnown;
            }
        }
        if (pageKnown == 0) {
            if (metrics != nullptr) {
                *metrics = {};
                metrics->error = "map contains an empty page";
            }
            return false;
        }
        for (std::uint8_t value : known) {
            append8(bytes, value);
        }
        for (const auto& cell : entry.second.cells) {
            append8(bytes, cell.height);
            append8(bytes, cell.material);
        }
    }
    append32(bytes, markers.m_nextId);
    append32(bytes, markers.m_trackedId);
    for (const auto& marker : markers.m_markers) {
        append32(bytes, marker.id);
        append32(bytes, static_cast<std::uint32_t>(marker.worldX));
        append32(bytes, static_cast<std::uint32_t>(marker.worldZ));
        append8(bytes, static_cast<std::uint8_t>(marker.kind));
        append8(bytes, static_cast<std::uint8_t>(marker.name.size()));
        bytes.insert(bytes.end(), marker.name.begin(), marker.name.end());
    }
    const auto appendSite = [&](const std::optional<KnownSite>& site) {
        append8(bytes, site.has_value() ? 1 : 0);
        if (site) {
            append32(bytes, static_cast<std::uint32_t>(site->worldX));
            append32(bytes, static_cast<std::uint32_t>(site->worldY));
            append32(bytes, static_cast<std::uint32_t>(site->worldZ));
        }
    };
    appendSite(knownWaystone);
    appendSite(boundWaystone);
    append64(bytes, hashPrefix(bytes, bytes.size()));
    if (bytes.size() > MaxFileBytes) {
        if (metrics != nullptr) {
            *metrics = {};
            metrics->error = "map exceeds file size limit";
        }
        return false;
    }
    return StorageTransaction::publish(
        filePath(), bytes,
        [&](const std::string& candidate, std::string& validationError) {
            Identity validatedIdentity;
            ExplorationAtlas validated;
            ExplorationMarkers validatedMarkers;
            std::optional<KnownSite> validatedSite, validatedBinding;
            if (!parseFile(candidate, validatedIdentity, validated,
                           validatedMarkers, validatedSite, validatedBinding,
                           validationError)) {
                return false;
            }
            if (validatedIdentity.worldId != identity.worldId ||
                validatedIdentity.seed != identity.seed ||
                validatedIdentity.terrainGenerationVersion !=
                    identity.terrainGenerationVersion ||
                validated.tileCount() != atlas.tileCount() ||
                validated.knownCellCount() != atlas.knownCellCount() ||
                validatedMarkers.m_nextId != markers.m_nextId ||
                validatedMarkers.m_trackedId != markers.m_trackedId ||
                validatedMarkers.size() != markers.size() ||
                validatedSite != knownWaystone || validatedBinding != boundWaystone) {
                validationError = "map candidate identity or count differs";
                return false;
            }
            for (std::size_t index = 0; index < markers.size(); ++index) {
                const auto& left = validatedMarkers.all()[index];
                const auto& right = markers.all()[index];
                if (left.id != right.id || left.worldX != right.worldX ||
                    left.worldZ != right.worldZ || left.name != right.name ||
                    left.kind != right.kind) {
                    validationError = "map candidate marker differs";
                    return false;
                }
            }
            return true;
        }, options, metrics);
}

bool ExplorationMapStore::quarantineInvalid(const Identity& expected,
                                             std::string* error) const
{
    if (error != nullptr) {
        error->clear();
    }
    ExplorationAtlas parsed;
    std::string loadError;
    const LoadStatus loaded = load(expected, parsed, &loadError);
    if (loaded == LoadStatus::Absent || loaded == LoadStatus::Loaded) {
        return true;
    }
    const fs::path source(filePath());
    const fs::path quarantine(filePath() + ".corrupt.failed");
    std::error_code statusError;
    const fs::file_status status = fs::symlink_status(source, statusError);
    if (statusError || !fs::is_regular_file(status) ||
        fs::is_symlink(status)) {
        if (error != nullptr) {
            *error = "invalid map path is not a real regular file";
        }
        return false;
    }
    std::error_code quarantineError;
    const fs::file_status quarantineStatus =
        fs::symlink_status(quarantine, quarantineError);
    if ((quarantineError &&
         quarantineError != std::errc::no_such_file_or_directory) ||
        (!quarantineError && fs::exists(quarantineStatus))) {
        if (error != nullptr) {
            *error = "exploration map quarantine slot is occupied";
        }
        return false;
    }
    std::error_code moveError;
    fs::rename(source, quarantine, moveError);
    if (moveError) {
        if (error != nullptr) {
            *error = "cannot quarantine invalid exploration map: " +
                     moveError.message();
        }
        return false;
    }
    return true;
}

bool ExplorationMapStore::validateFile(const std::string& path,
                                       std::string* error)
{
    Identity identity;
    ExplorationAtlas atlas;
    ExplorationMarkers markers;
    std::optional<KnownSite> knownWaystone, boundWaystone;
    std::string localError;
    const bool valid = parseFile(path, identity, atlas, markers,
                                 knownWaystone, boundWaystone,
                                 localError);
    if (error != nullptr) {
        *error = localError;
    }
    return valid;
}
