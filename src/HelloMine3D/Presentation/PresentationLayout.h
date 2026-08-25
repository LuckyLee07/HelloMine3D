#pragma once

#include <cstddef>
#include <string>

struct PresentationFontProbe
{
    bool usable = false;
    std::size_t bytes = 0;
    std::string diagnostic;
};

struct PresentationWindowLayout
{
    float width = 0.0f;
    float height = 0.0f;
    bool scrollRequired = false;
};

/// Performs a bounded, renderer-independent probe before a font is handed to
/// ImGui. Parsing still belongs to ImGui; missing, empty, oversized and
/// obviously unsupported files are rejected deterministically here.
PresentationFontProbe probePresentationFont(const std::string& path);

/// Fits a fixed-design presentation window inside the current display while
/// preserving a 15-pixel safe margin. scrollRequired tells the caller that
/// translated/scaled content needs a scrolling region.
PresentationWindowLayout fitPresentationWindow(
    float displayWidth, float displayHeight,
    float desiredWidth, float desiredHeight, float uiScale) noexcept;

/// Conservative renderer-independent line estimate used to prove that long
/// translated values remain bounded before renderer-specific wrapping.
std::size_t estimateWrappedPresentationLines(
    const std::string& utf8, std::size_t columns) noexcept;
