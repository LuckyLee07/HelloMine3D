#pragma once
#include "../Presentation/MinimapNavigation.h"

namespace {
void caseMinimapNavigation()
{
    using namespace MinimapNavigation;
    Memory memory;
    check("NAVIGATION/no-discoveries-by-default", memory.landmarks().empty());
    memory.observe({-17,64,-33}, Kind::Container);
    memory.observe({-17,64,-33}, Kind::Workbench);
    check("NAVIGATION/observations-deduplicate-with-negative-coordinates",
        memory.landmarks().size() == 1 && memory.landmarks()[0].position.x == -17 &&
        memory.landmarks()[0].kind == Kind::Workbench);
    for (int i=0;i<40;++i) memory.observe({i,64,2}, Kind::Waystone);
    check("NAVIGATION/memory-is-bounded-and-keeps-recent-anchors",
        memory.landmarks().size() == Memory::Capacity &&
        memory.landmarks().front().position.x == 8 && memory.landmarks().back().position.x == 39);
    memory.clear();
    check("NAVIGATION/world-switch-clears-memory", memory.landmarks().empty());
    check("NAVIGATION/zoom-preserves-fixed-sample-count",
        cellStep(64) == 1 && cellStep(128) == 2 && cellStep(256) == 4);
    MarkerLayout markers;
    check("NAVIGATION/markers-stay-inside-map-and-clear-player",
        !markers.reserve(0,0,80) && !markers.reserve(78,0,80) &&
        markers.reserve(20,0,80) && !markers.reserve(24,4,80));
    int accepted=1;
    for (int y=-60;y<=60;y+=20)
        for (int x=-60;x<=60;x+=20) accepted += markers.reserve(x,y,80);
    check("NAVIGATION/eight-visible-marker-limit", accepted == 8);
}
}
