#include "../../src/HelloMine3D/Presentation/TerrainMapView.h"
#include "../../src/HelloMine3D/Presentation/MapSurfaceRegion.h"
#include "../../src/HelloMine3D/Presentation/SurfaceMapHistory.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int checks=0;
void check(bool value, const char* label) {
    ++checks;
    if (!value) { std::cerr << "FAIL " << label << '\n'; std::exit(1); }
}
struct Sample { bool known = false; int height = 0; int material = 0; };
}
int main() {
    using namespace TerrainMapView;
    View view; view.yaw=std::numeric_limits<float>::infinity(); view.pitch=-5;
    view.zoom=100; view.panX=std::numeric_limits<float>::quiet_NaN(); view.panY=20;
    view.constrain();
    check(std::isfinite(view.yaw) && view.pitch==.35f && view.zoom==4 && view.panX==0 && view.panY==1,"finite bounded view");
    view={};
    Gesture gesture;
    gesture.begin(view,10,10,0); gesture.update(view,110,60,640,480,false);
    check(gesture.dragged && gesture.button == -1 && std::abs(view.yaw - .15f)<.001f && std::abs(view.pitch-1.15f)<.001f,"release-only drag rotates");
    const View rotated=view;
    gesture.begin(view,10,10,0); gesture.update(view,12,11,640,480,false);
    check(!gesture.dragged && view.yaw==rotated.yaw,"click jitter does not rotate");
    gesture.begin(view,10,10,1); gesture.update(view,74,58,640,480,true);
    check(gesture.dragged && std::abs(view.panX-.1f)<.001f && std::abs(view.panY-.1f)<.001f,"right drag pans proportionally");
    gesture.update(view,138,106,640,480,false);
    check(std::abs(view.panX-.2f)<.001f && std::abs(view.panY-.2f)<.001f,"pan uses start rather than accumulated deltas");
    gesture.update(view,999,999,640,480,false);
    check(std::abs(view.panX-.2f)<.001f,"released drag ignores later movement");
    view={};
    const Projection project(view);
    check(project(0,10,0).y < project(0,0,0).y,"real height projects upward");
    check(project(0,10,0).depth > project(0,0,0).depth,"height depth is nearer");
    view.yaw=0; const Projection north(view);
    check(north(0,0,-10).y<0 && north(10,0,0).x>0,"north and east orientation");
    std::array<Sample,9> cells{}; std::vector<Face> faces;
    build(cells,3,2,10,view,faces); check(faces.empty(),"all unknown has no invented terrain");
    cells[4]={true,10}; build(cells,3,2,10,view,faces);
    check(faces.size()==1 && faces[0].top && faces[0].cell==4,"isolated observation has only a surface");
    check(pick(faces,0,0)==4,"center picks known surface");
    check(pick(faces,100,100)==-1,"unknown stays unpickable");
    check(pick(faces,std::numeric_limits<float>::quiet_NaN(),0)==-1,"nonfinite pick rejected");
    Face degenerate; check(!contains(degenerate,0,0),"zero area cannot steal input");
    for (auto& c:cells)c={true,10};
    build(cells,3,2,10,view,faces); check(faces.size()==9,"flat region has no internal walls");
    cells[4].height=18;
    build(cells,3,2,10,view,faces);
    check(faces.size()==10,"one exposed front wall for north view");
    check(std::is_sorted(faces.begin(),faces.end(),[](const auto&a,const auto&b){return a.depth<b.depth;}),"far to near sorting");
    const auto elevated=north(0,8,0);
    check(pick(faces,elevated.x,elevated.y)==4,"frontmost elevated surface wins");
    cells[7].known=false;
    build(cells,3,2,10,view,faces);check(faces.size()==8,"unknown neighbour never invents cliff");
    view.yaw=.6f;
    for (auto& c:cells)c={true,10}; cells[4].height=18;
    build(cells,3,2,10,view,faces);check(faces.size()==11,"oblique view includes two exposed walls");
    build(cells,3,0,10,view,faces);check(faces.empty(),"zero step rejected");
    build(cells,65,2,10,view,faces);check(faces.empty(),"dimension mismatch rejected");
    build(cells,3,2,std::numeric_limits<float>::infinity(),view,faces);check(faces.empty(),"nonfinite origin rejected");
    std::array<Sample,65*65> full{};
    for(std::size_t i=0;i<full.size();++i)full[i]={i%7!=0,50+int(i%41)};
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<60;++i) { view.yaw=i*.11f; build(full,65,4,70,view,faces); }
    const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/60;
    check(faces.size()<=full.size()*3,"65-square geometry budget");
    for(const auto& face:faces) {
        if(!full[face.cell].known) { std::cerr<<"unknown face\n";return 1; }
        for(const auto& p:face.points) if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.depth))return 1;
    }
    check(true,"all emitted geometry is finite and observed");
    // A tall, off-centre region must fit without clipping at different views.
    for (float yaw : {0.f,.65f,1.8f,3.1f}) {
        view.yaw=yaw; build(full,65,4,70,view,faces);
        Bounds bounds;
        for (const auto& face:faces) for (const auto& p:face.points) bounds.include(p);
        const float scale=bounds.fittedScale(740,430,90);
        for (const auto& face:faces) for (const auto& p:face.points) {
            const float x=370+(p.x-(bounds.minX+bounds.maxX)*.5f)*scale;
            const float y=215+(p.y-(bounds.minY+bounds.maxY)*.5f)*scale;
            if (x<0 || x>740 || y<0 || y>430) return 1;
        }
    }
    check(true,"fitted relief retains complete observed geometry across rotations");
    Bounds empty;
    check(std::isfinite(empty.fittedScale(1,1,4)),"empty view fit stays finite");
    OverviewScale overview;
    const float initial=overview.cellPixels(900,400);
    overview.change(1.25f);
    check(overview.step==4 && overview.cellPixels(900,400)>initial,
        "zoom in enlarges immutable 4m cells");
    overview.change(.8f);
    check(std::abs(overview.zoom-1.f)<.001f && overview.step==4,"zoom round trip");
    overview.change(.8f);
    check(overview.step==8 && std::abs(overview.cellPixels(900,400)/overview.step-initial/4*.8f)<.001f,
        "coarser query preserves continuous displayed world scale");
    for(int i=0;i<100;++i) overview.change(.5f);
    check(overview.step==64 && overview.zoom==1,"zoom out has bounded query coverage");
    for(int i=0;i<100;++i) overview.change(2.f);
    check(overview.step==4 && overview.zoom==16,"zoom in has bounded geometry and magnification");
    const auto saved=overview; overview.change(std::numeric_limits<float>::quiet_NaN());
    check(overview.step==saved.step && overview.zoom==saved.zoom,"invalid zoom ignored");
    for (const auto& viewport : {std::pair<float,float>{810,479},{380,220},{1100,700}}) {
        overview.fitRegion(viewport.first,viewport.second,290);
        const float pixelsPerMetre=overview.cellPixels(viewport.first,viewport.second)/overview.step;
        check(std::abs(290*pixelsPerMetre/std::min(viewport.first,viewport.second)-.82f)<.001f,
            "initial and recenter fit fills viewport without coarse-scale drift");
    }
    MapSurfaceRegion<Sample,257,2> fine;
    fine.configure(-360,-504,8);
    check(fine.step==2 && fine.count==145 && fine.nextBatch().size()==195,
        "flat map preserves minimap detail across render scope under same query budget");
    auto firstFine=fine.nextBatch();
    fine.accept(firstFine,std::vector<Sample>(firstFine.size(),{true,81,3}));
    check(fine.cellAt(-360,-504)==fine.count/2*fine.count+fine.count/2 &&
        fine.cellAt(-359.01,-504)==fine.cellAt(-360,-504) &&
        fine.cellAt(-358.99,-504)!=fine.cellAt(-360,-504),
        "fine hover follows exact surface footprint including negative coordinates");
    check(fine.cellAt(10000,10000)==-1 && fine.cellAt(-360,-510)==-1 &&
        fine.cellAt(std::numeric_limits<double>::infinity(),0)==-1,
        "fine picking cannot invent unknown or out-of-range ground");
    fine.configure(0,0,64);
    check(fine.count<=257 && fine.step<=16 && fine.cells.size()<=257*257,
        "maximum live flat cache remains bounded independently of zoom");
    MapSurfaceRegion<Sample> region;
    check(region.configure(500,438,8) && region.step==4 && region.count==73,
        "detail scope follows render chunks rather than 130m minimap");
    check(region.centerZ==436 && region.count*region.step>=17*16,
        "render window covered including chunk-centre margin");
    const auto first=region.nextBatch();
    check(first.size()==195 && first.front().x==500 && first.front().z==436,
        "bounded centre-first surface batch");
    const auto revision=region.revision;
    check(!region.accept(first,{}) && region.cursor==0 && region.revision==revision,
        "lock contention cannot publish or skip pending observations");
    std::vector<bool> visited(region.cells.size(),false);
    for (int n=0;n<(int(region.cells.size())+194)/195;++n) {
        const auto batch=region.nextBatch();std::vector<Sample> samples;
        for (const auto& q:batch) {
            samples.push_back({true,70+(q.x/4+q.z/4)%20,0}); visited[q.cell]=true;
        }
        region.accept(batch,samples);
    }
    check(std::all_of(visited.begin(),visited.end(),[](bool v){return v;}),
        "one bounded sweep visits every column across all render chunks");
    check(region.surfaceHeight(500,438).has_value() && !region.surfaceHeight(9000,9000),
        "player marker uses only its observed ground not flying altitude");
    const auto ground=region.surfaceHeight(500,438);
    build(region.cells,region.count,region.step,*ground,view,faces);
    check(faces.size()>=region.cells.size() && faces.size()<=region.cells.size()*3,
        "expanded region geometry remains bounded and includes every known top");
    check(!region.configure(501,439,8),"subcell movement keeps exact sampled coordinates");
    check(region.configure(-1,-1,8) && region.centerX==-4 && region.centerZ==-4 &&
        !region.surfaceHeight(-1,-1),"negative relocation clears old-world-coordinate cache");
    auto batch=region.nextBatch();std::vector<Sample> known(batch.size(),{true,90,0});
    region.accept(batch,known);
    while (region.pendingCount()) {
        auto pendingBatch=region.nextBatch();
        region.accept(pendingBatch,std::vector<Sample>(pendingBatch.size(),{true,90,0}));
    }
    region.cursor=0;
    region.accept(region.nextBatch(),std::vector<Sample>(batch.size()));
    check(!region.surfaceHeight(-1,-1),"eviction removes stale live terrain");
    region.configure(std::numeric_limits<int>::max(),std::numeric_limits<int>::min(),64);
    check(region.count<=129 && region.step<=32 && region.nextBatch().size()<=195,
        "extreme coordinates and render distance remain bounded");
    bool sane=true;
    for(const auto& q:region.nextBatch()) sane &= q.cell>=0 && q.cell<int(region.cells.size());
    check(sane,"overflow boundary cannot wrap samples to opposite world edge");
    region.configure(0,0,15);
    check(region.count==129 && region.step==4,"largest detailed sampling side is fixed");
    for(auto& c:region.cells)c={true,80,0};
    build(region.cells,region.count,region.step,80,view,faces);
    check(faces.size()==129*129,"maximum flat surface has no invented walls");
    // Close/reopen does not configure a new identity; movement preserves exact
    // overlapping observations and prioritises the newly exposed strip.
    MapSurfaceRegion<Sample,257,2> moving;
    moving.configure(0,0,8);
    auto fill=[&](auto& grid) {
        int queries=0;
        while (grid.pendingCount()) {
            const auto next=grid.nextBatch(); std::vector<Sample> samples;
            for (const auto& q:next) samples.push_back({true,100+q.x/2,1});
            queries+=int(next.size());grid.accept(next,samples);
        }
        return queries;
    };
    check(fill(moving)==21025,"initial fine view consumes exactly its bounded surface count");
    const auto stableRevision=moving.revision;
    check(!moving.configure(0,0,8) && moving.pendingCount()==0 && moving.revision==stableRevision,
        "reopening without travel retains all data and pending work");
    moving.configure(2,0,8);
    check(moving.pendingCount()==145 && moving.surfaceHeight(0,0)==100,
        "two metre movement retains 20880 columns and queues only one new edge");
    auto edge=moving.nextBatch();
    check(edge.size()==145 && std::all_of(edge.begin(),edge.end(),[](const auto&q){return q.x==146;}),
        "new edge is serviced before already observed interior");
    moving.accept(edge,{});
    check(moving.pendingCount()==145,"lock busy does not lose edge priority");
    check(fill(moving)==145,"one edge completes in one bounded update instead of a whole sweep");
    moving.cursor=0;
    auto updated=moving.nextBatch();
    moving.accept(updated,std::vector<Sample>(updated.size(),{true,123,2}));
    check(moving.surfaceHeight(2,0)==123,"retained interior still refreshes actual edits after the edge completes");
    moving.cursor=0;moving.accept(moving.nextBatch(),std::vector<Sample>(updated.size(),{true,101,1}));
    moving.configure(4,2,8);
    check(moving.pendingCount()==289 && moving.surfaceHeight(0,0)==101,
        "diagonal travel preserves overlap without duplicating the corner");
    const auto stale=moving.nextBatch();moving.configure(6,2,8);
    const auto pendingBefore=moving.pendingCount();
    check(!moving.accept(stale,std::vector<Sample>(stale.size(),{true,999,2})) &&
        moving.pendingCount()==pendingBefore,"old-centre response cannot write into reused indices");
    fill(moving); moving.configure(6,2,16);
    check(moving.surfaceHeight(0,0)==101 && moving.pendingCount()<moving.cells.size(),
        "resolution change reuses only exact world coordinates");
    moving.configure(100000,-100000,8);
    check(moving.pendingCount()==moving.cells.size() && !moving.surfaceHeight(100000,-100000),
        "disjoint travel never wraps old observations to new land");
    SurfaceMapHistory<Sample,2> history;
    history.observe(-2,-2,{true,80,1}); history.observe(0,0,{true,90,2});
    check(history.at(-2,-2)->surface.height==80 && !history.at(2,0),
        "recent fine history retains only actual observed columns across tile seams");
    history.observe(-2,-2,{});
    check(history.at(-2,-2)->surface.height==80,"unload preserves last-known fine history");
    history.observe(-2,-2,{true,81,3});history.observe(16,0,{true,100,4});
    check(history.tileCount()==2 && history.at(-2,-2)->surface.height==81 && !history.at(0,0),
        "fine history updates edits and evicts least recently observed tile at capacity");
    history.observe(3,0,{true,50,5});
    check(!history.at(3,0) && !history.at(1e100,0),"unaligned and extreme fine samples stay unknown");
    SurfaceMapHistory<Sample> seamHistory;
    seamHistory.observe(14,0,{true,70,1});seamHistory.observe(16,0,{true,80,2});
    int visible=0;bool seamCorrect=false;
    seamHistory.visit(15.5,-.5,16.5,.5,[&](int x,int,const Sample& c,const Sample&w,const Sample&) {
        ++visible;seamCorrect=x==16 && c.height==80 && w.height==70;
    });
    check(visible==1 && seamCorrect,"viewport culling and cross-tile slope preserve detailed edges");
    seamHistory={};check(seamHistory.tileCount()==0 && !seamHistory.at(16,0),"world detach clears fine history");
    SurfaceMapHistory<Sample> boundedHistory;
    for (int tile=0;tile<2200;++tile) for (int z=0;z<16;z+=2) for (int x=0;x<16;x+=2)
        boundedHistory.observe(tile*16+x,z,{true,90,1});
    int boundedCells=0;
    boundedHistory.visit(-1,-1,40000,20,[&](int,int,const Sample&,const Sample&,const Sample&){++boundedCells;});
    check(boundedHistory.tileCount()==2048 && boundedCells==131072 && !boundedHistory.at(0,0),
        "long-distance history caps both tiles and drawable fine samples");
    boundedHistory={};
    for (int z=-256;z<=256;z+=2) for (int x=-256;x<=256;x+=2)
        boundedHistory.observe(x,z,{true,70,1});
    int completeFineView=0;
    boundedHistory.visit(-257,-257,257,257,[&](int,int,const Sample&,const Sample&,const Sample&){++completeFineView;});
    check(completeFineView==257*257,"maximum live fine view cannot evict its own detail history");
    std::cout<<"PASS "<<checks<<" checks; 65x65 build+sort mean "<<ms<<" ms; faces "<<faces.size()<<'\n';
}
