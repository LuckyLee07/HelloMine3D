#include "PauseNotificationCapture.h"
#include "OgreUserInterface.h"
#include "../Config.h"
#include "../Sandbox/GameApplicationFlow.h"
#include <OIS.h>
#include <imgui_internal.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cfloat>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
namespace fs = std::filesystem;
using Fields = std::vector<std::pair<std::string, std::string>>;
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error("Pause notification capture: " + message);
}
std::string q(const std::string& s) {
    std::ostringstream out; out << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}
template<class T> std::string n(T v) {
    require(std::isfinite(static_cast<double>(v)), "nonfinite number");
    std::ostringstream out; out << std::setprecision(10) << +v; return out.str();
}
std::string b(bool v) { return v ? "true" : "false"; }
std::string object(const Fields& values) {
    std::string s = "{";
    for (const auto& value : values) { if (s.size() > 1) s += ','; s += q(value.first) + ':' + value.second; }
    return s + '}';
}
std::string array(const std::vector<std::string>& values) {
    std::string s = "["; for (const auto& value : values) { if (s.size() > 1) s += ','; s += value; } return s + ']';
}
std::string xy(ImVec2 v) { return array({n(v.x), n(v.y)}); }
std::string rect(ImVec2 lo, ImVec2 hi) { return array({n(lo.x), n(lo.y), n(hi.x), n(hi.y)}); }
std::string rect(const ImRect& r) { return rect(r.Min, r.Max); }
bool enabled(const char* key) {
    const char* value = std::getenv(key);
    return value && (std::string(value) == "1" || std::string(value) == "true");
}
const char* phases[] = {"old_A_top", "old_A_scroll", "B_new_first", "B_new_settled",
    "B_manual_scroll", "B_repeat_same_text", "B_repeat_manual_scroll", "caption_submit",
    "caption_refresh", "caption_expiry", "status_expiry", "buttons_idle"};
constexpr std::size_t PhaseCount = sizeof(phases) / sizeof(phases[0]);
constexpr std::size_t FrameBytesLimit = 4u * 1024u * 1024u;
constexpr std::size_t SessionBytesLimit = 64u * 1024u * 1024u;
std::string base(std::size_t frame) {
    std::ostringstream out; out << "frame-" << std::setw(3) << std::setfill('0') << frame; return out.str();
}
}

