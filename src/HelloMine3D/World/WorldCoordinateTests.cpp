#include "WorldCoordinateTests.h"

#include "WorldCoordinates.h"
#include "WorldConstants.h"

#include <iostream>

namespace
{
    bool expectEqual(const char *name, int actual, int expected)
    {
        if (actual == expected) {
            return true;
        }

        std::cerr << name << " expected " << expected << " but got "
                  << actual << "\n";
        return false;
    }
}

namespace WorldCoordinateTests
{
    bool run()
    {
        bool passed = true;

        struct DivCase {
            int value;
            int divisor;
            int quotient;
            int remainder;
        };

        const DivCase divCases[] = {
            {0, CHUNK_SIZE, 0, 0},
            {1, CHUNK_SIZE, 0, 1},
            {CHUNK_SIZE - 1, CHUNK_SIZE, 0, CHUNK_SIZE - 1},
            {CHUNK_SIZE, CHUNK_SIZE, 1, 0},
            {CHUNK_SIZE + 1, CHUNK_SIZE, 1, 1},
            {-1, CHUNK_SIZE, -1, CHUNK_SIZE - 1},
            {-CHUNK_SIZE, CHUNK_SIZE, -1, 0},
            {-CHUNK_SIZE - 1, CHUNK_SIZE, -2, CHUNK_SIZE - 1},
            {-CHUNK_SIZE * 2, CHUNK_SIZE, -2, 0},
            {-CHUNK_SIZE * 2 - 1, CHUNK_SIZE, -3, CHUNK_SIZE - 1},
        };

        for (const auto &testCase : divCases) {
            passed &= expectEqual("WorldCoordinates::floorDiv",
                                  WorldCoordinates::floorDiv(
                                      testCase.value, testCase.divisor),
                                  testCase.quotient);
            passed &= expectEqual("WorldCoordinates::floorMod",
                                  WorldCoordinates::floorMod(
                                      testCase.value, testCase.divisor),
                                  testCase.remainder);
        }

        struct CoordCase {
            int world;
            int chunk;
            int block;
        };

        const CoordCase coordCases[] = {
            {0, 0, 0},
            {1, 0, 1},
            {CHUNK_SIZE - 1, 0, CHUNK_SIZE - 1},
            {CHUNK_SIZE, 1, 0},
            {CHUNK_SIZE + 1, 1, 1},
            {-1, -1, CHUNK_SIZE - 1},
            {-CHUNK_SIZE, -1, 0},
            {-CHUNK_SIZE - 1, -2, CHUNK_SIZE - 1},
            {-CHUNK_SIZE * 2, -2, 0},
            {-CHUNK_SIZE * 2 - 1, -3, CHUNK_SIZE - 1},
        };

        for (const auto &testCase : coordCases) {
            const VectorXZ chunk = WorldCoordinates::getChunkXZ(
                testCase.world, testCase.world);
            const VectorXZ block = WorldCoordinates::getBlockXZ(
                testCase.world, testCase.world);

            passed &= expectEqual("WorldCoordinates::getChunkXZ.x", chunk.x,
                                  testCase.chunk);
            passed &= expectEqual("WorldCoordinates::getBlockXZ.x", block.x,
                                  testCase.block);
            passed &= expectEqual("WorldCoordinates::getChunkXZ.z", chunk.z,
                                  testCase.chunk);
            passed &= expectEqual("WorldCoordinates::getBlockXZ.z", block.z,
                                  testCase.block);
        }

        struct FloatCase {
            float value;
            int block;
        };

        const FloatCase floatCases[] = {
            {0.f, 0},
            {0.99f, 0},
            {1.f, 1},
            {-0.01f, -1},
            {-1.f, -1},
            {-1.01f, -2},
        };

        for (const auto &testCase : floatCases) {
            passed &= expectEqual("WorldCoordinates::toBlockCoord",
                                  WorldCoordinates::toBlockCoord(
                                      testCase.value),
                                  testCase.block);
        }

        if (passed) {
            std::cout << "World coordinate tests passed.\n";
        }

        return passed;
    }
}
