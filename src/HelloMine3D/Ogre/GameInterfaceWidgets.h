#pragma once

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cfloat>

// Draw-only furniture shared by the in-world HUD and inventory panels.
// Input, item ownership and transfer rules remain with the callers.
namespace GameInterfaceWidgets
{
enum class Glyph { None, Play, Pause, Settings, Journal, Map, Exit, Home, Pin };

inline void glyph(ImDrawList* draw, Glyph kind, ImVec2 at, float size,
                  ImU32 colour = IM_COL32(222, 182, 110, 255))
{
    if (kind == Glyph::Journal || kind == Glyph::Map)
    {
        static const char* book[] = {".bbbbbbbbb..","baappppppbb.","baappppppdb.","baapddddpdb.",
            "baappppppdb.","baapddddpdb.","baappppppdb.","baapddddpdb.","baappppppdb.","baappppppbb.",".bbbbbbbbb..","..ddddddddd."};
        static const char* mapArt[] = {"....bbb.....",".bbbpwpbbb..","bpggpwpggpb.","bpggpwpggpb.",
            "bpggpwwpgpb.","bpggppwpppb.","bpgpppwpgpb.","bppggpwggpb.","bpggppwggpb.","bpggppwpppb.",".bbbpppbbb..","....bbb....."};
        const float p = size / 12.f;
        for (int y=0;y<12;++y) for (int x=0;x<12;++x) {
            const char c = (kind == Glyph::Journal ? book : mapArt)[y][x];
            if (c=='.') continue;
            const ImU32 ink = c=='b' ? IM_COL32(139,111,65,255) : c=='d' ? IM_COL32(133,127,99,255) :
                c=='a' ? IM_COL32(220,174,94,255) : c=='g' ? IM_COL32(129,151,97,255) :
                c=='w' ? IM_COL32(89,154,168,255) : IM_COL32(227,213,163,255);
            draw->AddRectFilled(ImVec2(at.x+x*p,at.y+y*p),ImVec2(at.x+(x+1)*p,at.y+(y+1)*p),ink);
        }
        return;
    }
    // Small integer-grid silhouettes share the pixel scale of item icons.
    const unsigned short* rows = nullptr;
    static const unsigned short play[] = {0x200,0x300,0x380,0x3c0,0x3e0,0x3f0,0x3e0,0x3c0,0x380,0x300,0x200,0};
    static const unsigned short pause[] = {0,0x318,0x318,0x318,0x318,0x318,0x318,0x318,0x318,0x318,0x318,0};
    static const unsigned short gear[] = {0x0f0,0x6f6,0x7fe,0x3fc,0x718,0xf0f,0xf0f,0x718,0x3fc,0x7fe,0x6f6,0x0f0};
    static const unsigned short journal[] = {0x3fc,0x606,0x602,0x6fa,0x602,0x6fa,0x602,0x6fa,0x602,0x606,0x3fc,0};
    static const unsigned short map[] = {0x030,0x1c8,0xe06,0x842,0x842,0x842,0x842,0x842,0x842,0xc07,0x138,0x0c0};
    static const unsigned short leave[] = {0x07c,0x064,0x064,0x264,0x364,0xff4,0x364,0x264,0x064,0x064,0x07c,0};
    static const unsigned short home[] = {0,0x060,0x0f0,0x1f8,0x3fc,0x7fe,0x318,0x318,0x378,0x378,0x3f8,0};
    static const unsigned short pin[] = {0x0f0,0x1f8,0x318,0x318,0x318,0x1f8,0x0f0,0x060,0x060,0x060,0,0};
    switch (kind) {
        case Glyph::Play: rows = play; break;
        case Glyph::Pause: rows = pause; break;
        case Glyph::Settings: rows = gear; break;
        case Glyph::Journal: rows = journal; break;
        case Glyph::Map: rows = map; break;
        case Glyph::Exit: rows = leave; break;
        case Glyph::Home: rows = home; break;
        case Glyph::Pin: rows = pin; break;
        default: return;
    }
    if (kind == Glyph::Settings) colour = IM_COL32(204,213,202,255);
    if (kind == Glyph::Exit) colour = IM_COL32(203,145,112,255);
    const float pixel = size / 12.f;
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x)
            if (rows[y] & (1u << (11 - x)))
                draw->AddRectFilled(ImVec2(at.x + x * pixel, at.y + y * pixel),
                    ImVec2(at.x + (x + 1) * pixel, at.y + (y + 1) * pixel), colour);
}

inline void surface(ImDrawList* draw, ImVec2 lo, ImVec2 hi,
                    bool accent = false, float scale = 1.f)
{
    draw->AddRectFilled(ImVec2(lo.x, lo.y + 3.f * scale),
        ImVec2(hi.x, hi.y + 3.f * scale), IM_COL32(5, 12, 16, 90), 3.f * scale);
    draw->AddRectFilled(lo, hi, IM_COL32(25, 45, 52, 239), 3.f * scale);
    draw->AddRect(lo, hi, IM_COL32(105, 132, 131, 200), 3.f * scale);
    draw->AddLine(ImVec2(lo.x+3.f,lo.y+2.f),ImVec2(hi.x-3.f,lo.y+2.f),IM_COL32(202,215,196,34));
    if (accent)
        draw->AddLine(ImVec2(lo.x + 1.f, lo.y + 5.f * scale),
            ImVec2(lo.x + 1.f, hi.y - 5.f * scale), IM_COL32(222, 182, 110, 255), 2.f * scale);
}

