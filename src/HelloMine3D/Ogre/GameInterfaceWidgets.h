#pragma once

#include <imgui.h>
#include <algorithm>

// Draw-only furniture shared by the in-world HUD and inventory panels.
// Input, item ownership and transfer rules remain with the callers.
namespace GameInterfaceWidgets
{
inline void slotFrame(ImDrawList* draw, ImVec2 lo, ImVec2 hi,
                      bool selected, bool hovered, bool pressed, float scale)
{
    const ImU32 rim = selected ? IM_COL32(224, 188, 119, 255) :
        hovered ? IM_COL32(149, 171, 168, 240) : IM_COL32(74, 92, 98, 255);
    const ImU32 top = pressed ? IM_COL32(26, 35, 37, 255) :
        selected ? IM_COL32(60, 64, 55, 255) : IM_COL32(35, 48, 53, 255);
    const ImU32 bottom = selected ? IM_COL32(35, 43, 39, 255) : IM_COL32(19, 29, 34, 255);
    const float inset = 2.f * scale;
    draw->AddRectFilled(ImVec2(lo.x, lo.y + 3.f * scale),
        ImVec2(hi.x, hi.y + 3.f * scale), IM_COL32(5, 11, 15, 165), 2.f);
    draw->AddRectFilledMultiColor(lo, hi, top, top, bottom, bottom);
    draw->AddRect(lo, hi, rim, 2.f, 0, selected ? 1.5f * scale : 1.f);
    draw->AddLine(ImVec2(lo.x + inset, lo.y + inset),
        ImVec2(hi.x - inset, lo.y + inset), IM_COL32(196, 215, 211, hovered ? 60 : 25));
    draw->AddLine(ImVec2(lo.x + inset, hi.y - inset),
        ImVec2(hi.x - inset, hi.y - inset), IM_COL32(0, 0, 0, 115));
    if (selected)
    {
        const float arm = std::min(9.f * scale, (hi.x - lo.x) * .2f);
        draw->AddLine(lo, ImVec2(lo.x + arm, lo.y), rim, 3.f * scale);
        draw->AddLine(lo, ImVec2(lo.x, lo.y + arm), rim, 3.f * scale);
        draw->AddLine(hi, ImVec2(hi.x - arm, hi.y), rim, 3.f * scale);
        draw->AddLine(hi, ImVec2(hi.x, hi.y - arm), rim, 3.f * scale);
    }
}

inline void panelFrame(float scale)
{
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 lo = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 hi(lo.x + size.x, lo.y + size.y);
    draw->AddRectFilled(lo, hi, IM_COL32(15, 24, 29, 248), 3.f);
    draw->AddRect(lo, hi, IM_COL32(102, 113, 105, 240), 3.f);
    draw->AddRect(ImVec2(lo.x + 4.f, lo.y + 4.f),
        ImVec2(hi.x - 4.f, hi.y - 4.f), IM_COL32(66, 81, 83, 140), 1.f);
    draw->AddLine(ImVec2(lo.x + 16.f * scale, lo.y),
        ImVec2(lo.x + std::min(size.x * .22f, 130.f * scale), lo.y),
        IM_COL32(224, 188, 119, 255), 3.f * scale);
}
}
