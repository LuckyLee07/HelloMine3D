#include "../../src/HelloMine3D/Diagnostics/VisualCameraSweep.h"
#include <iostream>
#include <limits>

int main()
{
    int checks = 0;
    auto require = [&](bool condition, const char* name) {
        ++checks;
        if (!condition) throw std::runtime_error(name);
    };
    auto rejected = [](const char* value, bool capture = true,
                       bool world = true, bool scenario = false) {
        try { VisualCameraSweep::parse(value, capture, world, scenario); }
        catch (const std::runtime_error&) { return true; }
        return false;
    };
    require(!VisualCameraSweep::parse(nullptr, false, false, true).enabled,
            "Normal launch must remain disabled");
    require(!VisualCameraSweep::parse("", true, true, false).enabled,
            "Empty option must remain disabled");
    require(rejected("0 0 0 90 0 12", false), "Reject non-hidden capture");
    require(rejected("0 0 0 90 0 12", true, false), "Reject missing world");
    require(rejected("0 0 0 90 0 12", true, true, true), "Reject mixed scenario");
    for (const char* invalid : {"0 0 0 90 0", "0 0 0 90 0 12 extra", "nan 0 0 0 0 12",
         "0 0 0 0 0 inf", "49 0 0 0 0 12", "40 40 0 0 0 12", "0 0 0 181 0 12",
         "0 0 0 0 -46 12", "0 0 0 0 0 0", "0 0 0 0 0 31"})
        require(rejected(invalid), "Reject malformed or unbounded route");
    const auto route = VisualCameraSweep::parse("24 -4 -8 -120 30 12", true, true, false);
    require(route.enabled, "Enable valid route");
    require(route.offset(-1) == std::array<double, 5>{} &&
            route.offset(4) == std::array<double, 5>{}, "Hold during warmup");
    require(route.offset(16) == route.delta && route.offset(1000) == route.delta,
            "Hold at destination");
    require(route.offset(std::numeric_limits<double>::quiet_NaN()) == std::array<double, 5>{},
            "Nonfinite time cannot poison camera");
    auto previous = route.offset(4);
    for (int sample = 1; sample <= 12000; ++sample)
    {
        const auto value = route.offset(4 + sample * .001);
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const double direction = route.delta[i] > 0 ? 1 : -1;
            if (value[i] * direction < previous[i] * direction ||
                std::abs(value[i]) > std::abs(route.delta[i]) + 1e-9 ||
                std::abs(value[i] - previous[i]) > std::abs(route.delta[i]) / 12 * 1.501 * .001)
                throw std::runtime_error("Route discontinuity, overshoot or reversed motion");
        }
        previous = value;
    }
    require(true, "12000 samples have bounded forward motion");
    require(std::abs(route.offset(4.001)[3]) < .000003 &&
            std::abs(route.offset(15.999)[3] - route.delta[3]) < .000003,
            "Entry and exit velocity ease to zero");
    std::cout << "[VISUAL_CAMERA_SWEEP] checks=" << checks << " samples=12000 status=PASS\n";
}