inline void progress(ImDrawList* draw, ImVec2 lo, ImVec2 hi,
                     float ratio, ImU32 colour = IM_COL32(222, 182, 110, 255))
{
    draw->AddRectFilled(lo, hi, IM_COL32(48, 67, 73, 245), 3.f);
    ratio = std::clamp(ratio, 0.f, 1.f);
    if (ratio > 0.f)
        draw->AddRectFilled(lo, ImVec2(lo.x + (hi.x - lo.x) * ratio, hi.y), colour, 3.f);
}

inline void playerArrow(ImDrawList* draw, ImVec2 tip, ImVec2 left, ImVec2 right)
{
    draw->AddTriangleFilled(tip, left, right, IM_COL32(247, 235, 195, 255));
    draw->AddTriangle(tip, left, right, IM_COL32(27, 43, 47, 255), 1.5f);
}

inline void compass(ImDrawList* draw, ImVec2 at, float dx, float dy, float scale, const char* label)
{
    const float length = std::max(.001f,std::hypot(dx,dy)); dx/=length; dy/=length;
    draw->AddCircleFilled(at,19.f*scale,IM_COL32(20,35,41,210),32);
    draw->AddCircle(at,19.f*scale,IM_COL32(166,184,174,180),32);
    const auto point = [&](float f,float r) { return ImVec2(at.x+(dx*f-dy*r)*scale,at.y+(dy*f+dx*r)*scale); };
    playerArrow(draw,point(13,0),point(-7,-5),point(-7,5));
    const float font=ImGui::GetFontSize()*.75f;
    const float w=ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,0,label).x;
    draw->AddText(ImGui::GetFont(),font,ImVec2(at.x-w*.5f,at.y-22.f*scale-font),IM_COL32(242,229,195,255),label);
}

inline void mapRuler(ImDrawList* draw, ImVec2 bottomRight, float pixelsPerMetre, float scale)
{
    const float target=72.f*scale/std::max(.001f,pixelsPerMetre);
    const float unit=std::pow(10.f,std::floor(std::log10(std::max(.01f,target))));
    const float count=target/unit;
    const float metres=(count>=5.f ? 5.f : count>=2.f ? 2.f : 1.f)*unit;
    const float width=metres*pixelsPerMetre;
    const ImVec2 a(bottomRight.x-width,bottomRight.y-22.f*scale), b(bottomRight.x,a.y);
    const ImU32 ink=IM_COL32(231,228,204,240);
    draw->AddLine(a,b,ink,1.5f);
    for (float x : {a.x,b.x}) draw->AddLine(ImVec2(x,a.y-3.f*scale),ImVec2(x,a.y+3.f*scale),ink,1.5f);
    char label[32]; std::snprintf(label,sizeof(label),"%g m",double(metres));
    const float font=ImGui::GetFontSize()*.7f;
    const float textWidth=ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,0,label).x;
    draw->AddText(ImGui::GetFont(),font,ImVec2((a.x+b.x-textWidth)*.5f,a.y+5.f*scale),ink,label);
}

// Scoped to the adventure overlays so unrelated settings and machine layouts
// retain their existing sizing and semantics.
struct OverlayStyle
{
    explicit OverlayStyle(float scale)
    {
        // Detail pages use a larger reading size than the unobtrusive HUD.
        // The existing locale font and user scale remain authoritative.
        ImGui::PushFont(nullptr, 24.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f * scale, 14.f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f * scale, 8.f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f * scale, 7.f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.f * scale);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(17, 32, 38, 65));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(89, 116, 119, 175));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(30, 49, 56, 245));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(56, 73, 72, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(69, 77, 66, 255));
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(113, 98, 58, 100));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(105, 103, 76, 100));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(128, 108, 65, 130));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(222, 182, 110, 255));
    }
    ~OverlayStyle() { ImGui::PopStyleColor(9); ImGui::PopStyleVar(6); ImGui::PopFont(); }
    OverlayStyle(const OverlayStyle&) = delete;
    OverlayStyle& operator=(const OverlayStyle&) = delete;
};

// The generated corner craft stays at native proportions; only the straight
// edges and quiet cloth center stretch. No image contains gameplay text.
inline void texturedPanel(ImDrawList* draw, ImTextureID texture,
                          ImVec2 lo, ImVec2 hi, float corner,
                          ImU32 tint = IM_COL32_WHITE)
{
    corner = std::min(corner, std::min(hi.x - lo.x, hi.y - lo.y) * .5f);
    const float x[] = {lo.x, lo.x + corner, hi.x - corner, hi.x};
    const float y[] = {lo.y, lo.y + corner, hi.y - corner, hi.y};
    // The opaque panel atlas has unused background outside its footprint.
    // Sample only the verified dark rectangle; glyphs use genuine alpha.
    const float uv[] = {.064f, .15f, .85f, .936f};
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            draw->AddImage(ImTextureRef(texture), ImVec2(x[col], y[row]),
                ImVec2(x[col + 1], y[row + 1]), ImVec2(uv[col], uv[row]),
                ImVec2(uv[col + 1], uv[row + 1]), tint);
}

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