std::string PauseNotificationCapture::validateEnvironment(const UserSettings& settings)
{
    const char* output = std::getenv("HELLOMINE3D_PAUSE_NOTIFICATIONS_DIR");
    if (!output) return {};
    const char* save = std::getenv("HELLOMINE3D_SAVE_DIR");
    const char* catalogue = std::getenv("HELLOMINE3D_CATALOGUE_DIR");
    require(output[0] && save && save[0] && catalogue && catalogue[0], "explicit nonempty save/catalogue/output required");
    require(enabled("HELLOMINE3D_WINDOW_HIDDEN") && enabled("HELLO_RENDER_CAPTURE"), "hidden render capture required");
    const auto a = fs::weakly_canonical(output), c = fs::weakly_canonical(catalogue), s = fs::weakly_canonical(save);
    require(a != c && a != s && c != s && a.parent_path() == c.parent_path() && a.parent_path() == s.parent_path(), "distinct sibling paths in one new session required");
    require(!fs::exists(a) && !fs::exists(c) && !fs::exists(s), "existing output/save/catalogue refused");
    for (const char* key : {"HELLO_PERF_CAPTURE", "HELLOMINE3D_RC_PERF_PROFILE", "HELLOMINE3D_E2_BATCH_MANIFEST",
        "HELLOMINE3D_MATERIAL_IDENTITY_CAPTURE_DIR", "HELLOMINE3D_CAMERA_DIAGNOSTICS_DIR", "HELLOMINE3D_FERN_WIND_CAPTURE_DIR",
        "HELLOMINE3D_VISUAL_CAMERA_SWEEP", "HELLOMINE3D_VISUAL_CAMERA_PATH", "HELLOMINE3D_PLAYER_MOTION_CAPTURE",
        "HELLOMINE3D_ACTOR_VISUAL_CAPTURE", "HELLOMINE3D_ACTOR_VISUAL_DISTANCE", "HELLOMINE3D_BLOCK_FEEDBACK_CAPTURE",
        "HELLOMINE3D_COMBAT_FIXTURE", "HELLOMINE3D_CONTAINER_FIXTURE", "HELLOMINE3D_CRAFTING_FIXTURE",
        "HELLOMINE3D_CROP_FIXTURE", "HELLOMINE3D_MACHINE_FIXTURE", "HELLOMINE3D_ORE_FIXTURE",
        "HELLOMINE3D_SPAWN_VALIDATION_ACTORS", "HELLOMINE3D_TRANSPARENT_FIXTURE", "HELLOMINE3D_VERTEX_LIGHTING_FIXTURE",
        "HELLOMINE3D_VERTICAL_SLICE_FIXTURE", "HELLOMINE3D_HUD_FIXTURE", "HELLOMINE3D_HUD_PAGE_FIXTURE",
        "HELLOMINE3D_HUD_INSPECT_SLOT", "HELLOMINE3D_V10E_SETTINGS_FIXTURE", "HELLOMINE3D_RESOURCE_PACKS",
        "HELLOMINE3D_TERRAIN_FALLBACK", "HELLOMINE3D_FORCE_LEGACY_TERRAIN", "HELLOMINE3D_V10C_FALLBACK",
        "HELLOMINE3D_V10D_SHADOW_FIXTURE", "HELLOMINE3D_V10D_SHADOW_FALLBACK", "HELLOMINE3D_V10E_POST_FIXTURE",
        "HELLOMINE3D_V10E_POST_FALLBACK", "HELLOMINE3D_CONTROLLED_CRASH", "HELLOMINE3D_EXIT_AFTER_FRAMES"})
        require(std::getenv(key) == nullptr, std::string("conflicting inherited diagnostic: ") + key);
    require(settings.windowX == 640 && settings.windowY == 480 && !settings.isFullscreen &&
        settings.uiScale == 1.25f && (settings.locale == "zh-CN" || settings.locale == "en-US") &&
        settings.audioCaptions && settings.renderDistance == 1 && settings.cameraPerspective == CameraPerspective::FirstPerson &&
        settings.directionalShadowQuality == DirectionalShadowQuality::Off && settings.postProcessingQuality == PostProcessingQuality::Off,
        "640x480 points, scale1.25, zh-CN/en-US, captions, RD1/first and Off shadow/post required");
    return a.string();
}

class PauseNotificationCapture::Impl
{
  public:
    Impl(std::string directory, std::string language) : output(std::move(directory)), locale(std::move(language)) {
        require(fs::create_directory(output), "output must be new with existing session parent");
        const bool chinese = locale == "zh-CN";
        for (int i = 0; i < 16; ++i) {
            if (i) { messageA += '\n'; messageB += '\n'; }
            messageA += i == 0 ? "A_START_OLD_STATUS" : (chinese ? "旧通知完整文字与滚动检查 " : "Old notification wrapped content ") + std::to_string(i);
            messageB += i == 0 ? "B_START_NEW_STATUS" : (chinese ? "新通知完整文字与滚动检查 " : "New notification wrapped content ") + std::to_string(i);
        }
        writeIndex();
    }
    void write(const std::string& file, const void* data, std::size_t bytes) {
        require(bytes <= FrameBytesLimit && bytes <= SessionBytesLimit - written, "bounded raw output exceeded");
        std::ofstream stream(fs::path(output) / file, std::ios::binary);
        require(bool(stream), "cannot open output " + file);
        if (bytes) stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
        stream.close(); require(bool(stream), "cannot write output " + file); written += bytes;
    }
    void textFile(const std::string& file, const std::string& value) { write(file, value.data(), value.size()); }
    void writeIndex() {
        textFile("index.json", object({{"schema", q("hellomine3d-pause-notification-capture-v1")},
            {"locale", q(locale)}, {"normal_input", "false"}, {"status", q(phase == PhaseCount ? "CAPTURED" : "CAPTURING")},
            {"captured_frames", n(phase)}, {"maximum_frames", n(PhaseCount)}, {"status_submit_count", n(statusSubmits)},
            {"caption_submit_count", n(captionSubmits)}, {"frames", array(frames)}, {"events", array(events)},
            {"scope", q("Actual OgreUserInterface pause rail and original ImGui backend draw data; injected public status/caption/OIS wheel in fresh hidden process; no ordinary input or font-raster replay proof.")}}));
    }
    void event(const std::string& type, const std::string& value, int wheel = 0) {
        events.push_back(object({{"tick", n(tick)}, {"phase", q(phases[phase])}, {"type", q(type)},
            {"text", q(value)}, {"wheel_relative", n(wheel)}, {"elapsed_seconds", n(elapsed())}, {"presentation_seconds", n(presentationSeconds)}}));
    }
    double elapsed() const { return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count(); }
    std::string output, locale, messageA, messageB, status, captionCue, captionText, rail = "null", statusText = "null";
    std::vector<std::string> events, frames;
    std::map<std::string, std::string> buttons;
    std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    std::size_t phase = 0, tick = 0, warm = 0, written = 0, statusSubmits = 0, captionSubmits = 0, enterTick = 0, statusSubmitTick = 0, recordedTick = 0;
    bool entered = false, wheelSent = false, pending = false, nativeFocused = false, simulationAllowed = false;
    bool lastRail = false, lastHovered = false, railPresent = false, hovered = false;
    float statusSeconds = 0, captionSeconds = 0, scroll = 0, maximum = 0, lastScroll = 0, wheel = 0;
    ImVec2 hoverPoint{}, display{}, displayPos{}, framebufferScale{};
    std::string packet;
    Fields statusTextFields;
    ImFont* statusFont = nullptr; ImTextureRef statusTexture;
    float statusFontSize = 0; double presentationSeconds = 0; float actualDeltaSeconds = 0;
};

