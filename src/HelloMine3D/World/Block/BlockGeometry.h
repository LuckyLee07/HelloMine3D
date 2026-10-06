#pragma once
#include "BlockDefinition.h"
#include "ChunkBlock.h"
#include <algorithm>
#include <cmath>
#include <limits>

// Pure, cell-local queries. Legacy cells retain their original whole-cell
// collision/selection; only registered v2 architectural IDs use precise boxes.
namespace BlockGeometry {
inline bool usesCompound(BlockId id) noexcept {
    return id==BlockId::StoneStep || id==BlockId::StoneWindowFrame;
}
inline bool validMetadata(Block_t id,BlockMetadata_t metadata) noexcept {
    return !usesCompound(static_cast<BlockId>(id)) || metadata<4;
}
inline bool validMetadata(ChunkBlock block) noexcept { return validMetadata(block.id,block.metadata); }
inline unsigned orientation(ChunkBlock block) noexcept { return block.metadata & 3u; }
inline BlockMetadata_t orientationFromYaw(float yaw) noexcept {
    if(!std::isfinite(yaw)) return 0;
    float angle=std::fmod(yaw,360.f);if(angle<0) angle+=360.f;
    return static_cast<BlockMetadata_t>(static_cast<unsigned>(std::floor((angle+45.f)/90.f)) & 3u);
}
struct Bounds { glm::vec3 minimum;glm::vec3 maximum; };
inline bool intersects(const Bounds &a,const Bounds &b,float epsilon=0.0001f) noexcept {
    for(int axis=0;axis<3;++axis)
        if(a.maximum[axis]<=b.minimum[axis]+epsilon || a.minimum[axis]>=b.maximum[axis]-epsilon) return false;
    return true;
}
template<class Visitor>
inline void collisionBoxes(const BlockDefinition &definition,ChunkBlock block,
                           const glm::ivec3 &position,const Visitor &visit) {
    if(!definition.collidable) return;
    const glm::vec3 origin(position);
    if(usesCompound(definition.id) && definition.render.shape.isCompound()) {
        for(const auto &box:definition.render.shape.variants[orientation(block)].boxes)
            if(box.collidable) visit(Bounds{origin+glm::vec3(box.minimum[0],box.minimum[1],box.minimum[2]),
                                           origin+glm::vec3(box.maximum[0],box.maximum[1],box.maximum[2])});
    } else visit(Bounds{origin,origin+glm::vec3(1.f)});
}
inline bool collides(const BlockDefinition &definition,ChunkBlock block,
                     const glm::ivec3 &position,const Bounds &bounds) {
    bool hit=false;collisionBoxes(definition,block,position,[&](const Bounds &box){hit|=intersects(box,bounds);});return hit;
}
inline bool rayBox(const glm::vec3 &origin,const glm::vec3 &direction,
                   const Bounds &box,float maxDistance,float &distance,glm::ivec3 &normal) noexcept {
    float near=0,far=maxDistance;normal=glm::ivec3(0);
    for(int axis=0;axis<3;++axis) {
        if(std::abs(direction[axis])<0.000001f) {
            if(origin[axis]<box.minimum[axis] || origin[axis]>box.maximum[axis]) return false;
            continue;
        }
        float first=(box.minimum[axis]-origin[axis])/direction[axis];
        float second=(box.maximum[axis]-origin[axis])/direction[axis];
        const int face=direction[axis]>0?-1:1;
        if(first>second) std::swap(first,second);
        if(first>near) {near=first;normal=glm::ivec3(0);normal[axis]=face;}
        far=std::min(far,second);if(near>far) return false;
    }
    distance=near;return near<=maxDistance && far>=0;
}
inline bool pick(const BlockDefinition &definition,ChunkBlock block,const glm::ivec3 &position,
                 const glm::vec3 &origin,const glm::vec3 &direction,float maxDistance,
                 float &distance,glm::ivec3 &normal) {
    bool hit=false;distance=maxDistance;const glm::vec3 offset(position);
    for(const auto &box:definition.render.shape.variants[orientation(block)].boxes) {
        if(!box.selectable) continue;
        const Bounds bounds{offset+glm::vec3(box.minimum[0],box.minimum[1],box.minimum[2]),
                            offset+glm::vec3(box.maximum[0],box.maximum[1],box.maximum[2])};
        float d=0;glm::ivec3 n(0);
        if(rayBox(origin,direction,bounds,distance,d,n)) {hit=true;distance=d;normal=n;}
    }
    return hit;
}
} // namespace BlockGeometry
