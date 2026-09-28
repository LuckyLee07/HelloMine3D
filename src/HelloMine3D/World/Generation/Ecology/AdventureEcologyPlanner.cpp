#include "AdventureEcologyPlanner.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
std::uint64_t mix(std::uint64_t v) noexcept {
    v ^= v >> 30; v *= 0xbf58476d1ce4e5b9ull;
    v ^= v >> 27; v *= 0x94d049bb133111ebull; return v ^ (v >> 31);
}
double unit(std::uint64_t v) noexcept { return static_cast<double>(v >> 11) / 9007199254740991.0; }
double smooth(double a, double b, double x) noexcept {
    const double t = std::clamp((x-a)/(b-a),0.0,1.0); return t*t*(3-2*t);
}
std::int64_t cell(int value) noexcept {
    std::int64_t q = value / AdventureEcologyPlanner::TreeCellSize;
    if(value % AdventureEcologyPlanner::TreeCellSize < 0) --q;
    return q;
}
}

std::uint64_t AdventureEcologyPlanner::hash(std::int64_t x, std::int64_t z, std::uint64_t salt) const noexcept {
    return mix(mix(static_cast<std::uint64_t>(m_seed)) ^ salt ^
        mix(static_cast<std::uint64_t>(x)+0x632be59bd9b4e019ull) ^
        mix(static_cast<std::uint64_t>(z)+0x8cb92baa3f3d8dd7ull));
}

double AdventureEcologyPlanner::noise(int x, int z, double scale, std::uint64_t salt) const noexcept {
    const double px=x/scale,pz=z/scale;
    const auto ix=static_cast<std::int64_t>(std::floor(px)),iz=static_cast<std::int64_t>(std::floor(pz));
    const double tx=smooth(0,1,px-ix),tz=smooth(0,1,pz-iz);
    const double a=unit(hash(ix,iz,salt)),b=unit(hash(ix+1,iz,salt));
    const double c=unit(hash(ix,iz+1,salt)),d=unit(hash(ix+1,iz+1,salt));
    return (a+(b-a)*tx)*(1-tz)+(c+(d-c)*tx)*tz;
}

TerrainFoundation::Column AdventureEcologyPlanner::sampleWaterColumn(
    int x, int z) const noexcept {
    const auto water=m_water.sample(x,z);
    auto column=water.column;
    if(m_relief)column.height=m_local.sample(x,z,water).height;
    return column;
}