PauseNotificationCapture::PauseNotificationCapture(std::string output, std::string locale)
    : m_impl(std::make_unique<Impl>(std::move(output), std::move(locale))) {}
PauseNotificationCapture::~PauseNotificationCapture() = default;

void PauseNotificationCapture::drive(OgreUserInterface& ui, GameApplicationFlow& flow, bool focused)
{
    auto& s = *m_impl;
    require(s.elapsed() < 45.0 || isComplete(), "45 second phase timeout; real hover/input may be unavailable");
    if (isComplete() || s.pending) return;
    ++s.tick; s.nativeFocused = focused;
    if (flow.state() == GameApplicationState::Playing) flow.pause();
    if (flow.state() != GameApplicationState::Paused) return;
    s.simulationAllowed = flow.acceptsWorldSimulation();
    if (s.warm++ < 2) return; // Process real startup focus events before injecting any event.
    if (!s.entered) {
        s.entered = true; s.wheelSent = false; s.enterTick = s.tick;
        if (s.phase == 0 || s.phase == 2 || s.phase == 5) {
            const auto& text = s.phase == 0 ? s.messageA : s.messageB;
            ui.setStatusMessage(text); s.statusSubmitTick = s.tick; ++s.statusSubmits; s.event("status_submit", text);
        }
        if (s.phase == 7 || s.phase == 8) {
            const auto text = s.locale == "zh-CN" ? "暂停字幕滚动保持检查" : "Pause caption scroll preservation check";
            ui.setAudioCaption("diagnostic.pause.scroll", text); ++s.captionSubmits; s.event("caption_submit", text);
        }
    }
    if (s.phase == 1 || s.phase == 4 || s.phase == 6) {
        if (!s.lastRail) return;
        OIS::MouseState mouse;
        mouse.X.abs = static_cast<int>(std::lround(s.hoverPoint.x));
        mouse.Y.abs = static_cast<int>(std::lround(s.hoverPoint.y));
        mouse.width = static_cast<int>(s.display.x); mouse.height = static_cast<int>(s.display.y);
        if (s.lastHovered && !s.wheelSent) { mouse.Z.rel = -120; s.wheelSent = true; s.event("wheel", "", -120); }
        ui.mouseMoved(OIS::MouseEvent(nullptr, mouse));
    }
}

