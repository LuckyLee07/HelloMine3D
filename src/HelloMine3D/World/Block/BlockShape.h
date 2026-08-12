#ifndef BLOCKSHAPE_H_INCLUDED
#define BLOCKSHAPE_H_INCLUDED

#include <array>
#include <string>
#include <vector>

using BlockShapeFace = std::array<float, 12>;

struct BlockShape {
    std::string name = "Cube";
    std::vector<BlockShapeFace> faces;
};

BlockShape loadBlockShape(const std::string &name,
                          const std::string &shapeDirectory);

#endif // BLOCKSHAPE_H_INCLUDED
