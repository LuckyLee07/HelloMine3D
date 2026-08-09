#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace RuntimeDebugOptions
{
    inline bool isEnabledValue(const char *value)
    {
        if (value == nullptr || value[0] == '\0') {
            return false;
        }

        std::string normalized(value);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::tolower(ch));
                       });
        return normalized != "0" && normalized != "false" &&
               normalized != "off" && normalized != "no";
    }

    inline bool showDebugInfoAtStartup()
    {
        return isEnabledValue(
            std::getenv("HELLOMINE3D_SHOW_DEBUG_INFO"));
    }
} // namespace RuntimeDebugOptions
