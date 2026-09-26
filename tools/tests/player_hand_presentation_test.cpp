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
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
