#include <algorithm>
#include <iostream>
#include <limits>
#include <glm/gtc/quaternion.hpp>
#include "Actor/EnemyPresentation.h"
#include "Actor/EnemyPresentationGallery.h"

namespace {
int failures = 0;
void check(const char* name, bool pass)
{
    std::cout << (pass ? "PASS " : "FAIL ") << name << '\n';
    failures += !pass;
}
bool near(glm::vec3 a, glm::vec3 b) { return glm::length(a - b) < .00002f; }
bool near(float a, float b) { return std::abs(a - b) < .0002f; }
bool insidePart(const EnemyVisualProfile& profile, const EnemyVisualPose& pose,
    std::size_t index, glm::vec3 point)
{
    const auto& part = profile.parts[index];
    const auto r = glm::radians(pose.rotations[index]);
    const auto q = glm::angleAxis(r.y, glm::vec3(0,1,0)) *
        glm::angleAxis(r.x, glm::vec3(1,0,0)) * glm::angleAxis(r.z, glm::vec3(0,0,1));
    const auto local = glm::conjugate(q) * (point - part.offset - pose.offsets[index]) / pose.scales[index];
    return glm::all(glm::lessThanEqual(glm::abs(local), part.scale * .5f));
}
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

    bool pivots = true, neck = true, extent = true, rotations = true, restContact = true, neckBridge = true;
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
                    std::size_t bridge = profile.partCount;
                    for (std::size_t i = 0; i < profile.partCount; ++i)
                        if (profile.parts[i].role == EnemyVisualPartRole::Neck) bridge = i;
                    neckBridge &= bridge < profile.partCount;
                    if (bridge < profile.partCount) {
                        const auto& connection = profile.parts[bridge];
                        // Sample an inset 80% cross-section, not a single pivot.
                        // Both ends must remain inside their neighboring volume.
                        for (int x = -2; x <= 2; ++x) for (int z = -2; z <= 2; ++z) {
                            const auto slice = glm::vec3(x * .2f, .25f, z * .2f) * connection.scale;
                            const auto upper = transformPoint(profile, pose, bridge, connection.offset + slice);
                            const auto lower = transformPoint(profile, pose, bridge,
                                connection.offset + glm::vec3(slice.x, -slice.y, slice.z));
                            neckBridge &= insidePart(profile, pose, 1, upper) && insidePart(profile, pose, 0, lower);
                        }
                    }
                }
                extent &= actor.position == glm::vec3(3,8,-3) && profile.partCount <= 8;
            }
        }
    }
    check("resting-limbs-connect-to-the-body", restContact);
    check("shoulder-and-hip-anchors-stay-fixed-through-all-poses", pivots);
    check("head-and-muzzle-share-one-fixed-neck-and-seam", neck);
    check("neck-has-volume-connected-to-head-and-torso-in-every-pose", neckBridge);
    check("part-rotation-matches-yaw-pitch-roll-quaternion", rotations);
    check("all-poses-bounded-with-unchanged-snapshot-and-part-budget", extent);
    const auto stalkerProfile = profileForType("hellomine:stalker");
    ActorSnapshot transition;
    transition.id = 7; transition.type = "hellomine:stalker";
    transition.combatState = MobCombatState::Windup;
    transition.combatStateTicksTotal = 20; transition.combatStateTicksRemaining = 0;
    const auto raised = poseFor(transition, stalkerProfile);
    transition.combatState = MobCombatState::Recover;
    transition.combatStateTicksRemaining = 20;
    const auto strike = poseFor(transition, stalkerProfile);
    PoseBlend blend;
    blend.update(transition, stalkerProfile, raised, 0.f);
    const auto middle = blend.update(transition, stalkerProfile, strike, 1.f / 60.f);
    check("state-change-moves-toward-strike-without-a-full-frame-snap",
        middle.rotations[3].x > raised.rotations[3].x &&
        middle.rotations[3].x < strike.rotations[3].x);
    const auto held = blend.update(transition, stalkerProfile, strike, 0.f);
    check("paused-frame-holds-articulation", held.rotations == middle.rotations &&
        held.scales == middle.scales && held.rootPitch == middle.rootPitch);
    std::array<EnemyVisualPose, 3> settled;
    const int rates[]{30,60,120};
    bool boundedSettle = true;
    for (int rate = 0; rate < 3; ++rate) {
        PoseBlend atRate; atRate.update(transition, stalkerProfile, raised, 0.f);
        float previous = raised.rotations[3].x;
        for (int frame = 0; frame < rates[rate] / 10; ++frame) {
            settled[rate] = atRate.update(transition, stalkerProfile, strike, 1.f/rates[rate]);
            boundedSettle &= settled[rate].rotations[3].x >= previous &&
                settled[rate].rotations[3].x <= strike.rotations[3].x;
            previous = settled[rate].rotations[3].x;
        }
    }
    check("settling-has-no-overshoot", boundedSettle);
    check("same-settling-time-at-30-60-120-fps",
        near(settled[0].rotations[3].x, settled[1].rotations[3].x) &&
        near(settled[0].rotations[3].x, settled[2].rotations[3].x));
    bool resets = true;
    for (const float dt : {-1.f, INFINITY, NAN, .3f}) {
        PoseBlend reset; reset.update(transition, stalkerProfile, raised, 0.f);
        resets &= reset.update(transition, stalkerProfile, strike, dt).rotations == strike.rotations;
    }
    PoseBlend teleport; teleport.update(transition, stalkerProfile, raised, 0.f);
    transition.position.x += 3.f;
    resets &= teleport.update(transition, stalkerProfile, strike, 1.f/60.f).rotations == strike.rotations;
    check("teleport-and-discontinuous-frame-reset-pose-history", resets);
    PoseBlend feedback; feedback.update(transition, stalkerProfile, raised, 0.f);
    transition.hitFeedback = 1.f;
    const auto hitTarget = poseFor(transition, stalkerProfile);
    const auto hitPose = feedback.update(transition, stalkerProfile, hitTarget, 1.f/60.f);
    check("hit-reaction-stays-immediate", hitPose.rootRoll == hitTarget.rootRoll &&
        hitPose.rootScale == hitTarget.rootScale);
    transition.deathPresentation = true;
    transition.deathPresentationTicksTotal = 8; transition.deathPresentationTicksRemaining = 4;
    const auto deathTarget = poseFor(transition, stalkerProfile);
    const auto deathPose = feedback.update(transition, stalkerProfile, deathTarget, 1.f/60.f);
    check("death-presentation-bypasses-blending", deathPose.rotations == deathTarget.rotations &&
        deathPose.scales == deathTarget.scales && deathPose.rootRoll == deathTarget.rootRoll &&
        deathPose.rootScale == deathTarget.rootScale && deathPose.rootYOffset == deathTarget.rootYOffset);
    bool blendedJoints = true, blendedBridge = true;
    for (const auto* type : types) {
        const auto profile = profileForType(type);
        ActorSnapshot actor; actor.type=type; actor.id=7;
        PoseBlend continuous;
        for (int frame=0; frame<360; ++frame) {
            const int section = (frame / 60) % 4;
            actor.combatState = section == 0 ? MobCombatState::Idle : section == 1 ?
                MobCombatState::Windup : section == 2 ? MobCombatState::Recover : MobCombatState::Chase;
            actor.combatStateTicksTotal=20;
            actor.combatStateTicksRemaining=20-(frame%60)/3;
            const auto pose = continuous.update(actor, profile, poseFor(actor,profile,frame*.075f),1.f/60.f);
            for (std::size_t part=0; part<profile.partCount; ++part) {
                const auto& def = profile.parts[part];
                if (def.role==EnemyVisualPartRole::LeftArm || def.role==EnemyVisualPartRole::RightArm ||
                    def.role==EnemyVisualPartRole::LeftLeg || def.role==EnemyVisualPartRole::RightLeg) {
                    const auto joint=def.offset+glm::vec3(0,def.scale.y*.5f,0);
                    blendedJoints &= near(transformPoint(profile,pose,part,joint),joint);
                }
                if (def.role==EnemyVisualPartRole::Muzzle) {
                    const auto& head = profile.parts[1];
                    const auto seam=(def.offset+head.offset)*.5f;
                    blendedJoints &= near(transformPoint(profile,pose,part,seam),transformPoint(profile,pose,1,seam));
                }
                if (def.role==EnemyVisualPartRole::Neck) {
                    for (int x=-2;x<=2;++x) for(int z=-2;z<=2;++z) {
                        const auto slice=glm::vec3(x*.2f,.25f,z*.2f)*def.scale;
                        const auto upper=transformPoint(profile,pose,part,def.offset+slice);
                        const auto lower=transformPoint(profile,pose,part,def.offset+glm::vec3(slice.x,-slice.y,slice.z));
                        blendedBridge &= insidePart(profile,pose,1,upper) && insidePart(profile,pose,0,lower);
                    }
                }
            }
        }
    }
    check("blended-poses-preserve-fixed-limb-pivots-and-muzzle-seam", blendedJoints);
    check("blended-poses-keep-volumetric-neck-connection", blendedBridge);
    EnemyDefinition galleryDefinition;
    galleryDefinition.type = "diagnostic-test";
    galleryDefinition.dimensions = {.13f, .61f, .24f};
    galleryDefinition.combat.mode = EnemyCombatMode::Ranged;
    galleryDefinition.combat.windupTicks = 12;
    galleryDefinition.combat.recoverTicks = 9;
    const auto gallery = gallerySnapshot(galleryDefinition, "windup", 0.f);
    check("gallery-copies-actual-type-dimensions-and-combat-mode",
        gallery.type == galleryDefinition.type &&
        gallery.dimensions == galleryDefinition.dimensions &&
        gallery.combatMode == EnemyCombatMode::Ranged);
    int windupSamples = 0, recoverySamples = 0;
    bool galleryTicks = true;
    for (int tick = 0; tick < 61; ++tick) {
        const auto sample = gallerySnapshot(galleryDefinition, "cycle", tick / 20.f);
        if (sample.combatState == MobCombatState::Windup) {
            ++windupSamples;
            galleryTicks &= sample.combatStateTicksTotal == 12 &&
                sample.combatStateTicksRemaining == 32 - tick;
        }
        if (sample.combatState == MobCombatState::Recover) {
            ++recoverySamples;
            galleryTicks &= sample.combatStateTicksTotal == 9 &&
                sample.combatStateTicksRemaining == 41 - tick;
        }
    }
    check("gallery-cycle-preserves-species-windup-and-recovery-ticks",
        galleryTicks && windupSamples == 12 && recoverySamples == 9);
    check("gallery-attack-boundary-matches-its-real-duration",
        gallerySnapshot(galleryDefinition, "cycle", 1.55f).combatState == MobCombatState::Windup &&
        gallerySnapshot(galleryDefinition, "cycle", 1.60f).combatState == MobCombatState::Recover &&
        gallerySnapshot(galleryDefinition, "cycle", 2.05f).combatState == MobCombatState::Idle);
    check("gallery-invalid-time-starts-at-rest",
        gallerySnapshot(galleryDefinition, "cycle", NAN).combatState == MobCombatState::Idle &&
        gallerySnapshot(galleryDefinition, "cycle", -2.f).combatState == MobCombatState::Idle);
    return failures ? 1 : 0;
}
