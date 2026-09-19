#include "World/Generation/Terrain/TerrainFoundation.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "usage: terrain_landform_test output.csv\n"; return 2; }
    std::ofstream out(argv[1]);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << "set,seed,x,z,old_height,height,biome,surface,dx,dz\n";
    constexpr std::array<int,8> seeds{{0,1,42,424,20260807,20260809,8675309,325322}};
    constexpr std::array<int,12> boundaries{{-161,-160,-159,-17,-16,-15,-1,0,1,15,16,17}};
    std::array<std::uint64_t,8> biomes{};
    std::uint64_t total=0, dry=0, changed=0, dryDelta=0, gentle=0, capped=0;
    int largestStep=0, minimum=176, maximum=0, minimumKinds=8, checks=0, failures=0;
    auto check=[&](const char* name,bool pass) {
        ++checks; failures+=!pass;
        std::cout << "[LANDFORM] " << (pass?"PASS ":"FAIL ") << name << '\n';
    };
    for (int seed:seeds) {
        TerrainFoundation foundation(seed), independent(seed);
        std::array<bool,8> kinds{};
        auto sample=[&](const char* set,int x,int z,bool macro) {
            const auto old=foundation.sampleV11(x,z), column=foundation.sampleV13(x,z);
            const int dx=foundation.sampleV13(x+1,z).height-column.height;
            const int dz=foundation.sampleV13(x,z+1).height-column.height;
            out << set << ',' << seed << ',' << x << ',' << z << ',' << old.height << ',' << column.height << ','
                << static_cast<int>(column.biome) << ',' << static_cast<int>(column.surface) << ',' << dx << ',' << dz << '\n';
            largestStep=std::max({largestStep,std::abs(dx),std::abs(dz)});
            minimum=std::min(minimum,column.height); maximum=std::max(maximum,column.height);
            if (!macro) return;
            ++total; ++biomes.at(static_cast<std::size_t>(column.biome));kinds.at(static_cast<std::size_t>(column.biome))=true;
            capped+=column.height==176;gentle+=(std::abs(dx)<=2)+(std::abs(dz)<=2);
            if (old.height>=64) { ++dry; const int delta=std::abs(column.height-old.height);changed+=delta>=2;dryDelta+=delta; }
        };
        for (int z=-2048;z<=2048;z+=32) for(int x=-2048;x<=2048;x+=32) sample("macro",x,z,true);
        for (int z=-64;z<=64;++z) for(int x=-64;x<=64;++x) sample("local",x,z,false);
        for (int boundary:boundaries) for(int other=-512;other<=512;++other) {
            sample("axis_x",boundary,other,false);sample("axis_z",other,boundary,false);
        }
        minimumKinds=std::min(minimumKinds,static_cast<int>(std::count(kinds.begin(),kinds.end(),true)));
        bool deterministic=true;
        for (int x:{std::numeric_limits<int>::min(),-17,-1,0,16,std::numeric_limits<int>::max()})
            for (int z:{std::numeric_limits<int>::min(),-161,-1,0,160,std::numeric_limits<int>::max()}) {
                const auto a=foundation.sampleV13(x,z),b=independent.sampleV13(x,z);
                deterministic &= a.height==b.height && a.biome==b.biome && a.surface==b.surface && a.height>=1 && a.height<=176;
            }
        check("independent-signed-extreme-coordinate-samples",deterministic);
    }
    check("height-safety-and-no-clipped-plateau",minimum>=1 && maximum<=176 && double(capped)/total<=.001);
    check("bounded-neighbour-steps",largestStep<=6);
    check("walkable-macro-slope-distribution",double(gentle)/(total*2)>=.95);
    check("all-eight-biomes-have-visible-coverage",std::all_of(biomes.begin(),biomes.end(),[&](auto count){return double(count)/total>=.0075;}));
    check("each-seed-has-at-least-seven-biomes",minimumKinds>=7);
    check("substantial-dry-terrain-change",double(changed)/dry>=.35 && double(dryDelta)/dry>=3 && double(dryDelta)/dry<=15);
    std::cout << "macro=" << total << " height=" << minimum << ".." << maximum << " slope_max=" << largestStep
        << " slope_le2=" << double(gentle)/(2*total) << " changed_dry=" << double(changed)/dry
        << " mean_dry_delta=" << double(dryDelta)/dry << " minimum_seed_biomes=" << minimumKinds << '\n';
    for(std::size_t i=0;i<biomes.size();++i)std::cout << "biome_" << i << "=" << double(biomes[i])/total << '\n';
    std::cout << "checks=" << checks << " failures=" << failures << '\n';return failures?1:0;
}
