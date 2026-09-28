#include <cstdlib>
#include <iostream>
#include <limits>
#include "../../src/HelloMine3D/Presentation/PlayerHandPresentation.h"

namespace {
int failures = 0;
void check(const char* name, bool passed)
{
    std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
    failures += !passed;
}
bool near(glm::vec3 a, glm::vec3 b) { return glm::length(a-b) < .00002f; }
}

int main()
{
    using namespace PlayerHandPresentation;
    const float dark = lightingExposure(.15f, .15f, 1.f);
    check("local-light-distinguishes-daylit-cave-and-torch",
          dark < .4f && dark > .1f &&
          lightingExposure(1.f, .15f, 1.f) > .99f &&
          lightingExposure(.15f, 1.f, 1.f) > .99f &&
          lightingExposure(.15f, .15f, 0.f) < dark);
    LightingState first;
    const float unknown = updateLighting(first, false, 1.f, 1.f, 1.f, .1f);
    check("unknown-sample-never-flashes-bright-and-first-known-snaps",
          !first.initialized && unknown < .4f &&
          std::abs(updateLighting(first, true, .15f, .15f, 1.f, 0.f) - dark) < 1e-6f);
    const float retained = first.exposure;
    check("unloaded-sample-keeps-light-and-paused-frame-does-not-advance",
          updateLighting(first, false, 1.f, 1.f, 1.f, 1.f) == retained &&
          updateLighting(first, true, 1.f, 1.f, 1.f, 0.f) == retained);
    LightingState at30{dark, true}, at60{dark, true};
    bool monotonic = true;
    for (int frame = 0; frame < 30; ++frame) {
        const float before = at30.exposure;
        updateLighting(at30, true, 1.f, .15f, 1.f, 1.f / 30.f);
        monotonic &= at30.exposure >= before && at30.exposure <= 1.f;
    }
    for (int frame = 0; frame < 60; ++frame)
        updateLighting(at60, true, 1.f, .15f, 1.f, 1.f / 60.f);
    check("lighting-transition-is-bounded-and-frame-rate-independent",
          monotonic && std::abs(at30.exposure - at60.exposure) < 1e-5f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    LightingState invalid;
    check("invalid-light-and-delta-remain-finite",
          std::isfinite(updateLighting(invalid, true, nan, inf, nan, inf)) &&
          updateLighting(invalid, true, 1.f, 1.f, 1.f, nan) == invalid.exposure &&
          lightingExposure(-1.f, 5.f, 5.f) <= 1.f);
    bool geometry = true, bounded = true, projection = true, rigid = true;
    for (auto grip : {Grip::Empty, Grip::Icon, Grip::Block}) {
        const auto& hand = mesh(grip);
        bounded &= !hand.empty() && hand.size() <= 54 && &mesh(grip) == &hand;
        for (const auto& face : hand) {
            const auto& f = face.geometry;
            geometry &= std::abs(glm::length(f.normal) - 1.f) < .00001f;
            geometry &= glm::dot(glm::cross(f.positions[1]-f.positions[0],
                f.positions[2]-f.positions[0]), f.normal) > .00001f;
            geometry &= glm::all(glm::greaterThanEqual(face.colour, glm::vec3(0))) &&
                glm::all(glm::lessThanEqual(face.colour, glm::vec3(255)));
            for (auto p : f.positions) bounded &= glm::length(p) < 1.9f;
        }
        for (float strength : {0.f,.35f,1.f})
            for (float contact : {0.f,.5f,1.f})
                for (int tick = 0; tick < 3600; ++tick) {
                    const auto pose = motion(tick/120., 1.f, strength, true, contact);
                    for (const auto& face : hand)
                        for (auto p : face.geometry.positions) {
                            auto transformed = pose.rotate(p);
                            // Keep a full model unit of clearance from the
                            // perspective eye throughout every action phase.
                            projection &= std::isfinite(transformed.z) && transformed.z < 2.f;
                        }
                    const glm::vec3 handPoint(-.28f,-.32f,.09f), handle(-.25f,-.25f,0);
                    rigid &= std::abs(glm::length(pose.rotate(handPoint)-pose.rotate(handle)) -
                        glm::length(handPoint-handle)) < .00001f;
                }
    }
    check("closed-nondegenerate-hand-faces-and-valid-colours", geometry);
    check("bounded-geometry-and-stable-three-grip-cache", bounded);
    check("all-grips-clear-perspective-eye-through-action-cycle", projection);
    check("hand-and-held-object-stay-rigid-through-action-cycle", rigid);

    bool off = true, reduced = true, returns = true, finite = true;
    const glm::vec3 point(.3f,-.9f,.2f);
    const auto rest = motion(0.,0.f,0.f,false,0.f);
    for (int tick = 0; tick < 7200; ++tick) {
        const double time = tick/60.;
        const auto disabled = motion(time,1.f,0.f,true,1.f);
        off &= near(disabled.rotate(point),rest.rotate(point)) && disabled.bob == 0.f && disabled.swing == 0.f;
        const auto full = motion(time,1.f,1.f,true,.3f);
        const auto low = motion(time,1.f,.35f,true,.3f);
        reduced &= std::abs((low.pitch-rest.pitch) - (full.pitch-rest.pitch)*.35f) < .00001f &&
            std::abs(low.bob - full.bob*.35f) < .00001f && std::abs(low.swing - full.swing*.35f) < .00001f;
        const auto stopped = motion(time,0.f,1.f,false,0.f);
        returns &= stopped.swing == 0.f;
    }
    for (double time : {-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        const auto invalid = motion(time,INFINITY,NAN,true,NAN);
        finite &= std::isfinite(invalid.pitch + invalid.yaw + invalid.roll + invalid.swing + invalid.bob);
    }
    check("off-is-static-during-walking-mining-and-contact", off);
    check("reduced-scales-action-and-walk-amplitude", reduced);
    check("ending-action-clears-swing-without-lingering-state", returns);
    check("invalid-presentation-inputs-remain-finite", finite);

    MotionInput action;
    action.ambientSeconds = 42.;
    action.actionSeconds = .10f;
    action.strength = 1.f;
    action.recoil = .7f;
    action.action = Action::Strike;
    const auto strike = motion(action);
    action.recoil = 0.f;
    const auto noRecoil = motion(action);
    check("recoil-drives-follow-through-without-contact-hold",
          strike.swing > noRecoil.swing && strike.swing > 0.f &&
              strike.swing <= 1.f);

    action.recoil = .6f;
    action.actionSeconds = .14f;
    action.action = Action::Use;
    const auto use = motion(action);
    action.action = Action::Consume;
    const auto consume = motion(action);
    check("use-and-consume-actions-have-distinct-bounded-poses",
          use.swing >= 0.f && use.swing <= 1.f &&
              consume.swing >= 0.f && consume.swing <= 1.f &&
              (std::abs(use.pitch-consume.pitch) > .01f ||
               std::abs(use.roll-consume.roll) > .01f));

    const auto miningPriority = actionPhase(
        true, .14f, Action::Strike, .08f);
    const auto feedbackPriority = actionPhase(
        false, .14f, Action::Strike, .08f);
    check("active-mining-keeps-its-own-action-phase",
          miningPriority.action == Action::Mining &&
              miningPriority.seconds == .14f &&
              !miningPriority.acceptsFeedbackContact &&
              feedbackPriority.action == Action::Strike &&
              feedbackPriority.seconds == .08f &&
              feedbackPriority.acceptsFeedbackContact);

    action.action = Action::Strike;
    action.recoil = .5f;
    action.actionSeconds = .18f;
    const auto missTail = motion(action);
    action.actionSeconds = .199f;
    const auto missNearRest = motion(action);
    action.actionSeconds = .20f;
    const auto missRest = motion(action);
    check("strike-pulse-recovers-continuously-through-020-seconds",
          missTail.swing > missNearRest.swing &&
              missNearRest.swing > missRest.swing &&
              missRest.swing == 0.f);

    action.recoil = 0.f;
    action.action = Action::Mining;
    const auto mining = motion(action);
    action.actionSeconds = 0.f;
    const auto miningReset = motion(action);
    action.actionSeconds = 1000000.f;
    const auto miningLate = motion(action);
    check("mining-elapsed-resets-and-remains-bounded",
          mining.swing > miningReset.swing && miningReset.swing == 0.f &&
              miningLate.swing >= 0.f && miningLate.swing <= 1.f &&
              std::isfinite(miningLate.pitch + miningLate.yaw +
                            miningLate.roll + miningLate.bob));
    // Sample full paths, including the seam, with independent continuity
    // and timing bounds rather than comparing against copied coefficients.
    bool continuous = true, distinctPath = false, offActions = true;
    for (Action kind : {Action::Mining, Action::Strike, Action::Use, Action::Consume}) {
        MotionInput in;
        in.action = kind; in.strength = 1.f; in.recoil = 1.f;
        auto previous = motion(in);
        for (int frame = 1; frame <= 1500; ++frame) {
            in.actionSeconds = frame / 1000.f;
            const auto current = motion(in);
            continuous &= std::abs(current.pitch - previous.pitch) < .09f &&
                std::abs(current.roll - previous.roll) < .10f &&
                current.swing >= 0.f && current.swing <= 1.f;
            previous = current;
            in.strength = 0.f;
            const auto disabled = motion(in);
            offActions &= disabled.swing == 0.f && disabled.bob == 0.f &&
                near(disabled.rotate(point), rest.rotate(point));
            in.strength = 1.f;
        }
    }
    action.action = Action::Mining; action.strength = 1.f; action.recoil = 0.f;
    action.ambientSeconds = 0.; action.contact = 0.f;
    action.actionSeconds = .2f / 2.7f;
    const auto windup = motion(action);
    action.actionSeconds = .42f / 2.7f;
    const auto impact = motion(action);
    distinctPath = windup.roll < rest.roll - .2f && impact.roll > rest.roll + .5f &&
        windup.pitch > rest.pitch && impact.pitch < rest.pitch;
    check("all-action-paths-are-continuous-at-subframe-and-cycle-boundaries", continuous);
    check("mining-lifts-before-a-distinct-downstroke", distinctPath);
    check("off-removes-every-action-even-with-stale-recoil", offActions);
    const auto contactPose = ToolActionPresentation::derive(Action::Strike, .01f, 1.f, 1.f, 1.f);
    const auto missPose = ToolActionPresentation::derive(Action::Strike, .01f, 1.f, 1.f, 0.f);
    check("actual-contact-holds-downstroke-while-miss-keeps-preparation",
        contactPose.strike == 1.f && contactPose.preparation == 0.f &&
        missPose.strike == 0.f && missPose.preparation > 0.f);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
