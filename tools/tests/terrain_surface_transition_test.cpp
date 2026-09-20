#include "World/Generation/Terrain/TerrainFoundation.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>

int main(int argc, char **argv)
{
    if (argc != 2) { std::cerr << "usage: terrain_surface_transition_test output.csv\n"; return 2; }
    using Surface = TerrainFoundation::Surface;
    std::ofstream out(argv[1]); out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "seed,x,z,height,biome,old_surface,surface\n";
    constexpr std::array<int,8> seeds{{0,1,42,424,20260807,20260809,8675309,325322}};
    std::uint64_t count=0,changed=0,connected=0,plateau=0,stone=0,oldDirt=0,retainedDirt=0;
    std::uint64_t legacyHash=1469598103934665603ull;
    bool identity=true,protectedGround=true,deterministic=true;
    int checks=0,failures=0;
    auto check=[&](const char *name,bool pass) {
        ++checks; failures+=!pass;
        std::cout << "[TRANSITION] " << (pass?"PASS ":"FAIL ") << name << '\n';
    };
    for (int seed:seeds) {
        TerrainFoundation f(seed),other(seed);
        auto sample=[&](int x,int z,bool neighbours) {
            const auto old=f.sampleV13(x,z),now=f.sampleV15(x,z);
            ++count;
            identity &= old.height==now.height && old.biome==now.biome;
            for (int value:{old.height,static_cast<int>(old.biome),static_cast<int>(old.surface)}) {
                legacyHash^=static_cast<std::uint32_t>(value);legacyHash*=1099511628211ull;
            }
            if(old.height<=65 || old.biome==TerrainBiome::Wetland ||
               (old.biome==TerrainBiome::Mountain && old.height>=100))
                protectedGround &= old.surface==now.surface;
            if(old.biome==TerrainBiome::RockPlateau) {
                ++plateau;stone+=now.surface==Surface::Stone;
                if(old.surface==Surface::Dirt) {++oldDirt;retainedDirt+=now.surface==Surface::Dirt;}
            }
            if(neighbours && old.surface!=now.surface) {
                ++changed;bool joined=false;
                for(const auto &d:std::array<std::array<int,2>,4>{{{{-1,0}},{{1,0}},{{0,-1}},{{0,1}}}})
                    joined |= f.sampleV15(x+d[0],z+d[1]).surface==now.surface;
                connected+=joined;
            }
            out << seed << ',' << x << ',' << z << ',' << now.height << ',' << static_cast<int>(now.biome)
                << ',' << static_cast<int>(old.surface) << ',' << static_cast<int>(now.surface) << '\n';
        };
        for(int z=-2048;z<=2048;z+=32) for(int x=-2048;x<=2048;x+=32) sample(x,z,true);
        for(int z=-64;z<=64;++z) for(int x=-64;x<=64;++x) sample(x,z,true);
        for(int boundary:{-161,-160,-159,-17,-16,-15,-1,0,1,15,16,17})
            for(int other=-128;other<=128;++other) {sample(boundary,other,true);sample(other,boundary,true);}
        for(int x:{std::numeric_limits<int>::min(),-17,-1,0,16,std::numeric_limits<int>::max()})
            for(int z:{std::numeric_limits<int>::min(),-161,-1,0,160,std::numeric_limits<int>::max()}) {
                const auto a=f.sampleV15(x,z),b=other.sampleV15(x,z),old=f.sampleV13(x,z);
                deterministic &= a.height==b.height && a.biome==b.biome && a.surface==b.surface;
                identity &= a.height==old.height && a.biome==old.biome;
            }
    }
    // Dense original capture regions include both contour soil and sand/rock margins.
    for(const auto &region:std::array<std::array<int,3>,3>{{{{0,-1024,608}},{{1,-1248,-1536}},{{42,64,480}}}}) {
        TerrainFoundation f(region[0]);int localChanged=0,localDirt=0,localRetained=0;
        for(int z=region[2]-128;z<region[2]+128;++z) for(int x=region[1]-128;x<region[1]+128;++x) {
            const auto old=f.sampleV13(x,z),now=f.sampleV15(x,z);
            localChanged+=old.surface!=now.surface;
            if(old.biome==TerrainBiome::RockPlateau && old.surface==Surface::Dirt) {
                ++localDirt;localRetained+=now.surface==Surface::Dirt;
            }
        }
        std::cout << "region_seed=" << region[0] << " changed=" << localChanged << " old_dirt=" << localDirt
            << " retained_dirt=" << localRetained << '\n';
        check("observed-rock-region-has-coherent-surface-change",localChanged>256 && localDirt>0 && localRetained<double(localDirt)*.70);
    }
    check("height-and-biome-identity-including-signed-extremes",identity);
    check("near-water-wetland-and-high-mountain-ground-unchanged",protectedGround);
    check("independent-signed-world-determinism",deterministic);
    check("surface-change-is-exercised",changed>1000);
    check("changed-columns-have-connected-material",changed>0 && double(connected)/changed>=.90);
    check("rock-plateau-retains-majority-bare-rock",plateau>0 && double(stone)/plateau>.50);
    check("contour-wide-soil-coverage-reduced",oldDirt>0 && double(retainedDirt)/oldDirt<.70);
    std::cout << "samples=" << count << " changed=" << changed << " connected=" << connected
        << " rock=" << stone << '/' << plateau << " old_dirt=" << oldDirt << " retained_dirt=" << retainedDirt
        << " legacy_hash=" << legacyHash << " checks=" << checks << " failures=" << failures << '\n';
    return failures?1:0;
}
