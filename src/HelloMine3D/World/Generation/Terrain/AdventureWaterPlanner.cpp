#include "AdventureWaterPlanner.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
std::uint64_t mix(std::uint64_t v) noexcept
{
    v^=v>>30;v*=0xbf58476d1ce4e5b9ull;v^=v>>27;v*=0x94d049bb133111ebull;return v^(v>>31);
}
double smooth(double lo,double hi,double value) noexcept
{
    const double t=std::clamp((value-lo)/(hi-lo),0.0,1.0);
    return t*t*t*(t*(t*6-15)+10);
}
double blend(double a,double b,double t) noexcept { return a+(b-a)*t; }
int boundedCoordinate(double value) noexcept
{
    return static_cast<int>(std::clamp(value,
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max())));
}
} // namespace

double AdventureWaterPlanner::random(std::int64_t x,std::int64_t z,std::uint64_t salt) const noexcept
{
    return static_cast<double>(mix(static_cast<std::uint64_t>(m_seed)^salt^
        mix(static_cast<std::uint64_t>(x))^mix(static_cast<std::uint64_t>(z)+0x943bca758460d127ull))>>11)/9007199254740991.0;
}

AdventureWaterPlanner::Node AdventureWaterPlanner::node(std::int64_t cellX,std::int64_t cellZ) const noexcept
{
    Node n;
    n.x=(static_cast<double>(cellX)+.25+.5*random(cellX,cellZ,0x7518b30aac148271ull))*CellSize;
    n.z=(static_cast<double>(cellZ)+.25+.5*random(cellX,cellZ,0x20ab72968ef47c51ull))*CellSize;
    const auto base=m_base.sample(boundedCoordinate(n.x),boundedCoordinate(n.z));
    n.height=base.column.height;
    n.lakeSite=base.column.biome!=TerrainBiome::Ocean && n.height>=59 && n.height<=86;
    n.basin=n.lakeSite && random(cellX,cellZ,0x1454c5e070883f29ull)<.25;
    // Plan the hydraulic grade before choosing outgoing edges. Incoming and
    // outgoing valleys then meet the same submerged lake inlet/outlet, rather
    // than cutting an isolated pond below an unchanged dry channel.
    if(n.basin)n.height=std::min(n.height,74.0);
    n.potential=n.height+random(cellX,cellZ,0x31839a45b9fc6502ull)*.25;
    n.width=3.5+2.5*(1-smooth(75,125,n.height))+std::max(0.0,base.moisture);
    return n;
}

AdventureWaterPlanner::FlowNode AdventureWaterPlanner::planNode(int cellX,int cellZ) const noexcept
{
    const auto a=node(cellX,cellZ);auto b=a;
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
        const auto candidate=node(static_cast<std::int64_t>(cellX)+dx,static_cast<std::int64_t>(cellZ)+dz);
        if(candidate.potential<b.potential)b=candidate;
    }
    return {a.x,a.z,a.height,a.potential,b.x,b.z,b.height,b.potential,b.potential<a.potential};
}

AdventureWaterPlanner::Tile AdventureWaterPlanner::buildTile(std::int64_t cellX,std::int64_t cellZ) const noexcept
{
    std::array<Node,25> nodes{};
    for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x)nodes[(z+2)*5+x+2]=node(cellX+x,cellZ+z);
    Tile result;
    for(int z=-1;z<=1;++z)for(int x=-1;x<=1;++x) {
        const auto &a=nodes[(z+2)*5+x+2];auto b=a;
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
            const auto &candidate=nodes[(z+dz+2)*5+x+dx+2];
            if(candidate.potential<b.potential)b=candidate;
        }
        const int index=(z+1)*3+x+1;
        if(a.lakeSite && (b.potential>=a.potential || a.basin)) {
            result.lakes[index]={a.x,a.z,
                34+26*random(cellX+x,cellZ+z,0x134dc5c90aa8576bull),true};
        }
        if(b.potential>=a.potential) {
            continue;
        }
        auto &edge=result.edges[index];edge.valid=true;
        edge.heightA=a.height;edge.heightB=b.height;edge.widthA=a.width;edge.widthB=b.width;
        const double length=std::hypot(b.x-a.x,b.z-a.z);
        const double bend=(random(cellX+x,cellZ+z,0x730941ad284f529bull)*2-1)*64;
        const Point control{(a.x+b.x)*.5-(b.z-a.z)/length*bend,(a.z+b.z)*.5+(b.x-a.x)/length*bend};
        for(std::size_t i=0;i<edge.points.size();++i) {
            const double t=static_cast<double>(i)/(edge.points.size()-1),u=1-t;
            edge.points[i]={u*u*a.x+2*u*t*control.x+t*t*b.x,u*u*a.z+2*u*t*control.z+t*t*b.z};
        }
        edge.minimumX=edge.maximumX=a.x;edge.minimumZ=edge.maximumZ=a.z;
        for(const auto p:edge.points) {
            edge.minimumX=std::min(edge.minimumX,p.x);edge.maximumX=std::max(edge.maximumX,p.x);
            edge.minimumZ=std::min(edge.minimumZ,p.z);edge.maximumZ=std::max(edge.maximumZ,p.z);
        }
    }
    return result;
}

