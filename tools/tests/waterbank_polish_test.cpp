#include "World/Generation/Ecology/AdventureEcologyPlanner.h"
#include <fstream>
#include <future>
#include <iostream>
#include <limits>

int main(int argc,char **argv) {
    if(argc!=2)return 2;std::ofstream out(argv[1]);out<<"seed,x,z,old_height,height,biome,deposit,old_surface,surface\n";
    int checks=0,failures=0;auto check=[&](const std::string &name,bool ok){++checks;failures+=!ok;std::cout<<"[BANK26] "<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
    auto same=[](const auto &a,const auto &b){return a.column.height==b.column.height && a.column.surface==b.column.surface && a.column.biome==b.column.biome;};
    for(int seed:{42,20260807,239701883}) {
        AdventureWaterPlanner oldWater(seed,25),water(seed,26);AdventureEcologyPlanner old(seed,25),now(seed,26);
        bool bounds=true,ocean=true,determinism=true,queries=true,downhill=true;int changed=0,wet=0,bars=0,barWalk=0,depth1=0,deep=0,steps=0,gentle=0,maxStep=0,river=0,lake=0;
        auto sample=[&](int x,int z,bool save) {
            const auto a=old.sample(x,z).column,b=now.sample(x,z).column,w=water.sample(x,z).column;
            const auto ws=water.sample(x,z);const auto parent=ws.base.column;
            bounds &= b.height>=1 && b.height<=176 && w.height<=parent.height && parent.height-w.height<=42;
            if(w.biome==TerrainBiome::River || w.biome==TerrainBiome::Lake)bounds &= w.height>=58 && w.height<64;
            if(parent.biome==TerrainBiome::Ocean)ocean &= a.height==b.height && a.surface==b.surface && a.biome==b.biome;
            queries &= now.sampleWaterColumn(x,z).height==b.height;
            changed+=a.height!=b.height;wet+=b.height<64;river+=b.biome==TerrainBiome::River;lake+=b.biome==TerrainBiome::Lake;
            depth1+=b.height==63;deep+=b.height<=60 && (b.biome==TerrainBiome::Lake || b.biome==TerrainBiome::River);
            bool walk=true;
            for(auto d:{std::pair<int,int>{1,0},{-1,0},{0,1},{0,-1}}) {
                const int step=std::abs(now.sampleWaterColumn(x+d.first,z+d.second).height-b.height);
                ++steps;gentle+=step<=2;maxStep=std::max(maxStep,step);walk &= step<=1;
            }
            if(ws.bankDeposit>.25 && b.height>=65 && b.height<=72) {++bars;barWalk+=walk;}
            if(save)out<<seed<<','<<x<<','<<z<<','<<a.height<<','<<b.height<<','<<int(b.biome)<<','<<ws.bankDeposit<<','<<int(a.surface)<<','<<int(b.surface)<<'\n';
        };
        for(int z=-2048;z<=2048;z+=16)for(int x=-2048;x<=2048;x+=16)sample(x,z,true);
        for(int z=-64;z<=64;++z)for(int x=-64;x<=64;++x)sample(x,z,true);
        for(int edge:{-513,-512,-511,-17,-16,-15,-1,0,1,15,16,17,511,512,513})
            for(int other=-128;other<=128;++other){sample(edge,other,false);sample(other,edge,false);}
        for(int z=-4;z<=4;++z)for(int x=-4;x<=4;++x) {
            const auto a=oldWater.planNode(x,z),b=water.planNode(x,z);
            downhill &= a.x==b.x && a.z==b.z && a.height==b.height && a.downstreamX==b.downstreamX && a.downstreamZ==b.downstreamZ &&
                (!b.drains || b.downstreamPotential<b.potential);
        }
        bool wetCore=true,commonPath=true;int cores=0,wider=0,narrower=0,asymmetric=0;
        for(int z=-3;z<=3;++z)for(int x=-3;x<=3;++x) {
            const auto path=water.channelPath(x,z),oldPath=oldWater.channelPath(x,z);
            commonPath &= path.valid==oldPath.valid && path.points==oldPath.points;
            if(!path.valid)continue;
            for(int part=0;part<8;++part) {
              const auto a=path.points[part],b=path.points[part+1];
              const int divisions=static_cast<int>(std::ceil(std::hypot(b[0]-a[0],b[1]-a[1])*2));
              bool previousWet=false;int previousX=0,previousZ=0;
              for(int step=0;step<=divisions;++step) {
                const double t=double(step)/divisions;
                const double px=a[0]+(b[0]-a[0])*t,pz=a[1]+(b[1]-a[1])*t;
                const int ix=static_cast<int>(std::lround(px)),iz=static_cast<int>(std::lround(pz));
                if(oldWater.sample(ix,iz).column.height>=64){previousWet=false;continue;}
                ++cores;wetCore &= water.sample(ix,iz).column.height<64;
                if(previousWet && ix!=previousX && iz!=previousZ) {
                    const bool oldBridge=oldWater.sample(ix,previousZ).column.height<64 || oldWater.sample(previousX,iz).column.height<64;
                    if(oldBridge)wetCore &= water.sample(ix,previousZ).column.height<64 || water.sample(previousX,iz).column.height<64;
                }
                previousWet=true;previousX=ix;previousZ=iz;
                if(step!=divisions/2)continue;
                const double length=std::hypot(b[0]-a[0],b[1]-a[1]),nx=-(b[1]-a[1])/length,nz=(b[0]-a[0])/length;
                int oldWidth=0,newWidth=0,leftChange=0,rightChange=0;
                for(int d=-40;d<=40;++d) {
                    const int sx=static_cast<int>(std::lround(px+nx*d)),sz=static_cast<int>(std::lround(pz+nz*d));
                    const int oldH=oldWater.sample(sx,sz).column.height,newH=water.sample(sx,sz).column.height;
                    oldWidth+=oldH<64;newWidth+=newH<64;
                    if(d<0)leftChange+=newH-oldH;else if(d>0)rightChange+=newH-oldH;
                }
                wider+=newWidth>oldWidth;narrower+=newWidth<oldWidth;asymmetric+=std::abs(leftChange-rightChange)>3;
              }
            }
        }
        for(int x:{std::numeric_limits<int>::min(),-513,-16,0,16,512,std::numeric_limits<int>::max()})
            for(int z:{std::numeric_limits<int>::min(),-512,-1,0,17,513,std::numeric_limits<int>::max()}) {
                const auto a=now.sample(x,z);
                for(int i=0;i<160;++i){(void)AdventureEcologyPlanner(seed+7,26).sample(i*513,-i*617);(void)old.sample(i*129,-i*83);}
                determinism &= same(a,now.sample(x,z));
                auto thread=std::async(std::launch::async,[=]{return AdventureEcologyPlanner(seed,26).sample(x,z);});determinism &= same(a,thread.get());
            }
        const auto tag=std::to_string(seed)+"/";
        check(tag+"water-height-and-cut-bounded",bounds);check(tag+"ocean-preserved",ocean);
        check(tag+"surface-and-shape-agree",queries);check(tag+"common-descending-nodes-preserved",downhill);
        check(tag+"negative-boundary-step-at-most-eight",maxStep<=8);check(tag+"at-least-95-percent-gentle",double(gentle)/steps>=.95);
        check(tag+"seed-version-thread-extremes-and-cache-eviction",determinism);
        check(tag+"real-river-lake-depth-range",river>20 && lake>20 && depth1>20 && deep>20);
        check(tag+"bounded-bank-landing-exercised",bars>10 && barWalk>5 && changed>100);
        check(tag+"existing-wet-channel-core-connected",commonPath && wetCore && cores>100);
        check(tag+"both-wider-and-narrower-cross-sections",wider>5 && narrower>5);
        check(tag+"unequal-bank-cross-section-change",asymmetric>20);
        std::cout<<"[BANK26_SECTIONS] cores="<<cores<<" wider="<<wider<<" narrower="<<narrower<<" asymmetric="<<asymmetric<<'\n';
        std::cout<<"[BANK26_COUNTS] seed="<<seed<<" changed="<<changed<<" river="<<river<<" lake="<<lake<<" shallow="<<depth1<<" deep="<<deep<<" bars="<<bars<<" landings="<<barWalk<<" max_step="<<maxStep<<" gentle="<<double(gentle)/steps<<'\n';
    }
    std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
