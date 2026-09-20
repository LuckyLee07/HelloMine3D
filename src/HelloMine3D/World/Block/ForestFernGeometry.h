#pragma once

#include "WetlandGrassGeometry.h"

// A low, open whorl made from the same pixel vegetation palette. It remains
// within one harvestable grass block, including its wind and selection mesh.
namespace ForestFernGeometry {
inline bool applies(ChunkBlock block, const BlockShape &shape) noexcept {
    return block.id==static_cast<Block_t>(BlockId::TallGrass) &&
        block.metadata==BlockMetadata::TallGrass::Fern && shape.name=="Cross" && shape.faces.size()==2;
}
inline WetlandGrassGeometry::Model build(unsigned variant, float scale) noexcept {
    WetlandGrassGeometry::Model result;
    for(int i=0;i<6;++i) {
        const float angle=i*1.04719755f+variant*.34f;
        const glm::vec3 d(std::cos(angle),0,std::sin(angle)),w(-d.z,0,d.x);
        const glm::vec3 root(.5f,.035f,.5f),tip=root+d*.43f+glm::vec3(0,.24f,0);
        const glm::vec3 mid=root+d*.21f+glm::vec3(0,.46f,0);
        auto &face=result.faces[result.count++];
        const std::array<glm::vec3,4> points{{root,mid-w*.11f,tip,mid+w*.11f}};
        for(std::size_t n=0;n<points.size();++n) {
            face.positions[n*3]=points[n].x;face.positions[n*3+1]=points[n].y*scale;face.positions[n*3+2]=points[n].z;
            face.repeat[n*2]=(n==0 || n==3)?1.f:0.f;
            face.repeat[n*2+1]=1-points[n].y;
        }
    }
    return result;
}
}
