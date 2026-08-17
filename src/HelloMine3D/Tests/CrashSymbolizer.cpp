#include "CrashSymbolizer.h"

#include "../Diagnostics/CrashSidecar.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

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
        fs::u8path(sidecar.moduleName).wstring().c_str(), 0,
        sidecar.moduleImageSize, nullptr, 0);
    if (moduleBase == 0)
    {
        std::cerr << "[CRASH_SYMBOLIZER] status=ERROR "
                     "error=module-load-failed\n";
    }
    else
    {
        SYMBOL_INFO_PACKAGE symbol{};
        symbol.si.SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol.si.MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        const DWORD64 address = moduleBase + sidecar.symbolProbeRva;
        if (SymFromAddr(process, address, &displacement, &symbol.si) == FALSE)
        {
            std::cerr << "[CRASH_SYMBOLIZER] status=ERROR "
                         "error=project-frame-unresolved\n";
        }
        else
        {
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
            std::cout << "[CRASH_SYMBOLIZER] status=PASS build_identity="
                      << sidecar.buildIdentity << " frame=" << symbol.si.Name
                      << " displacement=" << displacement << " source="
                      << source << ':' << lineNumber << '\n';
            result = 0;
        }
    }
    SymCleanup(process);
    return result;
#endif
}
