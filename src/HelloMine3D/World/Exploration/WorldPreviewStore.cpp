#include "WorldPreviewStore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;

constexpr std::array<char, 8> Magic{{'H', 'M', '3', 'D', 'P', 'V', 'W', '1'}};
constexpr std::size_t KnownMaskBytes =
    (WorldPreviewStore::CellCount + 7) / 8;
constexpr std::uint64_t HashBasis = 14695981039346656037ull;
constexpr std::uint64_t HashPrime = 1099511628211ull;
constexpr int SourceRowsPerSide = WorldPreviewStore::Width;
constexpr int FirstStoredRow =
    (SourceRowsPerSide - WorldPreviewStore::Height) / 2;

bool validWorldId(const std::string& id)
{
    if (id.empty() || id.size() > 64 ||
        !((id.front() >= 'a' && id.front() <= 'z') ||
          (id.front() >= '0' && id.front() <= '9'))) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](unsigned char value) {
        return (value >= 'a' && value <= 'z') ||
               (value >= '0' && value <= '9') || value == '-' ||
               value == '_';
    });
}

bool validIdentity(const WorldPreviewStore::Identity& identity)
{
    return validWorldId(identity.worldId) &&
           identity.terrainGenerationVersion > 0 &&
           identity.terrainGenerationVersion <= 1000000 &&
           identity.lastPlayedUtc > 0 &&
           identity.lastPlayedUtc <= 253402300799ll;
}

bool validRevision(const ExplorationMapStore::SourceRevision& revision)
{
    return revision.fileBytes > 0 &&
           revision.fileBytes <= ExplorationMapStore::MaxFileBytes;
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

std::uint64_t hashPrefix(const std::vector<char>& bytes, std::size_t length)
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
        if (m_offset == m_bytes.size()) {
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
    const std::int64_t converted =
        value <= std::uint32_t(std::numeric_limits<std::int32_t>::max())
            ? std::int64_t(value)
            : std::int64_t(value) - 0x100000000ll;
    return static_cast<std::int32_t>(converted);
}

std::int64_t signed64(std::uint64_t value)
{
    std::int64_t converted = 0;
    static_assert(sizeof(converted) == sizeof(value),
                  "preview integer width changed");
    std::memcpy(&converted, &value, sizeof(converted));
    return converted;
}

std::uint32_t floatBits(float value)
{
    std::uint32_t result = 0;
    static_assert(sizeof(result) == sizeof(value),
                  "preview float width changed");
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

float bitsFloat(std::uint32_t value)
{
    float result = 0.f;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

bool readPreviewBytes(const std::string& path, std::vector<char>& bytes,
                      std::string& error)
{
    std::error_code statusError;
    const fs::file_status status = fs::symlink_status(path, statusError);
    if (statusError || fs::is_symlink(status) ||
        !fs::is_regular_file(status)) {
        error = "preview path is not a safe regular file";
        return false;
    }
    std::error_code sizeError;
    const std::uintmax_t size = fs::file_size(path, sizeError);
    if (sizeError || size < 64 || size > WorldPreviewStore::MaxFileBytes) {
        error = "preview file size is outside limit";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "preview file cannot be opened";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.peek() != std::char_traits<char>::eof()) {
        error = "preview file cannot be read completely";
        return false;
    }
    return true;
}

bool sameIdentity(const WorldPreviewStore::Identity& left,
                  const WorldPreviewStore::Identity& right)
{
    return left.worldId == right.worldId && left.seed == right.seed &&
           left.terrainGenerationVersion ==
               right.terrainGenerationVersion &&
           left.lastPlayedUtc == right.lastPlayedUtc;
}
}

const WorldPreviewStore::Cell& WorldPreviewStore::Preview::at(
    int column, int row) const
{
    if (column < 0 || column >= width || row < 0 || row >= height ||
        cells.size() != static_cast<std::size_t>(width * height)) {
        throw std::out_of_range("preview cell is outside the fixed grid");
    }
    return cells[static_cast<std::size_t>(row * width + column)];
}

std::size_t WorldPreviewStore::Preview::knownCellCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        cells.begin(), cells.end(), [](const Cell& cell) {
            return cell.known;
        }));
}

std::int64_t WorldPreviewStore::Preview::worldXAt(int column) const noexcept
{
    return static_cast<std::int64_t>(centerX) +
           static_cast<std::int64_t>(column - width / 2) * metresPerCell;
}

std::int64_t WorldPreviewStore::Preview::worldZAt(int row) const noexcept
{
    return static_cast<std::int64_t>(centerZ) +
           static_cast<std::int64_t>(row - height / 2) * metresPerCell;
}

