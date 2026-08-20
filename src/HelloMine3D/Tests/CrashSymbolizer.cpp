#include "CrashSymbolizer.h"

#include "../Diagnostics/CrashSidecar.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#endif

namespace
{
    namespace fs = std::filesystem;

    bool optionValue(int argc, char** argv, const std::string& name,
                     std::string& value)
    {
        for (int index = 1; index + 1 < argc; ++index)
        {
            if (argv[index] == name)
            {
                value = argv[index + 1];
                return !value.empty();
            }
        }
        return false;
    }

#if defined(_WIN32)
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

    bool readSymbolIdentity(const fs::path& path,
                            SYMSRV_INDEX_INFOW& identity,
                            std::string& error)
    {
        identity = {};
        identity.sizeofstruct = sizeof(identity);
        if (SymSrvGetFileIndexInfoW(path.wstring().c_str(), &identity, 0) ==
            FALSE)
        {
            error = "cannot read image or PDB identity";
            return false;
        }
        return true;
    }

    struct DumpView
    {
        const unsigned char* bytes = nullptr;
        std::size_t size = 0;
        MINIDUMP_MEMORY_LIST* memoryList = nullptr;
        MINIDUMP_MEMORY64_LIST* memory64List = nullptr;
        MINIDUMP_MODULE_LIST* modules = nullptr;
        MINIDUMP_THREAD_LIST* threads = nullptr;

        bool contains(std::uint64_t offset, std::uint64_t length) const
        {
            return offset <= size && length <= size - offset;
        }

        bool copyMemory(DWORD64 address, void* destination,
                        DWORD requested, DWORD& copied) const
        {
            copied = 0;
            unsigned char* output =
                static_cast<unsigned char*>(destination);
            while (copied < requested)
            {
                bool found = false;
                if (memoryList != nullptr)
                {
                    for (ULONG32 index = 0;
                         index < memoryList->NumberOfMemoryRanges; ++index)
                    {
                        const MINIDUMP_MEMORY_DESCRIPTOR& range =
                            memoryList->MemoryRanges[index];
                        const DWORD64 begin = range.StartOfMemoryRange;
                        const DWORD64 length = range.Memory.DataSize;
                        const DWORD64 current = address + copied;
                        if (current < begin || current >= begin + length)
                        {
                            continue;
                        }
                        const DWORD64 offset = current - begin;
                        const DWORD available = static_cast<DWORD>(
                            std::min<DWORD64>(length - offset,
                                              requested - copied));
                        if (!contains(range.Memory.Rva + offset, available))
                        {
                            return false;
                        }
                        std::memcpy(output + copied,
                                    bytes + range.Memory.Rva + offset,
                                    available);
                        copied += available;
                        found = true;
                        break;
                    }
                }
                if (!found && memory64List != nullptr)
                {
                    ULONG64 fileOffset = memory64List->BaseRva;
                    for (ULONG64 index = 0;
                         index < memory64List->NumberOfMemoryRanges; ++index)
                    {
                        const MINIDUMP_MEMORY_DESCRIPTOR64& range =
                            memory64List->MemoryRanges[index];
                        const DWORD64 begin = range.StartOfMemoryRange;
                        const DWORD64 current = address + copied;
                        if (current >= begin &&
                            current < begin + range.DataSize)
                        {
                            const DWORD64 offset = current - begin;
                            const DWORD available = static_cast<DWORD>(
                                std::min<DWORD64>(range.DataSize - offset,
                                                  requested - copied));
                            if (!contains(fileOffset + offset, available))
                            {
                                return false;
                            }
                            std::memcpy(output + copied,
                                        bytes + fileOffset + offset,
                                        available);
                            copied += available;
                            found = true;
                            break;
                        }
                        fileOffset += range.DataSize;
                    }
                }
                if (!found)
                {
                    break;
                }
            }
            return copied > 0;
        }

        DWORD64 moduleBase(DWORD64 address) const
        {
            if (modules == nullptr)
            {
                return 0;
            }
            for (ULONG32 index = 0; index < modules->NumberOfModules;
                 ++index)
            {
                const MINIDUMP_MODULE& module = modules->Modules[index];
                if (address >= module.BaseOfImage &&
                    address < module.BaseOfImage + module.SizeOfImage)
                {
                    return module.BaseOfImage;
                }
            }
            return 0;
        }

