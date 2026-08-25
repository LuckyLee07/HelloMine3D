#include "LocalizedTextRegistry.h"

#include "../Util/ResourcePackResolver.h"
#include "../Util/ResourcePaths.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr const char* Header = "# HelloMine3D localized text v1";
    constexpr const char* DefaultLocale = "en-US";

    std::string trim(const std::string& value)
    {
        std::size_t begin = 0;
        while (begin < value.size() &&
               std::isspace(static_cast<unsigned char>(value[begin])))
        {
            ++begin;
        }
        std::size_t end = value.size();
        while (end > begin &&
               std::isspace(static_cast<unsigned char>(value[end - 1])))
        {
            --end;
        }
        return value.substr(begin, end - begin);
    }

    bool validUtf8(const std::string& value)
    {
        for (std::size_t index = 0; index < value.size();)
        {
            const unsigned char first =
                static_cast<unsigned char>(value[index]);
            std::size_t length = 0;
            std::uint32_t scalar = 0;
            if (first < 0x80u)
            {
                length = 1;
                scalar = first;
            }
            else if (first >= 0xc2u && first <= 0xdfu)
            {
                length = 2;
                scalar = first & 0x1fu;
            }
            else if (first >= 0xe0u && first <= 0xefu)
            {
                length = 3;
                scalar = first & 0x0fu;
            }
            else if (first >= 0xf0u && first <= 0xf4u)
            {
                length = 4;
                scalar = first & 0x07u;
            }
            else
            {
                return false;
            }
            if (index + length > value.size())
            {
                return false;
            }
            for (std::size_t offset = 1; offset < length; ++offset)
            {
                const unsigned char next =
                    static_cast<unsigned char>(value[index + offset]);
                if ((next & 0xc0u) != 0x80u)
                {
                    return false;
                }
                scalar = (scalar << 6u) | (next & 0x3fu);
            }
            if ((length == 2 && scalar < 0x80u) ||
                (length == 3 && scalar < 0x800u) ||
                (length == 4 && scalar < 0x10000u) ||
                scalar > 0x10ffffu ||
                (scalar >= 0xd800u && scalar <= 0xdfffu))
            {
                return false;
            }
            index += length;
        }
        return true;
    }

    struct ParsedCatalogue
    {
        std::string locale;
        std::unordered_map<std::string, std::string> values;
    };

    [[noreturn]] void reject(const LocalizedTextSource& source,
                             std::size_t line,
                             const std::string& message)
    {
        throw std::runtime_error(
            "Invalid localized text resource '" + source.name +
            "' at line " + std::to_string(line) + ": " + message);
    }

    ParsedCatalogue parse(const LocalizedTextSource& source)
    {
        if (source.content.empty() ||
            source.content.size() > LocalizedTextRegistry::MaxSourceBytes)
        {
            throw std::runtime_error("Localized text source '" + source.name +
                                     "' is empty or oversized.");
        }
        std::istringstream input(source.content);
        std::string line;
        std::size_t lineNumber = 0;
        bool headerSeen = false;
        bool localeSeen = false;
        ParsedCatalogue result;
        while (std::getline(input, line))
        {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (lineNumber == 1 && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xefu &&
                static_cast<unsigned char>(line[1]) == 0xbbu &&
                static_cast<unsigned char>(line[2]) == 0xbfu)
            {
                line.erase(0, 3);
            }
            line = trim(line);
            if (line.empty())
            {
                continue;
            }
            if (!headerSeen)
            {
                if (line != Header)
                {
                    reject(source, lineNumber,
                           "unsupported or missing header");
                }
                headerSeen = true;
                continue;
            }
            if (line.front() == '#')
            {
                continue;
            }
            std::istringstream values(line);
            std::string directive;
            values >> directive;
            if (directive == "locale")
            {
                if (localeSeen || !(values >> result.locale) ||
                    !LocalizedTextRegistry::isCanonicalLocale(result.locale))
                {
                    reject(source, lineNumber,
                           "locale is duplicate or non-canonical");
                }
                values >> std::ws;
                if (!values.eof())
                {
                    reject(source, lineNumber,
                           "locale has trailing data");
                }
                localeSeen = true;
                continue;
            }
            if (directive != "text" || !localeSeen)
            {
                reject(source, lineNumber,
                       "expected locale before text entries");
            }
            std::string key;
            std::string text;
            if (!(values >> key >> std::quoted(text)) ||
                !LocalizedTextRegistry::isCanonicalKey(key) ||
                text.empty() ||
                text.size() > LocalizedTextRegistry::MaxTextBytes ||
                !validUtf8(text))
            {
                reject(source, lineNumber,
                       "text key/value is invalid, empty or oversized");
            }
            values >> std::ws;
            if (!values.eof() ||
                !result.values.emplace(std::move(key), std::move(text)).second)
            {
                reject(source, lineNumber,
                       "text entry is duplicate or has trailing data");
            }
            if (result.values.size() >
                LocalizedTextRegistry::MaxKeysPerLocale)
            {
                reject(source, lineNumber, "text entry limit exceeded");
            }
        }
        if (!headerSeen || !localeSeen || result.values.empty())
        {
            reject(source, lineNumber,
                   "header, locale or text entries are missing");
        }
        return result;
    }
}

