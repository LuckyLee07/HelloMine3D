#include "../Diagnostics/CrashDiagnostics.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
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

int main()
{
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

    fs::remove_all(root, cleanupError);
    return suite.finish();
}