        std::wstring minidumpString(RVA rva) const
        {
            if (!contains(rva, sizeof(ULONG32)))
            {
                return {};
            }
            const MINIDUMP_STRING* value =
                reinterpret_cast<const MINIDUMP_STRING*>(bytes + rva);
            if (value->Length % sizeof(wchar_t) != 0 ||
                !contains(rva + offsetof(MINIDUMP_STRING, Buffer),
                          value->Length))
            {
                return {};
            }
            return std::wstring(value->Buffer,
                                value->Length / sizeof(wchar_t));
        }
    };

    DumpView* g_dumpView = nullptr;
    constexpr ULONG FunctionSymbolTag = 5;

    BOOL CALLBACK readDumpMemory(HANDLE, DWORD64 address, PVOID buffer,
                                 DWORD size, LPDWORD bytesRead)
    {
        DWORD copied = 0;
        const bool succeeded = g_dumpView != nullptr &&
            g_dumpView->copyMemory(address, buffer, size, copied);
        if (bytesRead != nullptr)
        {
            *bytesRead = copied;
        }
        return succeeded ? TRUE : FALSE;
    }

    DWORD64 CALLBACK dumpModuleBase(HANDLE, DWORD64 address)
    {
        return g_dumpView == nullptr ? 0 : g_dumpView->moduleBase(address);
    }

    bool sameFilename(const fs::path& path, const std::string& expected)
    {
        std::string actual = path.filename().u8string();
        std::string wanted = expected;
        std::transform(actual.begin(), actual.end(), actual.begin(),
                       [](unsigned char value) {
                           return static_cast<char>(std::tolower(value));
                       });
        std::transform(wanted.begin(), wanted.end(), wanted.begin(),
                       [](unsigned char value) {
                           return static_cast<char>(std::tolower(value));
                       });
        return actual == wanted;
    }

    std::string safeModuleName(const DumpView& dump, DWORD64 address)
    {
        if (dump.modules == nullptr)
        {
            return "unknown";
        }
        for (ULONG32 index = 0; index < dump.modules->NumberOfModules;
             ++index)
        {
            const MINIDUMP_MODULE& module = dump.modules->Modules[index];
            if (address >= module.BaseOfImage &&
                address < module.BaseOfImage + module.SizeOfImage)
            {
                const std::wstring path =
                    dump.minidumpString(module.ModuleNameRva);
                return path.empty()
                           ? "unknown"
                           : fs::path(path).filename().u8string();
            }
        }
        return "unknown";
    }
#endif
}

int runCrashSymbolizerCommand(int argc, char** argv)
{
    std::string sidecarText;
    std::string imageText;
    std::string pdbText;
    if (!optionValue(argc, argv, "--sidecar", sidecarText) ||
        !optionValue(argc, argv, "--image", imageText) ||
        !optionValue(argc, argv, "--pdb", pdbText))
    {
        std::cerr << "Usage: --symbolize --sidecar <file> --image <exe> "
                     "--pdb <file>\n";
        return 2;
    }

    const fs::path sidecarPath = fs::u8path(sidecarText);
    const fs::path imagePath = fs::u8path(imageText);
    const fs::path pdbPath = fs::u8path(pdbText);
    std::ifstream input(sidecarPath, std::ios::binary);
    const std::string payload((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    CrashSidecar sidecar;
    std::string error;
    if (!input.is_open() || !parseCrashSidecar(payload, sidecar, &error))
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID error=" << error
                  << '\n';
        return 2;
    }
    if (!fs::is_regular_file(imagePath) || !fs::is_regular_file(pdbPath))
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=image-or-pdb-missing\n";
        return 2;
    }
    const fs::path dumpPath = sidecarPath.parent_path() /
                              fs::u8path(sidecar.dumpFile);
    if (!fs::is_regular_file(dumpPath))
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=minidump-missing\n";
        return 2;
    }

