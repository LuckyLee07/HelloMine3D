#include "../Diagnostics/CrashDiagnostics.h"
#include "../Diagnostics/CrashReportInbox.h"
#include "../Diagnostics/CrashSidecar.h"
#include "CrashSymbolizer.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    class TestSuite
    {
      public:
        void check(const std::string& id, bool passed,
                   const std::string& detail = std::string())
        {
            ++m_checks;
            if (!passed)
            {
                ++m_failures;
            }
            std::cout << "[CRASH_DIAGNOSTICS_TEST] "
                      << (passed ? "PASS " : "FAIL ") << id;
            if (!detail.empty())
            {
                std::cout << " :: " << detail;
            }
            std::cout << '\n';
        }

        int finish() const
        {
            std::cout << "[CRASH_DIAGNOSTICS_TEST] checks=" << m_checks
                      << " failures=" << m_failures << '\n';
            std::cout << "[CRASH_DIAGNOSTICS_TEST] status="
                      << (m_failures == 0 ? "PASS" : "FAIL") << '\n';
            return m_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

      private:
        int m_checks = 0;
        int m_failures = 0;
    };

    bool throwsInvalid(const std::function<void()>& action)
    {
        try
        {
            action();
        }
        catch (const std::invalid_argument&)
        {
            return true;
        }
        return false;
    }

    std::string normalized(const fs::path& path)
    {
        std::error_code error;
        fs::path absolute = path;
        if (absolute.is_relative())
        {
            absolute = fs::absolute(absolute, error);
        }
        const fs::path canonical = fs::weakly_canonical(absolute, error);
        return (error ? absolute : canonical).lexically_normal().u8string();
    }
}

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--symbolize")
    {
        return runCrashSymbolizerCommand(argc, argv);
    }

    TestSuite suite;
    const fs::path root = fs::temp_directory_path() /
        ("hello-mine-crash-contract-" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()));
    const fs::path save = root / "state" / "world";
    const fs::path crashes = root / "state" / "crashes";

    const CrashDiagnosticsConfiguration defaults =
        resolveCrashDiagnosticsConfiguration(
            root.u8string(), std::string(), std::string(), std::string());
    suite.check(
        "H1/default-directories-are-dedicated-siblings",
        defaults.worldSaveDirectory ==
                normalized(root / "bin" / "saves" / "default") &&
            defaults.crashDirectory == normalized(root / "bin" / "crashes"));
    suite.check("H1/empty-trigger-is-disabled",
                defaults.controlledCrashPoint ==
                    ControlledCrashPoint::Disabled);

    const CrashDiagnosticsConfiguration controlled =
        resolveCrashDiagnosticsConfiguration(
            root.u8string(), save.u8string(), crashes.u8string(),
            "after-first-frame");
    suite.check("H1/exact-controlled-trigger-is-accepted",
                controlled.controlledCrashPoint ==
                    ControlledCrashPoint::AfterFirstFrame);
    suite.check(
        "H1/trigger-names-are-stable",
        std::string(controlledCrashPointName(
            ControlledCrashPoint::Disabled)) == "disabled" &&
            std::string(controlledCrashPointName(
                ControlledCrashPoint::AfterFirstFrame)) ==
                "after-first-frame");

    const CrashDiagnosticsConfiguration relative =
        resolveCrashDiagnosticsConfiguration(
            root.u8string(), "relative-save-fixture",
            "relative-crash-fixture", std::string());
    suite.check("H1/relative-overrides-follow-working-directory",
                relative.worldSaveDirectory ==
                        normalized("relative-save-fixture") &&
                    relative.crashDirectory ==
                        normalized("relative-crash-fixture"));
    suite.check("H1/separate-sibling-overrides-are-accepted",
                controlled.worldSaveDirectory == normalized(save) &&
                    controlled.crashDirectory == normalized(crashes));

    suite.check(
        "H1/equal-save-and-crash-directory-is-rejected",
        throwsInvalid([&]() {
            resolveCrashDiagnosticsConfiguration(
                root.u8string(), save.u8string(), save.u8string(),
                std::string());
        }));
    suite.check(
        "H1/crash-directory-under-save-is-rejected",
        throwsInvalid([&]() {
            resolveCrashDiagnosticsConfiguration(
                root.u8string(), save.u8string(),
                (save / "crashes").u8string(), std::string());
        }));
    suite.check(
        "H1/save-directory-under-crash-is-rejected",
        throwsInvalid([&]() {
            resolveCrashDiagnosticsConfiguration(
                root.u8string(), (crashes / "world").u8string(),
                crashes.u8string(), std::string());
        }));
    suite.check(
        "H1/unknown-controlled-trigger-is-rejected",
        throwsInvalid([&]() {
            resolveCrashDiagnosticsConfiguration(
                root.u8string(), save.u8string(), crashes.u8string(),
                "1");
        }));
    suite.check(
        "H1/empty-project-root-is-rejected",
        throwsInvalid([&]() {
            resolveCrashDiagnosticsConfiguration(
                std::string(), save.u8string(), crashes.u8string(),
                std::string());
        }));

    std::error_code cleanupError;
    fs::remove_all(root, cleanupError);
    const CrashDiagnosticsConfiguration ordinary =
        resolveCrashDiagnosticsConfiguration(
            root.u8string(), save.u8string(), crashes.u8string(),
            std::string());
    const CrashDiagnosticsInstallResult install =
        installCrashDiagnostics(ordinary);
