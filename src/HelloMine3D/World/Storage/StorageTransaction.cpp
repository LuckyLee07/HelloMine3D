#include "StorageTransaction.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace
{
    namespace fs = std::filesystem;

    bool injected(const StorageTransactionOptions &options,
                  StorageFaultPoint point, StorageTransactionMetrics &metrics)
    {
        if (options.faultPoint != point) {
            return false;
        }
        metrics.error = std::string("injected fault at ") +
                        storageFaultPointName(point);
        return true;
    }

    bool movePendingToQuarantine(const std::string &pending,
                                 const std::string &quarantine,
                                 StorageTransactionMetrics &metrics)
    {
        std::error_code error;
        const bool exists = fs::exists(pending, error);
        if (error) {
            if (!metrics.error.empty()) {
                metrics.error += "; ";
            }
            metrics.error += "cannot inspect candidate: " +
                             error.message();
            return false;
        }
        if (!exists) {
            return true;
        }
        fs::remove(quarantine, error);
        error.clear();
        fs::rename(pending, quarantine, error);
        if (error) {
            if (!metrics.error.empty()) {
                metrics.error += "; ";
            }
            metrics.error += "cannot quarantine candidate: " +
                             error.message();
            return false;
        }
        metrics.quarantinePath = quarantine;
        return true;
    }

    bool durableFlush(std::FILE *file)
    {
        if (std::fflush(file) != 0) {
            return false;
        }
#if defined(_WIN32)
        return _commit(_fileno(file)) == 0;
#else
        return fsync(fileno(file)) == 0;
#endif
    }

    bool replacePublished(const std::string &pending,
                          const std::string &target,
                          std::string &errorMessage)
    {
#if defined(_WIN32)
        const std::wstring pendingWide = fs::path(pending).wstring();
        const std::wstring targetWide = fs::path(target).wstring();
        if (!MoveFileExW(pendingWide.c_str(), targetWide.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            errorMessage = "atomic replace failed with Win32 error " +
                           std::to_string(GetLastError());
            return false;
        }
        return true;
#else
        if (std::rename(pending.c_str(), target.c_str()) != 0) {
            errorMessage = "atomic replace failed: " +
                           std::error_code(errno, std::generic_category())
                               .message();
            return false;
        }

        // File contents were flushed before rename. Directory fsync is the
        // final durability step where the host supports it; APFS and some
        // network filesystems may reject directory fsync, so it is best effort.
        const fs::path parent = fs::path(target).parent_path();
        const int directory =
            open(parent.empty() ? "." : parent.c_str(), O_RDONLY);
        if (directory >= 0) {
            (void)fsync(directory);
            close(directory);
        }
        return true;
#endif
    }
}

const char *storageFaultPointName(StorageFaultPoint point) noexcept
{
    switch (point) {
    case StorageFaultPoint::None:
        return "none";
    case StorageFaultPoint::BeforeWrite:
        return "before-write";
    case StorageFaultPoint::MidWrite:
        return "mid-write";
    case StorageFaultPoint::BeforeFlush:
        return "before-flush";
    case StorageFaultPoint::BeforeValidation:
        return "before-validation";
    case StorageFaultPoint::BeforeReplace:
        return "before-replace";
    }
    return "unknown";
}

bool StorageTransaction::publish(
    const std::string &targetPath, const std::vector<char> &payload,
    const StorageCandidateValidator &validator,
    const StorageTransactionOptions &options,
    StorageTransactionMetrics *metricsOutput)
{
    StorageTransactionMetrics metrics;
    const auto started = std::chrono::steady_clock::now();
    const auto finish = [&](bool result) {
        metrics.totalMilliseconds =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started)
                .count();
        if (metricsOutput != nullptr) {
            *metricsOutput = metrics;
        }
        return result;
    };
    if (targetPath.empty() || !validator) {
        metrics.error = "transaction requires target and validator";
        return finish(false);
    }

    const std::string pending = pendingPath(targetPath);
    const std::string quarantine = quarantinePath(targetPath);
    std::error_code pendingError;
    const bool pendingExists = fs::exists(pending, pendingError);
    if (pendingError) {
        metrics.error = "cannot inspect pending candidate: " +
                        pendingError.message();
        return finish(false);
    }
    if (pendingExists) {
        StorageTransactionMetrics staleMetrics;
        staleMetrics.error = "stale pending candidate";
        if (!movePendingToQuarantine(pending, quarantine, staleMetrics)) {
            metrics.error = staleMetrics.error;
            return finish(false);
        }
    }

    std::FILE *file = nullptr;
#if defined(_WIN32)
    if (_wfopen_s(&file, fs::path(pending).wstring().c_str(), L"wb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(pending.c_str(), "wb");
#endif
    if (file == nullptr) {
        metrics.error = "cannot open sibling pending candidate";
        return finish(false);
    }

    const auto failOpenCandidate = [&](const std::string &fallback) {
        if (metrics.error.empty()) {
            metrics.error = fallback;
        }
        std::fclose(file);
        movePendingToQuarantine(pending, quarantine, metrics);
        return finish(false);
    };

    if (injected(options, StorageFaultPoint::BeforeWrite, metrics)) {
        return failOpenCandidate("failure before write");
    }

    std::size_t writeSize = payload.size();
    if (options.faultPoint == StorageFaultPoint::MidWrite &&
        !payload.empty()) {
        writeSize = std::max<std::size_t>(1, payload.size() / 2);
    }
    metrics.bytesWritten =
        std::fwrite(payload.data(), 1, writeSize, file);
    if (metrics.bytesWritten != writeSize) {
        return failOpenCandidate("candidate write failed");
    }
    if (injected(options, StorageFaultPoint::MidWrite, metrics)) {
        return failOpenCandidate("failure during write");
    }
    if (injected(options, StorageFaultPoint::BeforeFlush, metrics)) {
        return failOpenCandidate("failure before flush");
    }
    if (!durableFlush(file)) {
        return failOpenCandidate("durable candidate flush failed");
    }
    metrics.durablyFlushed = true;
    if (std::fclose(file) != 0) {
        file = nullptr;
        metrics.error = "candidate close failed";
        movePendingToQuarantine(pending, quarantine, metrics);
        return finish(false);
    }
    file = nullptr;

    if (injected(options, StorageFaultPoint::BeforeValidation, metrics)) {
        movePendingToQuarantine(pending, quarantine, metrics);
        return finish(false);
    }
    std::string validationError;
    if (!validator(pending, validationError)) {
        metrics.error = validationError.empty()
                            ? "candidate validation failed"
                            : "candidate validation failed: " +
                                  validationError;
        movePendingToQuarantine(pending, quarantine, metrics);
        return finish(false);
    }
    metrics.candidateValidated = true;

    if (injected(options, StorageFaultPoint::BeforeReplace, metrics)) {
        movePendingToQuarantine(pending, quarantine, metrics);
        return finish(false);
    }
    if (!replacePublished(pending, targetPath, metrics.error)) {
        movePendingToQuarantine(pending, quarantine, metrics);
        return finish(false);
    }
    metrics.published = true;
    return finish(true);
}

std::string StorageTransaction::pendingPath(const std::string &targetPath)
{
    return targetPath + ".pending";
}

std::string StorageTransaction::quarantinePath(const std::string &targetPath)
{
    return targetPath + ".failed";
}
