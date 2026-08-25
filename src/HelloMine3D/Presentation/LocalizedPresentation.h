#pragma once

#include <string>

#include "../Item/Material.h"

namespace LocalizedPresentation
{
    std::string text(const std::string& locale, const std::string& key,
                     const std::string& fallback = {});
    std::string materialName(const std::string& locale, Material::ID id);
    std::string objectiveText(const std::string& locale,
                              const std::string& objectiveId,
                              const char* field,
                              const std::string& fallback = {});
    std::string audioCaption(const std::string& locale,
                             const std::string& cueId,
                             const std::string& fallback = {});
}
