#include "Actor/WildlifePresentation.h"
#include <iostream>
#include <limits>
#include <set>

namespace {
int checks=0, failures=0;
void check(const std::string& name, bool good) {
    ++checks; failures+=!good;
    std::cout<<(good?"PASS ":"FAIL ")<<name<<'\n';
}
float difference(const WildlifeVisualPose& a,const WildlifeVisualPose& b) {
    float d=std::abs(a.heightOffset-b.heightOffset);
    for(std::size_t i=0;i<a.rotations.size();++i)
        d=std::max(d,glm::length(a.rotations[i]-b.rotations[i]));
    return d;
}
glm::vec3 transform(const WildlifeVisualProfile& p,const WildlifeVisualPose& pose,
                    std::size_t i,glm::vec3 point) {
    return p.parts[i].offset+pose.offsets[i]+
        WildlifePresentation::pitchVector(point-p.parts[i].offset,pose.rotations[i].x);
}
bool joints(const WildlifeVisualProfile& p,const WildlifeVisualPose& pose) {
    std::size_t head=0;
    for(std::size_t i=0;i<p.partCount;++i)
        if(p.parts[i].role==WildlifeVisualRole::Head)head=i;
    for(std::size_t i=0;i<p.partCount;++i) {
        const auto& part=p.parts[i];
        if(part.role==WildlifeVisualRole::Leg) {
            const auto hip=part.offset+glm::vec3(0,.5f*part.scale.y,0);
            if(glm::distance(transform(p,pose,i,hip),hip)>1e-5f)return false;
        }
        if(part.role==WildlifeVisualRole::Ear) {
            const auto root=part.offset-glm::vec3(0,.5f*part.scale.y,0);
            if(glm::distance(transform(p,pose,i,root),transform(p,pose,head,root))>1e-5f)return false;
        }
        if(part.role==WildlifeVisualRole::Muzzle || part.role==WildlifeVisualRole::Beak ||
           part.role==WildlifeVisualRole::Neck) {
            const auto& h=p.parts[head];
            const auto lo=glm::max(part.offset-part.scale*.5f,h.offset-h.scale*.5f);
            const auto hi=glm::min(part.offset+part.scale*.5f,h.offset+h.scale*.5f);
            if(!glm::all(glm::lessThanEqual(lo,hi)))return false;
            const auto seam=(lo+hi)*.5f;
            if(glm::distance(transform(p,pose,i,seam),transform(p,pose,head,seam))>1e-5f)return false;
        }
    }
    return true;
}

ActorSnapshot motionSnapshot(glm::vec3 position = {0.f,64.f,0.f}) {
    ActorSnapshot snapshot;
    snapshot.id = 91;
    snapshot.type = WildlifeSpecies::Sheep;
    snapshot.dimensions = {.42f,.55f,.30f};
    snapshot.position = position;
    return snapshot;
}

void accepted(ActorSnapshot& snapshot, glm::vec3 to, float seconds,
              WildlifeMotionPath kind) {
    auto& history = snapshot.wildlifeMotionHistory;
    if (history.count == history.segments.size()) {
        for (std::size_t i = 1; i < history.count; ++i)
            history.segments[i - 1] = history.segments[i];
        --history.count;
    }
    history.segments[history.count++] = {++history.newestSequence,
        snapshot.position, to, seconds, kind};
    snapshot.position = to;
    snapshot.wildlifeMotionSeconds += seconds;
}

bool near(glm::vec3 actual, glm::vec3 expected, float tolerance = .0001f) {
    return glm::distance(actual, expected) <= tolerance;
}

void motionTests() {
    using Motion = WildlifePresentation::MotionBlend;
    using Reason = Motion::ResetReason;
    using Path = WildlifeMotionPath;
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        check("motion/initial-authoritative-anchor",
            motion.update(snapshot,0.f) == snapshot.position &&
            motion.resetReason() == Reason::Initial);
        accepted(snapshot,{.3f,65.f,0.f},.20f,Path::SupportRise);
        const auto rising = motion.update(snapshot,.10f);
        check("motion/up-rises-at-source-before-horizontal",
            near(rising,{0.f,64.65f,0.f}) && motion.pendingSegmentCount() == 1);
        check("motion/up-finishes-at-authoritative-target",
            motion.update(snapshot,.10f) == snapshot.position &&
            motion.pendingSegmentCount() == 0);
        accepted(snapshot,{.6f,64.f,0.f},.20f,Path::SupportDescent);
        const auto horizontal = motion.update(snapshot,.03f);
        const auto descending = motion.update(snapshot,.05f);
        check("motion/down-crosses-upper-plane-before-target-descent",
            near(horizontal,{.495f,65.f,0.f}) &&
            near(descending,{.6f,64.78f,0.f}));
        check("motion/down-finishes-without-overshoot",
            motion.update(snapshot,.20f) == snapshot.position);
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        accepted(snapshot,{.3f,65.f,0.f},.20f,Path::SupportRise);
        const auto before = motion.update(snapshot,.04f);
        accepted(snapshot,{.3f,65.f,.3f},.05f,Path::GroundedLevel);
        const auto after = motion.update(snapshot,.000001f);
        check("motion/new-tick-preserves-current-segment-continuity",
            near(before,{0.f,64.26f,0.f}) && glm::distance(after,before) < .0001f &&
            after.x == 0.f && after.z == 0.f && motion.resetReason() == Reason::None);
        check("motion/compressed-remaining-queue-catches-up-within-200ms",
            motion.update(snapshot,.20f) == snapshot.position &&
            motion.pendingSegmentCount() == 0);
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        accepted(snapshot,{.2f,65.f,0.f},.05f,Path::SupportRise);
        accepted(snapshot,{.2f,65.f,.2f},.05f,Path::GroundedLevel);
        accepted(snapshot,{.4f,64.f,.2f},.05f,Path::SupportDescent);
        accepted(snapshot,{.4f,64.f,0.f},.05f,Path::GroundedLevel);
        accepted(snapshot,{.6f,65.f,0.f},.05f,Path::SupportRise);
        const std::array<glm::vec3,5> expected{{
            {0.f,64.6f,0.f}, {.2f,65.f,.1f}, {.4f,64.6f,.2f},
            {.4f,64.f,.1f}, {.4f,64.6f,0.f}}};
        bool allCorners = true;
        for (std::size_t i = 0; i < expected.size(); ++i)
            allCorners &= near(motion.update(snapshot,i == 0 ? .02f : .04f),expected[i]) &&
                          motion.resetReason() == Reason::None;
        check("motion/five-skipped-ticks-replay-each-checked-corner",allCorners);
        check("motion/five-skipped-ticks-finish-in-200ms",
            near(motion.update(snapshot,.02f),snapshot.position) &&
            motion.pendingSegmentCount() == 0 && motion.seenSequence() == 5);
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        accepted(snapshot,{0.f,63.8f,0.f},.05f,Path::AirborneFall);
        check("motion/ordinary-fall-is-smooth-vertical",
            near(motion.update(snapshot,.025f),{0.f,63.9f,0.f}) &&
            motion.resetReason() == Reason::None);
        motion.update(snapshot,.025f);
        const float start = snapshot.position.y;
        for (int i = 0; i < 5; ++i)
            accepted(snapshot,snapshot.position-glm::vec3(0.f,.8f,0.f),.20f,Path::AirborneFall);
        const auto first = motion.update(snapshot,.005f);
        check("motion/skipped-falls-over-one-block-do-not-teleport",
            near(first,{0.f,start-.1f,0.f}) && first.y > snapshot.position.y &&
            motion.resetReason() == Reason::None && motion.pendingSegmentCount() == 5);
        check("motion/all-true-fall-segments-catch-up-within-200ms",
            motion.update(snapshot,.20f) == snapshot.position &&
            motion.pendingSegmentCount() == 0);
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        accepted(snapshot,{.3f,64.f,0.f},.05f,Path::GroundedLevel);
        accepted(snapshot,{0.f,64.f,0.f},.05f,Path::GroundedLevel);
        const auto outgoing = motion.update(snapshot,.025f);
        const auto returning = motion.update(snapshot,.05f);
        check("motion/return-to-same-target-still-consumes-both-sequences",
            near(outgoing,{.15f,64.f,0.f}) && near(returning,{.15f,64.f,0.f}) &&
            motion.seenSequence() == 2 && motion.pendingSegmentCount() == 1);
        check("motion/reversal-finishes-at-original-position",
            near(motion.update(snapshot,.025f),snapshot.position));
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        accepted(snapshot,{.3f,64.f,0.f},.05f,Path::GroundedLevel);
        const auto before = motion.update(snapshot,.025f);
        accepted(snapshot,{.3f,64.f,.3f},.05f,Path::GroundedLevel);
        bool frozen = true;
        for (float dt : {0.f,-.1f,std::numeric_limits<float>::quiet_NaN(),
                         std::numeric_limits<float>::infinity(),
                         -std::numeric_limits<float>::infinity()})
            frozen &= motion.update(snapshot,dt) == before &&
                      motion.seenSequence() == 1 && motion.pendingSegmentCount() == 1;
        check("motion/pause-negative-and-nonfinite-dt-freeze-without-consuming-new-history",frozen);
        const auto resumed = motion.update(snapshot,.000001f);
        check("motion/resume-consumes-new-history-without-restarting-current-path",
            glm::distance(resumed,before) < .0001f && resumed.z == 0.f &&
            motion.seenSequence() == 2 && motion.pendingSegmentCount() == 2);
        check("motion/resumed-path-reaches-target",motion.update(snapshot,.20f) == snapshot.position);
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        for (int i = 1; i <= 9; ++i)
            accepted(snapshot,{float(i)*.1f,64.f,0.f},.05f,Path::GroundedLevel);
        check("motion/history-gap-is-an-explicit-rebase",
            motion.update(snapshot,.01f) == snapshot.position &&
            motion.resetReason() == Reason::HistoryGap && motion.pendingSegmentCount() == 0);
        check("motion/history-is-a-bounded-eight-segment-value",
            snapshot.wildlifeMotionHistory.count == 8 &&
            snapshot.wildlifeMotionHistory.segments[0].sequence == 2 &&
            snapshot.wildlifeMotionHistory.newestSequence == 9);
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        for (int i = 1; i <= 8; ++i)
            accepted(snapshot,{float(i)*.1f,64.f,0.f},.05f,Path::GroundedLevel);
        motion.update(snapshot,.001f);
        const bool full = motion.pendingSegmentCount() == 8;
        accepted(snapshot,{.9f,64.f,0.f},.05f,Path::GroundedLevel);
        check("motion/queue-overflow-is-explicit-with-no-unchecked-diagonal",
            full && motion.update(snapshot,.001f) == snapshot.position &&
            motion.resetReason() == Reason::QueueOverflow && motion.pendingSegmentCount() == 0);
    }
    {
        auto snapshot = motionSnapshot();
        Motion motion;
        motion.update(snapshot,0.f);
        accepted(snapshot,{.3f,64.f,0.f},.05f,Path::GroundedLevel);
        const auto copied = snapshot;
        motion.update(snapshot,.01f);
        accepted(snapshot,{.3f,64.f,.3f},.05f,Path::GroundedLevel);
        check("motion/copied-snapshot-history-is-independent",
            copied.wildlifeMotionHistory.count == 1 &&
            copied.wildlifeMotionHistory.newestSequence == 1 &&
            copied.wildlifeMotionHistory.segments[0].to == glm::vec3(.3f,64.f,0.f));
        check("motion/long-frame-explicitly-clears-path",
            motion.update(snapshot,.30f) == snapshot.position &&
            motion.resetReason() == Reason::LongFrame && motion.pendingSegmentCount() == 0);
        snapshot.position.x += 100.f;
        check("motion/teleport-with-stale-history-is-explicit",
            motion.update(snapshot,.01f) == snapshot.position &&
            motion.resetReason() == Reason::Teleport);
        snapshot.wildlifeMotionHistory = {};
        snapshot.position.z += .3f;
        check("motion/history-disappearance-cannot-fall-back-to-a-diagonal",
            motion.update(snapshot,.01f) == snapshot.position &&
            motion.resetReason() == Reason::HistoryGap);
    }
    {
        bool invalid = true;
        for (int fault = 0; fault < 9; ++fault) {
            auto snapshot = motionSnapshot();
            Motion motion;
            motion.update(snapshot,0.f);
            accepted(snapshot,{.3f,64.f,0.f},.05f,Path::GroundedLevel);
            auto& history = snapshot.wildlifeMotionHistory;
            switch (fault) {
                case 0: history.count = 9; break;
                case 1: history.segments[0].seconds = 0.f; break;
                case 2: history.segments[0].seconds = std::numeric_limits<float>::quiet_NaN(); break;
                case 3: history.segments[0].kind = Path::None; break;
                case 4: history.segments[0].kind = Path::AirborneFall; break;
                case 5: history.segments[0].sequence = 0; break;
                case 6: history.segments[0].from.y = 257.f; break;
                case 7: history.segments[0].to.x = 2.f; break;
                case 8: history.newestSequence = 2; break;
            }
            invalid &= motion.update(snapshot,.01f) == snapshot.position &&
                       motion.resetReason() == Reason::InvalidHistory &&
                       motion.pendingSegmentCount() == 0;
        }
        check("motion/malformed-history-cannot-be-consumed",invalid);
    }
    {
        auto snapshot = motionSnapshot({0.f,64.25f,0.f});
        Motion motion;
        motion.update(snapshot,0.f);
        accepted(snapshot,{.3f,64.f,0.f},.20f,Path::GroundedLevel);
        check("motion/level-branch-grid-rounding-retains-axis-path",
            near(motion.update(snapshot,.05f),{.1375f,64.25f,0.f}) &&
            motion.resetReason() == Reason::None);
        motion.update(snapshot,.20f);
        snapshot = motionSnapshot();
        Motion gallery;
        gallery.update(snapshot,0.f);
        snapshot.position.x = .3f;
        snapshot.wildlifeMotionSeconds = .05f;
        check("motion/neutral-history-retains-old-gallery-interpolation",
            near(gallery.update(snapshot,.025f),{.15f,64.f,0.f}));
        snapshot.position.z = .3f;
        check("motion/neutral-gallery-also-freezes-paused-target",
            near(gallery.update(snapshot,0.f),{.15f,64.f,0.f}));
    }
    std::cout << "motion_segment_bytes=" << sizeof(WildlifeMotionSegment)
              << " motion_history_bytes=" << sizeof(WildlifeMotionHistory)
              << " motion_blend_bytes=" << sizeof(Motion)
              << " pose_blend_bytes=" << sizeof(WildlifePresentation::PoseBlend)
              << " actor_snapshot_bytes=" << sizeof(ActorSnapshot) << '\n';
    check("motion/fixed-history-and-render-queue-capacity",
        sizeof(WildlifeMotionHistory) <= 336 && sizeof(Motion) <= 512);
}
}
int main() {
    motionTests();
    for(const char* type:{WildlifeSpecies::Sheep,WildlifeSpecies::Rabbit,WildlifeSpecies::MarshBird}) {
        const auto profile=WildlifePresentation::profileFor(type);
        ActorSnapshot s;s.id=431;s.type=type;s.position={4,64,-8};
        s.dimensions = profile.speciesIndex==0 ? glm::vec3(.42f,.55f,.30f) : profile.speciesIndex==1 ? glm::vec3(.22f,.27f,.22f) : glm::vec3(.25f,.36f,.23f);
        WildlifePresentation::PoseBlend blend;
        auto a=blend.update(s,profile,s.position,0,1);
        s.rotation.y=170;s.wildlifeActivity=int(WildlifeActivity::Forage);
        auto b=blend.update(s,profile,s.position,1.f/60,1);
        check(std::string(type)+"/turn-and-activity-do-not-snap",
            b.yawDegrees>0 && b.yawDegrees<30 && difference(a.pose,b.pose)>.01f && difference(a.pose,b.pose)<5);
        const auto paused=blend.update(s,profile,s.position,0,1);
        check(std::string(type)+"/paused-angles-and-joints",paused.yawDegrees==b.yawDegrees && difference(paused.pose,b.pose)==0 && joints(profile,b.pose));
        WildlifePresentation::PoseBlend wrap;s.rotation.y=179;s.wildlifeActivity=0;wrap.update(s,profile,s.position,0,1);
        s.rotation.y=-179;auto shortest=wrap.update(s,profile,s.position,.05f,1);
        check(std::string(type)+"/shortest-heading-across-180",std::abs(shortest.yawDegrees)>179 && std::abs(std::remainder(shortest.yawDegrees+179,360.f))<2);
        const auto constant=[&](int fps){WildlifePresentation::PoseBlend p;s.rotation.y=0;s.wildlifeActivity=0;p.update(s,profile,s.position,0,1);s.rotation.y=90;s.wildlifeActivity=1;WildlifePresentation::AnimatedPose out;for(int i=0;i<fps;++i)out=p.update(s,profile,s.position,1.f/fps,1);return out;};
        const auto slow=constant(30),fast=constant(120);
        check(std::string(type)+"/30-120hz-target-equivalence",std::abs(slow.yawDegrees-fast.yawDegrees)<.001f && difference(slow.pose,fast.pose)<.001f);
        bool attached=true,bounded=true,smooth=true,feetClear=true;float greatest=0;std::set<int> forage;
        WildlifePresentation::PoseBlend cycles;s.position={0,64,0};s.rotation.y=0;s.wildlifeActivity=0;
        auto previous=cycles.update(s,profile,s.position,0,1);float maxLeg=0;
        for(int frame=1;frame<=1200;++frame) {
            s.wildlifeMotionSeconds=float(frame)/60;s.wildlifeActivity=(frame/180)%4;
            if(s.wildlifeActivity>=2)s.position.x+=.75f/60;
            if(frame%180==0)s.rotation.y=std::remainder(s.rotation.y+95,360.f);
            const auto p=cycles.update(s,profile,s.position,1.f/60,1);
            attached &= joints(profile,p.pose);
            greatest=std::max(greatest,difference(p.pose,previous.pose));smooth &= difference(p.pose,previous.pose)<7;
            bounded &= p.pose.heightOffset>=0 && p.pose.heightOffset<.08f;
            for(std::size_t i=0;i<profile.partCount;++i) {
                bounded &= glm::length(p.pose.rotations[i])<32 && glm::length(p.pose.offsets[i])<.4f;
                if(profile.parts[i].role==WildlifeVisualRole::Leg) {
                    maxLeg=std::max(maxLeg,std::abs(p.pose.rotations[i].x));
                    for(int y:{-1,1})for(int z:{-1,1}) {
                        const auto corner=profile.parts[i].offset+glm::vec3(0,y*.5f*profile.parts[i].scale.y,z*.5f*profile.parts[i].scale.z);
                        const float aboveGround=s.dimensions.y*(1+2*transform(profile,p.pose,i,corner).y)+p.pose.heightOffset;
                        feetClear &= aboveGround>=-.00001f;
                    }
                }
            }
            previous=p;
        }
        check(std::string(type)+"/1200-frame-connected-joints",attached);
        check(std::string(type)+"/feet-corners-stay-above-support-plane",feetClear);
        check(std::string(type)+"/live-bounded-smooth-transitions",bounded && smooth && maxLeg>8);
        std::cout<<"max_frame_rotation_delta="<<greatest<<" max_leg="<<maxLeg<<'\n';
        s.wildlifeActivity=int(WildlifeActivity::Wander);
        WildlifePresentation::AnimatedPose stationary;
        for(int i=0;i<120;++i)stationary=cycles.update(s,profile,s.position,1.f/60,1);
        bool neutralLegs=true;for(std::size_t i=0;i<profile.partCount;++i)if(profile.parts[i].role==WildlifeVisualRole::Leg)neutralLegs &= glm::length(stationary.pose.rotations[i])<.001f;
        check(std::string(type)+"/blocked-wander-stops-feet",neutralLegs && stationary.pose.heightOffset<.0001f);
        const auto off=cycles.update(s,profile,s.position,.016f,0);bool neutral=off.pose.heightOffset==0;
        for(std::size_t i=0;i<profile.partCount;++i)neutral &= off.pose.rotations[i]==glm::vec3(0) && off.pose.offsets[i]==glm::vec3(0);
        check(std::string(type)+"/off-is-immediate-neutral",neutral);
        WildlifePresentation::PoseBlend offPause;
        s.rotation.y=17;
        offPause.update(s,profile,s.position,0,0);
        s.rotation.y=73;
        const auto heldOff=offPause.update(s,profile,s.position,0,0);
        const auto resumedOff=offPause.update(s,profile,s.position,.016f,0);
        check(std::string(type)+"/off-paused-new-heading-is-held-until-resume",
            heldOff.yawDegrees==17 && resumedOff.yawDegrees==73);
        s.position.x+=100;s.rotation.y=-78;const auto tele=cycles.update(s,profile,s.position,.016f,1);
        check(std::string(type)+"/teleport-rebases-without-frantic-step",tele.yawDegrees==-78 && tele.pose.heightOffset==0);
        s.wildlifeActivity=1;s.rotation.y=42;const auto gap=cycles.update(s,profile,s.position,1,1);
        check(std::string(type)+"/long-frame-clears-history",gap.yawDegrees==42 && joints(profile,gap.pose));
        const auto bad=cycles.update(s,profile,s.position,std::numeric_limits<float>::quiet_NaN(),1);
        check(std::string(type)+"/invalid-frame-is-finite",std::isfinite(bad.yawDegrees) && joints(profile,bad.pose));
        for(int id=1;id<=24;++id){s.id=id;const auto pose=WildlifePresentation::poseFor(s,profile,0,1);for(std::size_t i=0;i<profile.partCount;++i)if(profile.parts[i].role==WildlifeVisualRole::Head)forage.insert(int(std::round(pose.rotations[i].x*10)));}
        check(std::string(type)+"/same-age-herd-has-distinct-foraging",forage.size()>=18);
        check(std::string(type)+"/fixed-geometry-and-history-budgets",profile.partCount<=8 && sizeof(WildlifePresentation::PoseBlend)<=192);
    }
    std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
