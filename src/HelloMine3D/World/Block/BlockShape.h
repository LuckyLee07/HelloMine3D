#ifndef BLOCKSHAPE_H_INCLUDED
#define BLOCKSHAPE_H_INCLUDED

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using BlockShapeFace = std::array<float, 12>;

// v2 boxes use a fixed eighth-metre grid, never a second world voxel store.
struct BlockShapeBox {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
    bool collidable = true;
    bool selectable = true;
    // front, back, left, right, top, bottom; 0=top, 1=side, 2=bottom.
    std::array<unsigned char, 6> materials{{1,1,1,1,0,2}};
};
struct BlockShapeSurface {
    BlockShapeFace positions{};
    std::array<float, 8> repeat{};
    unsigned char material = 1;
    // 0..5 front/back/left/right/top/bottom; 6 is an interior plane.
    unsigned char boundaryFace = 6;
    std::uint64_t boundaryMask = 0;
};
struct BlockShapeVariant {
    std::vector<BlockShapeBox> boxes;
    std::vector<BlockShapeSurface> surfaces;
    std::array<std::uint64_t, 6> boundaryCoverage{};
};
struct BlockShape {
    std::string name = "Cube";
    std::vector<BlockShapeFace> faces;
    int version = 1;
    bool fillsCell = false;
    bool fillsCollisionCell = false;
    std::array<BlockShapeVariant, 4> variants;
    static constexpr std::size_t MaxBoxes = 8;
    static constexpr std::size_t MaxSurfaces = 384;
    bool isCompound() const noexcept { return version == 2; }

};

BlockShape loadBlockShape(const std::string &name,
                          const std::string &shapeDirectory);
BlockShape loadBlockShapeFile(const std::string &name,
                              const std::string &path);

#endif // BLOCKSHAPE_H_INCLUDED
