#pragma once

#include <array>
#include <cmath>
#include <glm/glm.hpp>

// Render-only spit geometry. Local -Z is the leading end; sizes are scaled by
// the real collision diameter. No extra particles, blending or world state.
namespace ProjectilePresentation
{
constexpr float SurfaceRole = 10.f;
struct Mesh
{
    std::array<glm::vec3, 18> positions;
    std::array<unsigned int, 96> indices;
};

inline const Mesh& mesh()
{
    static const Mesh value = [] {
        Mesh result{};
        result.positions[0] = {0.f, 0.f, -.50f};
        result.positions[17] = {0.f, 0.f, .65f};
        for (unsigned i = 0; i < 8; ++i) {
            const float angle = static_cast<float>(i) * .7853981633974483f;
            result.positions[1+i] = {std::cos(angle)*.39f, std::sin(angle)*.39f, -.18f};
            result.positions[9+i] = {std::cos(angle)*.28f, std::sin(angle)*.28f, .20f};
            const unsigned next = (i+1)%8;
            const std::array<unsigned, 12> triangles{{
                0, 1+next, 1+i,
                1+i, 1+next, 9+next, 1+i, 9+next, 9+i,
                17, 9+i, 9+next}};
            for (unsigned j = 0; j < triangles.size(); ++j)
                result.indices[i*triangles.size()+j] = triangles[j];
        }
        return result;
    }();
    return value;
}

struct Frame { glm::vec3 right, up, back; };
inline Frame frame(const glm::vec3& velocity)
{
    const glm::vec3 forward = glm::normalize(velocity);
    const glm::vec3 reference = std::abs(forward.y) < .95f
        ? glm::vec3(0,1,0) : glm::vec3(0,0,1);
    const glm::vec3 right = glm::normalize(glm::cross(forward, reference));
    return {right, glm::cross(right, forward), -forward};
}

inline bool validRadius(float radius)
{
    return std::isfinite(radius) && radius > 0.f && radius <= .5f;
}
}
