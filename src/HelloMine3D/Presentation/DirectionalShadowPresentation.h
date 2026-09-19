#pragma once

#include <cmath>
#include <glm/glm.hpp>

// Render-only basis and texel anchoring. The solar orbit stays close to XY;
// world Z is a stable reference even when the sun passes directly overhead.
namespace DirectionalShadowPresentation
{
struct Frame
{
    glm::vec3 right;
    glm::vec3 up;
    glm::vec3 back;
    glm::vec3 position;
};

inline Frame cameraFrame(const glm::vec3& target, const glm::vec3& towardSun,
                         float distance, unsigned int textureSize)
{
    Frame frame;
    frame.back = glm::normalize(towardSun);
    const glm::vec3 reference = std::abs(frame.back.z) < 0.95f
        ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    frame.right = glm::normalize(glm::cross(reference, frame.back));
    frame.up = glm::cross(frame.back, frame.right);
    const float texel = 2.f * distance / static_cast<float>(textureSize);
    const glm::vec3 position = target + frame.back * distance;
    // Round symmetrically across the origin. Using fmod truncates towards zero
    // and creates a double-width cell there, which is visible during movement.
    const float x = glm::dot(position, frame.right);
    const float y = glm::dot(position, frame.up);
    frame.position = position + frame.right * (std::round(x / texel) * texel - x)
        + frame.up * (std::round(y / texel) * texel - y);
    return frame;
}
}