AdventureEcologyPlanner::Sample AdventureEcologyPlanner::sample(int x, int z) const noexcept {
    const auto water=m_water.sample(x,z);
    const auto &base=water.base;
    Sample result;
    result.column=water.column;result.region=base.region;result.moisture=base.moisture;
    const auto relief=m_relief?m_local.sample(x,z,water):LocalTerrainPlanner::Sample{};
    if(m_relief)result.column.height=relief.height;
    result.grove=noise(x,z,96,0x73906143ull)*.72+noise(x,z,28,0xf138ad7bull)*.28;
    const double patch=noise(x,z,19,0x4de2713aull);
    auto &c=result.column;
    using R=AdventureRegion;using S=TerrainFoundation::Surface;
    // Patch-scale regional choice makes a mixed ecotone instead of an abrupt
    // switch at the dominant region boundary. Core regions remain coherent.
    if(base.region!=R::Coast && base.region!=R::Ocean) {
        const double choice=noise(x,z,37,0xb15b259dull);
        double sum=0;
        for(std::size_t i=0;i<base.weights.size();++i) {
            sum+=base.weights[i];
            if(choice<=sum) {result.region=static_cast<R>(i);break;}
        }
    }
    const double alpine=base.weights[static_cast<std::size_t>(R::Alpine)];
    const double conifer=base.weights[static_cast<std::size_t>(R::ConiferHighland)];
    result.snowLine=136-20*alpine-6*conifer+(noise(x,z,73,0x1cec412bull)-.5)*18;
    result.shore=c.height>=62 && c.height<=67;
    if(c.height<64) {
        if(c.biome!=TerrainBiome::Ocean && (base.moisture>0 || result.region==R::Wetland))c.surface=S::Silt;
        else if(patch>.68)c.surface=S::Gravel;
        return result;
    }
    if((alpine+conifer)>.30 && c.height>=result.snowLine) {
        c.surface=patch>.22?S::Snow:S::Stone; return result;
    }
    if(result.shore) {
        if(result.region==R::Wetland || (base.moisture>.25 && patch>.38))c.surface=patch>.72?S::Clay:S::Silt;
        else if(patch>.57)c.surface=S::Gravel;
        else if(c.height<=65)c.surface=S::Sand;
        return result;
    }
    switch(result.region) {
        case R::Woodland:
        case R::ConiferHighland:
            if(c.surface!=S::Stone || c.height<result.snowLine-14) {
                c.surface=result.grove>.48 && patch>.25?S::ForestFloor:S::Grass;
                if(result.grove>.65 && patch>.73)c.surface=S::MossStone;
            }
            break;
        case R::Alpine:
            c.surface=c.height>result.snowLine-16?(patch>.58?S::Gravel:S::Stone):
                (result.grove>.55?S::ForestFloor:S::Grass);
            break;
        case R::Canyon:
            c.surface=c.height<83? (patch>.50?S::Gravel:S::Sand):
                (patch>.62?S::Clay:S::Stone);
            break;
        case R::Wetland:
            c.surface=patch>.62?S::Silt:S::Grass;
            if(patch>.78 && c.height>69)c.surface=S::MossStone;
            break;
        case R::Meadow:
            if(c.surface!=S::Stone)c.surface=S::Grass;
            break;
        case R::Dunes: c.surface=S::Sand; break;
        case R::Coast: case R::Ocean: break;
    }
    if(m_relief && c.height>68 && result.region!=R::Wetland && result.region!=R::Dunes) {
        // Regional patches remain coherent, but steep rock and deposited ground
        // now follow the actual parent slope instead of a contour-wide soil band.
        const auto offsetHeight=[&](int dx,int dz) {
            const auto px=std::clamp(static_cast<std::int64_t>(x)+dx,
                static_cast<std::int64_t>(std::numeric_limits<int>::min()),
                static_cast<std::int64_t>(std::numeric_limits<int>::max()));
            const auto pz=std::clamp(static_cast<std::int64_t>(z)+dz,
                static_cast<std::int64_t>(std::numeric_limits<int>::min()),
                static_cast<std::int64_t>(std::numeric_limits<int>::max()));
            return sampleWaterColumn(static_cast<int>(px),static_cast<int>(pz)).height;
        };
        // Four metres measure the ground the player actually steps on. The
        // broad patch slope must not strip a level landing of soil and seeds.
        const double dx=(offsetHeight(2,0)-offsetHeight(-2,0))*.25;
        const double dz=(offsetHeight(0,2)-offsetHeight(0,-2))*.25;
        const double actualSlope=std::hypot(dx,dz);
        const double exposed=smooth(.45,1.0,actualSlope);
        if(exposed>patch+.12)c.surface=S::Stone;
        else if(relief.deposit>.35 && patch>.30)
            c.surface=actualSlope>.5?S::Gravel:S::Dirt;
    }
    return result;
}

bool AdventureEcologyPlanner::treeAnchor(int x, int z) const noexcept {
    const auto cx=cell(x),cz=cell(z);const auto h=hash(cx,cz,0xf1e257a1ull);
    return cx*TreeCellSize+1+static_cast<int>((h>>9)%5)==x &&
        cz*TreeCellSize+1+static_cast<int>((h>>23)%5)==z;
}

AdventureEcologyPlanner::Tree AdventureEcologyPlanner::tree(int x, int z,const Sample &s) const noexcept {
    Tree result;
    if(!treeAnchor(x,z) || s.column.height<65 || s.column.height>s.snowLine-7)return result;
    using R=AdventureRegion;using S=TerrainFoundation::Surface;using T=AdventureTreeKind;
    if(s.column.surface==S::Snow || s.column.surface==S::Stone || s.column.surface==S::Gravel || s.column.surface==S::Clay)return result;
    const auto h=hash(cell(x),cell(z),0x9b17e2cfull);
    const double density=smooth(.25,.75,s.grove);
    double chance=0;
    switch(s.region) {
        case R::Woodland: chance=.14+.70*density*density;result.kind=unit(mix(h))<.33?T::Birch:T::Oak;break;
        case R::ConiferHighland: chance=.15+.74*density*density;result.kind=T::Spruce;break;
        case R::Alpine: chance=.22*density;result.kind=T::Spruce;break;
        case R::Meadow: chance=.015+.09*density*density;result.kind=T::Oak;break;
        case R::Wetland: chance=.05+.24*density;result.kind=T::Willow;break;
        case R::Dunes: chance=s.shore?.12:.035;result.kind=s.shore?T::Palm:T::Cactus;break;
        case R::Coast: chance=s.column.height>66?.14*density:0;result.kind=s.moisture<0?T::Palm:T::Oak;break;
        case R::Canyon: chance=.02;result.kind=T::Cactus;break;
        case R::Ocean: break;
    }
    if (m_polished) {
        // A broad glade field leaves walkable openings inside dense groves;
        // stature follows the same grove field, so edges build up gradually.
        const double glade = noise(x,z,52,0xa148df02ull);
        const bool forest = s.region==R::Woodland || s.region==R::ConiferHighland;
        if (forest) chance *= smooth(.22,.46,glade);
        chance *= smooth(0,16,s.snowLine-7-s.column.height);
        result.stature = density>.66 ? 2 : density>.30 ? 1 : 0;
        if (forest && glade<.46) result.stature=0;
    }
    if(unit(h)>=chance) {result.kind=T::None;return result;}
    // Four bounded queries only after candidate rejection. This also prevents
    // trees balancing on cliffs and rooted vegetation crossing a water edge.
    int low=s.column.height,high=low;
    for(const auto &offset : {std::pair<int,int>{-2,0},{2,0},{0,-2},{0,2}}) {
        const auto px=static_cast<std::int64_t>(x)+offset.first,pz=static_cast<std::int64_t>(z)+offset.second;
        if(px<std::numeric_limits<int>::min() || px>std::numeric_limits<int>::max() ||
           pz<std::numeric_limits<int>::min() || pz>std::numeric_limits<int>::max()) {result.kind=T::None;return result;}
        const int height=sampleWaterColumn(static_cast<int>(px),static_cast<int>(pz)).height;
        low=std::min(low,height);high=std::max(high,height);
    }
    if(low<64 || high-low>(result.kind==T::Spruce?5:3))result.kind=T::None;
    if (m_polished && unit(mix(h ^ 0x83a09bf1ull)) <
        .75*smooth(0,result.kind==T::Spruce?5:3,high-low)) result.kind=T::None;
    result.randomSeed=static_cast<int>((h^(h>>32))&0x7fffffff);
    result.height=s.column.height+1;
    return result;
}

