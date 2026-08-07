#include "RuntimeRenderCapture.h"

#include <SFML/Graphics/Image.hpp>
#include <glad/glad.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace RuntimeRenderCapture
{
namespace
{
    struct CaptureState
    {
        bool initialized = false;
        bool enabled = false;
        bool exitWhenComplete = false;
        bool timingStarted = false;
        double elapsedMs = 0.0;
        double maxDeltaMs = 250.0;
        std::size_t nextIndex = 0;
        int skippedLargeDeltaCount = 0;
        std::string outputDir;
        std::string prefix = "capture";
        std::vector<int> captureMs;
    };

    CaptureState &state()
    {
        static CaptureState captureState;
        return captureState;
    }

    bool isFalseEnvValue(const char *value)
    {
        if (value == nullptr || value[0] == '\0') {
            return false;
        }

        const std::string text(value);
        return text == "0" || text == "false" || text == "FALSE" ||
               text == "False" || text == "off" || text == "OFF";
    }

    bool isTrueEnvValue(const char *value)
    {
        return value != nullptr && value[0] != '\0' && !isFalseEnvValue(value);
    }

    const char *getEnvAlias(const char *primary, const char *legacy)
    {
        const char *value = std::getenv(primary);
        if (value != nullptr && value[0] != '\0') {
            return value;
        }

        return std::getenv(legacy);
    }

    std::string normalizeDirectory(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        while (path.size() > 1 && path[path.size() - 1] == '/') {
            path.erase(path.size() - 1);
        }
        return path;
    }

    bool createDirectoryIfMissing(const std::string &path)
    {
        if (path.empty() || (path.size() == 2 && path[1] == ':')) {
            return true;
        }

#if defined(_WIN32)
        return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
    }

    bool ensureDirectoryRecursive(const std::string &directory)
    {
        const std::string normalized = normalizeDirectory(directory);
        if (normalized.empty()) {
            return false;
        }

        std::size_t start = 0;
        if (normalized.size() >= 2 && normalized[1] == ':') {
            start = 2;
        }
        while (start < normalized.size() && normalized[start] == '/') {
            ++start;
        }

        std::size_t pos = start;
        while (pos <= normalized.size()) {
            const std::size_t slash = normalized.find('/', pos);
            const std::size_t end =
                slash == std::string::npos ? normalized.size() : slash;
            const std::string part = normalized.substr(0, end);
            if (!createDirectoryIfMissing(part)) {
                return false;
            }

            if (slash == std::string::npos) {
                break;
            }
            pos = slash + 1;
        }

        return true;
    }

    std::vector<int> parseCaptureTimes(const char *value)
    {
        std::vector<int> result;
        const std::string text =
            value != nullptr && value[0] != '\0' ? value : "2000";
        std::stringstream stream(text);
        std::string token;

        while (std::getline(stream, token, ',')) {
            std::stringstream tokenStream(token);
            std::string item;
            while (tokenStream >> item) {
                const int parsed = std::atoi(item.c_str());
                if (parsed >= 0) {
                    result.push_back(parsed);
                }
            }
        }

        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    std::string joinCaptureTimes(const std::vector<int> &captureMs)
    {
        std::ostringstream stream;
        for (std::size_t i = 0; i < captureMs.size(); ++i) {
            if (i > 0) {
                stream << ",";
            }
            stream << captureMs[i];
        }
        return stream.str();
    }

    double parsePositiveDoubleEnv(const char *name, double fallback)
    {
        const char *value = std::getenv(name);
        if (value == nullptr || value[0] == '\0') {
            return fallback;
        }

        const double parsed = std::atof(value);
        return parsed > 0.0 ? parsed : fallback;
    }

    void initialize()
    {
        CaptureState &captureState = state();
        if (captureState.initialized) {
            return;
        }

        captureState.initialized = true;
        captureState.enabled =
            isTrueEnvValue(getEnvAlias("HELLO_RENDER_CAPTURE",
                                       "HELLO_VISUAL_CAPTURE"));
        if (!captureState.enabled) {
            return;
        }

        const char *outputDir = getEnvAlias("HELLO_RENDER_CAPTURE_DIR",
                                            "HELLO_VISUAL_CAPTURE_DIR");
        if (outputDir == nullptr || outputDir[0] == '\0') {
            std::cerr << "[RenderCapture] disabled missing output dir\n";
            captureState.enabled = false;
            return;
        }

        captureState.outputDir = normalizeDirectory(outputDir);
        const char *prefix = getEnvAlias("HELLO_RENDER_CAPTURE_PREFIX",
                                         "HELLO_VISUAL_CAPTURE_PREFIX");
        captureState.prefix =
            prefix != nullptr && prefix[0] != '\0' ? prefix : "capture";
        captureState.exitWhenComplete =
            isTrueEnvValue(std::getenv("HELLO_RENDER_CAPTURE_EXIT"));
        captureState.captureMs = parseCaptureTimes(getEnvAlias(
            "HELLO_RENDER_CAPTURE_MS", "HELLO_VISUAL_CAPTURE_MS"));
        captureState.maxDeltaMs =
            parsePositiveDoubleEnv("HELLO_RENDER_CAPTURE_MAX_DELTA_MS", 250.0);

        if (captureState.captureMs.empty()) {
            std::cerr << "[RenderCapture] disabled missing capture times\n";
            captureState.enabled = false;
            return;
        }

        if (!ensureDirectoryRecursive(captureState.outputDir)) {
            std::cerr << "[RenderCapture] disabled cannot create output dir: "
                      << captureState.outputDir << "\n";
            captureState.enabled = false;
            return;
        }

        std::cout << "[RenderCapture] enabled dir=" << captureState.outputDir
                  << " prefix=" << captureState.prefix
                  << " capturesMs=" << joinCaptureTimes(captureState.captureMs)
                  << "\n";
    }

    std::string buildCapturePath(const CaptureState &captureState, int targetMs)
    {
        std::ostringstream stream;
        stream << captureState.outputDir << "/" << captureState.prefix << "_"
               << std::setw(5) << std::setfill('0') << targetMs << "ms.png";
        return stream.str();
    }

    bool captureWindow(const sf::Window &window, const std::string &path)
    {
        const sf::Vector2u size = window.getSize();
        if (size.x == 0 || size.y == 0) {
            std::cerr << "[RenderCapture] failed invalid window size path="
                      << path << "\n";
            return false;
        }

        std::vector<std::uint8_t> pixels(size.x * size.y * 4);

        GLint previousPackAlignment = 4;
        GLint previousReadBuffer = GL_BACK;
        glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
        glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, static_cast<GLsizei>(size.x),
                     static_cast<GLsizei>(size.y), GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
        glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
        glReadBuffer(static_cast<GLenum>(previousReadBuffer));

        sf::Image image(size, pixels.data());
        image.flipVertically();
        if (!image.saveToFile(path)) {
            std::cerr << "[RenderCapture] failed save path=" << path << "\n";
            return false;
        }

        std::cout << "[RenderCapture] captured path=" << path << "\n";
        return true;
    }
} // namespace

void update(const sf::Window &window, sf::Time deltaTime)
{
    initialize();

    CaptureState &captureState = state();
    if (!captureState.enabled ||
        captureState.nextIndex >= captureState.captureMs.size()) {
        return;
    }

    if (!captureState.timingStarted) {
        captureState.timingStarted = true;
        std::cout << "[RenderCapture] armed\n";
        return;
    }

    const double deltaMs = static_cast<double>(deltaTime.asSeconds()) * 1000.0;
    if (deltaMs > captureState.maxDeltaMs) {
        ++captureState.skippedLargeDeltaCount;
        if (captureState.skippedLargeDeltaCount <= 3) {
            std::cout << "[RenderCapture] skipLargeDelta deltaMs=" << deltaMs
                      << " maxDeltaMs=" << captureState.maxDeltaMs << "\n";
        }
        return;
    }

    if (deltaMs > 0.0) {
        captureState.elapsedMs += deltaMs;
    }

    while (captureState.nextIndex < captureState.captureMs.size() &&
           captureState.elapsedMs + 0.5 >=
               captureState.captureMs[captureState.nextIndex]) {
        const int targetMs = captureState.captureMs[captureState.nextIndex];
        if (!captureWindow(window, buildCapturePath(captureState, targetMs))) {
            break;
        }
        ++captureState.nextIndex;
    }
}

bool isComplete()
{
    initialize();

    const CaptureState &captureState = state();
    return !captureState.enabled ||
           captureState.nextIndex >= captureState.captureMs.size();
}

bool isEnabled()
{
    initialize();

    return state().enabled;
}

bool shouldCloseWindow()
{
    initialize();

    const CaptureState &captureState = state();
    return captureState.enabled && captureState.exitWhenComplete &&
           captureState.nextIndex >= captureState.captureMs.size();
}
} // namespace RuntimeRenderCapture