#if !defined(_WIN32)
    std::cerr << "[CRASH_SYMBOLIZER] status=UNSUPPORTED "
                 "error=windows-dbghelp-required\n";
    return 5;
#else
    SYMSRV_INDEX_INFOW imageIdentity{};
    SYMSRV_INDEX_INFOW pdbIdentity{};
    if (!readSymbolIdentity(imagePath, imageIdentity, error) ||
        !readSymbolIdentity(pdbPath, pdbIdentity, error))
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID error=" << error
                  << '\n';
        return 2;
    }
    const std::string imageGuid = compactGuid(imageIdentity.guid);
    const std::string pdbGuid = compactGuid(pdbIdentity.guid);
    const bool imageMatches =
        imageIdentity.timestamp == sidecar.moduleTimestamp &&
        imageIdentity.size == sidecar.moduleImageSize &&
        imageGuid == sidecar.pdbGuid &&
        imageIdentity.age == sidecar.pdbAge;
    const bool pdbMatches = pdbGuid == sidecar.pdbGuid &&
                            pdbIdentity.age == sidecar.pdbAge;
    if (!imageMatches || !pdbMatches)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=MISMATCH "
                     "error=symbol-identity-mismatch expected="
                  << sidecar.buildIdentity << '\n';
        return 3;
    }

    std::ifstream dumpInput(dumpPath, std::ios::binary);
    std::vector<unsigned char> dumpBytes;
    dumpBytes.assign(std::istreambuf_iterator<char>(dumpInput),
                     std::istreambuf_iterator<char>());
    if (!dumpInput.is_open() || dumpBytes.empty() ||
        dumpBytes.size() > 512u * 1024u * 1024u)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=minidump-size-invalid\n";
        return 2;
    }

    DumpView dump;
    dump.bytes = dumpBytes.data();
    dump.size = dumpBytes.size();
    ULONG streamSize = 0;
    PVOID stream = nullptr;
    if (MiniDumpReadDumpStream(dumpBytes.data(), ModuleListStream,
                               nullptr, &stream, &streamSize) == FALSE ||
        stream == nullptr)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=module-list-missing\n";
        return 2;
    }
    dump.modules = static_cast<MINIDUMP_MODULE_LIST*>(stream);
    stream = nullptr;
    if (MiniDumpReadDumpStream(dumpBytes.data(), ThreadListStream,
                               nullptr, &stream, &streamSize) == FALSE ||
        stream == nullptr)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=thread-list-missing\n";
        return 2;
    }
    dump.threads = static_cast<MINIDUMP_THREAD_LIST*>(stream);
    stream = nullptr;
    if (MiniDumpReadDumpStream(dumpBytes.data(), MemoryListStream,
                               nullptr, &stream, &streamSize) == TRUE)
    {
        dump.memoryList = static_cast<MINIDUMP_MEMORY_LIST*>(stream);
    }
    stream = nullptr;
    if (MiniDumpReadDumpStream(dumpBytes.data(), Memory64ListStream,
                               nullptr, &stream, &streamSize) == TRUE)
    {
        dump.memory64List = static_cast<MINIDUMP_MEMORY64_LIST*>(stream);
    }
    stream = nullptr;
    if (MiniDumpReadDumpStream(dumpBytes.data(), ExceptionStream,
                               nullptr, &stream, &streamSize) == FALSE ||
        stream == nullptr)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=exception-stream-missing\n";
        return 2;
    }
    const MINIDUMP_EXCEPTION_STREAM* exceptionStream =
        static_cast<const MINIDUMP_EXCEPTION_STREAM*>(stream);
    if (exceptionStream->ThreadContext.DataSize < sizeof(CONTEXT) ||
        !dump.contains(exceptionStream->ThreadContext.Rva,
                       sizeof(CONTEXT)))
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=exception-context-invalid\n";
        return 2;
    }
    CONTEXT context{};
    std::memcpy(&context,
                dump.bytes + exceptionStream->ThreadContext.Rva,
                sizeof(context));

    DWORD64 targetBase = 0;
    for (ULONG32 index = 0; index < dump.modules->NumberOfModules; ++index)
    {
        const MINIDUMP_MODULE& module = dump.modules->Modules[index];
        if (sameFilename(fs::path(
                             dump.minidumpString(module.ModuleNameRva)),
                         sidecar.moduleName))
        {
            targetBase = module.BaseOfImage;
            break;
        }
    }
    if (targetBase == 0)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=INVALID "
                     "error=project-module-missing\n";
        return 2;
    }

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
                  SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
    const std::wstring searchPath = pdbPath.parent_path().wstring();
    if (SymInitializeW(process, searchPath.c_str(), FALSE) == FALSE)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=ERROR "
                     "error=symbol-engine-init-failed\n";
        return 4;
    }

    int result = 4;
    const DWORD64 moduleBase = SymLoadModuleExW(
        process, nullptr, imagePath.wstring().c_str(),
        fs::u8path(sidecar.moduleName).wstring().c_str(), targetBase,
        sidecar.moduleImageSize, nullptr, 0);
    if (moduleBase == 0)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=ERROR "
                     "error=module-load-failed\n";
    }
    else
    {
        for (ULONG32 index = 0; index < dump.modules->NumberOfModules;
             ++index)
        {
            const MINIDUMP_MODULE& module = dump.modules->Modules[index];
            if (module.BaseOfImage == targetBase)
            {
                continue;
            }
            const std::wstring modulePath =
                dump.minidumpString(module.ModuleNameRva);
            if (!modulePath.empty() && fs::is_regular_file(modulePath))
            {
                SymLoadModuleExW(process, nullptr, modulePath.c_str(),
                                 fs::path(modulePath).filename().c_str(),
                                 module.BaseOfImage, module.SizeOfImage,
                                 nullptr, 0);
            }
        }

        STACKFRAME64 frame{};
#if defined(_M_X64)
        const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset = context.Rip;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86)
        const DWORD machine = IMAGE_FILE_MACHINE_I386;
        frame.AddrPC.Offset = context.Eip;
        frame.AddrFrame.Offset = context.Ebp;
        frame.AddrStack.Offset = context.Esp;
