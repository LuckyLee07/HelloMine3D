#include "CrashDiagnostics.h"

#include "CrashDiagnosticsPlatform.h"

#include <atomic>
#include <cctype>
#include <filesystem>
#include <mutex>
#include <stdexcept>

namespace
{
    namespace fs = std::filesystem;

    std::mutex g_installMutex;
    CrashDiagnosticsConfiguration g_configuration;
    std::atomic<bool> g_installed{false};
    std::atomic<bool> g_controlledCrashConsumed{false};

    fs::path normalizePath(const std::string& value)
    {
        std::error_code error;
        fs::path path = fs::u8path(value);
        if (path.is_relative())
        {
            path = fs::absolute(path, error);
            if (error)
            {
                throw std::invalid_argument(
                    "Unable to resolve crash diagnostics path '" + value +
                    "'.");
            }
        }

        const fs::path canonical = fs::weakly_canonical(path, error);
        return (error ? path : canonical).lexically_normal();
    }

    std::string comparableComponent(const fs::path& component)
    {
        std::string text = component.u8string();
#if defined(_WIN32)
        for (char& character : text)
        {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }
#endif
        return text;
    }

    bool isPathWithin(const fs::path& candidate, const fs::path& parent)
    {
        auto candidatePart = candidate.begin();
        for (auto parentPart = parent.begin();
             parentPart != parent.end(); ++parentPart, ++candidatePart)
        {
            if (candidatePart == candidate.end() ||
                comparableComponent(*candidatePart) !=
                    comparableComponent(*parentPart))
            {
                return false;
            }
        }
        return true;
    }

    ControlledCrashPoint parseControlledCrashPoint(
        const std::string& value)
    {
        if (value.empty())
        {
            return ControlledCrashPoint::Disabled;
        }
        if (value == "after-first-frame")
        {
            return ControlledCrashPoint::AfterFirstFrame;
        }
        throw std::invalid_argument(
            "Invalid HELLOMINE3D_CONTROLLED_CRASH value '" + value +
            "'; expected 'after-first-frame' or an empty value.");
    }
}

CrashDiagnosticsConfiguration resolveCrashDiagnosticsConfiguration(
    const std::string& projectRoot,
    const std::string& saveDirectoryOverride,
    const std::string& crashDirectoryOverride,
    const std::string& controlledCrashValue)
{
    if (projectRoot.empty())
    {
        throw std::invalid_argument(
            "Crash diagnostics requires a non-empty project root.");
    }

    const fs::path root = normalizePath(projectRoot);
    const fs::path saveDirectory = normalizePath(
        saveDirectoryOverride.empty()
            ? (root / "bin" / "saves" / "default").u8string()
            : saveDirectoryOverride);
    const fs::path crashDirectory = normalizePath(
        crashDirectoryOverride.empty()
            ? (root / "bin" / "crashes").u8string()
            : crashDirectoryOverride);

    if (isPathWithin(crashDirectory, saveDirectory) ||
        isPathWithin(saveDirectory, crashDirectory))
    {
        throw std::invalid_argument(
            "Crash directory must be separate from the active world save "
            "directory.");
    }

    CrashDiagnosticsConfiguration configuration;
    configuration.crashDirectory = crashDirectory.u8string();
    configuration.worldSaveDirectory = saveDirectory.u8string();
    configuration.controlledCrashPoint =
        parseControlledCrashPoint(controlledCrashValue);
    return configuration;
}

CrashDiagnosticsInstallResult installCrashDiagnostics(
    const CrashDiagnosticsConfiguration& configuration) noexcept
{
    std::lock_guard<std::mutex> lock(g_installMutex);

    CrashDiagnosticsInstallResult result;
    result.backend = CrashDiagnosticsPlatform::backendName();
    result.supported = CrashDiagnosticsPlatform::isSupported();

    if (g_installed.load(std::memory_order_acquire))
    {
        result.installed = true;
        return result;
    }
    if (!result.supported)
    {
        g_configuration = configuration;
        if (configuration.controlledCrashPoint !=
            ControlledCrashPoint::Disabled)
        {
            result.error =
                "No compatible crash dump backend is available on this "
                "platform.";
        }
        return result;
    }

    try
    {
        std::error_code error;
        const fs::path directory = fs::u8path(configuration.crashDirectory);
        fs::create_directories(directory, error);
        if (error || !fs::is_directory(directory, error) || error)
        {
            result.error =
                "Unable to create the dedicated crash directory.";
            return result;
        }

        if (!CrashDiagnosticsPlatform::install(
                configuration.crashDirectory, result.error))
        {
            if (result.error.empty())
            {
                result.error =
                    "The platform crash handler could not be installed.";
            }
            return result;
        }

        g_configuration = configuration;
        g_controlledCrashConsumed.store(false, std::memory_order_release);
        g_installed.store(true, std::memory_order_release);
        result.installed = true;
        return result;
    }
    catch (const std::exception& exception)
    {
        result.error = exception.what();
    }
    catch (...)
    {
        result.error = "Unknown crash-handler installation failure.";
    }
    return result;
}

const char* controlledCrashPointName(ControlledCrashPoint point) noexcept
{
    switch (point)
    {
    case ControlledCrashPoint::Disabled:
        return "disabled";
    case ControlledCrashPoint::AfterFirstFrame:
        return "after-first-frame";
    }
    return "unknown";
}

bool isControlledCrashRequested(ControlledCrashPoint point) noexcept
{
    return point != ControlledCrashPoint::Disabled &&
           g_installed.load(std::memory_order_acquire) &&
           g_configuration.controlledCrashPoint == point &&
           !g_controlledCrashConsumed.load(std::memory_order_acquire);
}

void triggerControlledCrashIfRequested(ControlledCrashPoint point) noexcept
{
    if (!isControlledCrashRequested(point))
    {
        return;
    }

    bool expected = false;
    if (!g_controlledCrashConsumed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
    {
        return;
    }
    CrashDiagnosticsPlatform::triggerControlledCrash();
}
