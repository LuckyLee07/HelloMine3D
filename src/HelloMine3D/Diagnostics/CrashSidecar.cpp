#include "CrashSidecar.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace
{
    constexpr std::size_t MaxSidecarBytes = 16 * 1024;

    bool fail(std::string* error, const std::string& message) noexcept
    {
        if (error != nullptr)
        {
            *error = message;
        }
        return false;
    }

    bool safeFilename(const std::string& value, const char* suffix)
    {
        if (value.empty() || value == "." || value == ".." ||
            value.size() > 128 || value.find('/') != std::string::npos ||
            value.find('\\') != std::string::npos ||
            value.find(':') != std::string::npos)
        {
            return false;
        }
        if (std::any_of(value.begin(), value.end(), [](char character) {
                const unsigned char byte =
                    static_cast<unsigned char>(character);
                return byte < 0x20u || byte > 0x7eu;
            }))
        {
            return false;
        }
        const std::string ending(suffix);
        return value.size() >= ending.size() &&
               value.compare(value.size() - ending.size(), ending.size(),
                             ending) == 0;
    }

    bool canonicalGuid(const std::string& value)
    {
        return value.size() == 32 &&
               std::all_of(value.begin(), value.end(), [](char character) {
                   return (character >= '0' && character <= '9') ||
                          (character >= 'a' && character <= 'f');
               });
    }

    bool parseUnsigned(const std::string& value, std::uint64_t maximum,
                       std::uint64_t& result)
    {
        if (value.empty() ||
            !std::all_of(value.begin(), value.end(), [](char character) {
                return character >= '0' && character <= '9';
            }))
        {
            return false;
        }
        try
        {
            std::size_t consumed = 0;
            result = std::stoull(value, &consumed, 10);
            return consumed == value.size() && result <= maximum;
        }
        catch (...)
        {
            return false;
        }
    }

    bool parseHex(const std::string& value, std::uint64_t& result)
    {
        if (value.size() < 3 || value[0] != '0' || value[1] != 'x' ||
            !std::all_of(value.begin() + 2, value.end(), [](char character) {
                return (character >= '0' && character <= '9') ||
                       (character >= 'a' && character <= 'f');
            }))
        {
            return false;
        }
        try
        {
            std::size_t consumed = 0;
            result = std::stoull(value.substr(2), &consumed, 16);
            return consumed == value.size() - 2;
        }
        catch (...)
        {
            return false;
        }
    }

    std::string hexValue(std::uint64_t value)
    {
        std::ostringstream output;
        output << "0x" << std::hex << std::nouppercase << value;
        return output.str();
    }
}

std::string crashBuildIdentity(const std::string& pdbGuid,
                               std::uint32_t pdbAge)
{
    return "pdb-" + pdbGuid + '-' + std::to_string(pdbAge);
}

bool validateCrashSidecar(const CrashSidecar& sidecar,
                          std::string* error) noexcept
{
    try
    {
        if (sidecar.schema != CrashSidecar::CurrentSchema)
        {
            return fail(error, "unsupported crash sidecar schema");
        }
        if (!safeFilename(sidecar.dumpFile, ".dmp") ||
            !safeFilename(sidecar.moduleName, ".exe"))
        {
            return fail(error, "sidecar contains an unsafe filename");
        }
        if (!canonicalGuid(sidecar.pdbGuid) || sidecar.pdbAge == 0 ||
            sidecar.buildIdentity !=
                crashBuildIdentity(sidecar.pdbGuid, sidecar.pdbAge))
        {
            return fail(error, "sidecar symbol identity is invalid");
        }
        if (sidecar.moduleTimestamp == 0 ||
            sidecar.moduleImageSize == 0 ||
            sidecar.exceptionCode == 0 || sidecar.symbolProbeRva == 0 ||
            sidecar.symbolProbeRva >= sidecar.moduleImageSize ||
            sidecar.exceptionRva >= sidecar.moduleImageSize ||
            sidecar.threadId == 0 || sidecar.uploadEnabled)
        {
            return fail(error, "sidecar numeric policy is invalid");
        }
        return true;
    }
    catch (...)
    {
        return fail(error, "sidecar validation failed");
    }
}

