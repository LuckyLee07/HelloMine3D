#include "World/Generation/Terrain/AdventureWaterPlanner.h"
#include <future>
#include <iostream>
#include <limits>

int main() {
    int checks=0,failures=0;
    const auto check=[&](const std::string &name,bool ok){++checks;failures+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
    for(int seed:{42,20260807,239701883}) {
        AdventureWaterPlanner water(seed,26);int samples=0,reverse=0,slow=0,turns=0;float maximum=0,step=0;
        bool bounded=true,stable=true;
        for(int cz=-3;cz<=3;++cz)for(int cx=-3;cx<=3;++cx) {
            const auto path=water.channelPath(cx,cz);if(!path.valid)continue;
            for(int part=1;part<7;++part)for(int tick=0;tick<16;++tick) {
                const auto a=path.points[part],b=path.points[part+1];const double t=double(tick)/16;
                const int x=int(std::lround(a[0]+(b[0]-a[0])*t)),z=int(std::lround(a[1]+(b[1]-a[1])*t));
                if(water.sample(x,z).column.biome!=TerrainBiome::River)continue;
                const auto flow=water.surfaceFlow(x,z);const double length=std::hypot(b[0]-a[0],b[1]-a[1]);
                const double alignment=(flow[0]*(b[0]-a[0])+flow[1]*(b[1]-a[1]))/length;
                const float speed=std::hypot(flow[0],flow[1]);++samples;reverse+=alignment<0;slow+=speed<.1;
                maximum=std::max(maximum,speed);bounded &= std::isfinite(speed) && speed<=.60001f;
                const auto next=water.surfaceFlow(x+1,z);step=std::max(step,std::hypot(next[0]-flow[0],next[1]-flow[1]));
                turns+=std::abs(flow[0]-.8f*speed)>.2f;
                if(alignment<0 && reverse<4)std::cout<<"reversed "<<seed<<' '<<x<<' '<<z<<' '<<alignment<<'\n';
            }
        }
        for(int x:{std::numeric_limits<int>::min(),-513,-512,-17,-16,-1,0,1,16,512,std::numeric_limits<int>::max()}) {
            const auto a=water.surfaceFlow(x,x);for(int i=0;i<100;++i)(void)AdventureWaterPlanner(seed+1).surfaceFlow(i*1024,-i*512);
            stable &= a==water.surfaceFlow(x,x);
            auto thread=std::async(std::launch::async,[=]{return AdventureWaterPlanner(seed).surfaceFlow(x,x);});stable &= a==thread.get();
            bounded &= std::isfinite(a[0]) && std::isfinite(a[1]) && std::hypot(a[0],a[1])<=.60001f;
        }
        const auto tag=std::to_string(seed)+"/";
        check(tag+"downstream-on-real-river-centres",samples>200 && double(reverse)/samples<.005);
        check(tag+"river-motion-present",slow<samples/20 && turns>100);
        check(tag+"bounded-signed-extremes",bounded);
        check(tag+"neighbour-continuity",step<.30f);
        check(tag+"seed-thread-and-cache-stability",stable);
        std::cout<<"seed="<<seed<<" samples="<<samples<<" reverse="<<reverse<<" slow="<<slow<<" turns="<<turns<<" max="<<maximum<<" step="<<step<<'\n';
    }
    std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
