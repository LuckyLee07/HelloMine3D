#include "Presentation/PlayerAvatarPresentation.h"

#include <algorithm>
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
        input.feedback.toolUse = 1.f;
        const Avatar::Pose tool = Avatar::derivePose(input, profile);
        check(tool.kind == Avatar::PoseKind::Tool, "tool envelope selects Tool");
        check(part(tool, profile, Avatar::PartRole::RightArm).rotationDegrees.x < -70.f,
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
        input.feedback.landing = .7f;
        input.feedback.toolUse = .8f;
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
        hostile.feedback = {nan, infinity, -infinity};
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

    void smoothingCase()
    {
        const Avatar::Profile profile = Avatar::defaultProfile();
        Avatar::Snapshot neutralInput;
        Avatar::Snapshot toolInput;
        toolInput.feedback.toolUse = 1.f;
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
    std::cout << "[PLAYER_AVATAR] checks=" << checks
              << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