void PauseNotificationCapture::beginUi(const std::string& status, float remaining,
    const std::string& cue, const std::string& caption, float captionRemaining, float actualDeltaSeconds)
{
    auto& s = *m_impl; require(std::isfinite(actualDeltaSeconds), "invalid presentation delta");
    s.actualDeltaSeconds = actualDeltaSeconds; s.presentationSeconds += std::max(0.f, actualDeltaSeconds);
    s.status = status; s.statusSeconds = remaining;
    s.captionCue = cue; s.captionText = caption; s.captionSeconds = captionRemaining;
    s.buttons.clear(); s.rail = "null"; s.statusText = "null"; s.railPresent = false; s.hovered = false; s.statusFont = nullptr; s.statusTextFields.clear();
    const auto& io = ImGui::GetIO(); s.display = io.DisplaySize; s.framebufferScale = io.DisplayFramebufferScale; s.wheel = io.MouseWheel;
}
void PauseNotificationCapture::observeButton(const char* key)
{
    auto& s = *m_impl;
    if (std::string(key) != "pause.resume" && std::string(key) != "pause.settings" && std::string(key) != "pause.save_main" && std::string(key) != "pause.save_quit") return;
    s.buttons[key] = object({{"rect", rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())},
        {"window_id", n(ImGui::GetCurrentWindow()->ID)}, {"window_focused", b(ImGui::IsWindowFocused())}, {"hovered", b(ImGui::IsItemHovered())}});
}
void PauseNotificationCapture::observeRail()
{
    auto& s = *m_impl; const auto* window = ImGui::GetCurrentWindow();
    s.railPresent = true; s.hovered = ImGui::IsWindowHovered(); s.scroll = ImGui::GetScrollY(); s.maximum = ImGui::GetScrollMaxY();
    s.hoverPoint = window->InnerClipRect.GetCenter();
    s.rail = object({{"name", q(window->Name)}, {"id", n(window->ID)}, {"position", xy(window->Pos)}, {"size", xy(window->Size)},
        {"inner_clip", rect(window->InnerClipRect)}, {"scroll_y", n(s.scroll)}, {"scroll_max_y", n(s.maximum)},
        {"content_size_previous", xy(window->ContentSize)}, {"hovered", b(s.hovered)}, {"focused", b(ImGui::IsWindowFocused())},
        {"draw_list_owner", q(ImGui::GetWindowDrawList()->_OwnerName)}});
}
void PauseNotificationCapture::observeStatusText(const ImDrawList& draw, ImFont& font,
    float fontSize, ImVec2 origin, float wrap, const std::string& text, ImU32 actualColour, int vertexBegin, int indexBegin)
{
    auto& s = *m_impl;
    s.statusFont = &font; s.statusFontSize = fontSize; s.statusTexture = draw._CmdHeader.TexRef;
    s.statusTextFields = {{"text", q(text)}, {"first_explicit_line", q(text.substr(0, text.find('\n')))},
        {"origin", xy(origin)}, {"font_size", n(fontSize)}, {"wrap", n(wrap)}, {"colour", n(actualColour)},
        {"measured_full_size", xy(font.CalcTextSizeA(fontSize, FLT_MAX, wrap, text.c_str()))},
        {"vertex_begin", n(vertexBegin)}, {"vertex_end", n(draw.VtxBuffer.Size)},
        {"index_begin", n(indexBegin)}, {"index_end", n(draw.IdxBuffer.Size)},
        {"draw_list_owner", q(draw._OwnerName ? draw._OwnerName : "")},
        {"add_text_clip", array({n(draw._CmdHeader.ClipRect.x), n(draw._CmdHeader.ClipRect.y), n(draw._CmdHeader.ClipRect.z), n(draw._CmdHeader.ClipRect.w)})}};
    s.statusText = object(s.statusTextFields);

}