void LocalizedTextRegistry::freeze(std::vector<LocalizedTextSource> sources,
                                   bool requireKeyParity)
{
    if (m_frozen)
    {
        throw std::runtime_error("Localized text registry is already frozen.");
    }
    if (sources.empty() || sources.size() > MaxLocales)
    {
        throw std::runtime_error("Localized text source count is invalid.");
    }

    std::unordered_map<std::string, Catalogue> catalogues;
    for (const LocalizedTextSource& source : sources)
    {
        ParsedCatalogue parsed = parse(source);
        if (!catalogues.emplace(std::move(parsed.locale),
                                std::move(parsed.values)).second)
        {
            throw std::runtime_error(
                "Duplicate localized text locale in '" + source.name + "'.");
        }
    }
    if (catalogues.find(DefaultLocale) == catalogues.end())
    {
        throw std::runtime_error(
            "Localized text registry requires the en-US fallback locale.");
    }
    if (requireKeyParity)
    {
        const Catalogue& fallback = catalogues.at(DefaultLocale);
        for (const auto& locale : catalogues)
        {
            if (locale.second.size() != fallback.size() ||
                !std::all_of(
                    fallback.begin(), fallback.end(),
                    [&locale](const auto& entry)
                    {
                        return locale.second.find(entry.first) !=
                               locale.second.end();
                    }))
            {
                throw std::runtime_error(
                    "Localized text locale '" + locale.first +
                    "' does not match the en-US semantic key set.");
            }
        }
    }
    m_catalogues = std::move(catalogues);
    m_frozen = true;
}

void LocalizedTextRegistry::freezeFromResourceView(
    const ResourcePackResolver& resolver)
{
    if (!resolver.isFrozen())
    {
        throw std::runtime_error(
            "Localized text loading requires a frozen resource view.");
    }
    std::vector<LocalizedTextSource> sources;
    for (const EffectiveResource& resource : resolver.effectiveResources())
    {
        if (resource.category != "text")
        {
            continue;
        }
        if (!resource.packName.empty())
        {
            throw std::runtime_error(
                "Resource-pack v1 cannot own localized text resource '" +
                resource.logicalPath + "'.");
        }
        std::ifstream input(resource.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) > MaxSourceBytes)
        {
            throw std::runtime_error(
                "Missing, empty or oversized localized text resource '" +
                resource.logicalPath + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size))
        {
            throw std::runtime_error(
                "Unable to read localized text resource '" +
                resource.logicalPath + "'.");
        }
        sources.push_back({resource.logicalPath, std::move(content)});
    }
    freeze(std::move(sources), true);
}

bool LocalizedTextRegistry::isFrozen() const noexcept
{
    return m_frozen;
}

bool LocalizedTextRegistry::hasLocale(const std::string& locale) const noexcept
{
    return m_catalogues.find(locale) != m_catalogues.end();
}

bool LocalizedTextRegistry::hasKey(const std::string& locale,
                                   const std::string& key) const noexcept
{
    const auto catalogue = m_catalogues.find(locale);
    return catalogue != m_catalogues.end() &&
           catalogue->second.find(key) != catalogue->second.end();
}

