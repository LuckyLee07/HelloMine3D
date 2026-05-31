#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace ResourcePaths
{
    inline std::filesystem::path projectRoot()
    {
        if (const char* root = std::getenv("HELLOMINE3D_ROOT"); root && *root)
        {
            return root;
        }

        auto current = std::filesystem::current_path();
        for (int i = 0; i < 4; ++i)
        {
            if (std::filesystem::exists(current / "media") &&
                std::filesystem::exists(current / "bin"))
            {
                return current;
            }
            current = current.parent_path();
        }

        return std::filesystem::current_path();
    }

    inline std::string media(const std::filesystem::path& relative)
    {
        return (projectRoot() / "media" / relative).string();
    }

    inline std::string bin(const std::filesystem::path& relative)
    {
        return (projectRoot() / "bin" / relative).string();
    }
} // namespace ResourcePaths
