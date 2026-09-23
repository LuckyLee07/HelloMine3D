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
        version != ExplorationMapStore::FormatVersion ||
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
    if (!validIdentity(identity) ||
        pageCount > ExplorationAtlas::MaxTiles || markerCount != 0 ||
        reader.offset() + std::size_t(pageCount) * TileBytes + 8 !=
            bytes.size()) {
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
    if (reader.offset() != bytes.size() - 8) {
        error = "map page length differs";
        return false;
    }
    atlas = std::move(parsed);
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
    atlas.clear();
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
    if (!parseFile(filePath(), loadedIdentity, atlas, localError)) {
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
        if (error != nullptr) {
            *error = "map belongs to another world identity";
        }
        return LoadStatus::IdentityMismatch;
    }
    return LoadStatus::Loaded;
}

bool ExplorationMapStore::save(
    const Identity& identity, const ExplorationAtlas& atlas,
    const StorageTransactionOptions& options,
    StorageTransactionMetrics* metrics) const
{
    if (!validIdentity(identity) || atlas.tileCount() >
            ExplorationAtlas::MaxTiles) {
        if (metrics != nullptr) {
            *metrics = {};
            metrics->error = "map identity or page count is invalid";
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
    bytes.reserve(64 + atlas.tileCount() * TileBytes);
    bytes.insert(bytes.end(), Magic.begin(), Magic.end());
    append32(bytes, FormatVersion);
    append8(bytes, static_cast<std::uint8_t>(identity.worldId.size()));
    bytes.insert(bytes.end(), identity.worldId.begin(),
                 identity.worldId.end());
    append32(bytes, static_cast<std::uint32_t>(identity.seed));
    append32(bytes, static_cast<std::uint32_t>(
                        identity.terrainGenerationVersion));
    append32(bytes, static_cast<std::uint32_t>(atlas.tileCount()));
    append32(bytes, 0); // Marker records enter a later format revision.
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
            if (!parseFile(candidate, validatedIdentity, validated,
                           validationError)) {
                return false;
            }
            if (validatedIdentity.worldId != identity.worldId ||
                validatedIdentity.seed != identity.seed ||
                validatedIdentity.terrainGenerationVersion !=
                    identity.terrainGenerationVersion ||
                validated.tileCount() != atlas.tileCount() ||
                validated.knownCellCount() != atlas.knownCellCount()) {
                validationError = "map candidate identity or count differs";
                return false;
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
    std::string localError;
    const bool valid = parseFile(path, identity, atlas, localError);
    if (error != nullptr) {
        *error = localError;
    }
    return valid;
}