std::vector<std::string> LocalizedTextRegistry::keys(
    const std::string& locale) const
{
    std::vector<std::string> result;
    const auto catalogue = m_catalogues.find(locale);
    if (catalogue == m_catalogues.end())
    {
        return result;
    }
    result.reserve(catalogue->second.size());
    for (const auto& entry : catalogue->second)
    {
        result.push_back(entry.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::string LocalizedTextRegistry::lookup(
    const std::string& locale, const std::string& key) const
{
    if (!m_frozen)
    {
        throw std::logic_error("Localized text queried before freeze.");
    }
    auto catalogue = m_catalogues.find(locale);
    if (catalogue == m_catalogues.end())
    {
        diagnose("unknown locale '" + locale + "'; using en-US");
        catalogue = m_catalogues.find(DefaultLocale);
    }
    auto found = catalogue->second.find(key);
    if (found != catalogue->second.end())
    {
        return found->second;
    }
    const Catalogue& fallback = m_catalogues.at(DefaultLocale);
    found = fallback.find(key);
    if (found != fallback.end())
    {
        diagnose("missing key '" + key + "' in locale '" + locale +
                 "'; using en-US");
        return found->second;
    }
    diagnose("missing key '" + key + "' in all locales");
    return "[" + key + "]";
}

const std::vector<std::string>&
LocalizedTextRegistry::diagnostics() const noexcept
{
    return m_diagnostics;
}

bool LocalizedTextRegistry::isCanonicalLocale(
    const std::string& value) noexcept
{
    if (value.size() != 5 || value[2] != '-')
    {
        return false;
    }
    return std::islower(static_cast<unsigned char>(value[0])) &&
           std::islower(static_cast<unsigned char>(value[1])) &&
           std::isupper(static_cast<unsigned char>(value[3])) &&
           std::isupper(static_cast<unsigned char>(value[4]));
}

bool LocalizedTextRegistry::isCanonicalKey(
    const std::string& value) noexcept
{
    if (value.empty() || value.size() > 96 || value.front() == '.' ||
        value.back() == '.')
    {
        return false;
    }
    bool segmentStart = true;
    for (unsigned char character : value)
    {
        if (character == '.')
        {
            if (segmentStart)
            {
                return false;
            }
            segmentStart = true;
            continue;
        }
        if (!(std::islower(character) || std::isdigit(character) ||
              (!segmentStart && (character == '_' || character == '-'))))
        {
            return false;
        }
        segmentStart = false;
    }
    return !segmentStart;
}

void LocalizedTextRegistry::diagnose(const std::string& message) const
{
    if (std::find(m_diagnostics.begin(), m_diagnostics.end(), message) !=
        m_diagnostics.end())
    {
        return;
    }
    if (m_diagnostics.size() < MaxDiagnostics)
    {
        m_diagnostics.push_back(message);
    }
}

LocalizedTextRegistry& runtimeLocalizedTextRegistry()
{
    static LocalizedTextRegistry registry;
    return registry;
}

void ensureRuntimeLocalizedTextRegistry()
{
    LocalizedTextRegistry& registry = runtimeLocalizedTextRegistry();
    if (registry.isFrozen())
    {
        return;
    }
    const char* files[] = {"text/en-US.text", "text/zh-CN.text"};
    std::vector<LocalizedTextSource> sources;
    for (const char* relative : files)
    {
        const std::string path = ResourcePaths::media(relative);
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 ||
            static_cast<std::size_t>(input.tellg()) >
                LocalizedTextRegistry::MaxSourceBytes)
        {
            throw std::runtime_error(
                "Missing, empty or oversized localized text resource '" +
                path + "'.");
        }
        const std::streamsize size = input.tellg();
        input.seekg(0, std::ios::beg);
        std::string content(static_cast<std::size_t>(size), '\0');
        if (!input.read(content.data(), size))
        {
            throw std::runtime_error(
                "Unable to read localized text resource '" + path + "'.");
        }
        sources.push_back({path, std::move(content)});
    }
    registry.freeze(std::move(sources), true);
}
