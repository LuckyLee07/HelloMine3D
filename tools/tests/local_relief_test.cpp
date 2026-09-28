#include "World/Generation/Ecology/AdventureEcologyPlanner.h"
#include <fstream>
#include <future>
#include <iostream>
#include <chrono>
#include <map>

int main(int argc,char **argv) {
    if(argc!=2)return 2;
    std::ofstream out(argv[1]);out << "seed,x,z,old_height,height,biome,old_surface,surface\n";
    int checks=0,failures=0;
    auto check=[&](const std::string &name,bool pass){++checks;failures+=!pass;std::cout<<"[LOCAL25] "<<(pass?"PASS ":"FAIL ")<<name<<'\n';};
    auto same=[](const auto &a,const auto &b){return a.height==b.height && a.biome==b.biome && a.surface==b.surface;};
    const auto start=std::chrono::steady_clock::now();
    for(int seed:{42,20260807,239701883}) {
        AdventureEcologyPlanner old(seed,24),now(seed,25);AdventureWaterPlanner water(seed);
        bool bound=true,wet=true,identity=true,deterministic=true;int maxStep=0,lowSteps=0,steps=0,changed=0,up=0,down=0,material=0;
        int changedJoined=0,materialJoined=0;std::map<int,int> levels;
        auto sample=[&](int x,int z,bool write){
            const auto a=old.sample(x,z).column,b=now.sample(x,z).column;
            const auto w=water.sample(x,z);
            const int delta=b.height-a.height;
            bound &= std::abs(delta)<=6 && b.height>=1 && b.height<=176;
            identity &= a.biome==b.biome && b.height==now.sampleWaterColumn(x,z).height;
            if(a.height<=68 || std::max(w.riverInfluence,w.lakeInfluence)>=.65)
                wet &= delta==0;
            bool joined=false,matJoined=false;
            for(const auto d:{std::pair<int,int>{1,0},{-1,0},{0,1},{0,-1}}) {
                const auto n=now.sample(x+d.first,z+d.second).column;
                const auto o=old.sampleWaterColumn(x+d.first,z+d.second);
                const int step=std::abs(n.height-b.height);maxStep=std::max(maxStep,step);++steps;lowSteps+=step<=2;
                joined |= n.height!=o.height;matJoined |= n.surface==b.surface;
            }
            if(delta) {++changed;changedJoined+=joined;up+=delta>0;down+=delta<0;}
            if(a.surface!=b.surface){++material;materialJoined+=matJoined;}
            ++levels[delta];
            if(write)out<<seed<<','<<x<<','<<z<<','<<a.height<<','<<b.height<<','<<int(b.biome)<<','<<int(a.surface)<<','<<int(b.surface)<<'\n';
        };
        for(int z=-2048;z<=2048;z+=16)for(int x=-2048;x<=2048;x+=16)sample(x,z,true);
        for(int z=-96;z<=96;++z)for(int x=-96;x<=96;++x)sample(x,z,true);
        for(int edge:{-65,-64,-63,-17,-16,-15,-1,0,1,15,16,17,63,64,65})
            for(int o=-96;o<=96;++o){sample(edge,o,false);sample(o,edge,false);}
        for(int x:{std::numeric_limits<int>::min(),-17,0,64,std::numeric_limits<int>::max()})
            for(int z:{std::numeric_limits<int>::min(),-65,0,16,std::numeric_limits<int>::max()}) {
                const auto first=now.sample(x,z).column;
                for(int n=0;n<160;++n)(void)AdventureEcologyPlanner(seed+1,25).sample(n*129,-n*73);
                deterministic &= same(first,now.sample(x,z).column);
                auto thread=std::async(std::launch::async,[=]{return AdventureEcologyPlanner(seed,25).sample(x,z).column;});
                deterministic &= same(first,thread.get());
            }
        const auto label=std::to_string(seed)+"/";
        check(label+"bounded-relief-and-height",bound);
        check(label+"water-and-shore-preserved",wet);
        check(label+"shape-ecology-and-biome-agree",identity);
        check(label+"signed-extreme-cache-thread-determinism",deterministic);
        check(label+"neighbour-step-at-most-eight",maxStep<=8);
        check(label+"at-least-95-percent-small-steps",double(lowSteps)/steps>=.95);
        check(label+"raised-and-lowered-route-shapes",up>100 && down>100);
        check(label+"local-shapes-connected",changed>0 && double(changedJoined)/changed>=.95);
        check(label+"slope-materials-coherent",material>100 && double(materialJoined)/material>=.90);
        std::cout<<"[LOCAL25_COUNTS] seed="<<seed<<" changed="<<changed<<" up="<<up<<" down="<<down<<" materials="<<material
                 <<" max_step="<<maxStep<<" small_step_ratio="<<double(lowSteps)/steps<<" joined="<<double(changedJoined)/changed
                 <<" material_joined="<<double(materialJoined)/material<<" deltas=";
        for(auto entry:levels)std::cout<<entry.first<<':'<<entry.second<<',';std::cout<<'\n';
    }
    std::cout<<"checks="<<checks<<" failures="<<failures<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<'\n';
    return failures?1:0;
}
