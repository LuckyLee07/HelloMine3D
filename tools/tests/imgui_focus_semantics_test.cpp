#include "imgui.h"
#include <iostream>
struct Result { bool activated, freshClick; };
static Result run(int scenario) {
    ImGui::CreateContext(); auto& io=ImGui::GetIO();
    io.DisplaySize=ImVec2(400,300);io.DeltaTime=1.f/60;io.IniFilename=nullptr;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ImVec2 center;
    auto frame=[&]() {
        ImGui::NewFrame();ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(250,200));
        ImGui::Begin("test");bool pressed=ImGui::Button("action",ImVec2(100,40));
        auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();center=ImVec2((a.x+b.x)/2,(a.y+b.y)/2);
        ImGui::End();ImGui::Render();return pressed;
    };
    frame();io.AddMousePosEvent(center.x,center.y);frame();
    io.AddMouseButtonEvent(0,true);frame();
    if(scenario==2) {
        io.ClearEventsQueue(); io.ClearInputKeys(); io.ClearInputMouse();
    }
    if(scenario!=0) io.AddFocusEvent(false);
    io.AddMouseButtonEvent(0,false);
    if(scenario==2) io.AddFocusEvent(true);
    bool activated=frame();
    io.AddFocusEvent(true);io.AddMousePosEvent(center.x,center.y);frame();
    io.AddMouseButtonEvent(0,true);frame();
    io.AddMouseButtonEvent(0,false);bool fresh=frame();
    ImGui::DestroyContext();return {activated,fresh};
}
int main(){
    Result plain=run(0),lost=run(1),fast=run(2);
    int failures=0;
    auto check=[&](const char* name,bool pass) {
        std::cout<<"[IMGUI_FOCUS_SEMANTICS] "<<(pass?"PASS ":"FAIL ")<<name<<'\n';
        if(!pass) ++failures;
    };
    check("control-release-without-focus-activates",plain.activated);
    check("focus-loss-release-does-not-activate",!lost.activated);
    check("clear-fast-loss-gain-release-does-not-activate",!fast.activated);
    check("normal-click-after-fast-loss-gain-works",fast.freshClick);
    std::cout<<"[IMGUI_FOCUS_SEMANTICS] failures="<<failures<<'\n';
    return failures?1:0;
}