WorldPreviewStore::WorldPreviewStore(std::string worldDirectory)
    : m_worldDirectory(std::move(worldDirectory))
{
    if (m_worldDirectory.empty()) {
        throw std::invalid_argument(
            "world preview store requires a world directory");
    }
}

std::string WorldPreviewStore::filePath() const
{
    return (fs::path(m_worldDirectory) / "world-preview.hmp").string();
}

bool WorldPreviewStore::parseFile(
    const std::string& path, Identity& identity,
    ExplorationMapStore::SourceRevision& sourceRevision, Preview& preview,
    std::string& error)
{
    identity = {};
    sourceRevision = {};
    preview = {};
    std::vector<char> bytes;
    if (!readPreviewBytes(path, bytes, error)) {
        return false;
    }
    if (!std::equal(Magic.begin(), Magic.end(), bytes.begin())) {
        error = "preview magic differs";
        return false;
    }
    const std::uint64_t expectedHash = hashPrefix(bytes, bytes.size() - 8);
    Reader hashReader(bytes);
    std::uint8_t ignored = 0;
    for (std::size_t index = 0; index < bytes.size() - 8; ++index) {
        hashReader.read8(ignored);
    }
    std::uint64_t storedHash = 0;
    if (!hashReader.read64(storedHash) || storedHash != expectedHash) {
        error = "preview checksum differs";
        return false;
    }

    Reader reader(bytes);
    for (std::size_t index = 0; index < Magic.size(); ++index) {
        reader.read8(ignored);
    }
    std::uint32_t version = 0;
    std::uint8_t idLength = 0;
    if (!reader.read32(version) || version != FormatVersion ||
        !reader.read8(idLength) || idLength == 0 || idLength > 64 ||
        reader.offset() + idLength > bytes.size() - 8) {
        error = "preview version or identity length differs";
        return false;
    }
    identity.worldId.reserve(idLength);
    for (std::uint8_t index = 0; index < idLength; ++index) {
        std::uint8_t byte = 0;
        reader.read8(byte);
        identity.worldId.push_back(static_cast<char>(byte));
    }

    std::uint32_t rawSeed = 0, rawTerrain = 0;
    std::uint64_t rawLastPlayed = 0, rawFileBytes = 0;
    std::uint64_t rawSourceChecksum = 0;
    std::uint32_t rawCenterX = 0, rawCenterZ = 0, rawYaw = 0;
    std::uint8_t width = 0, height = 0, scale = 0, reserved = 0;
    std::uint32_t storedKnown = 0;
    if (!reader.read32(rawSeed) || !reader.read32(rawTerrain) ||
        !reader.read64(rawLastPlayed) || !reader.read64(rawFileBytes) ||
        !reader.read64(rawSourceChecksum) ||
        !reader.read32(rawCenterX) || !reader.read32(rawCenterZ) ||
        !reader.read32(rawYaw) || !reader.read8(width) ||
        !reader.read8(height) || !reader.read8(scale) ||
        !reader.read8(reserved) || !reader.read32(storedKnown)) {
        error = "preview header is truncated";
        return false;
    }
    identity.seed = signed32(rawSeed);
    identity.terrainGenerationVersion = signed32(rawTerrain);
    identity.lastPlayedUtc = signed64(rawLastPlayed);
    sourceRevision.fileBytes = rawFileBytes;
    sourceRevision.trailingChecksum = rawSourceChecksum;
    preview.centerX = signed32(rawCenterX);
    preview.centerZ = signed32(rawCenterZ);
    preview.playerYaw = bitsFloat(rawYaw);
    if (!validIdentity(identity) || !validRevision(sourceRevision) ||
        !std::isfinite(preview.playerYaw) || width != Width ||
        height != Height || scale != MetresPerPixel || reserved != 0 ||
        storedKnown > CellCount) {
        error = "preview identity, dimensions or value is invalid";
        return false;
    }
    preview.width = width;
    preview.height = height;
    preview.metresPerCell = scale;

    const std::size_t expectedSize = reader.offset() + KnownMaskBytes +
                                     CellCount * 2 + 8;
    if (bytes.size() != expectedSize) {
        error = "preview payload length differs";
        return false;
    }
    std::array<std::uint8_t, KnownMaskBytes> known{};
    for (std::uint8_t& byte : known) {
        if (!reader.read8(byte)) {
            error = "preview known mask is truncated";
            return false;
        }
    }
    if ((known.back() & 0xfeu) != 0) {
        error = "preview known mask has out-of-range bits";
        return false;
    }
    preview.cells.resize(CellCount);
    std::size_t actualKnown = 0;
    for (std::size_t index = 0; index < CellCount; ++index) {
        std::uint8_t surfaceHeight = 0, material = 0;
        if (!reader.read8(surfaceHeight) || !reader.read8(material)) {
            error = "preview surface payload is truncated";
            return false;
        }
        const bool occupied =
            (known[index / 8] & std::uint8_t(1u << (index % 8))) != 0;
        if ((occupied && material >=
                             static_cast<std::uint8_t>(BlockId::NUM_TYPES)) ||
            (!occupied && (surfaceHeight != 0 || material != 0))) {
            error = "preview surface value is invalid";
            return false;
        }
        preview.cells[index] = {
            occupied, surfaceHeight, static_cast<BlockId>(material)};
        actualKnown += occupied;
    }
    if (actualKnown != storedKnown || reader.offset() != bytes.size() - 8) {
        error = "preview known count or payload end differs";
        return false;
    }
    return true;
}

