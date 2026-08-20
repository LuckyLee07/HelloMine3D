#include "OgreBootstrap.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "../Diagnostics/CrashDiagnostics.h"
#include "../Diagnostics/CrashReportInbox.h"
#include "../Util/ResourcePaths.h"

namespace
{
    bool isTruthy(const char* value)
    {
        if (value == nullptr)
        {
            return false;
        }

        const std::string text(value);
        return text == "1" || text == "true" || text == "TRUE" ||
               text == "on" || text == "ON";
    }

    std::string environmentValue(const char* name)
    {
        const char* value = std::getenv(name);
        return value == nullptr ? std::string() : std::string(value);
    }
}

int main()
{
    try
    {
        const CrashDiagnosticsConfiguration crashConfiguration =
            resolveCrashDiagnosticsConfiguration(
                ResourcePaths::projectRoot(),
                environmentValue("HELLOMINE3D_SAVE_DIR"),
                environmentValue("HELLOMINE3D_CRASH_DIR"),
                environmentValue("HELLOMINE3D_CONTROLLED_CRASH"));
        const CrashDiagnosticsInstallResult crashInstall =
            installCrashDiagnostics(crashConfiguration);

        std::cout << "[CRASH_DIAGNOSTICS] backend="
                  << crashInstall.backend
                  << " supported=" << (crashInstall.supported ? 1 : 0)
                  << " installed=" << (crashInstall.installed ? 1 : 0)
                  << " upload=0 controlled="
                  << controlledCrashPointName(
                         crashConfiguration.controlledCrashPoint)
                  << '\n';

        if ((crashInstall.supported && !crashInstall.installed) ||
            (crashConfiguration.controlledCrashPoint !=
                 ControlledCrashPoint::Disabled &&
             !crashInstall.installed))
        {
            std::cerr << "Crash diagnostics startup failed";
            if (!crashInstall.error.empty())
            {
                std::cerr << ": " << crashInstall.error;
            }
            std::cerr << '\n';
            return EXIT_FAILURE;
        }

        CrashReportInboxResult crashInbox =
            scanCrashReportInbox(crashConfiguration.crashDirectory);
        std::cout << "[CRASH_REPORT] pending="
                  << crashInbox.reports.size()
                  << " ignored=" << crashInbox.ignoredReports
                  << " invalid=" << crashInbox.invalidReports
                  << " upload=0";
        if (!crashInbox.error.empty())
        {
            std::cout << " scan_error=" << crashInbox.error;
        }
        std::cout << '\n';

        return runOgreBootstrap(
            isTruthy(std::getenv("HELLOMINE3D_VALIDATE_ONLY")),
            std::move(crashInbox.reports));
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Crash diagnostics startup failed: "
                  << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
