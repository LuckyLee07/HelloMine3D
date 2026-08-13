#include "ResourcePackResolver.h"
#include "ResourcePaths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
    namespace fs = std::filesystem;

    constexpr const char *PackHeader = "# HelloMine3D resource pack v1";

    std::string trim(const std::string &value)
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

    std::string lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char character)
                       {
                           return static_cast<char>(std::tolower(character));
                       });
        return value;
    }

    std::string generic(const fs::path &value)
    {
        return value.generic_string();
    }

    bool isValidPackName(const std::string &name)
    {
        if (name.empty() || name.size() > 80)
        {
            return false;
        }
        return std::all_of(name.begin(), name.end(), [](unsigned char value)
        {
            return std::isalnum(value) || value == ' ' || value == '_' ||
                   value == '-' || value == '.';
        });
    }

    bool isAllowedCategory(const std::string &category)
    {
        static const std::set<std::string> allowed = {
            "block", "font", "resource-script", "shader", "shape",
            "texture"};
        return allowed.find(category) != allowed.end();
    }

    bool isCanonicalLogicalPath(const std::string &logicalPath)
    {
        if (logicalPath.empty() || logicalPath.front() == '/' ||
            logicalPath.front() == '\\' ||
            logicalPath.find('\\') != std::string::npos ||
            logicalPath.find(':') != std::string::npos)
        {
            return false;
        }
        const fs::path parsed(logicalPath);
        if (parsed.is_absolute())
        {
            return false;
        }
        for (const fs::path &part : parsed)
        {
            if (part == "." || part == ".." || part.empty())
            {
                return false;
            }
        }
        return generic(parsed.lexically_normal()) == logicalPath;
    }

    bool isWithin(const fs::path &root, const fs::path &candidate)
    {
        const std::string rootText = lower(generic(root));
        const std::string candidateText = lower(generic(candidate));
        if (candidateText == rootText)
        {
            return true;
        }
        return candidateText.size() > rootText.size() &&
               candidateText.compare(0, rootText.size(), rootText) == 0 &&
               candidateText[rootText.size()] == '/';
    }

    std::vector<std::string> splitPackList(const std::string &value)
    {
        std::vector<std::string> result;
        std::stringstream input(value);
        std::string item;
        while (std::getline(input, item, ';'))
        {
            item = trim(item);
            if (!item.empty())
            {
                result.push_back(item);
            }
        }
        return result;
    }

    std::vector<std::string> configuredPackReferences(
        const std::string &projectRoot)
    {
        const char *overrideValue = std::getenv("HELLOMINE3D_RESOURCE_PACKS");
        if (overrideValue != nullptr)
        {
            return splitPackList(overrideValue);
        }

        const fs::path config =
            fs::path(projectRoot) / "bin" / "resource-packs.txt";
        std::ifstream input(config);
        if (!input)
        {
            throw std::runtime_error(
                "Missing resource-pack configuration: expected '" +
                generic(config) + "'.");
        }

        std::vector<std::string> result;
        std::string line;
        while (std::getline(input, line))
        {
            line = trim(line);
            if (!line.empty() && line.front() != '#')
            {
                result.push_back(line);
            }
        }
        return result;
    }

    fs::path resolvePackRoot(const fs::path &projectRoot,
                             const std::string &reference)
    {
        fs::path candidate(reference);
        if (!candidate.is_absolute())
        {
            if (reference.find('/') != std::string::npos ||
                reference.find('\\') != std::string::npos ||
                reference == "." || reference == "..")
            {
                throw std::runtime_error(
                    "Invalid resource-pack reference '" + reference +
                    "': configured names must not contain path separators or traversal.");
            }
            candidate = projectRoot / "packs" / candidate;
        }
        std::error_code error;
        if (fs::is_symlink(fs::symlink_status(candidate, error)))
        {
            throw std::runtime_error(
                "Resource-pack roots must not be symlinks: '" +
                generic(candidate) + "'.");
        }
        error.clear();
        const fs::path canonical = fs::weakly_canonical(candidate, error);
        if (error || !fs::is_directory(canonical))
        {
            throw std::runtime_error(
                "Missing resource-pack directory for reference '" +
                reference + "': expected '" + generic(candidate) + "'.");
        }
        return canonical;
    }

    ResourcePackInfo loadPackInfo(const fs::path &packRoot)
    {
        const fs::path metadataPath = packRoot / "pack.meta";
        std::ifstream input(metadataPath);
        if (!input)
        {
            throw std::runtime_error(
                "Missing resource-pack metadata: expected '" +
                generic(metadataPath) + "'.");
        }

        std::string header;
        std::getline(input, header);
        if (!header.empty() && header.back() == '\r')
        {
            header.pop_back();
        }
        if (header != PackHeader)
        {
            throw std::runtime_error(
                "Invalid resource-pack metadata '" + generic(metadataPath) +
                "': unsupported or missing header.");
        }

        std::map<std::string, std::string> fields;
        std::string line;
        while (std::getline(input, line))
        {
            line = trim(line);
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            const std::size_t separator = line.find('=');
            if (separator == std::string::npos || separator == 0 ||
                separator + 1 >= line.size())
            {
                throw std::runtime_error(
                    "Invalid resource-pack metadata '" +
                    generic(metadataPath) + "': expected key=value.");
            }
            const std::string key = trim(line.substr(0, separator));
            const std::string value = trim(line.substr(separator + 1));
            if ((key != "name" && key != "format") || value.empty() ||
                !fields.emplace(key, value).second)
            {
                throw std::runtime_error(
                    "Invalid resource-pack metadata '" +
                    generic(metadataPath) + "': unknown, empty or duplicate key '" +
                    key + "'.");
            }
        }
        if (!isValidPackName(fields["name"]))
        {
            throw std::runtime_error(
                "Invalid resource-pack name in '" + generic(metadataPath) +
                "'.");
        }
        int format = 0;
        try
        {
            std::size_t consumed = 0;
            format = std::stoi(fields["format"], &consumed);
            if (consumed != fields["format"].size())
            {
                format = 0;
            }
        }
        catch (...)
        {
            format = 0;
        }
        if (format != ResourcePackResolver::SupportedFormatVersion)
        {
            throw std::runtime_error(
                "Incompatible resource-pack format for '" + fields["name"] +
                "': got " + fields["format"] + ", expected 1.");
        }
        return {fields["name"], generic(packRoot), format};
    }

    using OverrideMap = std::unordered_map<std::string, std::string>;

    OverrideMap scanPack(
        const ResourcePackInfo &pack,
        const std::unordered_map<std::string, ResourcePackRequirement>
            &requirements)
    {
        const fs::path packRoot(pack.rootPath);
        const fs::path canonicalRoot = fs::canonical(packRoot);
        OverrideMap overrides;
        std::set<std::string> caseInsensitivePaths;
        for (fs::recursive_directory_iterator iterator(packRoot), end;
             iterator != end; ++iterator)
        {
            const fs::directory_entry &entry = *iterator;
            if (entry.is_symlink())
            {
                throw std::runtime_error(
                    "Resource pack '" + pack.name +
                    "' contains a forbidden symlink: '" +
                    generic(entry.path()) + "'.");
            }
            if (!entry.is_regular_file())
            {
                continue;
            }
            const fs::path relative =
                fs::relative(entry.path(), packRoot).lexically_normal();
            const std::string logicalPath = generic(relative);
            if (logicalPath == "pack.meta")
            {
                continue;
            }
            if (!isCanonicalLogicalPath(logicalPath))
            {
                throw std::runtime_error(
                    "Resource pack '" + pack.name +
                    "' contains a non-canonical logical path '" +
                    logicalPath + "'.");
            }
            const fs::path canonicalSource = fs::canonical(entry.path());
            if (!isWithin(canonicalRoot, canonicalSource))
            {
                throw std::runtime_error(
                    "Resource pack '" + pack.name +
                    "' escapes its root through '" + logicalPath + "'.");
            }
            const auto requirement = requirements.find(logicalPath);
            if (requirement == requirements.end() ||
                !isAllowedCategory(requirement->second.category))
            {
                throw std::runtime_error(
                    "Resource pack '" + pack.name +
                    "' contains stale or unsupported override '" +
                    logicalPath + "'.");
            }
            if (!caseInsensitivePaths.insert(lower(logicalPath)).second)
            {
                throw std::runtime_error(
                    "Resource pack '" + pack.name +
                    "' contains a duplicate logical path '" + logicalPath +
                    "'.");
            }
            if (entry.file_size() == 0)
            {
                throw std::runtime_error(
                    "Resource pack '" + pack.name + "' override '" +
                    logicalPath + "' is empty at '" +
                    generic(canonicalSource) + "'.");
            }
            overrides.emplace(logicalPath, generic(canonicalSource));
        }
        return overrides;
    }
}