WorldPreviewStore::LoadStatus WorldPreviewStore::load(
    const Identity& expected, Preview& preview, std::string* error) const
{
    preview = {};
    if (error != nullptr) {
        error->clear();
    }
    const auto fail = [&](LoadStatus status, const std::string& message) {
        if (error != nullptr) {
            *error = message;
        }
        return status;
    };
    std::error_code statusError;
    const fs::file_status status =
        fs::symlink_status(filePath(), statusError);
    if (statusError == std::errc::no_such_file_or_directory) {
        return LoadStatus::Absent;
    }
    if (statusError) {
        return fail(LoadStatus::Unreadable,
                    "preview path status cannot be read: " +
                        statusError.message());
    }
    if (fs::is_symlink(status) || !fs::is_regular_file(status)) {
        return fail(LoadStatus::UnsafePath,
                    "preview path is a symlink or non-regular file");
    }

    Identity loadedIdentity;
    ExplorationMapStore::SourceRevision storedSource;
    Preview loaded;
    std::string parseError;
    if (!parseFile(filePath(), loadedIdentity, storedSource, loaded,
                   parseError)) {
        const bool unreadable =
            parseError.find("cannot be opened") != std::string::npos ||
            parseError.find("cannot be read completely") !=
                std::string::npos;
        return fail(unreadable ? LoadStatus::Unreadable
                               : LoadStatus::Corrupt,
                    parseError);
    }
    if (!validIdentity(expected) ||
        loadedIdentity.worldId != expected.worldId ||
        loadedIdentity.seed != expected.seed ||
        loadedIdentity.terrainGenerationVersion !=
            expected.terrainGenerationVersion) {
        return fail(LoadStatus::IdentityMismatch,
                    "preview belongs to another world identity");
    }
    if (loadedIdentity.lastPlayedUtc != expected.lastPlayedUtc) {
        return fail(LoadStatus::StaleSource,
                    "preview last-played identity is stale");
    }

    ExplorationMapStore::SourceRevision currentSource;
    std::string sourceError;
    const ExplorationMapStore mapStore(m_worldDirectory);
    const auto sourceStatus =
        mapStore.sourceRevision(currentSource, &sourceError);
    if (sourceStatus ==
        ExplorationMapStore::SourceRevisionStatus::UnsafePath) {
        return fail(LoadStatus::UnsafePath, sourceError);
    }
    if (sourceStatus ==
        ExplorationMapStore::SourceRevisionStatus::Unreadable) {
        return fail(LoadStatus::Unreadable, sourceError);
    }
    if (sourceStatus !=
            ExplorationMapStore::SourceRevisionStatus::Available ||
        currentSource != storedSource) {
        return fail(LoadStatus::StaleSource,
                    sourceError.empty()
                        ? "preview source revision is stale"
                        : "preview source is unavailable: " + sourceError);
    }
    preview = std::move(loaded);
    return LoadStatus::Loaded;
}

