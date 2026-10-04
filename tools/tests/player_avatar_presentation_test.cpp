#include "Presentation/PlayerAvatarPresentation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <type_traits>

namespace Avatar = PlayerAvatarPresentation;

namespace
{
    int failures = 0;
    int checks = 0;

    void check(bool condition, const std::string& label)
    {
        ++checks;
        if (condition) return;
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }

    bool near(float left, float right, float tolerance = .001f)
    {
        return std::abs(left - right) <= tolerance;
    }

    bool finite(Avatar::Vec3 value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
               std::isfinite(value.z);
    }

    const Avatar::PartTransform& part(const Avatar::Pose& pose,
                                      const Avatar::Profile& profile,
                                      Avatar::PartRole role)
    {
        const std::size_t index = Avatar::partIndex(profile, role);
        if (index >= profile.partCount) {
            std::cerr << "missing part in test\n";
            std::abort();
        }
        return pose.parts[index];
    }

    float articulationMagnitude(const Avatar::Pose& pose,
                                const Avatar::Profile& profile)
    {
        float result = std::abs(pose.rootOffset.x) +
                       std::abs(pose.rootOffset.y) +
                       std::abs(pose.rootOffset.z) +
                       std::abs(pose.rootRotationDegrees.x) +
                       std::abs(pose.rootRotationDegrees.y) +
                       std::abs(pose.rootRotationDegrees.z);
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            const Avatar::Vec3 r = pose.parts[index].rotationDegrees;
            result += std::abs(r.x) + std::abs(r.y) + std::abs(r.z);
        }
        return result;
    }

    void profileCase()
    {
        const Avatar::Profile profile = Avatar::defaultProfile();
        check(profile.partCount == 8, "default profile has eight bounded parts");
        check(profile.partCount <= Avatar::Profile::MaximumParts,
              "profile part count respects capacity");
        check(Avatar::Profile::MaximumTriangles == 96,
              "eight cuboids stay within 96 triangles");

        std::set<int> roles;
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            const Avatar::PartDefinition& value = profile.parts[index];
            roles.insert(static_cast<int>(value.role));
            check(finite(value.centre) && finite(value.size) && finite(value.pivot),
                  "profile geometry remains finite");
            check(value.size.x > 0.f && value.size.y > 0.f && value.size.z > 0.f,
                  "profile boxes have positive extents");
            check(value.size.x <= 1.f && value.size.y <= 1.f && value.size.z <= 1.f,
                  "profile boxes stay player scale");
        }
        check(roles.size() == profile.partCount, "default roles are unique");
        for (Avatar::PartRole required : {Avatar::PartRole::Head,
             Avatar::PartRole::Torso, Avatar::PartRole::LeftArm,
             Avatar::PartRole::RightArm, Avatar::PartRole::LeftLeg,
             Avatar::PartRole::RightLeg}) {
            check(Avatar::partIndex(profile, required) < profile.partCount,
                  "all six body roles are present");
        }
        check(std::is_trivially_copyable<Avatar::Snapshot>::value &&
              std::is_trivially_copyable<Avatar::Pose>::value,
              "snapshot and pose remain copy-only values");

        Avatar::Profile malformed = profile;
        malformed.partCount = 999;
        const Avatar::Pose malformedPose = Avatar::derivePose({}, malformed);
        check(finite(malformedPose.rootOffset) &&
              Avatar::partIndex(malformed, static_cast<Avatar::PartRole>(999)) ==
                  Avatar::Profile::MaximumParts,
              "malformed public part count is capped to array capacity");
    }

    void gaitCase()
    {
        const Avatar::Profile profile = Avatar::defaultProfile();
        Avatar::Snapshot input;
        check(Avatar::derivePose(input, profile).kind == Avatar::PoseKind::Idle,
              "stationary grounded pose is Idle");
        input.grounded = true;
        input.movementSeconds = .19f;
        input.movementStrength = 1.f;
        input.velocity = {0.f, 0.f, -4.5f};
        const Avatar::Pose pose = Avatar::derivePose(
            input, profile, Avatar::MotionStrength::Full);
        const float leftLeg = part(pose, profile,
            Avatar::PartRole::LeftLeg).rotationDegrees.x;
        const float rightLeg = part(pose, profile,
            Avatar::PartRole::RightLeg).rotationDegrees.x;
        const float leftArm = part(pose, profile,
            Avatar::PartRole::LeftArm).rotationDegrees.x;
        const float rightArm = part(pose, profile,
            Avatar::PartRole::RightArm).rotationDegrees.x;
        check(pose.kind == Avatar::PoseKind::Walk, "moving grounded pose is Walk");
        check(std::abs(leftLeg) > 20.f, "walk sample has visible leg swing");
        check(near(leftLeg, -rightLeg), "left and right legs are opposite");
        check(near(leftArm, -rightArm), "left and right arms are opposite");
        check(leftLeg * rightArm > 0.f,
              "contralateral right arm follows left leg");
    }

    void actionCase()
    {
        const Avatar::Profile profile = Avatar::defaultProfile();
        Avatar::Snapshot input;

        input.grounded = false;
        input.velocity.y = 4.f;
        const Avatar::Pose airborne = Avatar::derivePose(input, profile);
        check(airborne.kind == Avatar::PoseKind::Airborne,
              "ungrounded pose is Airborne");
        check(!near(part(airborne, profile, Avatar::PartRole::LeftLeg)
                        .rotationDegrees.x,
                    part(airborne, profile, Avatar::PartRole::RightLeg)
                        .rotationDegrees.x),
              "airborne silhouette separates the legs");

        input = {};
        input.grounded = true;
        input.feedback.landing = 1.f;
        const Avatar::Pose land = Avatar::derivePose(input, profile);
        check(land.kind == Avatar::PoseKind::Land, "landing envelope selects Land");
        check(land.rootOffset.y < -.1f, "land pose visibly compresses the root");

        input = {};
        input.feedback.tool.preparation = 1.f;
        const Avatar::Pose tool = Avatar::derivePose(input, profile);
        check(tool.kind == Avatar::PoseKind::Tool, "tool envelope selects Tool");
        check(part(tool, profile, Avatar::PartRole::RightArm).rotationDegrees.x > 90.f,
              "tool pose raises the working arm");

        input.feedback.hurt = 1.f;
        const Avatar::Pose hurt = Avatar::derivePose(input, profile);
        check(hurt.kind == Avatar::PoseKind::Hurt,
              "hurt feedback takes visible precedence");
        check(hurt.rootRotationDegrees.z > 9.f,
              "hurt pose has a distinct bounded recoil");
        check(!near(articulationMagnitude(airborne, profile),
                    articulationMagnitude(land, profile)) &&
              !near(articulationMagnitude(land, profile),
                    articulationMagnitude(tool, profile)) &&
              !near(articulationMagnitude(tool, profile),
                    articulationMagnitude(hurt, profile)),
              "air, land, tool, and hurt have distinct signatures");
    }

    void strengthAndBoundsCase()
    {
        const Avatar::Profile profile = Avatar::defaultProfile();
        Avatar::Snapshot input;
        input.movementSeconds = .19f;
        input.movementStrength = 1.f;
        input.velocity = {0.f, 0.f, -4.5f};
        input.feedback.landing = .7f;
        input.feedback.tool.preparation = .8f;
        input.feedback.hurt = .6f;
        const Avatar::Pose off = Avatar::derivePose(
            input, profile, Avatar::MotionStrength::Off);
        const Avatar::Pose reduced = Avatar::derivePose(
            input, profile, Avatar::MotionStrength::Reduced);
        const Avatar::Pose full = Avatar::derivePose(
            input, profile, Avatar::MotionStrength::Full);
        check(near(articulationMagnitude(off, profile), 0.f),
              "Off removes decorative articulation");
        check(articulationMagnitude(reduced, profile) > 0.f &&
              articulationMagnitude(reduced, profile) <
                  articulationMagnitude(full, profile),
              "Reduced stays between Off and Full");

        Avatar::Snapshot hostile;
        const float infinity = std::numeric_limits<float>::infinity();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        hostile.position = {nan, infinity, -infinity};
        hostile.rotationDegrees = {infinity, nan, -infinity};
        hostile.velocity = {nan, infinity, nan};
        hostile.movementSeconds = infinity;
        hostile.movementStrength = infinity;
        hostile.feedback.landing = nan;
        hostile.feedback.tool = {infinity, nan, -infinity, nan};
        hostile.feedback.hurt = -infinity;
        const Avatar::Pose bounded = Avatar::derivePose(hostile, profile);
        check(finite(bounded.worldPosition) &&
              std::isfinite(bounded.facingYawDegrees) &&
              finite(bounded.rootOffset) && finite(bounded.rootRotationDegrees),
              "invalid copied input is sanitized");
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            check(finite(bounded.parts[index].rotationDegrees) &&
                  finite(bounded.parts[index].offset) &&
                  finite(bounded.parts[index].scale),
                  "all bounded part transforms remain finite");
            check(std::abs(bounded.parts[index].rotationDegrees.x) <= 105.f &&
                  std::abs(bounded.parts[index].rotationDegrees.y) <= 45.f &&
                  std::abs(bounded.parts[index].rotationDegrees.z) <= 45.f,
                  "part rotation caps are respected");
        }
    }

    Avatar::Vec3 rotate(Avatar::Vec3 p, Avatar::Vec3 degrees)
    {
        constexpr float radians = 3.14159265359f / 180.f;
        const float x = degrees.x * radians, y = degrees.y * radians, z = degrees.z * radians;
        p = {p.x*std::cos(z)-p.y*std::sin(z), p.x*std::sin(z)+p.y*std::cos(z), p.z};
        p = {p.x, p.y*std::cos(x)-p.z*std::sin(x), p.y*std::sin(x)+p.z*std::cos(x)};
        return {p.x*std::cos(y)+p.z*std::sin(y), p.y, -p.x*std::sin(y)+p.z*std::cos(y)};
    }

    bool nearVector(Avatar::Vec3 left, Avatar::Vec3 right, float tolerance = .001f)
    {
        return near(left.x, right.x, tolerance) && near(left.y, right.y, tolerance) &&
               near(left.z, right.z, tolerance);
    }

    bool sameLocalPose(const Avatar::Pose& left, const Avatar::Pose& right,
                       const Avatar::Profile& profile, float tolerance = .001f)
    {
        if (!nearVector(left.rootOffset, right.rootOffset, tolerance) ||
            !nearVector(left.rootRotationDegrees, right.rootRotationDegrees, tolerance) ||
            !near(left.weights.walk, right.weights.walk, tolerance) || left.kind != right.kind)
            return false;
        for (std::size_t index = 0; index < profile.partCount; ++index) {
            const auto& a = left.parts[index];
            const auto& b = right.parts[index];
            if (!nearVector(a.rotationDegrees, b.rotationDegrees, tolerance) ||
                !nearVector(a.offset, b.offset, tolerance) ||
                !nearVector(a.scale, b.scale, tolerance)) return false;
        }
        return true;
    }

    struct TravelDirection
    {
        const char* name;
        float forward, right;
    };

    constexpr std::array<TravelDirection, 8> travelDirections{{
        {"forward", 1.f, 0.f}, {"backward", -1.f, 0.f},
        {"right", 0.f, 1.f}, {"left", 0.f, -1.f},
        {"forward-right", 1.f, 1.f}, {"forward-left", 1.f, -1.f},
        {"backward-right", -1.f, 1.f}, {"backward-left", -1.f, -1.f}}};

    Avatar::Snapshot moving(float forward, float right, float yaw = 0.f,
                            float speed = 4.5f, float strength = 1.f,
                            float seconds = .19f)
    {
        constexpr float radians = 3.14159265359f / 180.f;
        const float angle = yaw * radians;
        const float length = std::hypot(forward, right);
        Avatar::Snapshot result;
        result.rotationDegrees.y = yaw;
        result.movementSeconds = seconds;
        result.movementStrength = strength;
        if (length > 0.f) {
            forward /= length; right /= length;
            // PlayerController basis: forward=(sin yaw,-cos yaw),
            // right=(cos yaw,sin yaw), including negative and wrapped yaw.
            result.velocity = {speed * (forward * std::sin(angle) + right * std::cos(angle)),
                0.f, speed * (-forward * std::cos(angle) + right * std::sin(angle))};
        }
        return result;
    }

    // Reconstruct the renderer's public transforms independently. The unit
    // cube scales around its centre; centre-pivot itself is not scaled.
    Avatar::Vec3 partPoint(const Avatar::Pose& pose, const Avatar::Profile& profile,
                           Avatar::PartRole role, Avatar::Vec3 halfCorner)
    {
        const std::size_t index = Avatar::partIndex(profile, role);
        const auto& definition = profile.parts[index];
        const auto& transform = pose.parts[index];
        const auto rotated = rotate({definition.centre.x - definition.pivot.x +
                                         halfCorner.x * definition.size.x * transform.scale.x,
                                     definition.centre.y - definition.pivot.y +
                                         halfCorner.y * definition.size.y * transform.scale.y,
                                     definition.centre.z - definition.pivot.z +
                                         halfCorner.z * definition.size.z * transform.scale.z},
                                    transform.rotationDegrees);
        return {definition.pivot.x + transform.offset.x + rotated.x,
                definition.pivot.y + transform.offset.y + rotated.y,
                definition.pivot.z + transform.offset.z + rotated.z};
    }

    Avatar::Vec3 rootPoint(const Avatar::Pose& pose, Avatar::Vec3 point)
    {
        const auto rotated = rotate(point, pose.rootRotationDegrees);
        return {rotated.x + pose.rootOffset.x, rotated.y + pose.rootOffset.y,
                rotated.z + pose.rootOffset.z};
    }

    Avatar::Vec3 foot(const Avatar::Pose& pose, const Avatar::Profile& profile,
                      Avatar::PartRole role)
    {
        return rootPoint(pose, partPoint(pose, profile, role, {0.f, -.5f, 0.f}));
    }

    struct LegBounds
    {
        float minimumY = std::numeric_limits<float>::infinity();
        float minimumX = std::numeric_limits<float>::infinity();
        float maximumX = -std::numeric_limits<float>::infinity();
        bool allFinite = true;
    };

    LegBounds legBounds(const Avatar::Pose& pose, const Avatar::Profile& profile,
                        Avatar::PartRole role)
    {
        LegBounds result;
        for (float x : {-.5f, .5f}) for (float y : {-.5f, .5f}) for (float z : {-.5f, .5f}) {
            const auto local = partPoint(pose, profile, role, {x, y, z});
            const auto rooted = rootPoint(pose, local);
            result.allFinite &= finite(local) && finite(rooted);
            result.minimumY = std::min(result.minimumY, rooted.y);
            // A common root rigid transform preserves the separating plane;
            // use that plane before the root rotation, rather than world X.
            result.minimumX = std::min(result.minimumX, local.x);
            result.maximumX = std::max(result.maximumX, local.x);
        }
        return result;
    }

    bool groundedLegCorners(const Avatar::Pose& pose, const Avatar::Profile& profile)
    {
        const auto left = legBounds(pose, profile, Avatar::PartRole::LeftLeg);
        const auto right = legBounds(pose, profile, Avatar::PartRole::RightLeg);
        return left.allFinite && right.allFinite && left.minimumY >= -.001f &&
               right.minimumY >= -.001f &&
               near(std::min(left.minimumY, right.minimumY), 0.f, .001f) &&
               pose.rootOffset.y >= -.18001f && pose.rootOffset.y <= .08001f;
    }

    bool separatedLegCorners(const Avatar::Pose& pose, const Avatar::Profile& profile)
    {
        const auto left = legBounds(pose, profile, Avatar::PartRole::LeftLeg);
        const auto right = legBounds(pose, profile, Avatar::PartRole::RightLeg);
        return left.allFinite && right.allFinite && left.maximumX < right.minimumX;
    }

    float travelProjection(Avatar::Vec3 point, Avatar::Vec3 origin,
                           const TravelDirection& direction, float yaw)
    {
        constexpr float radians = 3.14159265359f / 180.f;
        const float angle = yaw * radians;
        const float x = point.x - origin.x, z = point.z - origin.z;
        // The renderer applies -yaw to a local -Z-facing body.
        const float worldX = x * std::cos(angle) - z * std::sin(angle);
        const float worldZ = x * std::sin(angle) + z * std::cos(angle);
        const float length = std::hypot(direction.forward, direction.right);
        const float vx = (direction.forward * std::sin(angle) + direction.right * std::cos(angle)) / length;
        const float vz = (-direction.forward * std::cos(angle) + direction.right * std::sin(angle)) / length;
        return worldX * vx + worldZ * vz;
    }

    void directionAndSpeedCase()
    {
        const auto profile = Avatar::defaultProfile();
        const auto idle = Avatar::derivePose({}, profile);
        const auto forward = Avatar::derivePose(moving(1.f, 0.f), profile);
        const auto backward = Avatar::derivePose(moving(-1.f, 0.f), profile);
        const auto right = Avatar::derivePose(moving(0.f, 1.f), profile);
        const auto left = Avatar::derivePose(moving(0.f, -1.f), profile);
        const auto leg = Avatar::PartRole::LeftLeg;
        const auto otherLeg = Avatar::PartRole::RightLeg;
        check(near(part(forward, profile, leg).rotationDegrees.x,
                   -part(backward, profile, leg).rotationDegrees.x) &&
              near(part(forward, profile, otherLeg).rotationDegrees.x,
                   -part(backward, profile, otherLeg).rotationDegrees.x),
              "backward velocity mirrors both forward leg pitches at the same phase");
        for (const auto role : {leg, otherLeg}) {
            const auto a = foot(forward, profile, role), b = foot(backward, profile, role);
            check(near(a.z, -b.z) && near(a.x, b.x) && near(a.y, b.y),
                  "forward and backward physical foot ends mirror in local Z");
        }
        const auto rightFoot = foot(right, profile, otherLeg), leftFoot = foot(left, profile, leg);
        check(rightFoot.x > foot(idle, profile, otherLeg).x + .1f &&
              leftFoot.x < foot(idle, profile, leg).x - .1f,
              "right and left strafing have visibly outward foot displacement");
        check(near(rightFoot.x, -leftFoot.x) && near(rightFoot.y, leftFoot.y) &&
              near(rightFoot.z, leftFoot.z) &&
              near(foot(right, profile, leg).x, -foot(left, profile, otherLeg).x),
              "opposite strafing mirrors the physical left and right foot ends");
        check(near(part(right, profile, leg).rotationDegrees.x, 0.f) &&
              near(part(right, profile, otherLeg).rotationDegrees.x, 0.f) &&
              part(right, profile, otherLeg).rotationDegrees.z > 10.f &&
              part(left, profile, leg).rotationDegrees.z < -10.f,
              "pure strafing opens the outward leg without forward leg pitch");
        check(near(right.rootRotationDegrees.z, -left.rootRotationDegrees.z) &&
              std::abs(right.rootRotationDegrees.z) > 2.f &&
              near(forward.rootRotationDegrees.x, -backward.rootRotationDegrees.x),
              "root lean follows copied forward and lateral velocity");

        for (const auto& direction : travelDirections) {
            const auto base = Avatar::derivePose(moving(direction.forward, direction.right), profile);
            for (float yaw : {0.f, 90.f, -90.f, 37.f, 397.f}) {
                const auto posed = Avatar::derivePose(moving(direction.forward, direction.right, yaw), profile);
                const std::string label = std::string(direction.name) + " yaw=" + std::to_string(yaw);
                check(sameLocalPose(base, posed, profile) &&
                      near(posed.facingYawDegrees, std::remainder(yaw, 360.f)),
                      "yaw and world velocity rotate together without changing local pose: " + label);
                const float leftAlong = travelProjection(foot(posed, profile, leg), foot(idle, profile, leg), direction, yaw);
                const float rightAlong = travelProjection(foot(posed, profile, otherLeg), foot(idle, profile, otherLeg), direction, yaw);
                const float baseLeft = travelProjection(foot(base, profile, leg), foot(idle, profile, leg), direction, 0.f);
                const float baseRight = travelProjection(foot(base, profile, otherLeg), foot(idle, profile, otherLeg), direction, 0.f);
                check(near(leftAlong, baseLeft) && near(rightAlong, baseRight),
                      "physical foot projection onto actual travel is yaw invariant: " + label);
                if (direction.right == 0.f)
                    check(leftAlong > .25f && rightAlong < -.25f,
                          "front and back gait foot ends oppose actual travel: " + label);
                else if (direction.forward == 0.f)
                    check((direction.right > 0.f ? rightAlong - leftAlong : leftAlong - rightAlong) > .1f,
                          "lateral physical foot ends visibly distinguish actual travel: " + label);
            }
            for (float speed : {.25f, 2.25f, 45.f, 1.e20f}) {
                const auto varied = Avatar::derivePose(moving(direction.forward, direction.right, 0.f, speed), profile);
                check(sameLocalPose(base, varied, profile),
                      "speed only supplies direction; caller strength owns gait amplitude: " + std::string(direction.name));
            }
            const auto half = Avatar::derivePose(moving(direction.forward, direction.right, 0.f, 2.25f, .5f), profile);
            bool halfRotations = near(half.weights.walk, .5f);
            for (const auto role : {leg, otherLeg}) {
                const auto fullRotation = part(base, profile, role).rotationDegrees;
                const auto halfRotation = part(half, profile, role).rotationDegrees;
                halfRotations &= nearVector(halfRotation,
                    {fullRotation.x * .5f, fullRotation.y * .5f, fullRotation.z * .5f});
            }
            check(halfRotations, "caller half strength halves leg articulation: " + std::string(direction.name));
        }
        const auto diagonal = Avatar::derivePose(moving(1.f, 1.f), profile);
        constexpr float normalizedAxis = .70710678118f;
        check(near(part(diagonal, profile, leg).rotationDegrees.x,
                   part(forward, profile, leg).rotationDegrees.x * normalizedAxis) &&
              near(part(diagonal, profile, otherLeg).rotationDegrees.z,
                   part(right, profile, otherLeg).rotationDegrees.z * normalizedAxis),
              "diagonal motion shares normalized forward and lateral articulation");
    }

    void invalidMovementAndStrengthCase()
    {
        const auto profile = Avatar::defaultProfile();
        const auto idle = Avatar::derivePose({}, profile);
        const float infinity = std::numeric_limits<float>::infinity();
        const float nan = std::numeric_limits<float>::quiet_NaN();
        for (const auto velocity : {Avatar::Vec3{0.f, 0.f, 0.f}, {0.f, 9.f, 0.f},
             {0.f, 0.f, -.00005f}, {.0001f, 0.f, 0.f}, {.00006f, 0.f, -.00006f}, {nan, 0.f, -4.5f},
             {4.5f, 0.f, nan}, {infinity, 0.f, -4.5f}, {4.5f, 0.f, -infinity}}) {
            auto input = moving(1.f, 0.f);
            input.velocity = velocity;
            const auto pose = Avatar::derivePose(input, profile);
            check(pose.kind == Avatar::PoseKind::Idle && near(pose.weights.walk, 0.f) &&
                  sameLocalPose(pose, idle, profile),
                  "zero, tiny, vertical, or invalid XZ velocity cannot animate a stale movement clock");
        }
        auto thresholdDiagonal = moving(1.f, 1.f);
        thresholdDiagonal.velocity = {.00009f, 0.f, -.00009f};
        check(sameLocalPose(Avatar::derivePose(thresholdDiagonal, profile),
                            Avatar::derivePose(moving(1.f, 1.f), profile), profile),
              "horizontal length above epsilon still walks when both components are below epsilon");
        auto thresholdAxis = moving(0.f, 1.f);
        thresholdAxis.velocity = {.000101f, 0.f, 0.f};
        check(sameLocalPose(Avatar::derivePose(thresholdAxis, profile),
                            Avatar::derivePose(moving(0.f, 1.f), profile), profile),
              "horizontal length just above epsilon preserves caller strength");
        for (float clock : {0.f, .19f, .4f, 1000.f}) {
            auto fake = moving(0.f, 0.f, 0.f, 0.f, 1.f, clock);
            check(sameLocalPose(Avatar::derivePose(fake, profile), idle, profile),
                  "stationary fake clock and strength remain the stationary pose");
        }
        for (const auto& direction : travelDirections) {
            const auto input = moving(direction.forward, direction.right);
            const auto full = Avatar::derivePose(input, profile, Avatar::MotionStrength::Full);
            const auto reduced = Avatar::derivePose(input, profile, Avatar::MotionStrength::Reduced);
            const auto off = Avatar::derivePose(input, profile, Avatar::MotionStrength::Off);
            bool noLegArticulation = true, reducedLegs = true;
            float fullMagnitude = 0.f, reducedMagnitude = 0.f;
            for (const auto role : {Avatar::PartRole::LeftLeg, Avatar::PartRole::RightLeg}) {
                const auto a = part(full, profile, role).rotationDegrees;
                const auto b = part(reduced, profile, role).rotationDegrees;
                noLegArticulation &= nearVector(part(off, profile, role).rotationDegrees, {});
                reducedLegs &= nearVector(b, {a.x * .55f, a.y * .55f, a.z * .55f});
                fullMagnitude += std::abs(a.x) + std::abs(a.y) + std::abs(a.z);
                reducedMagnitude += std::abs(b.x) + std::abs(b.y) + std::abs(b.z);
            }
            check(noLegArticulation && near(articulationMagnitude(off, profile), 0.f),
                  "Off removes all directional decorative articulation: " + std::string(direction.name));
            check(reducedLegs && reducedMagnitude > 0.f && reducedMagnitude < fullMagnitude,
                  "Reduced retains a smaller directional gait: " + std::string(direction.name));
        }
    }

    void groundedGeometryCase()
    {
        const auto profile = Avatar::defaultProfile();
        constexpr float cycleSeconds = 6.28318530718f / 8.2f;
        for (const auto& direction : travelDirections)
            for (const auto strength : {Avatar::MotionStrength::Off, Avatar::MotionStrength::Reduced, Avatar::MotionStrength::Full}) {
                bool ground = true, separated = true;
                for (int phase = 0; phase < 16; ++phase) {
                    const auto input = moving(direction.forward, direction.right, 37.f, 4.5f, 1.f,
                                              phase * cycleSeconds / 16.f);
                    const auto pose = Avatar::derivePose(input, profile, strength);
                    ground &= groundedLegCorners(pose, profile);
                    separated &= separatedLegCorners(pose, profile);
                }
                const std::string label = std::string(direction.name) + " strength=" + std::to_string(static_cast<int>(strength));
                check(ground, "all eight corners of both legs stay above ground with a contacting lowest corner across 16 phases: " + label);
                check(separated, "left and right leg boxes keep a real separating plane across 16 phases: " + label);
            }
    }

    void workingArmGaitCase()
    {
        const auto profile = Avatar::defaultProfile();
        for (int action = 0; action < 4; ++action)
            for (const auto strength : {Avatar::MotionStrength::Full, Avatar::MotionStrength::Reduced})
                for (const auto& direction : travelDirections) {
                    auto movingInput = moving(direction.forward, direction.right);
                    switch (action) {
                    case 0: movingInput.feedback.tool.preparation = 1.f; break;
                    case 1: movingInput.feedback.tool.strike = 1.f; break;
                    case 2: movingInput.feedback.tool.use = 1.f; break;
                    case 3: movingInput.feedback.tool.consume = 1.f; break;
                    }
                    auto stillInput = movingInput;
                    stillInput.velocity = {}; stillInput.movementStrength = 0.f;
                    const auto movingPose = Avatar::derivePose(movingInput, profile, strength);
                    const auto stillPose = Avatar::derivePose(stillInput, profile, strength);
                    const auto& movingArm = part(movingPose, profile, Avatar::PartRole::RightArm);
                    const auto& stillArm = part(stillPose, profile, Avatar::PartRole::RightArm);
                    check(nearVector(movingArm.rotationDegrees, stillArm.rotationDegrees) &&
                          nearVector(movingArm.offset, stillArm.offset) && nearVector(movingArm.scale, stillArm.scale),
                          "full raw tool envelope suppresses all working-arm gait axes: action=" + std::to_string(action) +
                          " strength=" + std::to_string(static_cast<int>(strength)) + " " + direction.name);
                }
    }

    void directionalSmoothingCase()
    {
        const auto profile = Avatar::defaultProfile();
        constexpr float cycleSeconds = 6.28318530718f / 8.2f;
        for (const auto& direction : travelDirections)
            for (const auto strength : {Avatar::MotionStrength::Full, Avatar::MotionStrength::Reduced}) {
                bool ground = true, separated = true, frameRatesMatch = true, converges = true, gradual = true;
                for (int phase = 0; phase < 16; ++phase) {
                    const float seconds = phase * cycleSeconds / 16.f;
                    const auto initial = Avatar::derivePose(moving(direction.forward, direction.right, 37.f, 4.5f, 1.f, seconds), profile, strength);
                    const auto reverse = Avatar::derivePose(moving(-direction.forward, -direction.right, 37.f, 4.5f, 1.f, seconds), profile, strength);
                    Avatar::Pose at30, at60, final30, final60;
                    for (int fps : {30, 60}) {
                        Avatar::PoseHistory history;
                        Avatar::smoothPose(history, initial, profile, 0.f);
                        for (int frame = 0; frame < fps; ++frame) {
                            const auto result = Avatar::smoothPose(history, reverse, profile, 1.f / fps);
                            ground &= groundedLegCorners(result, profile);
                            separated &= separatedLegCorners(result, profile);
                            if (frame == 0) {
                                // Check only rotations here: the foot correction
                                // intentionally makes root Y a nonlinear result.
                                gradual &= !nearVector(result.rootRotationDegrees, reverse.rootRotationDegrees, .00001f);
                                for (std::size_t index = 0; index < profile.partCount; ++index)
                                    for (int axis = 0; axis < 3; ++axis) {
                                        const auto component = [axis](Avatar::Vec3 value) { return axis == 0 ? value.x : axis == 1 ? value.y : value.z; };
                                        const float a = component(initial.parts[index].rotationDegrees);
                                        const float b = component(reverse.parts[index].rotationDegrees);
                                        const float value = component(result.parts[index].rotationDegrees);
                                        gradual &= value >= std::min(a, b) - .001f && value <= std::max(a, b) + .001f;
                                    }
                            }
                            if (frame + 1 == fps / 5) { if (fps == 30) at30 = result; else at60 = result; }
                            if (frame + 1 == fps) { if (fps == 30) final30 = result; else final60 = result; }
                        }
                    }
                    frameRatesMatch &= sameLocalPose(at30, at60, profile, .002f);
                    for (const auto role : {Avatar::PartRole::LeftLeg, Avatar::PartRole::RightLeg})
                        frameRatesMatch &= nearVector(foot(at30, profile, role), foot(at60, profile, role), .0002f);
                    converges &= sameLocalPose(final30, reverse, profile, .002f) && sameLocalPose(final60, reverse, profile, .002f);
                }
                const std::string label = std::string(direction.name) + " strength=" + std::to_string(static_cast<int>(strength));
                check(ground && separated, "every smoothed reversal keeps all leg corners grounded and legs separated across 16 phases: " + label);
                check(gradual, "direction reversal moves between limb rotations without a first-frame target snap: " + label);
                check(frameRatesMatch, "30 and 60 Hz reverse poses and physical foot ends agree at the same 200 ms: " + label);
                check(converges, "30 and 60 Hz reverse articulation converges after one second: " + label);

                bool stoppedGround = true, stoppedSeparated = true, stoppedSettled = true;
                for (int fps : {30, 60}) {
                    Avatar::PoseHistory history;
                    const auto initial = Avatar::derivePose(moving(direction.forward, direction.right), profile, strength);
                    const auto stopped = Avatar::derivePose({}, profile, strength);
                    Avatar::smoothPose(history, initial, profile, 0.f);
                    Avatar::Pose result;
                    for (int frame = 0; frame < fps; ++frame) {
                        result = Avatar::smoothPose(history, stopped, profile, 1.f / fps);
                        stoppedGround &= groundedLegCorners(result, profile);
                        stoppedSeparated &= separatedLegCorners(result, profile);
                    }
                    stoppedSettled &= sameLocalPose(result, stopped, profile, .002f);
                }
                check(stoppedGround && stoppedSeparated && stoppedSettled,
                      "stopping keeps residual smoothed leg corners grounded after walk weight becomes zero: " + label);
            }
        const auto target = Avatar::derivePose(moving(1.f, 1.f), profile);
        for (float delta : {0.f, -.1f, .251f, std::numeric_limits<float>::quiet_NaN()}) {
            Avatar::PoseHistory history;
            if (delta != 0.f) Avatar::smoothPose(history, Avatar::derivePose({}, profile), profile, 0.f);
            const auto result = Avatar::smoothPose(history, target, profile, delta);
            check(groundedLegCorners(result, profile) && separatedLegCorners(result, profile) &&
                  sameLocalPose(result, target, profile),
                  "first sample and invalid-delta smoothing branches retain grounded target geometry");
        }
    }

    void toolDirectionCase()
    {
        const auto profile = Avatar::defaultProfile();
        const auto armIndex = Avatar::partIndex(profile, Avatar::PartRole::RightArm);
        const auto& arm = profile.parts[armIndex];
        bool forward = true, bounded = true, layers = true;
        for (auto action : {ToolActionPresentation::Action::Mining,
                            ToolActionPresentation::Action::Strike,
                            ToolActionPresentation::Action::Use,
                            ToolActionPresentation::Action::Consume}) {
            for (int tick = 0; tick < 800; ++tick) {
                Avatar::Snapshot snapshot;
                snapshot.feedback.tool = ToolActionPresentation::derive(
                    action, tick / 1000.f, 1.f, 1.f, 0.f);
                const auto pose = Avatar::derivePose(snapshot, profile);
                const auto wrist = rotate({0.f, arm.centre.y-arm.pivot.y-arm.size.y*.5f, 0.f},
                                          pose.parts[armIndex].rotationDegrees);
                if (pose.weights.tool > .1f) forward &= wrist.z < -.03f;
                bounded &= finite(wrist) && std::abs(wrist.x) < .7f && std::abs(wrist.z) < .8f;
                layers &= near(part(pose, profile, Avatar::PartRole::Hair).rotationDegrees.x,
                               part(pose, profile, Avatar::PartRole::Head).rotationDegrees.x) &&
                          near(part(pose, profile, Avatar::PartRole::Belt).rotationDegrees.y,
                               part(pose, profile, Avatar::PartRole::Torso).rotationDegrees.y);
            }
        }
        check(forward, "every active tool path reaches local forward rather than behind player");
        check(bounded, "all working wrist paths stay within arm reach");
        check(layers, "head and torso cosmetic seams follow all action paths");
        Avatar::Snapshot input;
        input.feedback.tool.use = 1.f;
        const auto use = Avatar::derivePose(input, profile);
        input.feedback.tool = {}; input.feedback.tool.consume = 1.f;
        const auto eat = Avatar::derivePose(input, profile);
        check(part(eat, profile, Avatar::PartRole::RightArm).rotationDegrees.z < -20.f &&
              part(eat, profile, Avatar::PartRole::RightArm).rotationDegrees.x >
              part(use, profile, Avatar::PartRole::RightArm).rotationDegrees.x + 20.f,
              "consume moves hand inward and higher than forward use");
        input.feedback.tool = {}; input.feedback.tool.strike = 1.f;
        const auto strike = Avatar::derivePose(input, profile);
        input.feedback.tool = {}; input.feedback.tool.preparation = 1.f;
        const auto lift = Avatar::derivePose(input, profile);
        check(part(lift, profile, Avatar::PartRole::RightArm).rotationDegrees.x >
              part(strike, profile, Avatar::PartRole::RightArm).rotationDegrees.x + 40.f,
              "tool lift and downstroke have separate silhouettes");
    }

    void smoothingCase()
    {
        const Avatar::Profile profile = Avatar::defaultProfile();
        Avatar::Snapshot neutralInput;
        Avatar::Snapshot toolInput;
        toolInput.feedback.tool.preparation = 1.f;
        const Avatar::Pose neutral = Avatar::derivePose(neutralInput, profile);
        const Avatar::Pose tool = Avatar::derivePose(toolInput, profile);

        Avatar::PoseHistory at60;
        Avatar::PoseHistory at30;
        Avatar::smoothPose(at60, neutral, profile, 0.f);
        Avatar::smoothPose(at30, neutral, profile, 0.f);
        Avatar::Pose result60;
        Avatar::Pose result30;
        for (int frame = 0; frame < 60; ++frame)
            result60 = Avatar::smoothPose(at60, tool, profile, 1.f / 60.f);
        for (int frame = 0; frame < 30; ++frame)
            result30 = Avatar::smoothPose(at30, tool, profile, 1.f / 30.f);

        const float arm60 = part(result60, profile,
            Avatar::PartRole::RightArm).rotationDegrees.x;
        const float arm30 = part(result30, profile,
            Avatar::PartRole::RightArm).rotationDegrees.x;
        check(near(arm60, arm30, .002f),
              "exponential smoothing is frame-rate independent");
        check(near(arm60, part(tool, profile,
            Avatar::PartRole::RightArm).rotationDegrees.x, .002f),
              "one second settles on the tool target");

        Avatar::PoseHistory invalidDelta;
        Avatar::smoothPose(invalidDelta, neutral, profile, 0.f);
        const Avatar::Pose snapped = Avatar::smoothPose(
            invalidDelta, tool, profile,
            std::numeric_limits<float>::quiet_NaN());
        check(near(part(snapped, profile, Avatar::PartRole::RightArm)
                       .rotationDegrees.x,
                   part(tool, profile, Avatar::PartRole::RightArm)
                       .rotationDegrees.x),
              "invalid frame delta safely snaps instead of poisoning history");

        for (float delta : {-1.f, .251f}) {
            Avatar::PoseHistory discontinuity;
            Avatar::smoothPose(discontinuity, neutral, profile, 0.f);
            const Avatar::Pose discontinuityPose = Avatar::smoothPose(
                discontinuity, tool, profile, delta);
            check(near(part(discontinuityPose, profile,
                            Avatar::PartRole::RightArm).rotationDegrees.x,
                       part(tool, profile,
                            Avatar::PartRole::RightArm).rotationDegrees.x),
                  "negative and over-250ms deltas snap to the target");
        }

        Avatar::PoseHistory customMaximum;
        Avatar::smoothPose(customMaximum, neutral, profile, 0.f);
        Avatar::SmoothingSettings settings;
        settings.maximumDeltaSeconds = .5f;
        const Avatar::Pose cappedMaximum = Avatar::smoothPose(
            customMaximum, tool, profile, .3f, settings);
        check(near(part(cappedMaximum, profile,
                        Avatar::PartRole::RightArm).rotationDegrees.x,
                   part(tool, profile,
                        Avatar::PartRole::RightArm).rotationDegrees.x),
              "custom settings cannot raise the 250ms discontinuity limit");
    }
}

int main()
{
    profileCase();
    gaitCase();
    actionCase();
    strengthAndBoundsCase();
    smoothingCase();
    toolDirectionCase();
    directionAndSpeedCase();
    invalidMovementAndStrengthCase();
    groundedGeometryCase();
    workingArmGaitCase();
    directionalSmoothingCase();
    std::cout << "[PLAYER_AVATAR] checks=" << checks
              << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