#if defined(_WIN32)
    const bool installContract =
        install.backend == "windows-dbghelp" && install.supported &&
        install.installed && install.error.empty() &&
        fs::is_directory(crashes) &&
        fs::directory_iterator(crashes) == fs::directory_iterator();
#else
    const bool installContract =
        install.backend == "unsupported" && !install.supported &&
        !install.installed && install.error.empty() && !fs::exists(crashes);
#endif
    suite.check("H1/ordinary-install-produces-no-dump", installContract,
                install.error);

    CrashSidecar sidecar;
    sidecar.dumpFile = "HelloMine3D-fixture.dmp";
    sidecar.moduleName = "HelloMine3D.exe";
    sidecar.pdbGuid = "0123456789abcdef0123456789abcdef";
    sidecar.pdbAge = 1;
    sidecar.buildIdentity =
        crashBuildIdentity(sidecar.pdbGuid, sidecar.pdbAge);
    sidecar.moduleTimestamp = 1;
    sidecar.moduleImageSize = 4096;
    sidecar.exceptionCode = 0xe0424d33u;
    sidecar.exceptionRva = 0;
    sidecar.symbolProbeRva = 128;
    sidecar.threadId = 7;
    const std::string serialized = serializeCrashSidecar(sidecar);
    CrashSidecar parsed;
    std::string sidecarError;
    suite.check("H2/sidecar-roundtrip-is-versioned-and-sanitized",
                parseCrashSidecar(serialized, parsed, &sidecarError) &&
                    parsed.buildIdentity == sidecar.buildIdentity &&
                    serialized.find(root.u8string()) == std::string::npos &&
                    serialized.find("upload_enabled 0") !=
                        std::string::npos,
                sidecarError);

    const std::string pathLeak =
        serialized.substr(0, serialized.find("dump_file")) +
        "dump_file \"C:\\\\Users\\\\person\\\\dump.dmp\"\n" +
        serialized.substr(serialized.find("module_name"));
    suite.check("H2/sidecar-rejects-personal-paths",
                !parseCrashSidecar(pathLeak, parsed, &sidecarError));
    suite.check("H2/sidecar-rejects-unknown-fields",
                !parseCrashSidecar(serialized + "mystery 1\n", parsed,
                                   &sidecarError));
    std::string mismatched = serialized;
    const std::size_t identity = mismatched.find(sidecar.buildIdentity);
    if (identity != std::string::npos)
    {
        mismatched.replace(identity, sidecar.buildIdentity.size(),
                           "pdb-ffffffffffffffffffffffffffffffff-1");
    }
    suite.check("H2/sidecar-rejects-mismatched-build-identity",
                !parseCrashSidecar(mismatched, parsed, &sidecarError));

    fs::create_directories(crashes);
    {
        std::ofstream dump(crashes / sidecar.dumpFile,
                           std::ios::binary | std::ios::trunc);
        dump << "local-minidump-fixture";
        std::ofstream report(crashes / "HelloMine3D-fixture.crash.txt",
                             std::ios::binary | std::ios::trunc);
        report << serialized;
        std::ofstream malformed(crashes / "malformed.crash.txt",
                                std::ios::binary | std::ios::trunc);
        malformed << "schema 99\n";
        std::ofstream orphan(crashes / "orphan.crash.txt",
                             std::ios::binary | std::ios::trunc);
        CrashSidecar orphanSidecar = sidecar;
        orphanSidecar.dumpFile = "missing.dmp";
        orphan << serializeCrashSidecar(orphanSidecar);
    }
    const CrashReportInboxResult pending =
        scanCrashReportInbox(crashes.u8string());
    suite.check("H3/inbox-publishes-only-valid-local-pairs",
                pending.error.empty() && pending.reports.size() == 1 &&
                    pending.invalidReports == 2 &&
                    pending.ignoredReports == 0);
    suite.check("H3/copied-details-are-sanitized-and-offline",
                pending.reports.size() == 1 &&
                    pending.reports.front().clipboardText.find(
                        root.u8string()) == std::string::npos &&
                    pending.reports.front().clipboardText.find(
                        sidecar.buildIdentity) != std::string::npos &&
                    pending.reports.front().clipboardText.find(
                        "upload_enabled=0") != std::string::npos);
    std::string acknowledgementError;
    suite.check("H3/ignore-is-persisted-without-deleting-artifacts",
                pending.reports.size() == 1 &&
                    acknowledgeCrashReport(pending.reports.front(),
                                           &acknowledgementError) &&
                    fs::is_regular_file(
                        pending.reports.front().acknowledgementPath) &&
                    fs::is_regular_file(pending.reports.front().dumpPath) &&
                    fs::is_regular_file(pending.reports.front().sidecarPath),
                acknowledgementError);
    const CrashReportInboxResult acknowledged =
        scanCrashReportInbox(crashes.u8string());
    suite.check("H3/acknowledged-report-does-not-prompt-again",
                acknowledged.error.empty() &&
                    acknowledged.reports.empty() &&
                    acknowledged.ignoredReports == 1 &&
                    acknowledged.invalidReports == 2);
    const CrashReportInboxResult missingInbox =
        scanCrashReportInbox((root / "missing-crash-directory").u8string());
    suite.check("H3/missing-inbox-is-an-empty-nonfatal-state",
                missingInbox.error.empty() &&
                    missingInbox.reports.empty());

    fs::remove_all(root, cleanupError);
    return suite.finish();
}