bool WorldPreviewStore::save(
    const Identity& identity, int centerX, int centerZ, float playerYaw,
    const ExplorationAtlas& atlas,
    const StorageTransactionOptions& options,
    StorageTransactionMetrics* metrics) const
{
    const auto fail = [&](const std::string& message) {
        if (metrics != nullptr) {
            *metrics = {};
            metrics->error = message;
        }
        return false;
    };
    if (!validIdentity(identity) || !std::isfinite(playerYaw)) {
        return fail("preview identity or player yaw is invalid");
    }
    ExplorationMapStore::SourceRevision sourceRevision;
    std::string sourceError;
    const ExplorationMapStore mapStore(m_worldDirectory);
    if (mapStore.sourceRevision(sourceRevision, &sourceError) !=
        ExplorationMapStore::SourceRevisionStatus::Available) {
        return fail("preview source revision is unavailable: " +
                    sourceError);
    }

    std::error_code targetError;
    const fs::file_status targetStatus =
        fs::symlink_status(filePath(), targetError);
    if (targetError != std::errc::no_such_file_or_directory &&
        (targetError || fs::is_symlink(targetStatus) ||
         !fs::is_regular_file(targetStatus))) {
        return fail("preview target is a symlink or non-regular path");
    }

    const std::vector<ExplorationAtlas::OverviewSample> square =
        atlas.overviewAt(centerX, centerZ, SourceRowsPerSide,
                         MetresPerPixel);
    if (square.size() != static_cast<std::size_t>(
                             SourceRowsPerSide * SourceRowsPerSide)) {
        return fail("preview source overview could not be sampled");
    }
    Preview preview;
    preview.centerX = centerX;
    preview.centerZ = centerZ;
    preview.playerYaw = playerYaw;
    preview.cells.resize(CellCount);
    std::size_t knownCount = 0;
    for (int row = 0; row < Height; ++row) {
        for (int column = 0; column < Width; ++column) {
            const auto& sample = square[static_cast<std::size_t>(
                (row + FirstStoredRow) * SourceRowsPerSide + column)];
            Cell& cell = preview.cells[static_cast<std::size_t>(
                row * Width + column)];
            if (!sample.known) {
                continue;
            }
            cell = {true, sample.surface.height, sample.surface.material};
            ++knownCount;
        }
    }

    std::vector<char> bytes;
    bytes.reserve(MaxFileBytes);
    bytes.insert(bytes.end(), Magic.begin(), Magic.end());
    append32(bytes, FormatVersion);
    append8(bytes, static_cast<std::uint8_t>(identity.worldId.size()));
    bytes.insert(bytes.end(), identity.worldId.begin(),
                 identity.worldId.end());
    append32(bytes, static_cast<std::uint32_t>(identity.seed));
    append32(bytes, static_cast<std::uint32_t>(
                        identity.terrainGenerationVersion));
    append64(bytes, static_cast<std::uint64_t>(identity.lastPlayedUtc));
    append64(bytes, sourceRevision.fileBytes);
    append64(bytes, sourceRevision.trailingChecksum);
    append32(bytes, static_cast<std::uint32_t>(centerX));
    append32(bytes, static_cast<std::uint32_t>(centerZ));
    append32(bytes, floatBits(playerYaw));
    append8(bytes, Width);
    append8(bytes, Height);
    append8(bytes, MetresPerPixel);
    append8(bytes, 0);
    append32(bytes, static_cast<std::uint32_t>(knownCount));
    std::array<std::uint8_t, KnownMaskBytes> known{};
    for (std::size_t index = 0; index < preview.cells.size(); ++index) {
        if (preview.cells[index].known) {
            known[index / 8] |= std::uint8_t(1u << (index % 8));
        }
    }
    for (std::uint8_t byte : known) {
        append8(bytes, byte);
    }
    for (const Cell& cell : preview.cells) {
        append8(bytes, cell.known ? cell.height : 0);
        append8(bytes, cell.known
                           ? static_cast<std::uint8_t>(cell.material)
                           : 0);
    }
    append64(bytes, hashPrefix(bytes, bytes.size()));
    if (bytes.size() > MaxFileBytes) {
        return fail("preview exceeds file size limit");
    }

    return StorageTransaction::publish(
        filePath(), bytes,
        [&](const std::string& candidate, std::string& validationError) {
            Identity validatedIdentity;
            ExplorationMapStore::SourceRevision validatedSource;
            Preview validated;
            if (!parseFile(candidate, validatedIdentity, validatedSource,
                           validated, validationError)) {
                return false;
            }
            if (!sameIdentity(validatedIdentity, identity) ||
                validatedSource != sourceRevision ||
                validated.centerX != preview.centerX ||
                validated.centerZ != preview.centerZ ||
                floatBits(validated.playerYaw) !=
                    floatBits(preview.playerYaw) ||
                validated.cells.size() != preview.cells.size() ||
                validated.knownCellCount() != knownCount) {
                validationError =
                    "preview candidate identity or summary differs";
                return false;
            }
            for (std::size_t index = 0; index < preview.cells.size();
                 ++index) {
                const Cell& left = validated.cells[index];
                const Cell& right = preview.cells[index];
                if (left.known != right.known || left.height != right.height ||
                    left.material != right.material) {
                    validationError = "preview candidate surface differs";
                    return false;
                }
            }
            return true;
        }, options, metrics);
}

bool WorldPreviewStore::validateFile(const std::string& path,
                                     std::string* error)
{
    Identity identity;
    ExplorationMapStore::SourceRevision sourceRevision;
    Preview preview;
    std::string localError;
    const bool valid =
        parseFile(path, identity, sourceRevision, preview, localError);
    if (error != nullptr) {
        *error = localError;
    }
    return valid;
}
