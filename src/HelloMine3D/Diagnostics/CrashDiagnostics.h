#pragma once

#include <string>

enum class ControlledCrashPoint
{
    Disabled,
    AfterFirstFrame
};

struct CrashDiagnosticsConfiguration
{
    std::string crashDirectory;
    std::string worldSaveDirectory;
    ControlledCrashPoint controlledCrashPoint =
        ControlledCrashPoint::Disabled;
};

struct CrashDiagnosticsInstallResult
{
    std::string backend;
    bool supported = false;
    bool installed = false;
    std::string error;
};

// Empty overrides select the ordinary runtime defaults. Relative overrides
// retain the process-working-directory semantics used by the world runtime.
// Throws std::invalid_argument when the trigger is unknown or the crash and
// save directories overlap.
CrashDiagnosticsConfiguration resolveCrashDiagnosticsConfiguration(
    const std::string& projectRoot,
    const std::string& saveDirectoryOverride,
    const std::string& crashDirectoryOverride,
    const std::string& controlledCrashValue);

CrashDiagnosticsInstallResult installCrashDiagnostics(
    const CrashDiagnosticsConfiguration& configuration) noexcept;

const char* controlledCrashPointName(ControlledCrashPoint point) noexcept;

bool isControlledCrashRequested(ControlledCrashPoint point) noexcept;

// The request is consumed once. A configured, installed platform backend does
// not return; unsupported platforms never enter this function from the client.
void triggerControlledCrashIfRequested(ControlledCrashPoint point) noexcept;
