#ifndef WORLDCOORDINATES_H_INCLUDED
#define WORLDCOORDINATES_H_INCLUDED

#include "../Maths/Vector2XZ.h"

namespace WorldCoordinates
{
    int toBlockCoord(float value);
    int floorDiv(int value, int divisor);
    int floorMod(int value, int divisor);
    VectorXZ getBlockXZ(int x, int z);
    VectorXZ getChunkXZ(int x, int z);
}

#endif // WORLDCOORDINATES_H_INCLUDED
