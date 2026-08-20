#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct PendingCrashReport
{
    std::string dumpPath;
    std::string sidecarPath;
    std::string acknowledgementPath;
    std::string dumpFile;
    std::string moduleName;
    std::string buildIdentity;
    std::string exceptionCode;
    std::string clipboardText;
};

struct CrashReportInboxResult
{
    std::vector<PendingCrashReport> reports;
    std::size_t ignoredReports = 0;
    std::size_t invalidReports = 0;
    std::string error;
};

CrashReportInboxResult scanCrashReportInbox(
    const std::string& crashDirectory) noexcept;

bool acknowledgeCrashReport(const PendingCrashReport& report,
                            std::string* error = nullptr) noexcept;

bool openCrashReportLocation(const PendingCrashReport& report,
                             std::string* error = nullptr) noexcept;
