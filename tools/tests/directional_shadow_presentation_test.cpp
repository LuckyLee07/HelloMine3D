#include "Presentation/DirectionalShadowPresentation.h"
#include <iostream>
#include <stdexcept>

int main()
{
    using namespace DirectionalShadowPresentation;
    int checks = 0;
    auto check = [&](const char* name, bool ok) {
        if (!ok) throw std::runtime_error(name);
        ++checks; std::cout << "[SHADOW_CAMERA] PASS " << name << '\n';
    };
    for (unsigned size : {1024u, 2048u}) {
        const float distance = size == 1024 ? 64.f : 96.f;
        auto previous = cameraFrame({-375.f, 71.f, -375.f}, {0.001f, 1.f, 0.00018f}, distance, size);
        bool continuous = true, orthonormal = true;
        for (int tick = 0; tick <= 200; ++tick) {
            const float x = 0.001f - tick * 0.00001f;
            const auto frame = cameraFrame({-375.f, 71.f, -375.f}, {x, 1.f, x * 0.18f}, distance, size);
            continuous &= glm::dot(previous.right, frame.right) > .99999f &&
                glm::dot(previous.up, frame.up) > .99999f;
            orthonormal &= std::abs(glm::dot(frame.right, frame.up)) < .00001f &&
                glm::dot(glm::cross(frame.right, frame.up), frame.back) > .99999f;
            previous = frame;
        }
        check("noon-basis-does-not-flip", continuous);
        check("right-handed-orthonormal", orthonormal);
        const float texel = 2.f * distance / size;
        const auto first = cameraFrame({0,0,0}, {0,1,0}, distance, size);
        const auto tiny = cameraFrame({texel*.2f,0,texel*.2f}, {0,1,0}, distance, size);
        check("subtexel-camera-movement-stable", glm::length(first.position - tiny.position) < .00001f);
        const auto negative = cameraFrame({-texel*.7f,0,-texel*.7f}, {0,1,0}, distance, size);
        const auto positive = cameraFrame({texel*.7f,0,texel*.7f}, {0,1,0}, distance, size);
        check("origin-cells-symmetric", std::abs(negative.position.x + positive.position.x) < .00001f &&
            std::abs(positive.position.x - texel) < .00001f);
        check("camera-extrusion-keeps-depth", std::abs(glm::dot(first.position, first.back)-distance)<.00001f);
    }
    const auto z = cameraFrame({1,2,3}, {0,0,1}, 64, 512);
    check("non-solar-pole-remains-finite", std::isfinite(z.position.x) && glm::length(z.right) > .999f);
    std::cout << "checks=" << checks << " failures=0\n";
}
