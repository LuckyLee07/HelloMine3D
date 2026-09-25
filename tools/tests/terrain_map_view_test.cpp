#include "../../src/HelloMine3D/Presentation/TerrainMapView.h"
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
struct Sample { bool known = false; int height = 0; };
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
    std::cout<<"PASS "<<checks<<" checks; 65x65 build+sort mean "<<ms<<" ms; faces "<<faces.size()<<'\n';
}
