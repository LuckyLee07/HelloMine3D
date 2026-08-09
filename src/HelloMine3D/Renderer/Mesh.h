#ifndef RENDERER_MESH_H_INCLUDED
#define RENDERER_MESH_H_INCLUDED

#include <cstdint>
#include <vector>

/// @brief Mesh struct used for the purpose of constructing block meshes.
struct Mesh {
    std::vector<float> vertexPositions;
    std::vector<float> textureCoords;
    std::vector<std::uint32_t> indices;
};

#endif // RENDERER_MESH_H_INCLUDED
