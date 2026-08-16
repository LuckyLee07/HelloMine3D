#pragma once

#include <string>

namespace CrashDiagnosticsPlatform
{
    const char* backendName() noexcept;
    bool isSupported() noexcept;

    // Installs the platform handler for a directory that already exists.
    // Platform exception types stay behind this private boundary.
    bool install(const std::string& crashDirectory,
                 std::string& error) noexcept;

    [[noreturn]] void triggerControlledCrash() noexcept;
}
