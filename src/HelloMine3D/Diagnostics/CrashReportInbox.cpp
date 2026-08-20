#include "CrashReportInbox.h"

#include "CrashDiagnosticsPlatform.h"
#include "CrashSidecar.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>

namespace
{
    namespace fs = std::filesystem;

    constexpr std::size_t MaxReports = 64;

    bool fail(std::string* error, const std::string& message) noexcept
    {
        if (error != nullptr)
        {
            *error = message;
        }
        return false;
    }

    std::string hexadecimal(std::uint32_t value)
    {
        std::ostringstream output;
        output << "0x" << std::hex << std::nouppercase << value;
        return output.str();
    }

    std::string readBoundedText(const fs::path& path)
    {
        std::error_code error;
        const std::uintmax_t size = fs::file_size(path, error);
        if (error || size == 0 || size > 16 * 1024)
        {
            return {};
        }
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            return {};
        }
        return std::string(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
    }

    bool isRegularNonEmpty(const fs::path& path)
    {
        std::error_code error;
        return fs::is_regular_file(path, error) && !error &&
               fs::file_size(path, error) > 0 && !error;
    }
}

CrashReportInboxResult scanCrashReportInbox(
    const std::string& crashDirectory) noexcept
{
    CrashReportInboxResult result;
    try
    {
        if (crashDirectory.empty())
        {
            result.error = "Crash report directory is empty.";
            return result;
        }

        const fs::path directory = fs::u8path(crashDirectory);
        std::error_code error;
        if (!fs::exists(directory, error))
        {
            return result;
        }
        if (error || !fs::is_directory(directory, error) || error)
        {
            result.error = "Crash report path is not a readable directory.";
            return result;
        }

        struct Candidate
        {
            fs::path sidecar;
            fs::file_time_type modified;
        };
        std::vector<Candidate> candidates;
        for (fs::directory_iterator iterator(directory, error), end;
             !error && iterator != end; iterator.increment(error))
        {
            if (!iterator->is_regular_file(error) || error)
            {
                continue;
            }
            const std::string name = iterator->path().filename().u8string();
            constexpr const char* suffix = ".crash.txt";
            if (name.size() <= 10 ||
                name.compare(name.size() - 10, 10, suffix) != 0)
            {
                continue;
            }
            candidates.push_back(
                {iterator->path(), iterator->last_write_time(error)});
            if (error)
            {
                break;
            }
        }
        if (error)
        {
            result.error = "Unable to enumerate local crash reports.";
            return result;
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& left, const Candidate& right)
                  {
                      if (left.modified != right.modified)
                      {
                          return left.modified > right.modified;
                      }
                      return left.sidecar.filename().u8string() <
                             right.sidecar.filename().u8string();
                  });

        for (const Candidate& candidate : candidates)
        {
            const fs::path acknowledgement =
                fs::u8path(candidate.sidecar.u8string() + ".ack");
            if (fs::is_regular_file(acknowledgement, error) && !error)
            {
                ++result.ignoredReports;
                continue;
            }
            error.clear();

            CrashSidecar sidecar;
            std::string parseError;
            const std::string payload = readBoundedText(candidate.sidecar);
            if (payload.empty() ||
                !parseCrashSidecar(payload, sidecar, &parseError))
            {
                ++result.invalidReports;
                continue;
            }
            const fs::path dump = directory / fs::u8path(sidecar.dumpFile);
            if (!isRegularNonEmpty(dump))
            {
                ++result.invalidReports;
                continue;
            }

            PendingCrashReport report;
            report.dumpPath = dump.u8string();
            report.sidecarPath = candidate.sidecar.u8string();
            report.acknowledgementPath = acknowledgement.u8string();
            report.dumpFile = sidecar.dumpFile;
            report.moduleName = sidecar.moduleName;
            report.buildIdentity = sidecar.buildIdentity;
            report.exceptionCode = hexadecimal(sidecar.exceptionCode);
            report.clipboardText =
                "HelloMine3D local crash report\n"
                "dump=" + sidecar.dumpFile + "\nmodule=" +
                sidecar.moduleName + "\nbuild_identity=" +
                sidecar.buildIdentity + "\nexception_code=" +
                report.exceptionCode + "\nupload_enabled=0\n";
            result.reports.push_back(std::move(report));
            if (result.reports.size() >= MaxReports)
            {
                break;
            }
        }
    }
    catch (const std::exception& exception)
    {
        result.error = exception.what();
    }
    catch (...)
    {
        result.error = "Unknown crash report inbox failure.";
    }
    return result;
}

bool acknowledgeCrashReport(const PendingCrashReport& report,
                            std::string* error) noexcept
{
    try
    {
        if (report.acknowledgementPath.empty() || report.dumpFile.empty())
        {
            return fail(error, "Crash report acknowledgement is invalid.");
        }
        const fs::path destination =
            fs::u8path(report.acknowledgementPath);
        const fs::path pending =
            fs::u8path(report.acknowledgementPath + ".pending");
        if (destination.parent_path() != fs::u8path(report.sidecarPath)
                                               .parent_path())
        {
            return fail(error, "Crash report acknowledgement escaped its directory.");
        }

        std::error_code filesystemError;
        fs::remove(pending, filesystemError);
        std::ofstream output(pending, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            return fail(error, "Unable to create crash report acknowledgement.");
        }
        output << "schema 1\nreport " << std::quoted(report.dumpFile)
               << "\nupload_enabled 0\n";
        output.flush();
        if (!output)
        {
            output.close();
            fs::remove(pending, filesystemError);
            return fail(error, "Unable to write crash report acknowledgement.");
        }
        output.close();
        fs::rename(pending, destination, filesystemError);
        if (filesystemError)
        {
            fs::remove(pending, filesystemError);
            return fail(error, "Unable to publish crash report acknowledgement.");
        }
        return true;
    }
    catch (const std::exception& exception)
    {
        return fail(error, exception.what());
    }
    catch (...)
    {
        return fail(error, "Unknown crash report acknowledgement failure.");
    }
}

bool openCrashReportLocation(const PendingCrashReport& report,
                             std::string* error) noexcept
{
    if (report.dumpPath.empty())
    {
        return fail(error, "Crash report path is empty.");
    }
    return CrashDiagnosticsPlatform::openReportLocation(report.dumpPath,
                                                         error);
}
