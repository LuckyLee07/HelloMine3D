#pragma once
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

// Water only: subtract the neighbour's immutable opaque 8x8 boundary coverage.
// Each free rectangle is intersected with the original two triangles, retaining
// their diagonal and affine attributes. No snapshot/world/light queries here.
namespace WaterBoundaryClip {
constexpr int MaximumRectangles = 64;
// One eighth-grid cell needs at most two triangles. A merged rectangle
// crossed away from its corners contains at least two cells, so its <=4
// triangles never exceed that cell budget (one-cell diagonal is corner/corner).
constexpr int MaximumTriangles = 64 * 2;
constexpr int MaximumVertices = MaximumTriangles * 3;
struct Point { float u, v; };
using Triangle = std::array<Point, 3>;
inline float cross(Point a, Point b, Point c) noexcept {
    return (b.u-a.u)*(c.v-a.v)-(b.v-a.v)*(c.u-a.u);
}
inline std::array<float,3> weights(Point p,const Triangle &t) noexcept {
    const float determinant=cross(t[0],t[1],t[2]);
    const float b=cross(t[0],p,t[2])/determinant;
    const float c=cross(t[0],t[1],p)/determinant;
    return {1.f-b-c,b,c};
}
inline bool covered(std::uint64_t mask,int u,int v) noexcept {
    return u>=0 && u<8 && v>=0 && v<8 && (mask & (std::uint64_t(1)<<(u+8*v)))!=0;
}
inline bool pin(std::uint64_t mask,Point p,int verticalComponent) noexcept {
    const float y=verticalComponent==0?p.u:p.v;
    // Original top/bottom retain their offset; only opaque cut edges are fixed.
    // The certified VS suppresses waves at positive raw shore, so the shared
    // bank top is .90 and every eighth-grid internal cut is <=.875. This also
    // avoids resampling a different wave at a new rectangle/diagonal vertex.
    if(y<=0.f || y>=1.f) return false;
    constexpr float epsilon=.0001f;
    for(float du:{-epsilon,epsilon})for(float dv:{-epsilon,epsilon})
        if(covered(mask,int(std::floor(p.u*8+du)),int(std::floor(p.v*8+dv))))return true;
    return false;
}
struct Polygon { std::array<Point,8> points{};int count=0; };
inline Polygon cut(const Polygon &input,int axis,float edge,bool greater) noexcept {
    Polygon output;
    const auto coordinate=[&](Point p){return axis==0?p.u:p.v;};
    const auto inside=[&](Point p){return greater?coordinate(p)>=edge:coordinate(p)<=edge;};
    const auto push=[&](Point p){
        if(output.count>0) { const auto q=output.points[output.count-1];
            if(std::abs(p.u-q.u)<.000001f && std::abs(p.v-q.v)<.000001f)return; }
        assert(output.count<int(output.points.size()));output.points[output.count++]=p;
    };
    if(input.count==0)return output;
    Point a=input.points[input.count-1];bool aInside=inside(a);
    for(int i=0;i<input.count;++i) {
        const Point b=input.points[i];const bool bInside=inside(b);
        if(aInside!=bInside) {
            const float t=(edge-coordinate(a))/(coordinate(b)-coordinate(a));
            Point p{a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t};
            if(axis==0)p.u=edge;else p.v=edge;
            push(p);
        }
        if(bInside)push(b);
        a=b;aInside=bInside;
    }
    if(output.count>1) {const auto a=output.points[0],b=output.points[output.count-1];
        if(std::abs(a.u-b.u)<.000001f && std::abs(a.v-b.v)<.000001f)--output.count;}
    return output;
}
template<class Visitor>
int visit(std::uint64_t mask,const std::array<Point,4> &corners,bool flipped,const Visitor &emit) {
    const std::array<std::array<int,3>,2> source=flipped?
        std::array<std::array<int,3>,2>{{{{0,1,3}},{{1,2,3}}}}:
        std::array<std::array<int,3>,2>{{{{0,1,2}},{{2,3,0}}}};
    std::uint64_t remaining=~mask;int triangles=0,rectangles=0;
    for(int v=0;v<8;++v)for(int u=0;u<8;++u) {
        if(!covered(remaining,u,v))continue;
        int width=1;while(u+width<8 && covered(remaining,u+width,v))++width;
        int height=1;
        for(;v+height<8;++height) {
            bool full=true;for(int x=u;x<u+width;++x)full&=covered(remaining,x,v+height);
            if(!full)break;
        }
        for(int y=v;y<v+height;++y)for(int x=u;x<u+width;++x)remaining&=~(std::uint64_t(1)<<(x+8*y));
        ++rectangles;int rectangleTriangles=0;
        for(const auto &indices:source) {
            Polygon p;p.count=3;
            for(int i=0;i<3;++i)p.points[i]=corners[indices[i]];
            p=cut(p,0,u/8.f,true);p=cut(p,0,(u+width)/8.f,false);
            p=cut(p,1,v/8.f,true);p=cut(p,1,(v+height)/8.f,false);
            for(int i=1;i+1<p.count;++i) {
                const Triangle triangle{{p.points[0],p.points[i],p.points[i+1]}};
                if(std::abs(cross(triangle[0],triangle[1],triangle[2]))<=.0000001f)continue;
                emit(triangle,indices);++triangles;++rectangleTriangles;
            }
        }
        // A line divides one rectangle into at most five + three corners.
        assert(rectangleTriangles<=4);
        (void)rectangleTriangles;
    }
    assert(rectangles<=MaximumRectangles && triangles<=MaximumTriangles);
    (void)rectangles;
    return triangles;
}
} // namespace WaterBoundaryClip