AdventureEcologyPlanner::GroundCover AdventureEcologyPlanner::groundCover(int x, int z,const Sample &s) const noexcept {
    if(s.column.height<64 || s.column.height>=s.snowLine)return BlockId::Air;
    using R=AdventureRegion;using S=TerrainFoundation::Surface;
    const double patch=noise(x,z,24,0xace176d3ull),roll=unit(hash(x,z,0x6da542b9ull));
    const auto surface=s.column.surface;
    if(surface==S::Stone || surface==S::Snow || surface==S::Gravel || surface==S::Clay || surface==S::MossStone)return BlockId::Air;
    if(surface==S::Sand)return (s.region==R::Dunes || s.region==R::Canyon) && roll<.006?BlockId::DeadShrub:BlockId::Air;
    const bool wet=s.region==R::Wetland || (s.shore && surface==S::Silt);
    const bool forest=s.region==R::Woodland || s.region==R::ConiferHighland;
    double chance=wet?.025+.18*patch*patch:forest?.006+.07*patch*patch:.005+.035*patch*patch;
    if (m_polished) {
        // Compact patches with bare lanes instead of evenly distributed stems.
        chance *= smooth(.25,.68,noise(x,z,11,0xdeb47c13ull))*1.7;
        chance *= smooth(0,10,s.snowLine-s.column.height);
    }
    if(roll>=chance)return BlockId::Air;
    if (m_polished) {
        // Neighbour queries only for accepted cover candidates, with the same
        // signed-coordinate bounds as tree roots. Steep ledges stay open.
        int low=s.column.height,high=low;
        for(const auto &offset:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}}) {
            const auto px=static_cast<std::int64_t>(x)+offset.first;
            const auto pz=static_cast<std::int64_t>(z)+offset.second;
            if(px<std::numeric_limits<int>::min() || px>std::numeric_limits<int>::max() ||
               pz<std::numeric_limits<int>::min() || pz>std::numeric_limits<int>::max())return BlockId::Air;
            const int height=sampleWaterColumn(static_cast<int>(px),static_cast<int>(pz)).height;
            low=std::min(low,height);high=std::max(high,height);
        }
        if(high-low>2 || unit(mix(hash(x,z,0x7d12bf4aull)))<.85*smooth(0,3,high-low))return BlockId::Air;
    }
    if(wet)return GroundCover(BlockId::TallGrass,
        !m_polished || s.shore || patch>.60 ? BlockMetadata::TallGrass::Reed : BlockMetadata::TallGrass::Fern);
    if(forest)return GroundCover(BlockId::TallGrass,
        !m_polished || patch>.35 ? BlockMetadata::TallGrass::Fern : BlockMetadata::TallGrass::Mature);
    if(s.region==R::Meadow && patch>.62 && unit(mix(hash(x,z,31)))<.15)return BlockId::Rose;
    return GroundCover(BlockId::TallGrass,BlockMetadata::TallGrass::Mature);
}

bool AdventureEcologyPlanner::supportsPlant(BlockId ground,BlockId plant) noexcept {
    if(plant==BlockId::DeadShrub)return ground==BlockId::Sand || ground==BlockId::Clay || ground==BlockId::Gravel;
    return ground==BlockId::Grass || ground==BlockId::ForestFloor || ground==BlockId::Silt || ground==BlockId::Dirt;
}