const AdventureWaterPlanner::Tile &AdventureWaterPlanner::tile(std::int64_t cellX,std::int64_t cellZ) const noexcept
{
    // Only immutable derived graph geometry is cached. Fixed per-thread storage
    // avoids races and unbounded world/seed growth; every hit checks its full key.
    struct Entry { bool valid=false;int seed=0;std::int64_t x=0,z=0;Tile value; };
    thread_local std::array<Entry,64> cache{};
    static_assert(sizeof(cache)<=256*1024,"Water graph cache must stay bounded per thread");
    const auto hash=mix(static_cast<std::uint64_t>(m_seed)^mix(static_cast<std::uint64_t>(cellX))^
                        mix(static_cast<std::uint64_t>(cellZ)+0x8107985611fc43a7ull));
    auto &entry=cache[hash%cache.size()];
    if(!entry.valid || entry.seed!=m_seed || entry.x!=cellX || entry.z!=cellZ) {
        entry.value=buildTile(cellX,cellZ);entry.seed=m_seed;entry.x=cellX;entry.z=cellZ;entry.valid=true;
    }
    return entry.value;
}

AdventureWaterPlanner::Sample AdventureWaterPlanner::sample(int worldX,int worldZ) const noexcept
{
    const auto base=m_base.sample(worldX,worldZ);
    Sample result{base.column,0,0,base};
    if(base.column.biome==TerrainBiome::Ocean)return result;
    const double x=worldX,z=worldZ;
    const auto &graph=tile(static_cast<std::int64_t>(std::floor(x/CellSize)),
                           static_cast<std::int64_t>(std::floor(z/CellSize)));
    const double original=base.column.height;
    double height=original;
    for(const auto &edge:graph.edges) {
        if(!edge.valid)continue;
        if(x<edge.minimumX-40 || x>edge.maximumX+40 || z<edge.minimumZ-40 || z>edge.maximumZ+40)continue;
        double bestDistanceSquared=std::numeric_limits<double>::max(),bestT=0;
        for(std::size_t i=0;i+1<edge.points.size();++i) {
            const auto a=edge.points[i],b=edge.points[i+1];
            const double dx=b.x-a.x,dz=b.z-a.z;
            const double t=std::clamp(((x-a.x)*dx+(z-a.z)*dz)/(dx*dx+dz*dz),0.0,1.0);
            const double offsetX=x-blend(a.x,b.x,t),offsetZ=z-blend(a.z,b.z,t);
            const double distanceSquared=offsetX*offsetX+offsetZ*offsetZ;
            if(distanceSquared<bestDistanceSquared){bestDistanceSquared=distanceSquared;bestT=(i+t)/(edge.points.size()-1);}
        }
        const double width=blend(edge.widthA,edge.widthB,bestT);
        const double influence=1-smooth(width,width+32,std::sqrt(bestDistanceSquared));
        if(influence<=0)continue;
        const double bed=std::max(58.0,blend(edge.heightA,edge.heightB,bestT)-12);
        const double drop=std::clamp(original-bed,0.0,42.0)*influence;
        height=std::min(height,original-drop);
        result.riverInfluence=std::max(result.riverInfluence,drop>0?influence:0);
    }
    for(const auto &lake:graph.lakes) {
        if(!lake.valid)continue;
        const double distance=std::hypot(x-lake.x,z-lake.z);
        const double influence=1-smooth(lake.radius*.42,lake.radius,distance);
        const double bed=60+std::min(2.0,distance/lake.radius*2);
        const double drop=std::clamp(original-bed,0.0,42.0)*influence;
        height=std::min(height,original-drop);
        result.lakeInfluence=std::max(result.lakeInfluence,drop>0?influence:0);
    }
    auto &column=result.column;
    column.height=static_cast<int>(std::lround(height));
    if(column.height==base.column.height)return result;
    if(column.height<64) {
        column.biome=result.lakeInfluence>=result.riverInfluence?TerrainBiome::Lake:TerrainBiome::River;
        column.surface=base.moisture>.1?TerrainFoundation::Surface::Dirt:TerrainFoundation::Surface::Sand;
    } else if(column.height<=67) {
        column.surface=column.height<=65?TerrainFoundation::Surface::Sand:TerrainFoundation::Surface::Grass;
    } else if(original-height>12 && column.height>96) {
        column.surface=TerrainFoundation::Surface::Stone;
    }
    return result;
}