void PauseNotificationCapture::afterBackend(const ImDrawData& data, int action)
{
    auto& s = *m_impl; s.lastRail = s.railPresent; s.lastHovered = s.hovered; s.lastScroll = s.scroll;
    if (!s.entered || s.pending || isComplete()) return;
    const bool statusLive = s.statusSeconds > 0 && !s.status.empty();
    bool ready = s.buttons.size() == 4;
    if (s.phase < 10) ready = ready && statusLive && s.railPresent && s.maximum > 0 && s.statusText != "null";
    if (s.phase == 1 || s.phase == 4 || s.phase == 6) ready = ready && s.wheelSent && s.wheel < 0 && s.scroll > 1;
    if (s.phase == 7 || s.phase == 8) ready = ready && s.captionSeconds > 0 && s.captionCue == "diagnostic.pause.scroll";
    if (s.phase == 9) ready = ready && s.captionSeconds <= 0;
    if (s.phase == 10 || s.phase == 11) ready = ready && !statusLive && s.captionSeconds <= 0;
    if (s.phase == 2 || s.phase == 5)
        require(s.tick == s.statusSubmitTick && ready, "first backend after status submission must be captured or fail setup");
    if (s.phase == 3)
        require(s.tick == s.recordedTick + 1 && ready, "B settled must be the immediately following backend");
    if (!ready) return;
    require(s.phase < PhaseCount && data.Valid, "invalid or excessive backend frame");
    if (s.statusFont) {
        // Resolve current font texture/UVs after the real backend has processed
        // dynamic atlas updates; retain the original draw ranges and origin.
        auto* baked = s.statusFont->GetFontBaked(s.statusFontSize);
        std::vector<std::string> glyphs;
        const std::string first = s.status.substr(0, s.status.find('\n'));
        const char* c = first.c_str();
        while (*c) {
            unsigned int codepoint = 0;
            const int length = ImTextCharFromUtf8(&codepoint, c, first.c_str() + first.size());
            require(length > 0, "invalid first-line UTF8"); c += length;
            const auto* glyph = baked->FindGlyph(static_cast<ImWchar>(codepoint));
            require(glyph != nullptr, "first-line glyph unavailable");
            glyphs.push_back(object({{"codepoint", n(codepoint)}, {"actual_codepoint", n(glyph->Codepoint)},
                {"advance", n(glyph->AdvanceX)}, {"bounds", array({n(glyph->X0), n(glyph->Y0), n(glyph->X1), n(glyph->Y1)})},
                {"uv", array({n(glyph->U0), n(glyph->V0), n(glyph->U1), n(glyph->V1)})},
                {"visible", b(glyph->Visible)}, {"coloured", b(glyph->Colored)}}));
        }
        s.statusTextFields.push_back({"baked_size", n(baked->Size)});
        s.statusTextFields.push_back({"glyphs", array(glyphs)});
        s.statusTextFields.push_back({"font_texture_id", q(std::to_string(static_cast<std::uint64_t>(s.statusTexture.GetTexID())))});
        s.statusText = object(s.statusTextFields);
    }

    std::size_t bytes = 0; std::vector<std::string> lists;
    for (int i = 0; i < data.CmdListsCount; ++i) {
        const auto& list = *data.CmdLists[i];
        const auto vb = std::size_t(list.VtxBuffer.Size) * sizeof(ImDrawVert), ib = std::size_t(list.IdxBuffer.Size) * sizeof(ImDrawIdx);
        require(vb <= FrameBytesLimit - bytes && ib <= FrameBytesLimit - bytes - vb, "UI frame byte limit"); bytes += vb + ib;
        const auto name = base(s.phase) + "-list-" + std::to_string(i);
        s.write(name + ".vbo.bin", list.VtxBuffer.Data, vb); s.write(name + ".ibo.bin", list.IdxBuffer.Data, ib);
        std::vector<std::string> commands;
        const auto& callbacks = ImGui::GetPlatformIO();
        for (const auto& cmd : list.CmdBuffer) {
            const char* role = "none";
            if (cmd.UserCallback != nullptr) {
                if (cmd.UserCallback == ImDrawCallback_ResetRenderState ||
                    cmd.UserCallback == callbacks.DrawCallback_ResetRenderState) role = "reset";
                else if (cmd.UserCallback == callbacks.DrawCallback_SetSamplerNearest) role = "nearest";
                else if (cmd.UserCallback == callbacks.DrawCallback_SetSamplerLinear) role = "linear";
                else role = "unknown";
            }
            commands.push_back(object({{"element_count", n(cmd.ElemCount)},
            {"index_offset", n(cmd.IdxOffset)}, {"vertex_offset", n(cmd.VtxOffset)},
            {"clip", array({n(cmd.ClipRect.x), n(cmd.ClipRect.y), n(cmd.ClipRect.z), n(cmd.ClipRect.w)})},
            {"texture_id", q(std::to_string(static_cast<std::uint64_t>(cmd.GetTexID())))}, {"has_callback", b(cmd.UserCallback != nullptr)},
            {"reset_callback", b(std::string(role) == "reset")}, {"callback_role", q(role)}}));
        }
        lists.push_back(object({{"owner", q(list._OwnerName ? list._OwnerName : "")}, {"vertex_file", q(name + ".vbo.bin")}, {"index_file", q(name + ".ibo.bin")},
            {"vertex_count", n(list.VtxBuffer.Size)}, {"index_count", n(list.IdxBuffer.Size)}, {"commands", array(commands)}}));
    }
    Fields buttonFields; for (const auto& button : s.buttons) buttonFields.push_back(button);
    s.packet = object({{"schema", q("hellomine3d-pause-notification-frame-v1")}, {"phase", q(phases[s.phase])}, {"frame", n(s.phase)},
        {"tick", n(s.tick)}, {"phase_enter_tick", n(s.enterTick)}, {"last_status_submit_tick", n(s.statusSubmitTick)}, {"locale", q(s.locale)}, {"normal_input", "false"}, {"actual_native_focused", b(s.nativeFocused)},
        {"simulation_allowed", b(s.simulationAllowed)}, {"pending_action", n(action)}, {"elapsed_seconds", n(s.elapsed())},
        {"presentation_seconds", n(s.presentationSeconds)}, {"actual_delta_seconds", n(s.actualDeltaSeconds)},
        {"status", object({{"text", q(s.status)}, {"remaining_seconds", n(s.statusSeconds)}, {"submit_count", n(s.statusSubmits)}})},
        {"caption", object({{"cue", q(s.captionCue)}, {"text", q(s.captionText)}, {"remaining_seconds", n(s.captionSeconds)}, {"submit_count", n(s.captionSubmits)}})},
        {"io", object({{"mouse_position", xy(ImGui::GetIO().MousePos)}, {"mouse_wheel", n(s.wheel)}, {"app_focus_lost", b(ImGui::GetIO().AppFocusLost)}})},
        {"rail", s.rail}, {"status_text", s.statusText}, {"buttons", object(buttonFields)},
        {"draw_data", object({{"display_position", xy(data.DisplayPos)}, {"display_size", xy(data.DisplaySize)},
            {"framebuffer_scale", xy(data.FramebufferScale)}, {"vertex_stride", n(sizeof(ImDrawVert))},
            {"position_offset", n(offsetof(ImDrawVert, pos))}, {"uv_offset", n(offsetof(ImDrawVert, uv))},
            {"colour_offset", n(offsetof(ImDrawVert, col))}, {"colour_packing", q("IM_COL32_RGBA_shifts_" + std::to_string(IM_COL32_R_SHIFT) + "_" + std::to_string(IM_COL32_G_SHIFT) + "_" + std::to_string(IM_COL32_B_SHIFT) + "_" + std::to_string(IM_COL32_A_SHIFT))},
            {"index_bytes", n(sizeof(ImDrawIdx))}, {"lists", array(lists)}})},
        {"png", q(base(s.phase) + ".png")}, {"png_source", q("actual RenderWindow::writeContentsToFile after actual ImGui backend, before swap; unmodified PNG")},
        {"image_row_origin", q("top_left")}});
    s.pending = true;
}
bool PauseNotificationCapture::isFramePending() const noexcept { return m_impl->pending; }
bool PauseNotificationCapture::isComplete() const noexcept { return m_impl->phase == PhaseCount; }
std::size_t PauseNotificationCapture::frameCount() const noexcept { return m_impl->phase; }
std::string PauseNotificationCapture::framePngPath() const { return (fs::path(m_impl->output) / (base(m_impl->phase) + ".png")).string(); }
void PauseNotificationCapture::finishFrame()
{
    auto& s = *m_impl; require(s.pending && fs::is_regular_file(framePngPath()) && fs::file_size(framePngPath()) > 0, "actual PNG must precede packet completion");
    const auto pngBytes = fs::file_size(framePngPath());
    require(pngBytes <= 8u * 1024u * 1024u && pngBytes <= SessionBytesLimit - s.written, "PNG/session byte bound exceeded");
    s.written += static_cast<std::size_t>(pngBytes);
    s.recordedTick = s.tick;
    s.textFile(base(s.phase) + ".json", s.packet); s.frames.push_back(q(base(s.phase) + ".json"));
    ++s.phase; s.pending = false; s.entered = false; s.wheelSent = false; s.writeIndex();
}
