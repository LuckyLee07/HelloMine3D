#pragma once

#include <cstdint>
#include <string>

struct CrashSidecar
{
    static constexpr int CurrentSchema = 1;

    int schema = CurrentSchema;
    std::string dumpFile;
    std::string moduleName;
    std::string buildIdentity;
    std::uint32_t moduleTimestamp = 0;
    std::uint32_t moduleImageSize = 0;
    std::string pdbGuid;
    std::uint32_t pdbAge = 0;
    std::uint32_t exceptionCode = 0;
    std::uint64_t exceptionRva = 0;
    std::uint64_t symbolProbeRva = 0;
    std::uint32_t threadId = 0;
    bool uploadEnabled = false;
};

std::string crashBuildIdentity(const std::string& pdbGuid,
                               std::uint32_t pdbAge);

bool validateCrashSidecar(const CrashSidecar& sidecar,
                          std::string* error = nullptr) noexcept;

std::string serializeCrashSidecar(const CrashSidecar& sidecar);

bool parseCrashSidecar(const std::string& text, CrashSidecar& sidecar,
                       std::string* error = nullptr) noexcept;
