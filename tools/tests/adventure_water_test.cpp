#include "World/Generation/Terrain/AdventureWaterPlanner.h"
#include <array>
#include <cmath>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>

int main(int argc,char **argv)
{
    if(argc!=2)return 2;
    std::ofstream csv(argv[1]);csv.exceptions(std::ios::failbit|std::ios::badbit);
    csv << "seed,x,z,old_height,height,biome,river,lake,dx,dz\n";
    constexpr std::array<int,8> seeds{{0,1,42,424,20260807,20260809,8675309,325322}};
    int checks=0,failures=0,maxStep=0;
    std::size_t total=0,gentle=0,changed=0,rivers=0,lakes=0,dryValleys=0,flowing=0,terminals=0;
    bool safety=true,oldOcean=true,downhill=true,deterministic=true;
    const auto same=[](const auto &a,const auto &b) {
        return a.column.height==b.column.height && a.column.biome==b.column.biome &&
            a.column.surface==b.column.surface && a.riverInfluence==b.riverInfluence && a.lakeInfluence==b.lakeInfluence;
    };
    const auto check=[&](const char *name,bool pass){++checks;failures+=!pass;std::cout << "[WATER_PLAN] " << (pass?"PASS ":"FAIL ") << name << '\n';};
    for(int seed:seeds) {
        AdventureWaterPlanner water(seed);AdventureTerrainPlanner base(seed);
        for(int z=-2048;z<=2048;z+=32)for(int x=-2048;x<=2048;x+=32) {
            const auto old=base.sample(x,z).column;const auto sample=water.sample(x,z);const auto c=sample.column;
            const int dx=water.sample(x+1,z).column.height-c.height,dz=water.sample(x,z+1).column.height-c.height;
            ++total;gentle+=std::abs(dx)<=2 && std::abs(dz)<=2;maxStep=std::max(maxStep,std::max(std::abs(dx),std::abs(dz)));
            changed+=c.height!=old.height;rivers+=c.biome==TerrainBiome::River;lakes+=c.biome==TerrainBiome::Lake;
            dryValleys+=c.height!=old.height && c.height>=64 && sample.riverInfluence>0;
            safety &= c.height>=1 && c.height<=176 && c.height<=old.height && old.height-c.height<=42;
            if(old.biome==TerrainBiome::Ocean)oldOcean &= old.height==c.height && old.biome==c.biome && old.surface==c.surface;
            if(c.biome==TerrainBiome::River || c.biome==TerrainBiome::Lake)safety &= c.height<64 && c.height>=58;
            csv << seed << ',' << x << ',' << z << ',' << old.height << ',' << c.height << ',' << static_cast<int>(c.biome)
                << ',' << sample.riverInfluence << ',' << sample.lakeInfluence << ',' << dx << ',' << dz << '\n';
        }
        for(int z=-4;z<=4;++z)for(int x=-4;x<=4;++x) {
            const auto n=water.planNode(x,z);
            if(n.drains) {++flowing;downhill &= n.downstreamPotential<n.potential && n.downstreamHeight<=n.height;}
            else ++terminals;
        }
        for(int edge:{-513,-512,-511,-161,-160,-159,-17,-16,-15,-1,0,1,15,16,17,511,512,513})
            for(int other=-256;other<=256;++other)for(int axis=0;axis<2;++axis) {
                const int x=axis?other:edge,z=axis?edge:other;
                const int h=water.sample(x,z).column.height;
                maxStep=std::max({maxStep,std::abs(water.sample(x+1,z).column.height-h),std::abs(water.sample(x,z+1).column.height-h)});
            }
        const auto before=water.sample(-512,-1);
        for(int i=0;i<256;++i) {(void)water.sample(i*512,-i*512);(void)AdventureWaterPlanner(seed+11).sample(i*512,-i*512);}
        deterministic &= same(before,water.sample(-512,-1));
        auto future=std::async(std::launch::async,[seed] {return AdventureWaterPlanner(seed).sample(-512,-1);});
        deterministic &= same(before,future.get());
        for(int x:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})for(int z:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
            const auto a=water.sample(x,z),b=AdventureWaterPlanner(seed).sample(x,z);
            deterministic &= same(a,b);safety &= a.column.height>=1 && a.column.height<=176 && std::isfinite(a.riverInfluence);
        }
    }
    check("bounded-real-water-height-and-cut",safety);
    check("existing-ocean-unchanged",oldOcean);
    check("directed-graph-descends-with-terminals",downhill && flowing>400 && terminals>0);
    check("cache-eviction-seed-switch-thread-and-extremes",deterministic);
    check("river-lake-and-dry-valley-present",rivers>100 && lakes>10 && dryValleys>100);
    check("localized-water-shaping",double(changed)/total>.01 && double(changed)/total<.25);
    check("continuous-tile-and-chunk-boundaries",maxStep<=8 && double(gentle)/total>=.95);
    std::cout << "samples=" << total << " changed=" << changed << " river=" << rivers << " lake=" << lakes
        << " dry_valley=" << dryValleys << " flows=" << flowing << " terminals=" << terminals
        << " max_step=" << maxStep << " gentle=" << double(gentle)/total << '\n';
    std::cout << "checks=" << checks << " failures=" << failures << '\n';return failures?1:0;
}
