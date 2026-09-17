#pragma once

#include <array>
#include <vector>
#include "../Maths/glm.h"

// Pure presentation geometry. Tile coordinates and alpha are copied inputs;
// this builder owns neither world objects nor inventory state.
namespace ItemVisualGeometry {
constexpr int IconEdge = 16;
using Mask = std::array<bool, IconEdge * IconEdge>;
struct Face {
    std::array<glm::vec3, 4> positions;
    std::array<glm::vec2, 4> uv;
    glm::vec2 tile;
    glm::vec3 normal;
};
using Mesh = std::vector<Face>;
inline Face face(std::array<glm::vec3, 4> positions, glm::vec2 tile)
{
    return {positions, {{{0,0}, {0,1}, {1,1}, {1,0}}}, tile,
            glm::normalize(glm::cross(positions[1] - positions[0], positions[2] - positions[0]))};
}
inline Mesh cube(glm::vec2 top, glm::vec2 side, glm::vec2 bottom)
{
    return {
        face({{{-.5f,.5f,.5f}, {-.5f,-.5f,.5f}, {.5f,-.5f,.5f}, {.5f,.5f,.5f}}}, side),
        face({{{.5f,.5f,-.5f}, {.5f,-.5f,-.5f}, {-.5f,-.5f,-.5f}, {-.5f,.5f,-.5f}}}, side),
        face({{{.5f,.5f,.5f}, {.5f,-.5f,.5f}, {.5f,-.5f,-.5f}, {.5f,.5f,-.5f}}}, side),
        face({{{-.5f,.5f,-.5f}, {-.5f,-.5f,-.5f}, {-.5f,-.5f,.5f}, {-.5f,.5f,.5f}}}, side),
        face({{{-.5f,.5f,-.5f}, {-.5f,.5f,.5f}, {.5f,.5f,.5f}, {.5f,.5f,-.5f}}}, top),
        face({{{-.5f,-.5f,.5f}, {-.5f,-.5f,-.5f}, {.5f,-.5f,-.5f}, {.5f,-.5f,.5f}}}, bottom)
    };
}
inline Mesh extrudedIcon(glm::vec2 tile, const Mask& mask)
{
    constexpr float halfDepth = .075f;
    Mesh mesh;
    const auto opaque = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < IconEdge && y < IconEdge && mask[y * IconEdge + x];
    };
    bool any = false;
    for (bool value : mask) any |= value;
    if (!any) return mesh;
    mesh.push_back(face({{{-.5f,.5f,halfDepth}, {-.5f,-.5f,halfDepth},
                         {.5f,-.5f,halfDepth}, {.5f,.5f,halfDepth}}}, tile));
    auto back = face({{{.5f,.5f,-halfDepth}, {.5f,-.5f,-halfDepth},
                      {-.5f,-.5f,-halfDepth}, {-.5f,.5f,-halfDepth}}}, tile);
    for (auto& uv : back.uv) uv.x = 1.f - uv.x;
    mesh.push_back(back);
    for (int y = 0; y < IconEdge; ++y) {
        for (int x = 0; x < IconEdge; ++x) {
            if (!opaque(x, y)) continue;
            const float left = x / 16.f - .5f, right = (x + 1) / 16.f - .5f;
            const float top = .5f - y / 16.f, bottom = .5f - (y + 1) / 16.f;
            const auto edge = [&](std::array<glm::vec3, 4> points) {
                Face side = face(points, tile);
                side.uv.fill(glm::vec2((x + .5f) / 16.f, (y + .5f) / 16.f));
                mesh.push_back(side);
            };
            if (!opaque(x - 1, y))
                edge({{{left,top,-halfDepth}, {left,bottom,-halfDepth}, {left,bottom,halfDepth}, {left,top,halfDepth}}});
            if (!opaque(x + 1, y))
                edge({{{right,top,halfDepth}, {right,bottom,halfDepth}, {right,bottom,-halfDepth}, {right,top,-halfDepth}}});
            if (!opaque(x, y - 1))
                edge({{{left,top,-halfDepth}, {left,top,halfDepth}, {right,top,halfDepth}, {right,top,-halfDepth}}});
            if (!opaque(x, y + 1))
                edge({{{left,bottom,halfDepth}, {left,bottom,-halfDepth}, {right,bottom,-halfDepth}, {right,bottom,halfDepth}}});
        }
    }
    return mesh;
}
}
