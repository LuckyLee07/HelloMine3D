#include "../Util/ResourcePaths.h"
#include "../World/Block/BlockId.h"
#include "../World/Storage/ChunkStorageData.h"
#include "../World/WorldConstants.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    std::string makeSmokeChunkRoot()
    {
        const auto now = std::chrono::system_clock::now()
                             .time_since_epoch()
                             .count();
        return ResourcePaths::bin("test_saves/save_load_smoke_" +
                                  std::to_string(now) + "/chunks");
    }

    std::size_t blockIndex(int x, int y, int z)
    {
        return static_cast<std::size_t>(y * CHUNK_AREA + z * CHUNK_SIZE + x);
    }

    bool expectEqual(const char *label, int actual, int expected)
    {
        if (actual == expected) {
            return true;
        }

        std::cerr << label << " expected " << expected << " but got "
                  << actual << "\n";
        return false;
    }
}

int main()
{
    const std::string chunkRoot = makeSmokeChunkRoot();
    const ChunkStorageData storage(chunkRoot);

    constexpr int targetX = 1;
    constexpr int targetY = 5;
    constexpr int targetZ = 1;
    const std::size_t targetIndex = blockIndex(targetX, targetY, targetZ);

    StoredChunkData saved;
    saved.x = -2;
    saved.z = 3;
    saved.sectionCount = 1;
    saved.blockIds.assign(CHUNK_VOLUME, static_cast<Block_t>(BlockId::Air));
    saved.metadata.assign(CHUNK_VOLUME, 0);
    saved.blockIds[targetIndex] = static_cast<Block_t>(BlockId::IronOre);
    saved.metadata[targetIndex] = 7;

    BlockEntityRecord record;
    record.position = {targetX, targetY, targetZ};
    record.type = "hellomine:test_block_entity";
    record.payload = "{\"value\":42}";
    saved.blockEntities.push_back(record);

    if (!storage.saveChunkData(saved)) {
        std::cerr << "Chunk save failed.\n";
        return EXIT_FAILURE;
    }

    StoredChunkData loaded;
    if (!storage.loadChunkData(saved.x, saved.z, loaded)) {
        std::cerr << "Chunk reload failed.\n";
        return EXIT_FAILURE;
    }

    bool passed = true;
    passed &= expectEqual("chunk x", loaded.x, saved.x);
    passed &= expectEqual("chunk z", loaded.z, saved.z);
    passed &= expectEqual("section count",
                          static_cast<int>(loaded.sectionCount),
                          static_cast<int>(saved.sectionCount));
    passed &= expectEqual("block count", static_cast<int>(loaded.blockIds.size()),
                          CHUNK_VOLUME);
    passed &= expectEqual("metadata count",
                          static_cast<int>(loaded.metadata.size()),
                          CHUNK_VOLUME);

    if (targetIndex < loaded.blockIds.size() &&
        targetIndex < loaded.metadata.size()) {
        passed &= expectEqual("edited block id", loaded.blockIds[targetIndex],
                              static_cast<int>(BlockId::IronOre));
        passed &= expectEqual("edited block metadata",
                              loaded.metadata[targetIndex], 7);
    }
    else {
        std::cerr << "Reloaded chunk payload is too small.\n";
        passed = false;
    }

    passed &= expectEqual("block entity count",
                          static_cast<int>(loaded.blockEntities.size()), 1);
    if (!loaded.blockEntities.empty()) {
        const auto &loadedRecord = loaded.blockEntities.front();
        passed &= expectEqual("block entity x", loadedRecord.position.x,
                              targetX);
        passed &= expectEqual("block entity y", loadedRecord.position.y,
                              targetY);
        passed &= expectEqual("block entity z", loadedRecord.position.z,
                              targetZ);
        if (loadedRecord.type != record.type ||
            loadedRecord.payload != record.payload) {
            std::cerr << "Block entity payload mismatch.\n";
            passed = false;
        }
    }

    if (!passed) {
        return EXIT_FAILURE;
    }

    std::cout << "World save/load smoke passed.\n";
    return EXIT_SUCCESS;
}
