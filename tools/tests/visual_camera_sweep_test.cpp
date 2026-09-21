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
    const auto hill = VisualCameraSweep::parse("40 0 0 0 0 6", true, true, false,
        "0 0 0 20 10 0 40 0 0");
    require(hill.offset(7)[1] == 10.0 && hill.offset(10) == hill.delta,
            "Intermediate hill follows the frozen path instead of clipping through it");
    require(hill.offset(4) == std::array<double, 5>{} && hill.offset(100) == hill.delta,
            "Waypoint route holds at both ends");
    for (const char *invalid : {"0 0 0", "0 0 0 20 10", "1 0 0 40 0 0",
            "0 0 0 49 0 0 40 0 0", "0 0 0 40 1 0", "0 0 0 nan 0 0 40 0 0",
            "0 0 0 40 0 0 extra", "0 0 0 -40 0 0 40 0 0",
            "0 0 0 1 0 0 2 0 0 3 0 0 4 0 0 5 0 0 6 0 0 7 0 0 8 0 0 9 0 0 10 0 0 11 0 0 12 0 0 40 0 0"}) {
        bool invalidRejected = false;
        try { VisualCameraSweep::parse("40 0 0 0 0 6", true, true, false, invalid); }
        catch (const std::runtime_error&) { invalidRejected = true; }
        require(invalidRejected, "Reject malformed or out-of-bounds waypoint path");
    }
    bool normalRejected = false;
    try { VisualCameraSweep::parse(nullptr, false, false, false, "0 0 0 1 0 0"); }
    catch (const std::runtime_error&) { normalRejected = true; }
    require(normalRejected, "Waypoints cannot activate normal-launch camera control");
    std::cout << "[VISUAL_CAMERA_SWEEP] checks=" << checks << " samples=12000 status=PASS\n";
}
