#include "WorldCoordinates.h"

#include "WorldConstants.h"

#include <cmath>

namespace WorldCoordinates
{
    int toBlockCoord(float value)
    {
        return static_cast<int>(std::floor(value));
    }

    int floorDiv(int value, int divisor)
    {
        int quotient = value / divisor;
        int remainder = value % divisor;
        if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
            --quotient;
        }

        return quotient;
    }

    int floorMod(int value, int divisor)
    {
        int remainder = value % divisor;
        if (remainder < 0) {
            remainder += divisor;
        }

        return remainder;
    }

    VectorXZ getBlockXZ(int x, int z)
    {
        return {floorMod(x, CHUNK_SIZE), floorMod(z, CHUNK_SIZE)};
    }

    VectorXZ getChunkXZ(int x, int z)
    {
        return {floorDiv(x, CHUNK_SIZE), floorDiv(z, CHUNK_SIZE)};
    }
}