std::string serializeCrashSidecar(const CrashSidecar& sidecar)
{
    std::string error;
    if (!validateCrashSidecar(sidecar, &error))
    {
        throw std::invalid_argument(error);
    }

    std::ostringstream output;
    output << "schema " << sidecar.schema << '\n'
           << "dump_file " << std::quoted(sidecar.dumpFile) << '\n'
           << "module_name " << std::quoted(sidecar.moduleName) << '\n'
           << "build_identity " << sidecar.buildIdentity << '\n'
           << "module_timestamp " << sidecar.moduleTimestamp << '\n'
           << "module_image_size " << sidecar.moduleImageSize << '\n'
           << "pdb_guid " << sidecar.pdbGuid << '\n'
           << "pdb_age " << sidecar.pdbAge << '\n'
           << "exception_code " << hexValue(sidecar.exceptionCode) << '\n'
           << "exception_rva " << hexValue(sidecar.exceptionRva) << '\n'
           << "symbol_probe_rva " << hexValue(sidecar.symbolProbeRva) << '\n'
           << "thread_id " << sidecar.threadId << '\n'
           << "upload_enabled " << (sidecar.uploadEnabled ? 1 : 0) << '\n';
    return output.str();
}

bool parseCrashSidecar(const std::string& text, CrashSidecar& sidecar,
                       std::string* error) noexcept
{
    try
    {
        if (text.empty() || text.size() > MaxSidecarBytes)
        {
            return fail(error, "crash sidecar size is invalid");
        }

        CrashSidecar loaded;
        std::set<std::string> fields;
        std::istringstream input(text);
        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty())
            {
                continue;
            }
            std::istringstream record(line);
            std::string key;
            if (!(record >> key) || !fields.emplace(key).second)
            {
                return fail(error, "duplicate or empty sidecar field");
            }

            std::string value;
            if (key == "dump_file" || key == "module_name")
            {
                if (!(record >> std::quoted(value)))
                {
                    return fail(error, "invalid quoted sidecar field");
                }
            }
            else if (!(record >> value))
            {
                return fail(error, "missing sidecar field value");
            }
            record >> std::ws;
            if (!record.eof())
            {
                return fail(error, "trailing sidecar field data");
            }

            std::uint64_t number = 0;
            if (key == "schema")
            {
                if (!parseUnsigned(value, std::numeric_limits<int>::max(),
                                   number))
                    return fail(error, "invalid schema");
                loaded.schema = static_cast<int>(number);
            }
            else if (key == "dump_file") loaded.dumpFile = value;
            else if (key == "module_name") loaded.moduleName = value;
            else if (key == "build_identity") loaded.buildIdentity = value;
            else if (key == "pdb_guid") loaded.pdbGuid = value;
            else if (key == "module_timestamp" ||
                     key == "module_image_size" || key == "pdb_age" ||
                     key == "thread_id" || key == "upload_enabled")
            {
                if (!parseUnsigned(
                        value, std::numeric_limits<std::uint32_t>::max(),
                        number))
                    return fail(error, "invalid decimal sidecar field");
                if (key == "module_timestamp")
                    loaded.moduleTimestamp = static_cast<std::uint32_t>(number);
                else if (key == "module_image_size")
                    loaded.moduleImageSize = static_cast<std::uint32_t>(number);
                else if (key == "pdb_age")
                    loaded.pdbAge = static_cast<std::uint32_t>(number);
                else if (key == "thread_id")
                    loaded.threadId = static_cast<std::uint32_t>(number);
                else if (number <= 1)
                    loaded.uploadEnabled = number != 0;
                else
                    return fail(error, "invalid upload policy");
            }
            else if (key == "exception_code" || key == "exception_rva" ||
                     key == "symbol_probe_rva")
            {
                if (!parseHex(value, number))
                    return fail(error, "invalid hexadecimal sidecar field");
                if (key == "exception_code")
                {
                    if (number > std::numeric_limits<std::uint32_t>::max())
                        return fail(error, "exception code is out of range");
                    loaded.exceptionCode =
                        static_cast<std::uint32_t>(number);
                }
                else if (key == "exception_rva")
                    loaded.exceptionRva = number;
                else
                    loaded.symbolProbeRva = number;
            }
            else
            {
                return fail(error, "unknown crash sidecar field");
            }
        }

        if (fields.size() != 13 || !validateCrashSidecar(loaded, error))
        {
            return fields.size() == 13
                       ? false
                       : fail(error, "crash sidecar field set is incomplete");
        }
        sidecar = std::move(loaded);
        return true;
    }
    catch (const std::exception& exception)
    {
        return fail(error, exception.what());
    }
    catch (...)
    {
        return fail(error, "unknown crash sidecar parse failure");
    }
}
