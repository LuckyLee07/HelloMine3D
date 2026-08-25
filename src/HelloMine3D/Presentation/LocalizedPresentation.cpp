#include "LocalizedPresentation.h"

#include "LocalizedTextRegistry.h"

namespace
{
    std::string materialKeyPart(Material::ID id)
    {
        const std::string stableId = Material::toStringId(id);
        const std::size_t separator = stableId.find(':');
        return separator == std::string::npos
                   ? stableId
                   : stableId.substr(separator + 1);
    }
}

std::string LocalizedPresentation::text(
    const std::string& locale, const std::string& key,
    const std::string& fallback)
{
    const LocalizedTextRegistry& registry = runtimeLocalizedTextRegistry();
    if (!registry.hasKey(locale, key) && !registry.hasKey("en-US", key) &&
        !fallback.empty())
    {
        (void)registry.lookup(locale, key);
        return fallback;
    }
    return registry.lookup(locale, key);
}

std::string LocalizedPresentation::materialName(
    const std::string& locale, Material::ID id)
{
    const Material& material = Material::toMaterial(id);
    return text(locale, "material." + materialKeyPart(id) + ".name",
                material.name);
}

std::string LocalizedPresentation::objectiveText(
    const std::string& locale, const std::string& objectiveId,
    const char* field, const std::string& fallback)
{
    if (objectiveId.empty())
    {
        return fallback;
    }
    return text(locale, "objective." + objectiveId + "." + field,
                fallback);
}

std::string LocalizedPresentation::audioCaption(
    const std::string& locale, const std::string& cueId,
    const std::string& fallback)
{
    return text(locale, "audio." + cueId + ".caption", fallback);
}
