#include "CrashDiagnosticsPlatform.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>

#include <exception>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>

namespace
{
    namespace fs = std::filesystem;

    constexpr DWORD ControlledCrashExceptionCode = 0xE0424D33u;
    constexpr DWORD DumpWaitMilliseconds = 15000u;

    std::mutex g_installMutex;
    bool g_installed = false;
    std::wstring g_dumpPath;
    HANDLE g_dumpRequested = nullptr;
    HANDLE g_dumpComplete = nullptr;
    HANDLE g_dumpThread = nullptr;
    EXCEPTION_POINTERS* g_exceptionPointers = nullptr;
    DWORD g_crashingThreadId = 0;
    volatile LONG g_handlingException = 0;

    std::wstring makeDumpName()
    {
        SYSTEMTIME time{};
        GetSystemTime(&time);
        std::wostringstream name;
        name << L"HelloMine3D-" << time.wYear;
        name.width(2);
        name.fill(L'0');
        name << time.wMonth;
        name.width(2);
        name << time.wDay << L'-';
        name.width(2);
        name << time.wHour;
        name.width(2);
        name << time.wMinute;
        name.width(2);
        name << time.wSecond << L'-';
        name.width(3);
        name << time.wMilliseconds << L'-' << GetCurrentProcessId()
             << L".dmp";
        return name.str();
    }

    DWORD WINAPI dumpThreadMain(void*)
    {
        if (WaitForSingleObject(g_dumpRequested, INFINITE) != WAIT_OBJECT_0)
        {
            SetEvent(g_dumpComplete);
            return 1;
        }

        HANDLE file = CreateFileW(
            g_dumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        bool succeeded = false;
        if (file != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION exceptionInformation{};
            exceptionInformation.ThreadId = g_crashingThreadId;
            exceptionInformation.ExceptionPointers = g_exceptionPointers;
            exceptionInformation.ClientPointers = FALSE;

            succeeded = MiniDumpWriteDump(
                            GetCurrentProcess(), GetCurrentProcessId(), file,
                            MiniDumpNormal, &exceptionInformation, nullptr,
                            nullptr) == TRUE;
            LARGE_INTEGER size{};
            succeeded = succeeded && GetFileSizeEx(file, &size) == TRUE &&
                        size.QuadPart > 0;
            CloseHandle(file);
            if (!succeeded)
            {
                DeleteFileW(g_dumpPath.c_str());
            }
        }

        SetEvent(g_dumpComplete);
        return succeeded ? 0 : 1;
    }

    LONG WINAPI topLevelExceptionFilter(
        EXCEPTION_POINTERS* exceptionPointers)
    {
        if (InterlockedCompareExchange(
                &g_handlingException, 1, 0) != 0)
        {
            return EXCEPTION_EXECUTE_HANDLER;
        }

        g_exceptionPointers = exceptionPointers;
        g_crashingThreadId = GetCurrentThreadId();
        MemoryBarrier();
        if (g_dumpRequested != nullptr && g_dumpComplete != nullptr &&
            SetEvent(g_dumpRequested) == TRUE)
        {
            // A bounded wait prevents a failed writer from recursively
            // trapping the crashing process forever.
            WaitForSingleObject(g_dumpComplete, DumpWaitMilliseconds);
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void closeInstallationHandles()
    {
        if (g_dumpThread != nullptr)
        {
            CloseHandle(g_dumpThread);
            g_dumpThread = nullptr;
        }
        if (g_dumpComplete != nullptr)
        {
            CloseHandle(g_dumpComplete);
            g_dumpComplete = nullptr;
        }
        if (g_dumpRequested != nullptr)
        {
            CloseHandle(g_dumpRequested);
            g_dumpRequested = nullptr;
        }
    }
}

namespace CrashDiagnosticsPlatform
{
    const char* backendName() noexcept
    {
        return "windows-dbghelp";
    }

    bool isSupported() noexcept
    {
        return true;
    }

    bool install(const std::string& crashDirectory,
                 std::string& error) noexcept
    {
        std::lock_guard<std::mutex> lock(g_installMutex);
        if (g_installed)
        {
            return true;
        }

        try
        {
            const fs::path directory = fs::u8path(crashDirectory);
            g_dumpPath = (directory / makeDumpName()).wstring();
            if (g_dumpPath.empty() || g_dumpPath.size() >= MAX_PATH)
            {
                error = "Crash dump path is empty or exceeds the supported "
                        "Windows path limit.";
                return false;
            }

            g_dumpRequested = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            g_dumpComplete = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (g_dumpRequested == nullptr || g_dumpComplete == nullptr)
            {
                error = "Unable to create crash-writer synchronization "
                        "events.";
                closeInstallationHandles();
                return false;
            }

            g_dumpThread = CreateThread(
                nullptr, 0, dumpThreadMain, nullptr, 0, nullptr);
            if (g_dumpThread == nullptr)
            {
                error = "Unable to start the dedicated crash-writer thread.";
                closeInstallationHandles();
                return false;
            }

            SetUnhandledExceptionFilter(topLevelExceptionFilter);
            g_installed = true;
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
        }
        catch (...)
        {
            error = "Unknown Windows crash-handler installation failure.";
        }
        closeInstallationHandles();
        return false;
    }

    [[noreturn]] void triggerControlledCrash() noexcept
    {
        RaiseException(ControlledCrashExceptionCode,
                       EXCEPTION_NONCONTINUABLE, 0, nullptr);
        TerminateProcess(GetCurrentProcess(), ControlledCrashExceptionCode);
        for (;;)
        {
        }
    }
}

#endif
