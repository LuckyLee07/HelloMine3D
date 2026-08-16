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

    [[noreturn]] void triggerControlledCrash() noexcept
    {
        std::abort();
    }
}

#endif