#else
#error Unsupported Windows symbolizer architecture
#endif
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;

        g_dumpView = &dump;
        std::size_t frameCount = 0;
        std::size_t projectFrameCount = 0;
        DWORD64 previousAddress = 0;
        std::vector<DWORD64> emittedAddresses;
        for (std::size_t index = 0; index < 64 &&
             frame.AddrPC.Offset != 0; ++index)
        {
            const DWORD64 address = frame.AddrPC.Offset;
            if (address == previousAddress)
            {
                break;
            }
            previousAddress = address;
            emittedAddresses.push_back(address);
            const bool projectFrame =
                address >= targetBase &&
                address < targetBase + sidecar.moduleImageSize;

            SYMBOL_INFO_PACKAGE symbol{};
            symbol.si.SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol.si.MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            std::string symbolName = "unknown";
            if (SymFromAddr(process, address, &displacement,
                            &symbol.si) == TRUE)
            {
                symbolName = symbol.si.Name;
            }
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            std::string source = "unknown";
            DWORD lineNumber = 0;
            if (SymGetLineFromAddr64(process, address, &lineDisplacement,
                                     &line) == TRUE &&
                line.FileName != nullptr)
            {
                source = fs::path(line.FileName).filename().u8string();
                lineNumber = line.LineNumber;
            }
            std::cout << "[CRASH_SYMBOLIZER_FRAME] index=" << frameCount
                      << " module=" << safeModuleName(dump, address)
                      << " address=0x" << std::hex << address << std::dec
                      << " symbol=" << symbolName
                      << " displacement=" << displacement
                      << " source=" << source << ':' << lineNumber
                      << " project=" << (projectFrame ? 1 : 0) << '\n';
            ++frameCount;
            if (projectFrame)
            {
                ++projectFrameCount;
            }

            if (StackWalk64(machine, process, GetCurrentThread(), &frame,
                            &context, readDumpMemory,
                            SymFunctionTableAccess64, dumpModuleBase,
                            nullptr) == FALSE)
            {
                break;
            }
        }

        if (projectFrameCount == 0 && dump.threads != nullptr)
        {
            const MINIDUMP_THREAD* crashingThread = nullptr;
            for (ULONG32 index = 0;
                 index < dump.threads->NumberOfThreads; ++index)
            {
                if (dump.threads->Threads[index].ThreadId ==
                    exceptionStream->ThreadId)
                {
                    crashingThread = &dump.threads->Threads[index];
                    break;
                }
            }
            if (crashingThread != nullptr)
            {
                const MINIDUMP_MEMORY_DESCRIPTOR& stack =
                    crashingThread->Stack;
                const DWORD64 stackBegin = stack.StartOfMemoryRange;
                const DWORD64 stackSize = stack.Memory.DataSize;
#if defined(_M_X64)
                const DWORD64 stackPointer = context.Rsp;
#else
                const DWORD64 stackPointer = context.Esp;
#endif
                const DWORD64 scanBegin =
                    std::max<DWORD64>(stackBegin, stackPointer);
                const DWORD64 skipped = scanBegin - stackBegin;
                if (skipped < stackSize &&
                    dump.contains(stack.Memory.Rva + skipped,
                                  stackSize - skipped))
                {
                    const DWORD64 scanBytes = std::min<DWORD64>(
                        stackSize - skipped, 1024u * 1024u);
                    for (DWORD64 offset = 0;
                         offset + sizeof(DWORD64) <= scanBytes &&
                         frameCount < 64;
                         offset += sizeof(DWORD64))
                    {
                        DWORD64 address = 0;
                        std::memcpy(
                            &address,
                            dump.bytes + stack.Memory.Rva + skipped + offset,
                            sizeof(address));
                        if (address < targetBase ||
                            address >= targetBase + sidecar.moduleImageSize ||
                            std::find(emittedAddresses.begin(),
                                      emittedAddresses.end(), address) !=
                                emittedAddresses.end())
                        {
                            continue;
                        }

                        SYMBOL_INFO_PACKAGE symbol{};
                        symbol.si.SizeOfStruct = sizeof(SYMBOL_INFO);
                        symbol.si.MaxNameLen = MAX_SYM_NAME;
                        DWORD64 displacement = 0;
                        if (SymFromAddr(process, address, &displacement,
                                        &symbol.si) == FALSE ||
                            symbol.si.Tag != FunctionSymbolTag)
                        {
                            continue;
                        }
                        IMAGEHLP_LINE64 line{};
                        line.SizeOfStruct = sizeof(line);
                        DWORD lineDisplacement = 0;
                        std::string source = "unknown";
                        DWORD lineNumber = 0;
                        if (SymGetLineFromAddr64(
                                process, address, &lineDisplacement,
                                &line) == TRUE && line.FileName != nullptr)
                        {
                            source = fs::path(line.FileName)
                                         .filename().u8string();
                            lineNumber = line.LineNumber;
                        }
                        std::cout
                            << "[CRASH_SYMBOLIZER_FRAME] index="
                            << frameCount << " module=" << sidecar.moduleName
                            << " address=0x" << std::hex << address
                            << std::dec << " symbol=" << symbol.si.Name
                            << " displacement=" << displacement
                            << " source=" << source << ':' << lineNumber
                            << " project=1 unwind=stack-scan\n";
                        emittedAddresses.push_back(address);
                        ++frameCount;
                        ++projectFrameCount;
                    }
                }
            }
        }
        g_dumpView = nullptr;
        if (frameCount > 0 && projectFrameCount > 0)
        {
            std::cout << "[CRASH_SYMBOLIZER] status=PASS "
                         "mode=minidump-stack-hybrid"
                      << " build_identity=" << sidecar.buildIdentity
                      << " exception_thread="
                      << exceptionStream->ThreadId
                      << " frames=" << frameCount
                      << " project_frames=" << projectFrameCount << '\n';
            result = 0;
        }
        else
        {
            std::cerr << "[CRASH_SYMBOLIZER] status=ERROR "
                         "error=project-stack-frame-unresolved\n";
        }
    }
    SymCleanup(process);
    return result;
#endif
}