void ResourcePackResolver::freeze(
    const std::string &projectRoot,
    const std::vector<ResourcePackRequirement> &requirements,
    const std::vector<std::string> &enabledPackReferences)
{
    if (m_frozen)
    {
        throw std::runtime_error(
            "The effective resource view is already frozen for this process.");
    }
    if (requirements.empty())
    {
        throw std::runtime_error(
            "Cannot freeze an empty effective resource view.");
    }

    const fs::path canonicalProjectRoot = fs::weakly_canonical(projectRoot);
    m_projectRoot = generic(canonicalProjectRoot);
    std::unordered_map<std::string, ResourcePackRequirement> byPath;
    std::set<std::string> caseInsensitivePaths;
    for (const ResourcePackRequirement &requirement : requirements)
    {
        if (!isCanonicalLogicalPath(requirement.logicalPath) ||
            !caseInsensitivePaths.insert(lower(requirement.logicalPath)).second ||
            !byPath.emplace(requirement.logicalPath, requirement).second)
        {
            throw std::runtime_error(
                "Invalid or duplicate base logical resource path '" +
                requirement.logicalPath + "'.");
        }
    }

    std::vector<OverrideMap> packOverrides;
    std::set<std::string> packNames;
    for (const std::string &reference : enabledPackReferences)
    {
        const fs::path packRoot =
            resolvePackRoot(canonicalProjectRoot, reference);
        ResourcePackInfo info = loadPackInfo(packRoot);
        if (!packNames.insert(lower(info.name)).second)
        {
            throw std::runtime_error(
                "Duplicate enabled resource-pack name '" + info.name + "'.");
        }
        packOverrides.push_back(scanPack(info, byPath));
        m_packs.push_back(std::move(info));
    }

    std::vector<ResourcePackRequirement> sorted = requirements;
    std::sort(sorted.begin(), sorted.end(),
              [](const ResourcePackRequirement &left,
                 const ResourcePackRequirement &right)
              {
                  return left.category + "|" + left.logicalPath <
                         right.category + "|" + right.logicalPath;
              });
    for (const ResourcePackRequirement &requirement : sorted)
    {
        EffectiveResource effective{
            requirement.category,
            requirement.logicalPath,
            generic(canonicalProjectRoot / fs::path(requirement.logicalPath)),
            ""};
        for (std::size_t index = 0; index < packOverrides.size(); ++index)
        {
            const auto found =
                packOverrides[index].find(requirement.logicalPath);
            if (found != packOverrides[index].end())
            {
                effective.sourcePath = found->second;
                effective.packName = m_packs[index].name;
                break;
            }
        }
        std::ifstream input(effective.sourcePath,
                            std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0)
        {
            const std::string owner = effective.packName.empty()
                                          ? "base resources"
                                          : "resource pack '" +
                                                effective.packName + "'";
            throw std::runtime_error(
                "Missing or empty effective " + requirement.category +
                " resource '" + requirement.logicalPath + "' from " + owner +
                " at '" + effective.sourcePath + "'.");
        }
        m_effectiveResources.push_back(std::move(effective));
    }
    m_frozen = true;
}

