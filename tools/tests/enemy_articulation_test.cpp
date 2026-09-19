#include <algorithm>
#include <iostream>
#include <limits>
#include <glm/gtc/quaternion.hpp>
#include "Actor/EnemyPresentation.h"

namespace {
int failures = 0;
void check(const char* name, bool pass)
{
    std::cout << (pass ? "PASS " : "FAIL ") << name << '\n';
    failures += !pass;
}
bool near(glm::vec3 a, glm::vec3 b) { return glm::length(a - b) < .00002f; }
bool near(float a, float b) { return std::abs(a - b) < .0002f; }
glm::vec3 transformPoint(const EnemyVisualProfile& profile,
    const EnemyVisualPose& pose, std::size_t index, glm::vec3 bodyPoint)
{
    const auto& part = profile.parts[index];
    return part.offset + pose.offsets[index] + EnemyPresentation::rotatePart(
        (bodyPoint - part.offset) * pose.scales[index], pose.rotations[index]);
}
}

int main()
{
    using namespace EnemyPresentation;
    bool directions = true, translated = true, bounded = true;
    for (int direction = 0; direction < 8; ++direction) {
        const float a = glm::radians(direction * 45.f);
        GaitPhase origin, shifted;
        float phase = 0.f, shiftedPhase = 0.f;
        for (int i = 0; i <= 40; ++i) {
            const glm::vec3 position(std::cos(a) * i / 40.f, 0.f, std::sin(a) * i / 40.f);
            phase = origin.update(position, true);
            shiftedPhase = shifted.update(position + glm::vec3(-20, 8, 30), true);
        }
        directions &= near(phase, 4.5f);
        translated &= near(phase, shiftedPhase);
    }
    check("equal-distance-gait-in-all-eight-directions", directions);
    check("gait-independent-of-positive-or-negative-origin", translated);

    GaitPhase track;
    check("first-snapshot-does-not-invent-travel", track.update({500, 5, -500}, true) == 0.f);
    const float stepped = track.update({500.125f, 5, -500}, true);
    check("stationary-and-vertical-motion-hold-phase",
        track.update({500.125f, 8, -500}, true) == stepped &&
        track.update({500.125f, 8, -500}, true) == stepped);
    check("nonwalking-motion-rebases-without-stepping",
        track.update({501, 8, -500}, false) == stepped);
    const float resumed = track.update({501.125f, 8, -500}, true);
    check("resume-uses-only-new-travel", near(resumed, stepped + .125f * 4.5f));
    check("teleport-rebases-without-a-giant-stride",
        track.update({-5000, 8, 9000}, true) == resumed &&
        near(track.update({-4999.875f, 8, 9000}, true), resumed + .125f * 4.5f));
    check("invalid-position-resets-and-recovers",
        track.update({INFINITY, 0, 0}, true) == 0.f &&
        track.update({10, 0, 10}, true) == 0.f &&
        near(track.update({10.125f, 0, 10}, true), .125f * 4.5f));
    for (int i = 0; i < 20000; ++i) {
        const float phase = track.update({i * .125f, 0, 0}, true);
        bounded &= std::isfinite(phase) && phase >= 0.f && phase < 6.283186f;
    }
    check("long-travel-phase-stays-bounded", bounded);

    bool pivots = true, neck = true, extent = true, rotations = true, restContact = true;
    const char* types[]{"hellomine:stalker", "hellomine:brute", "hellomine:spitter",
        "hellomine:waystone_stalker", "hellomine:waystone_brute", "unknown"};
    for (const auto* type : types) {
        const auto profile = profileForType(type);
        ActorSnapshot actor; actor.type = type; actor.id = 7;
        actor.position = {3, 8, -3}; actor.combatStateTicksTotal = 20;
        const auto& torso = profile.parts[0];
        for (std::size_t i = 0; i < profile.partCount; ++i) {
            const auto& part = profile.parts[i];
            if (part.role < EnemyVisualPartRole::LeftArm || part.role > EnemyVisualPartRole::RightLeg) continue;
            // Resting limb and body boxes must touch in all three axes.
            const glm::vec3 overlap = (part.scale + torso.scale) * .5f - glm::abs(part.offset - torso.offset);
            restContact &= glm::all(glm::greaterThanEqual(overlap, glm::vec3(-.00001f)));
        }
        for (auto state : {MobCombatState::Idle, MobCombatState::Chase, MobCombatState::Windup, MobCombatState::Recover}) {
            actor.combatState = state;
            for (int step = 0; step <= 20; ++step) {
                actor.combatStateTicksRemaining = step;
                const auto pose = poseFor(actor, profile, step * .31f);
                for (std::size_t i = 0; i < profile.partCount; ++i) {
                    const auto& part = profile.parts[i];
                    if (part.role >= EnemyVisualPartRole::LeftArm && part.role <= EnemyVisualPartRole::RightLeg) {
                        const glm::vec3 joint = part.offset + glm::vec3(0, part.scale.y * .5f, 0);
                        pivots &= near(transformPoint(profile, pose, i, joint), joint);
                    }
                    const auto r = glm::radians(pose.rotations[i]);
                    const auto q = glm::angleAxis(r.y, glm::vec3(0,1,0)) *
                        glm::angleAxis(r.x, glm::vec3(1,0,0)) * glm::angleAxis(r.z, glm::vec3(0,0,1));
                    rotations &= near(rotatePart({.3f,-.4f,.8f},pose.rotations[i]), q * glm::vec3(.3f,-.4f,.8f));
                    for (int corner = 0; corner < 8; ++corner) {
                        const glm::vec3 sign(corner & 1 ? .5f : -.5f, corner & 2 ? .5f : -.5f, corner & 4 ? .5f : -.5f);
                        const auto point = transformPoint(profile, pose, i, part.offset + part.scale * sign);
                        extent &= std::isfinite(glm::length(point)) && glm::length(point) < 1.6f;
                    }
                }
                if (profile.archetype == EnemyVisualArchetype::Spitter) {
                    // A point in the head/muzzle overlap must have exactly the
                    // same transformed position from either member of the assembly.
                    neck &= near(transformPoint(profile, pose, 1, {0,.23f,-.34f}),
                                 transformPoint(profile, pose, 2, {0,.23f,-.34f}));
                    const auto& head = profile.parts[1];
                    const auto joint = head.offset + glm::vec3(0,-head.scale.y*.5f,head.scale.z*.25f);
                    neck &= near(transformPoint(profile, pose, 1, joint), joint);
                }
                extent &= actor.position == glm::vec3(3,8,-3) && profile.partCount <= 8;
            }
        }
    }
    check("resting-limbs-connect-to-the-body", restContact);
    check("shoulder-and-hip-anchors-stay-fixed-through-all-poses", pivots);
    check("head-and-muzzle-share-one-fixed-neck-and-seam", neck);
    check("part-rotation-matches-yaw-pitch-roll-quaternion", rotations);
    check("all-poses-bounded-with-unchanged-snapshot-and-part-budget", extent);
    return failures ? 1 : 0;
}
