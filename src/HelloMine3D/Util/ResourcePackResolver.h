#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct ResourcePackRequirement
{
    std::string category;
    std::string logicalPath;
};

struct ResourcePackInfo
{
    std::string name;
    std::string rootPath;
    int formatVersion = 0;
};

struct EffectiveResource
{
    std::string category;
    std::string logicalPath;
    std::string sourcePath;
    std::string packName;
};

/// Builds one immutable resource view before gameplay and Ogre construction.
/// Directory packs may only replace logical paths already present in the base
/// startup manifest; they cannot add block ids, scripts or native code.
class ResourcePackResolver
{
  public:
    static constexpr int SupportedFormatVersion = 1;

    void freeze(const std::string &projectRoot,
                const std::vector<ResourcePackRequirement> &requirements,
                const std::vector<std::string> &enabledPackReferences);
    void freezeFromEnvironment(
        const std::string &projectRoot,
        const std::vector<ResourcePackRequirement> &requirements);

    bool isFrozen() const noexcept;
    std::string resolve(const std::string &logicalPath) const;
    std::vector<std::string>
    resourceDirectories(const std::string &logicalDirectory) const;

    const std::vector<ResourcePackInfo> &packs() const noexcept;
    const std::vector<EffectiveResource> &effectiveResources() const noexcept;
    std::size_t overrideCount() const noexcept;
    std::string effectiveManifest() const;

  private:
    std::string m_projectRoot;
    std::vector<ResourcePackInfo> m_packs;
    std::vector<EffectiveResource> m_effectiveResources;
    bool m_frozen = false;
};

ResourcePackResolver &runtimeResourcePackResolver();

