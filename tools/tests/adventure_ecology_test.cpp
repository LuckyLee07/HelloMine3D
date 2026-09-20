#include "World/Generation/Ecology/AdventureEcologyPlanner.h"
#include <array>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>

int main(int argc,char **argv) {
    if(argc!=2)return 2;
    std::ofstream csv(argv[1]);csv << "seed,x,z,region,biome,height,surface,grove,snowline,tree,cover\n";
    int checks=0,failures=0;std::array<std::size_t,11> surfaces{};std::array<std::size_t,7> trees{};
    std::array<std::size_t,4> grass{};std::size_t samples=0,covered=0;bool height=true,treeSupport=true,deterministic=true;
    const auto check=[&](const char *name,bool pass) {++checks;failures+=!pass;std::cout << "[ECOLOGY_PLAN] " << (pass?"PASS ":"FAIL ") << name << '\n';};
    const auto same=[](const auto &a,const auto &b) {return a.column.height==b.column.height && a.column.surface==b.column.surface && a.column.biome==b.column.biome && a.region==b.region && a.grove==b.grove && a.snowLine==b.snowLine;};
    for(int seed:{0,1,42,424,20260807,20260809,8675309,325322}) {
        AdventureEcologyPlanner plan(seed);AdventureWaterPlanner water(seed);std::array<std::size_t,7> localTrees{};
        for(int z=-2048;z<=2048;z+=28)for(int x=-2048;x<=2048;x+=28) {
            const auto s=plan.sample(x,z);height &= s.column.height==water.sample(x,z).column.height;
            ++samples;++surfaces[static_cast<int>(s.column.surface)];
            const auto cover=plan.groundCover(x,z,s);covered+=cover.id!=BlockId::Air;
            if(cover.id==BlockId::TallGrass)++grass[cover.metadata];
            AdventureEcologyPlanner::Tree tree;
            // Find the unique cell anchor in this seven-square neighbourhood.
            for(int dz=0;dz<7;++dz)for(int dx=0;dx<7;++dx)if(plan.treeAnchor(x+dx,z+dz)) {
                const auto root=plan.sample(x+dx,z+dz);tree=plan.tree(x+dx,z+dz,root);
                if(tree.kind!=AdventureTreeKind::None)treeSupport &= root.column.height>=65 && root.column.height<=root.snowLine-7 && tree.height==root.column.height+1;
            }
            ++trees[static_cast<int>(tree.kind)];++localTrees[static_cast<int>(tree.kind)];
            csv << seed << ',' << x << ',' << z << ',' << static_cast<int>(s.region) << ',' << static_cast<int>(s.column.biome) << ',' << s.column.height << ',' << static_cast<int>(s.column.surface) << ',' << s.grove << ',' << s.snowLine << ',' << static_cast<int>(tree.kind) << ',' << static_cast<int>(cover.id) << '\n';
        }
        check(("seed-"+std::to_string(seed)+"-three-tree-families").c_str(),localTrees[1]>5 && localTrees[2]>5 && localTrees[3]>5 && localTrees[4]>5);
        const auto first=plan.sample(-513,1023);
        for(int i=0;i<100;++i)(void)AdventureEcologyPlanner(seed+3).sample(i*1024,-i*512);
        deterministic &= same(first,plan.sample(-513,1023));
        auto other=std::async(std::launch::async,[seed]{return AdventureEcologyPlanner(seed).sample(-513,1023);});
        deterministic &= same(first,other.get());
        for(int x:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})for(int z:{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}) {
            const auto a=plan.sample(x,z);deterministic &= same(a,AdventureEcologyPlanner(seed).sample(x,z));
            (void)plan.tree(x,z,a);(void)plan.groundCover(x,z,a);
        }
    }
    check("water-and-height-plan-preserved",height);
    check("tree-root-and-variable-treeline",treeSupport);
    check("seed-switch-thread-and-extreme-coordinates",deterministic);
    bool materials=true;for(int i=5;i<=10;++i)materials &= surfaces[i]>100;
    check("all-six-real-ground-materials",materials);
    check("distinct-grass-fern-reed-combinations",grass[1]>100 && grass[2]>100 && grass[3]>100);
    check("open-ground-retained",double(covered)/samples>.005 && double(covered)/samples<.10);
    std::cout << "samples=" << samples << " covered=" << covered << " surfaces=";for(auto n:surfaces)std::cout << n << ',';
    std::cout << " trees=";for(auto n:trees)std::cout << n << ',';std::cout << " grass=";for(auto n:grass)std::cout << n << ',';
    std::cout << "\nchecks=" << checks << " failures=" << failures << '\n';return failures?1:0;
}
