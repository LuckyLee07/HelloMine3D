#pragma once

#include "AdventureWaterPlanner.h"
#include <algorithm>
#include <cmath>
#include <limits>

// v25 local landforms. Patches follow the measured parent slope, so a spur,
// shallow pass and resting shelf have spatial meaning instead of extra noise.
// Fixed pure-value caches never contain resident chunks or saved block state.
class LocalTerrainPlanner {
  public:
    struct Sample { int height=0; double slope=0, deposit=0; };
    explicit LocalTerrainPlanner(int seed) noexcept : m_seed(seed),m_water(seed) {}

    Sample sample(int x,int z,const AdventureWaterPlanner::Sample &water) const noexcept
    {
        const auto &base=water.column;
        Sample result{base.height,0,0};
        if(base.height<=68 || base.biome==TerrainBiome::Ocean)
            return result;
        const auto cx=static_cast<std::int64_t>(std::floor(double(x)/64));
        const auto cz=static_cast<std::int64_t>(std::floor(double(z)/64));
        double offset=0,weight=0,slope=0,deposit=0;
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx) {
            const auto &p=patch(cx+dx,cz+dz);
            const double px=double(x)-p.x,pz=double(z)-p.z;
            const double distance=std::hypot(px,pz);
            const double w=1-smooth(0,76,distance);
            weight+=w;slope+=w*p.slope;deposit+=w*p.deposit;
            if(!p.active)continue;
            // u follows downhill; v follows the contour. Compact support makes
            // patch addition continuous even when the queried grid cell changes.
            const double u=px*p.downX+pz*p.downZ;
            const double v=-px*p.downZ+pz*p.downX;
            double shape=0,change=0;
            if(p.kind<2) {
                shape=(1-smooth(0,29,std::abs(u)))*(1-smooth(0,13,std::abs(v)));
                change=p.kind==0?5.0:-4.5;
            } else {
                // A broad contour shelf blends into both the upper slope and
                // its downhill toe. The central landing remains truly level.
                shape=(1-smooth(5,18,std::abs(u)))*(1-smooth(9,26,std::abs(v)));
                change=std::clamp(p.height-double(base.height),-6.0,6.0);
            }
            offset+=shape*change*p.strength;
        }
        if(weight>0) {result.slope=slope/weight;result.deposit=deposit/weight;}
        const auto &weights=water.base.weights;
        const double rugged=weights[2]+weights[4]+weights[6];
        const double terrainStrength=(.35+.65*rugged)*(1-weights[3])*(1-weights[5]);
        const double waterFade=1-smooth(.02,.65,std::max(water.riverInfluence,water.lakeInfluence));
        const double influence=smooth(68,78,base.height)*waterFade;
        const double delta=std::clamp(offset*terrainStrength*influence,-6.0,6.0);
        result.height=std::clamp(static_cast<int>(std::lround(base.height+delta)),1,176);
        // Lowered shallow slots collect a thin gravel/soil apron; raised spurs
        // expose their parent rock. Near-water materials remain ecology-owned.
        result.deposit+=std::max(0.0,-delta)*.18;
        return result;
    }

  private:
    struct Patch {
        int seed=0;std::int64_t cellX=0,cellZ=0;
        double x=0,z=0,height=0,downX=1,downZ=0,slope=0,deposit=0,strength=0;
        int kind=0;bool active=false,valid=false;
    };
    static double smooth(double a,double b,double x) noexcept {
        const double t=std::clamp((x-a)/(b-a),0.0,1.0);return t*t*(3-2*t);
    }
    static std::uint64_t mix(std::uint64_t v) noexcept {
        v^=v>>30;v*=0xbf58476d1ce4e5b9ull;v^=v>>27;v*=0x94d049bb133111ebull;return v^(v>>31);
    }
    static int bounded(double x) noexcept {
        return static_cast<int>(std::clamp(x,double(std::numeric_limits<int>::min()),double(std::numeric_limits<int>::max())));
    }
    const Patch &patch(std::int64_t x,std::int64_t z) const noexcept {
        thread_local std::array<Patch,128> cache{};
        static_assert(sizeof(cache)<=20*1024,"Local terrain patch cache stays bounded");
        const auto h=mix(static_cast<std::uint64_t>(m_seed)^mix(static_cast<std::uint64_t>(x))^
            mix(static_cast<std::uint64_t>(z)+0xd481a2753ull));
        auto &p=cache[h%cache.size()];
        if(p.valid && p.seed==m_seed && p.cellX==x && p.cellZ==z)return p;
        p={};p.seed=m_seed;p.cellX=x;p.cellZ=z;
        p.x=double(x)*64+32+double((h>>8)%13)-6;
        p.z=double(z)*64+32+double((h>>16)%13)-6;
        const auto centre=m_water.sample(bounded(p.x),bounded(p.z));
        p.height=centre.column.height;
        const double west=m_water.sample(bounded(p.x-12),bounded(p.z)).column.height;
        const double east=m_water.sample(bounded(p.x+12),bounded(p.z)).column.height;
        const double north=m_water.sample(bounded(p.x),bounded(p.z-12)).column.height;
        const double south=m_water.sample(bounded(p.x),bounded(p.z+12)).column.height;
        const double gx=(east-west)/24,gz=(south-north)/24;
        p.slope=std::hypot(gx,gz);
        if(p.slope>.0001) {p.downX=-gx/p.slope;p.downZ=-gz/p.slope;}
        p.deposit=std::max(0.0,(west+east+north+south)*.25-p.height)*.15;
        p.kind=static_cast<int>((h>>24)%3);
        p.strength=smooth(.05,.32,p.slope);
        p.active=p.height>70 && centre.column.biome!=TerrainBiome::Ocean;
        p.valid=true;return p;
    }
    int m_seed=0;
    AdventureWaterPlanner m_water;
};
