#include "CrashDiagnosticsPlatform.h"

#if !defined(_WIN32)

#include <cstdlib>

namespace CrashDiagnosticsPlatform
{
    const char* backendName() noexcept
    {
        return "unsupported";
    }

    bool isSupported() noexcept
    {
        return false;
    }

    bool install(const std::string&, std::string&) noexcept
    {
        return false;
    }

    bool openReportLocation(const std::string&, std::string* error) noexcept
    {
        if (error != nullptr)
        {
            *error = "Opening a crash report is unsupported on this platform.";
        }
        return false;
    }

    [[noreturn]] void triggerControlledCrash() noexcept
    {
        std::abort();
    }
}

#endif
