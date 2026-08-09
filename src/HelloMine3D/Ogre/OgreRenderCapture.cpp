#include "OgreRenderCapture.h"

#include <OgreRenderWindow.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    struct CaptureOptions
    {
        bool enabled = false;
        bool exitWhenComplete = false;
        double maxDeltaMs = 250.0;
        std::string outputDirectory;
        std::string prefix = "capture";
        std::vector<int> captureMs;
    };

    bool isFalseValue(const char *value)
    {
        if (value == nullptr || value[0] == '\0')
        {
            return false;
        }

        const std::string text(value);
        return text == "0" || text == "false" || text == "FALSE" ||
               text == "False" || text == "off" || text == "OFF";
    }

    bool isTrueValue(const char *value)
    {
        return value != nullptr && value[0] != '\0' &&
               !isFalseValue(value);
    }

    const char *getAlias(const char *primary, const char *legacy)
    {
        const char *value = std::getenv(primary);
        if (value != nullptr && value[0] != '\0')
        {
            return value;
        }
        return std::getenv(legacy);
    }

    std::vector<int> parseCaptureTimes(const char *value)
    {
        std::vector<int> result;
        std::stringstream stream(
            value != nullptr && value[0] != '\0' ? value : "2000");
        std::string token;
        while (std::getline(stream, token, ','))
        {
            std::stringstream tokenStream(token);
            std::string item;
            while (tokenStream >> item)
            {
                const int parsed = std::atoi(item.c_str());
                if (parsed >= 0)
                {
                    result.push_back(parsed);
                }
            }
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    double parsePositiveDouble(const char *name, double fallback)
    {
        const char *value = std::getenv(name);
        if (value == nullptr || value[0] == '\0')
        {
            return fallback;
        }
        const double parsed = std::atof(value);
        return parsed > 0.0 ? parsed : fallback;
    }

    CaptureOptions readOptions()
    {
        CaptureOptions options;
        options.enabled = isTrueValue(
            getAlias("HELLO_RENDER_CAPTURE", "HELLO_VISUAL_CAPTURE"));
        options.exitWhenComplete =
            isTrueValue(std::getenv("HELLO_RENDER_CAPTURE_EXIT"));
        options.maxDeltaMs = parsePositiveDouble(
            "HELLO_RENDER_CAPTURE_MAX_DELTA_MS", options.maxDeltaMs);
        options.captureMs = parseCaptureTimes(getAlias(
            "HELLO_RENDER_CAPTURE_MS", "HELLO_VISUAL_CAPTURE_MS"));

        const char *outputDirectory = getAlias(
            "HELLO_RENDER_CAPTURE_DIR", "HELLO_VISUAL_CAPTURE_DIR");
        if (outputDirectory != nullptr)
        {
            options.outputDirectory = outputDirectory;
        }
        const char *prefix = getAlias(
            "HELLO_RENDER_CAPTURE_PREFIX", "HELLO_VISUAL_CAPTURE_PREFIX");
        if (prefix != nullptr && prefix[0] != '\0')
        {
            options.prefix = prefix;
        }
        return options;
    }

    OgreRenderCaptureValidation validateOptions(const CaptureOptions &options)
    {
        OgreRenderCaptureValidation validation;
        validation.enabled = options.enabled;
        validation.targetCount = options.captureMs.size();
        if (!options.enabled)
        {
            validation.valid = true;
            validation.message = "disabled";
            return validation;
        }
        if (options.outputDirectory.empty())
        {
            validation.message = "capture output directory is missing";
            return validation;
        }
        if (options.captureMs.empty())
        {
            validation.message = "capture time list is empty";
            return validation;
        }
        validation.valid = true;
        validation.message = "ok";
        return validation;
    }
}

class OgreRenderCapture::Impl
{
  public:
    explicit Impl(Ogre::RenderWindow &renderWindow)
        : window(&renderWindow)
        , options(readOptions())
    {
        const OgreRenderCaptureValidation validation =
            validateOptions(options);
        if (!validation.valid)
        {
            std::cerr << "[OgreRenderCapture] disabled "
                      << validation.message << '\n';
            options.enabled = false;
            return;
        }
        if (!options.enabled)
        {
            return;
        }

        std::error_code error;
        std::filesystem::create_directories(options.outputDirectory, error);
        if (error)
        {
            std::cerr << "[OgreRenderCapture] disabled cannot create dir: "
                      << options.outputDirectory << " error="
                      << error.message() << '\n';
            options.enabled = false;
            return;
        }

        std::cout << "[OgreRenderCapture] enabled dir="
                  << options.outputDirectory << " prefix=" << options.prefix
                  << " targets=" << options.captureMs.size() << '\n';
    }

    void update(float deltaSeconds)
    {
        if (!options.enabled || nextIndex >= options.captureMs.size())
        {
            return;
        }
        if (!timingStarted)
        {
            timingStarted = true;
            std::cout << "[OgreRenderCapture] armed\n";
            return;
        }

        const double deltaMs = static_cast<double>(deltaSeconds) * 1000.0;
        if (deltaMs <= 0.0 || deltaMs > options.maxDeltaMs)
        {
            return;
        }
        elapsedMs += deltaMs;

        while (nextIndex < options.captureMs.size() &&
               elapsedMs + 0.5 >= options.captureMs[nextIndex])
        {
            const int targetMs = options.captureMs[nextIndex];
            std::ostringstream filename;
            filename << options.prefix << '_' << std::setw(5)
                     << std::setfill('0') << targetMs << "ms.png";
            const std::filesystem::path path =
                std::filesystem::path(options.outputDirectory) /
                filename.str();
            window->writeContentsToFile(path.string());
            std::cout << "[OgreRenderCapture] captured path="
                      << path.string() << '\n';
            ++nextIndex;
        }
    }

    Ogre::RenderWindow *window = nullptr;
    CaptureOptions options;
    bool timingStarted = false;
    double elapsedMs = 0.0;
    std::size_t nextIndex = 0;
};

OgreRenderCapture::OgreRenderCapture(Ogre::RenderWindow &window)
    : m_impl(std::make_unique<Impl>(window))
{
}

OgreRenderCapture::~OgreRenderCapture() = default;

void OgreRenderCapture::update(float deltaSeconds)
{
    m_impl->update(deltaSeconds);
}

bool OgreRenderCapture::isEnabled() const
{
    return m_impl->options.enabled;
}

bool OgreRenderCapture::isComplete() const
{
    return !m_impl->options.enabled ||
           m_impl->nextIndex >= m_impl->options.captureMs.size();
}

bool OgreRenderCapture::shouldCloseWindow() const
{
    return m_impl->options.enabled && m_impl->options.exitWhenComplete &&
           isComplete();
}

OgreRenderCaptureValidation OgreRenderCapture::validateConfiguration()
{
    return validateOptions(readOptions());
}
