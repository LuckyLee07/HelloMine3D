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
}
int main() {
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