void ResourcePackResolver::freezeFromEnvironment(
    const std::string &projectRoot,
    const std::vector<ResourcePackRequirement> &requirements)
{
    freeze(projectRoot, requirements,
           configuredPackReferences(projectRoot));
}

bool ResourcePackResolver::isFrozen() const noexcept
{
    return m_frozen;
}

std::string ResourcePackResolver::resolve(
    const std::string &logicalPath) const
{
    if (!isCanonicalLogicalPath(logicalPath))
    {
        throw std::runtime_error(
            "Invalid logical resource path '" + logicalPath + "'.");
    }
    if (!m_frozen)
    {
        const fs::path root = m_projectRoot.empty()
                                  ? fs::path(ResourcePaths::projectRoot())
                                  : fs::path(m_projectRoot);
        return generic(root / fs::path(logicalPath));
    }
    const auto found = std::find_if(
        m_effectiveResources.begin(), m_effectiveResources.end(),
        [&logicalPath](const EffectiveResource &resource)
        {
            return resource.logicalPath == logicalPath;
        });
    if (found == m_effectiveResources.end())
    {
        throw std::runtime_error(
            "Logical resource path is outside the frozen manifest: '" +
            logicalPath + "'.");
    }
    return found->sourcePath;
}

std::vector<std::string> ResourcePackResolver::resourceDirectories(
    const std::string &logicalDirectory) const
{
    if (!m_frozen)
    {
        throw std::runtime_error(
            "Resource directories requested before the effective view was frozen.");
    }
    const std::string prefix = logicalDirectory + "/";
    std::vector<std::string> result;
    const bool hasBase = std::any_of(
        m_effectiveResources.begin(), m_effectiveResources.end(),
        [&prefix](const EffectiveResource &resource)
        {
            return resource.packName.empty() &&
                   resource.logicalPath.rfind(prefix, 0) == 0;
        });
    if (hasBase)
    {
        result.push_back(generic(fs::path(m_projectRoot) /
                                 fs::path(logicalDirectory)));
    }

    // Ogre's resource index gives later locations precedence. Add packs from
    // lowest to highest priority so the first enabled pack wins exactly as it
    // does for directly resolved block and shape files.
    for (auto pack = m_packs.rbegin(); pack != m_packs.rend(); ++pack)
    {
        const bool selected = std::any_of(
            m_effectiveResources.begin(), m_effectiveResources.end(),
            [&prefix, &pack](const EffectiveResource &resource)
            {
                return resource.packName == pack->name &&
                       resource.logicalPath.rfind(prefix, 0) == 0;
            });
        if (selected)
        {
            result.push_back(generic(fs::path(pack->rootPath) /
                                     fs::path(logicalDirectory)));
        }
    }
    return result;
}

const std::vector<ResourcePackInfo> &ResourcePackResolver::packs() const noexcept
{
    return m_packs;
}

const std::vector<EffectiveResource> &
ResourcePackResolver::effectiveResources() const noexcept
{
    return m_effectiveResources;
}

std::size_t ResourcePackResolver::overrideCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(
        m_effectiveResources.begin(), m_effectiveResources.end(),
        [](const EffectiveResource &resource)
        {
            return !resource.packName.empty();
        }));
}

std::string ResourcePackResolver::effectiveManifest() const
{
    if (!m_frozen)
    {
        throw std::runtime_error(
            "Effective resource manifest requested before freeze.");
    }
    std::ostringstream output;
    output << "# HelloMine3D effective resource manifest v1\n";
    for (const EffectiveResource &resource : m_effectiveResources)
    {
        output << resource.category << '|' << resource.logicalPath << '|'
               << (resource.packName.empty() ? "base" : resource.packName)
               << '\n';
    }
    return output.str();
}

ResourcePackResolver &runtimeResourcePackResolver()
{
    static ResourcePackResolver resolver;
    return resolver;
}
