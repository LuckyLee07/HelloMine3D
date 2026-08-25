#include "PresentationLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

namespace
{
    constexpr std::size_t MaxPresentationFontBytes = 32u * 1024u * 1024u;
    constexpr float SafeMargin = 15.0f;

    bool supportedFontSignature(const std::array<unsigned char, 4>& signature)
    {
        const bool trueType = signature[0] == 0x00u &&
                              signature[1] == 0x01u &&
                              signature[2] == 0x00u &&
                              signature[3] == 0x00u;
        const bool openType = signature[0] == 'O' && signature[1] == 'T' &&
                              signature[2] == 'T' && signature[3] == 'O';
        const bool collection = signature[0] == 't' && signature[1] == 't' &&
                                signature[2] == 'c' && signature[3] == 'f';
        const bool legacyTrueType =
            signature[0] == 't' && signature[1] == 'r' &&
            signature[2] == 'u' && signature[3] == 'e';
        return trueType || openType || collection || legacyTrueType;
    }
}

PresentationFontProbe probePresentationFont(const std::string& path)
{
    PresentationFontProbe result;
    if (path.empty())
    {
        result.diagnostic = "Missing presentation font path.";
        return result;
    }

    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 4)
    {
        result.diagnostic = "Missing or empty presentation font: " + path;
        return result;
    }
    const std::streamoff size = input.tellg();
    if (size > static_cast<std::streamoff>(MaxPresentationFontBytes))
    {
        result.diagnostic = "Oversized presentation font: " + path;
        return result;
    }
    result.bytes = static_cast<std::size_t>(size);
    input.seekg(0, std::ios::beg);
    std::array<unsigned char, 4> signature{};
    if (!input.read(reinterpret_cast<char*>(signature.data()),
                    static_cast<std::streamsize>(signature.size())) ||
        !supportedFontSignature(signature))
    {
        result.diagnostic = "Unsupported presentation font: " + path;
        return result;
    }
    result.usable = true;
    return result;
}

PresentationWindowLayout fitPresentationWindow(
    float displayWidth, float displayHeight,
    float desiredWidth, float desiredHeight, float uiScale) noexcept
{
    const float safeDisplayWidth =
        std::isfinite(displayWidth) && displayWidth > 0.0f
            ? displayWidth
            : 640.0f;
    const float safeDisplayHeight =
        std::isfinite(displayHeight) && displayHeight > 0.0f
            ? displayHeight
            : 480.0f;
    const float safeDesiredWidth =
        std::isfinite(desiredWidth) && desiredWidth > 0.0f
            ? desiredWidth
            : 1.0f;
    const float safeDesiredHeight =
        std::isfinite(desiredHeight) && desiredHeight > 0.0f
            ? desiredHeight
            : 1.0f;
    const float safeScale =
        std::isfinite(uiScale) ? std::clamp(uiScale, 0.75f, 1.75f) : 1.0f;
    const float availableWidth =
        std::max(1.0f, safeDisplayWidth - SafeMargin * 2.0f);
    const float availableHeight =
        std::max(1.0f, safeDisplayHeight - SafeMargin * 2.0f);

    PresentationWindowLayout result;
    result.width = std::min(safeDesiredWidth, availableWidth);
    result.height = std::min(safeDesiredHeight, availableHeight);
    result.scrollRequired =
        safeDesiredWidth * safeScale > result.width ||
        safeDesiredHeight * safeScale > result.height;
    return result;
}

std::size_t estimateWrappedPresentationLines(
    const std::string& utf8, std::size_t columns) noexcept
{
    if (utf8.empty())
    {
        return 0;
    }
    const std::size_t safeColumns = std::max<std::size_t>(1, columns);
    std::size_t lines = 1;
    std::size_t lineColumns = 0;
    for (unsigned char value : utf8)
    {
        if (value == '\n')
        {
            ++lines;
            lineColumns = 0;
            continue;
        }
        if ((value & 0xc0u) == 0x80u)
        {
            continue;
        }
        if (lineColumns == safeColumns)
        {
            ++lines;
            lineColumns = 0;
        }
        ++lineColumns;
    }
    return lines;
}
