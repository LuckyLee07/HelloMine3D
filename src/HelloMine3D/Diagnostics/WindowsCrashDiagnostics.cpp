#include "CrashDiagnosticsPlatform.h"
#include "CrashSidecar.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>

#include <exception>
#include <filesystem>
#include <iomanip>
#include <limits>
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
    std::wstring g_sidecarPath;
    std::string g_dumpFile;
    std::string g_moduleName;
    std::string g_pdbGuid;
    std::uint32_t g_pdbAge = 0;
    std::uint32_t g_moduleTimestamp = 0;
    std::uint32_t g_moduleImageSize = 0;
    HANDLE g_dumpRequested = nullptr;
    HANDLE g_dumpComplete = nullptr;
    HANDLE g_dumpThread = nullptr;
    EXCEPTION_POINTERS* g_exceptionPointers = nullptr;
    DWORD g_crashingThreadId = 0;
    DWORD g_exceptionCode = 0;
    std::uint64_t g_exceptionRva = 0;
    std::uint64_t g_symbolProbeRva = 0;
    volatile LONG g_handlingException = 0;

    struct CodeViewRecord
    {
        DWORD signature;
        GUID guid;
        DWORD age;
    };

    std::string compactGuid(const GUID& guid)
    {
        std::ostringstream output;
        output << std::hex << std::nouppercase << std::setfill('0')
               << std::setw(8) << guid.Data1 << std::setw(4) << guid.Data2
               << std::setw(4) << guid.Data3;
        for (unsigned char byte : guid.Data4)
        {
            output << std::setw(2) << static_cast<unsigned>(byte);
        }
        return output.str();
    }

    bool readModuleIdentity(std::string& error)
    {
        HMODULE module = GetModuleHandleW(nullptr);
        if (module == nullptr)
        {
            error = "Unable to locate the executable module.";
            return false;
        }

        const auto base = reinterpret_cast<const unsigned char*>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        {
            error = "Executable DOS header is invalid.";
            return false;
        }
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
            base + static_cast<std::size_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.SizeOfImage == 0)
        {
            error = "Executable PE header is invalid.";
            return false;
        }

        g_moduleTimestamp = nt->FileHeader.TimeDateStamp;
        g_moduleImageSize = nt->OptionalHeader.SizeOfImage;
        const IMAGE_DATA_DIRECTORY debugDirectory =
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
        if (debugDirectory.VirtualAddress == 0 ||
            debugDirectory.Size < sizeof(IMAGE_DEBUG_DIRECTORY) ||
            debugDirectory.VirtualAddress >= g_moduleImageSize ||
            debugDirectory.Size >
                g_moduleImageSize - debugDirectory.VirtualAddress)
        {
            error = "Executable has no bounded debug directory.";
            return false;
        }

        const auto* records = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(
            base + debugDirectory.VirtualAddress);
        const std::size_t count =
            debugDirectory.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
        constexpr DWORD RsdsSignature = 0x53445352u;
        for (std::size_t index = 0; index < count; ++index)
        {
            const IMAGE_DEBUG_DIRECTORY& record = records[index];
            if (record.Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
                record.AddressOfRawData == 0 ||
                record.SizeOfData < sizeof(CodeViewRecord) ||
                record.AddressOfRawData >= g_moduleImageSize ||
                record.SizeOfData >
                    g_moduleImageSize - record.AddressOfRawData)
            {
                continue;
            }
            const auto* codeView = reinterpret_cast<const CodeViewRecord*>(
                base + record.AddressOfRawData);
            if (codeView->signature == RsdsSignature && codeView->age > 0)
            {
                g_pdbGuid = compactGuid(codeView->guid);
                g_pdbAge = codeView->age;
                break;
            }
        }
        if (g_pdbGuid.empty() || g_pdbAge == 0)
        {
            error = "Executable has no RSDS symbol identity.";
            return false;
        }

        wchar_t modulePath[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(
            module, modulePath, static_cast<DWORD>(MAX_PATH));
        if (length == 0 || length >= MAX_PATH)
        {
            error = "Unable to resolve the executable filename.";
            return false;
        }
        g_moduleName = fs::path(modulePath).filename().u8string();
        return true;
    }

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

    bool writeSidecar()
    {
        try
        {
            CrashSidecar sidecar;
            sidecar.dumpFile = g_dumpFile;
            sidecar.moduleName = g_moduleName;
            sidecar.pdbGuid = g_pdbGuid;
            sidecar.pdbAge = g_pdbAge;
            sidecar.buildIdentity =
                crashBuildIdentity(sidecar.pdbGuid, sidecar.pdbAge);
            sidecar.moduleTimestamp = g_moduleTimestamp;
            sidecar.moduleImageSize = g_moduleImageSize;
            sidecar.exceptionCode = g_exceptionCode;
            sidecar.exceptionRva = g_exceptionRva;
            sidecar.symbolProbeRva = g_symbolProbeRva;
            sidecar.threadId = g_crashingThreadId;
            const std::string payload = serializeCrashSidecar(sidecar);

            HANDLE file = CreateFileW(
                g_sidecarPath.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                return false;
            }
            DWORD written = 0;
            const bool succeeded =
                payload.size() <= std::numeric_limits<DWORD>::max() &&
                WriteFile(file, payload.data(),
                          static_cast<DWORD>(payload.size()), &written,
                          nullptr) == TRUE &&
                written == payload.size() && FlushFileBuffers(file) == TRUE;
            CloseHandle(file);
            if (!succeeded)
            {
                DeleteFileW(g_sidecarPath.c_str());
            }
            return succeeded;
        }
        catch (...)
        {
            return false;
        }
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
            succeeded = succeeded && writeSidecar();
            if (!succeeded)
            {
                DeleteFileW(g_sidecarPath.c_str());
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
        g_exceptionCode = exceptionPointers != nullptr &&
                                  exceptionPointers->ExceptionRecord != nullptr
                              ? exceptionPointers->ExceptionRecord
                                    ->ExceptionCode
                              : 0;
        g_exceptionRva = 0;
        if (exceptionPointers != nullptr &&
            exceptionPointers->ExceptionRecord != nullptr)
        {
            const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(
                GetModuleHandleW(nullptr));
            const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(
                exceptionPointers->ExceptionRecord->ExceptionAddress);
            if (address >= base && address - base < g_moduleImageSize)
            {
                g_exceptionRva = address - base;
            }
        }
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
            const fs::path dumpPath = directory / makeDumpName();
            g_dumpPath = dumpPath.wstring();
            g_sidecarPath =
                (dumpPath.parent_path() /
                 (dumpPath.stem().wstring() + L".crash.txt"))
                    .wstring();
            g_dumpFile = dumpPath.filename().u8string();
            if (g_dumpPath.empty() || g_dumpPath.size() >= MAX_PATH)
            {
                error = "Crash dump path is empty or exceeds the supported "
                        "Windows path limit.";
                return false;
            }
            if (!readModuleIdentity(error))
            {
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
        const std::uintptr_t moduleBase = reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));
        const std::uintptr_t probe = reinterpret_cast<std::uintptr_t>(
            &CrashDiagnosticsPlatform::triggerControlledCrash);
        g_symbolProbeRva = probe >= moduleBase ? probe - moduleBase : 0;
        // Route the deterministic validation exception through the same dump
        // writer with real exception pointers. Windows does not guarantee
        // that an application-raised exception reaches the process-wide
        // unhandled filter before another runtime intercepts it.
        __try
        {
            RaiseException(ControlledCrashExceptionCode,
                           EXCEPTION_NONCONTINUABLE, 0, nullptr);
        }
        __except (topLevelExceptionFilter(GetExceptionInformation()))
        {
        }
        TerminateProcess(GetCurrentProcess(), ControlledCrashExceptionCode);
        for (;;)
        {
        }
    }
}

#endif
