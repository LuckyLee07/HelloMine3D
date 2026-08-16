#include "ChunkStorageData.h"

#include "../../Util/ResourcePaths.h"
#include "../WorldConstants.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace
{
    constexpr std::array<char, 8> ChunkMagic{
        {'H', 'M', 'C', 'H', 'N', 'K', '1', '\0'}};
    constexpr std::uint32_t ChunkFormatVersion = 2;
    constexpr std::uint32_t MinSupportedChunkFormatVersion = 1;
    constexpr std::uint32_t MaxStoredSections = 64;
    constexpr std::uint32_t MaxStoredBlockEntities = 4096;

    bool createDirectory(const std::string &path)
    {
        if (path.empty() || ResourcePaths::exists(path)) {
            return true;
        }

        const auto parent = ResourcePaths::parentPath(path);
        if (parent != path && !createDirectory(parent)) {
            return false;
        }

#if defined(_WIN32)
        if (_mkdir(path.c_str()) == 0) {
#else
        if (mkdir(path.c_str(), 0755) == 0) {
#endif
            return true;
        }

        return errno == EEXIST;
    }

    template <typename T> bool readValue(std::istream &stream, T &value)
    {
        stream.read(reinterpret_cast<char *>(&value), sizeof(T));
        return static_cast<bool>(stream);
    }

    template <typename T> void writeValue(std::ostream &stream, const T &value)
    {
        stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
    }

    bool readString(std::istream &stream, std::string &value,
                    std::size_t maximumSize)
    {
        std::uint32_t size = 0;
        if (!readValue(stream, size) || size > maximumSize) {
            return false;
        }

        value.resize(size);
        if (size > 0) {
            stream.read(&value[0], static_cast<std::streamsize>(size));
        }

        return static_cast<bool>(stream);
    }

    void writeString(std::ostream &stream, const std::string &value)
    {
        writeValue(stream, static_cast<std::uint32_t>(value.size()));
        if (!value.empty()) {
            stream.write(value.data(), static_cast<std::streamsize>(
                                           value.size()));
        }
    }

    bool readBlockEntities(std::istream &stream,
                           std::vector<BlockEntityRecord> &blockEntities)
    {
        std::uint32_t blockEntityCount = 0;
        if (!readValue(stream, blockEntityCount) ||
            blockEntityCount > MaxStoredBlockEntities) {
            return false;
        }

        blockEntities.clear();
        blockEntities.reserve(blockEntityCount);
        for (std::uint32_t i = 0; i < blockEntityCount; ++i) {
            BlockEntityRecord record;
            if (!readValue(stream, record.position.x) ||
                !readValue(stream, record.position.y) ||
                !readValue(stream, record.position.z) ||
                !readString(stream, record.type, MaxBlockEntityTypeSize) ||
                !readString(stream, record.payload,
                            MaxBlockEntityPayloadSize)) {
                return false;
            }

            blockEntities.push_back(std::move(record));
        }

        return true;
    }

    void writeBlockEntities(
        std::ostream &stream,
        const std::vector<BlockEntityRecord> &blockEntities)
    {
        writeValue(stream, static_cast<std::uint32_t>(blockEntities.size()));
        for (const auto &record : blockEntities) {
            writeValue(stream, static_cast<std::int32_t>(record.position.x));
            writeValue(stream, static_cast<std::int32_t>(record.position.y));
            writeValue(stream, static_cast<std::int32_t>(record.position.z));
            writeString(stream, record.type);
            writeString(stream, record.payload);
        }
    }

    bool validateBlockEntityAttachments(
        const std::vector<BlockEntityRecord> &blockEntities,
        const std::vector<Block_t> &blockIds, std::string *errorMessage)
    {
        for (const BlockEntityRecord &record : blockEntities) {
            const std::size_t index =
                static_cast<std::size_t>(record.position.y) * CHUNK_AREA +
                static_cast<std::size_t>(record.position.z) * CHUNK_SIZE +
                static_cast<std::size_t>(record.position.x);
            if (index >= blockIds.size() ||
                blockIds[index] == static_cast<Block_t>(BlockId::Air)) {
                if (errorMessage != nullptr) {
                    *errorMessage =
                        "block entity record is not attached to a block";
                }
                return false;
            }
        }
        return true;
    }

    bool loadChunkDataFromPath(const std::string &path, int x, int z,
                               StoredChunkData &data,
                               std::string *errorMessage,
                               bool logFailure)
    {
        const auto fail = [&](const std::string &message) {
            if (errorMessage != nullptr) {
                *errorMessage = message;
            }
            if (logFailure) {
                std::cerr << message << ": " << path << '\n';
            }
            return false;
        };

        std::ifstream input(path, std::ios::binary);
        if (!input.is_open()) {
            return fail("Unable to open chunk file");
        }

        std::array<char, ChunkMagic.size()> magic{};
        input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        if (!input || magic != ChunkMagic) {
            return fail("Invalid chunk file magic");
        }

        std::uint32_t version = 0;
        std::int32_t storedX = 0;
        std::int32_t storedZ = 0;
        std::uint32_t chunkSize = 0;
        std::uint32_t sectionCount = 0;
        if (!readValue(input, version) || !readValue(input, storedX) ||
            !readValue(input, storedZ) || !readValue(input, chunkSize) ||
            !readValue(input, sectionCount)) {
            return fail("Truncated chunk header");
        }

        if (version < MinSupportedChunkFormatVersion ||
            version > ChunkFormatVersion || storedX != x || storedZ != z ||
            chunkSize != CHUNK_SIZE || sectionCount > MaxStoredSections) {
            return fail("Unsupported chunk file");
        }

        const auto blockCount =
            static_cast<std::size_t>(sectionCount) * CHUNK_VOLUME;
        std::vector<Block_t> blockIds(blockCount);
        if (blockCount > 0) {
            input.read(reinterpret_cast<char *>(blockIds.data()),
                       static_cast<std::streamsize>(blockIds.size() *
                                                    sizeof(Block_t)));
        }
        if (!input) {
            return fail("Truncated chunk block data");
        }

        std::vector<BlockMetadata_t> metadata(blockCount, 0);
        std::vector<BlockEntityRecord> blockEntities;
        if (version >= 2) {
            if (blockCount > 0) {
                input.read(reinterpret_cast<char *>(metadata.data()),
                           static_cast<std::streamsize>(metadata.size() *
                                                        sizeof(BlockMetadata_t)));
            }
            if (!input) {
                return fail("Truncated chunk metadata");
            }
            if (!readBlockEntities(input, blockEntities)) {
                return fail("Truncated or invalid chunk block entity data");
            }
        }

        if (input.peek() != std::char_traits<char>::eof()) {
            return fail("Unexpected trailing chunk data");
        }

        std::string blockEntityError;
        if (!validateBlockEntityRecords(blockEntities, sectionCount,
                                        &blockEntityError)) {
            return fail("Invalid chunk block entity data: " +
                        blockEntityError);
        }
        if (!validateBlockEntityAttachments(blockEntities, blockIds,
                                            &blockEntityError)) {
            return fail("Invalid chunk block entity data: " +
                        blockEntityError);
        }

        data.x = x;
        data.z = z;
        data.sectionCount = sectionCount;
        data.blockIds = std::move(blockIds);
        data.metadata = std::move(metadata);
        data.blockEntities = std::move(blockEntities);
        return true;
    }
}

ChunkStorageData::ChunkStorageData(std::string rootDirectory)
    : m_rootDirectory(std::move(rootDirectory))
{
}

bool ChunkStorageData::loadChunkData(int x, int z, StoredChunkData &data) const
{
    return loadChunkDataFromPath(chunkPath(x, z), x, z, data, nullptr,
                                 true);
}

bool ChunkStorageData::loadChunkFile(const std::string &path, int x, int z,
                                     StoredChunkData &data,
                                     std::string *errorMessage)
{
    return loadChunkDataFromPath(path, x, z, data, errorMessage, false);
}

bool ChunkStorageData::saveChunkData(const StoredChunkData &data) const
{
    return saveChunkData(data, {}, nullptr);
}

bool ChunkStorageData::saveChunkData(
    const StoredChunkData &data, const StorageTransactionOptions &options,
    StorageTransactionMetrics *metrics) const
{
    if (!ensureRootDirectory()) {
        std::cerr << "Unable to create chunk save directory: "
                  << m_rootDirectory << '\n';
        return false;
    }

    if (data.sectionCount > MaxStoredSections ||
        data.sectionCount >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
        std::cerr << "Unable to save oversized chunk data: "
                  << chunkPath(data.x, data.z) << '\n';
        return false;
    }

    const auto expectedBlockCount = data.sectionCount * CHUNK_VOLUME;
    if (data.blockIds.size() != expectedBlockCount ||
        data.metadata.size() != expectedBlockCount) {
        std::cerr << "Unable to save incomplete chunk data: "
                  << chunkPath(data.x, data.z) << '\n';
        return false;
    }

    if (data.blockEntities.size() > MaxStoredBlockEntities) {
        std::cerr << "Unable to save too many chunk block entities: "
                  << chunkPath(data.x, data.z) << '\n';
        return false;
    }
    std::string blockEntityError;
    if (!validateBlockEntityRecords(data.blockEntities, data.sectionCount,
                                    &blockEntityError)) {
        std::cerr << "Unable to save invalid chunk block entities: "
                  << blockEntityError << " in "
                  << chunkPath(data.x, data.z) << '\n';
        return false;
    }
    if (!validateBlockEntityAttachments(data.blockEntities, data.blockIds,
                                        &blockEntityError)) {
        std::cerr << "Unable to save invalid chunk block entities: "
                  << blockEntityError << " in "
                  << chunkPath(data.x, data.z) << '\n';
        return false;
    }

    std::ostringstream output(std::ios::out | std::ios::binary);

    output.write(ChunkMagic.data(),
                 static_cast<std::streamsize>(ChunkMagic.size()));
    writeValue(output, ChunkFormatVersion);
    writeValue(output, static_cast<std::int32_t>(data.x));
    writeValue(output, static_cast<std::int32_t>(data.z));
    writeValue(output, static_cast<std::uint32_t>(CHUNK_SIZE));
    writeValue(output, static_cast<std::uint32_t>(data.sectionCount));

    if (expectedBlockCount > 0) {
        output.write(reinterpret_cast<const char *>(data.blockIds.data()),
                     static_cast<std::streamsize>(expectedBlockCount *
                                                  sizeof(Block_t)));
        output.write(reinterpret_cast<const char *>(data.metadata.data()),
                     static_cast<std::streamsize>(expectedBlockCount *
                                                  sizeof(BlockMetadata_t)));
    }
    writeBlockEntities(output, data.blockEntities);

    if (!output) {
        std::cerr << "Unable to serialize chunk save file: "
                  << chunkPath(data.x, data.z) << '\n';
        return false;
    }

    const std::string serialized = output.str();
    const std::vector<char> payload(serialized.begin(), serialized.end());
    const auto validator = [x = data.x, z = data.z](
                               const std::string &candidate,
                               std::string &validationError) {
        StoredChunkData validated;
        return ChunkStorageData::loadChunkFile(candidate, x, z, validated,
                                               &validationError);
    };
    StorageTransactionMetrics localMetrics;
    StorageTransactionMetrics *resultMetrics =
        metrics != nullptr ? metrics : &localMetrics;
    if (!StorageTransaction::publish(chunkPath(data.x, data.z), payload,
                                     validator, options, resultMetrics)) {
        std::cerr << "Unable to publish chunk save file: "
                  << chunkPath(data.x, data.z);
        if (!resultMetrics->error.empty()) {
            std::cerr << " (" << resultMetrics->error << ')';
        }
        std::cerr << '\n';
        return false;
    }
    return true;
}

std::string ChunkStorageData::chunkPath(int x, int z) const
{
    return ResourcePaths::join(m_rootDirectory,
                               "chunk_" + std::to_string(x) + "_" +
                                   std::to_string(z) + ".hmcchunk");
}

bool ChunkStorageData::ensureRootDirectory() const
{
    return createDirectory(m_rootDirectory);
}
