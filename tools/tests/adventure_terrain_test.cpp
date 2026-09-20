#include "World/Generation/Terrain/AdventureTerrainPlanner.h"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <utility>

int main(int argc, char **argv)
{
    if (argc != 2) { std::cerr << "usage: adventure_terrain_test samples.csv\n"; return 2; }
    std::ofstream out(argv[1]);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << "seed,x,z,height,biome,surface,region,core,dx,dz\n";
    constexpr std::array<int, 8> seeds{{0,1,42,424,20260807,20260809,8675309,325322}};
    constexpr std::array<int, 12> edges{{-161,-160,-159,-17,-16,-15,-1,0,1,15,16,17}};
    std::array<std::size_t, 9> counts{};
    std::array<bool, 7> coreSquare{};
    std::array<double, 7> coreHeight{}, coreHeightSquared{}, coreCount{};
    std::size_t wetCore=0, wetWater=0, wetDry=0;
    int checks=0, failures=0, slopeMax=0, minimum=176, maximum=0, minimumKinds=7;
    std::size_t gentle=0, total=0;
    bool finite=true, order=true, pure=true;
    auto check=[&](const char *name,bool pass) {
        ++checks; failures+=!pass;
        std::cout << "[ADVENTURE] " << (pass?"PASS ":"FAIL ") << name << '\n';
    };
    for (int seed : seeds) {
        AdventureTerrainPlanner planner(seed), independent(seed);
        std::array<std::set<std::pair<int,int>>, 7> cores;
        std::array<bool, 7> kinds{};
        auto sample=[&](int x,int z,bool macro) {
            const auto a=planner.sample(x,z);
            const int h=a.column.height;
            const int dx=planner.sample(x+1,z).column.height-h;
            const int dz=planner.sample(x,z+1).column.height-h;
            const auto region=static_cast<std::size_t>(a.region);
            double sum=0;
            for(double w:a.weights) {finite &= std::isfinite(w) && w>=0 && w<=1; sum+=w;}
            finite &= std::abs(sum-1)<1e-10 && std::isfinite(a.temperature) && std::isfinite(a.moisture);
            minimum=std::min(minimum,h); maximum=std::max(maximum,h);
            slopeMax=std::max(slopeMax,std::max(std::abs(dx),std::abs(dz)));
            if (!macro) return;
            ++total; ++counts.at(region);
            gentle+=(std::abs(dx)<=2 && std::abs(dz)<=2);
            const bool core=region<7 && a.weights[region]>=.75 && a.land>=.96 &&
                (h>=64 || a.region==AdventureRegion::Wetland);
            if(region<7) {kinds[region]=true; if(core)cores[region].insert({x/32,z/32});}
            if(core) {
                coreHeight[region]+=h;coreHeightSquared[region]+=h*h;++coreCount[region];
                if(a.region==AdventureRegion::Wetland) {++wetCore;wetWater+=h<64;wetDry+=h>=65;}
            }
            out << seed << ',' << x << ',' << z << ',' << h << ',' << static_cast<int>(a.column.biome)
                << ',' << static_cast<int>(a.column.surface) << ',' << region << ',' << core << ',' << dx << ',' << dz << '\n';
        };
        for(int z=-2048;z<=2048;z+=32) for(int x=-2048;x<=2048;x+=32) sample(x,z,true);
        for(int z=-64;z<=64;++z) for(int x=-64;x<=64;++x) sample(x,z,false);
        for(int edge:edges) for(int other=-512;other<=512;++other) {sample(edge,other,false);sample(other,edge,false);}
        int kindCount=0;for(bool kind:kinds)kindCount+=kind;
        minimumKinds=std::min(minimumKinds,kindCount);
        for(std::size_t region=0;region<cores.size();++region) for(auto p:cores[region]) {
            bool square=true;
            for(int dz=0;dz<=4;++dz)for(int dx=0;dx<=4;++dx)square &= cores[region].count({p.first+dx,p.second+dz})!=0;
            coreSquare[region]=coreSquare[region]||square;
        }
        for(int x:{std::numeric_limits<int>::min(),-385,-1,0,384,std::numeric_limits<int>::max()})
            for(int z:{std::numeric_limits<int>::min(),-769,-1,0,768,std::numeric_limits<int>::max()}) {
                const auto a=planner.sample(x,z);
                (void)planner.sample(z,x);
                const auto b=planner.sample(x,z),c=independent.sample(x,z);
                order &= a.column.height==b.column.height && a.region==b.region && a.weights==b.weights;
                pure &= a.column.height==c.column.height && a.column.biome==c.column.biome &&
                    a.column.surface==c.column.surface && a.region==c.region && a.weights==c.weights &&
                    a.column.height>=1 && a.column.height<176;
            }
    }
    check("finite-normalized-region-blending",finite);
    check("independent-signed-extreme-coordinate-samples",pure);
    check("query-order-independent",order);
    check("height-safe-without-cap",minimum>=1 && maximum<176);
    check("bounded-neighbour-steps",slopeMax<=8);
    check("macro-slopes-at-least-95-percent-le2",double(gentle)/total>=.95);
    check("each-seed-at-least-five-inland-regions",minimumKinds>=5);
    bool coverage=true,cores=true;
    for(std::size_t i=0;i<7;++i) {coverage &= double(counts[i])/total>=.01; cores &= coreSquare[i];}
    check("seven-inland-regions-have-visible-coverage",coverage);
    check("each-inland-region-has-128-metre-core",cores);
    std::array<double,7> mean{},deviation{};
    for(std::size_t i=0;i<mean.size();++i)if(coreCount[i]>0) {
        mean[i]=coreHeight[i]/coreCount[i];
        deviation[i]=std::sqrt(std::max(0.0,coreHeightSquared[i]/coreCount[i]-mean[i]*mean[i]));
    }
    check("regional-altitude-identity",mean[2]>mean[0]+20 && mean[6]>mean[2]+10 && mean[4]>mean[0]+20);
    check("regional-relief-not-flat-relabelled-ground",deviation[2]>6 && deviation[3]>3 && deviation[4]>7 && deviation[6]>9);
    check("wetland-core-mixes-water-and-dry-ground",wetCore>0 && double(wetWater)/wetCore>.1 && double(wetDry)/wetCore>.1);
    std::cout << "height=" << minimum << ".." << maximum << " max_step=" << slopeMax
              << " gentle=" << double(gentle)/total << " minimum_seed_regions=" << minimumKinds << '\n';
    for(std::size_t i=0;i<counts.size();++i)std::cout << "region=" << i << " share=" << double(counts[i])/total
        << " core=" << (i<7 && coreSquare[i]) << '\n';
    for(std::size_t i=0;i<mean.size();++i)std::cout << "core_region=" << i << " mean_height=" << mean[i] << " deviation=" << deviation[i] << '\n';
    std::cout << "checks=" << checks << " failures=" << failures << '\n';
    return failures?1:0;
}
