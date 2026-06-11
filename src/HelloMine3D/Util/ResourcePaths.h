#pragma once

#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace ResourcePaths
{
    inline bool exists(const std::string& path)
    {
#if defined(_WIN32)
        return _access(path.c_str(), 0) == 0;
#else
        return access(path.c_str(), F_OK) == 0;
#endif
    }

    inline std::string parentPath(const std::string& path)
    {
        const auto pos = path.find_last_of("\\/");
        if (pos == std::string::npos)
        {
            return path;
        }

        return path.substr(0, pos);
    }

    inline std::string join(const std::string& lhs, const std::string& rhs)
    {
        if (lhs.empty())
        {
            return rhs;
        }

        const char last = lhs[lhs.size() - 1];
        if (last == '\\' || last == '/')
        {
            return lhs + rhs;
        }

        return lhs + "\\" + rhs;
    }

    inline std::string currentPath()
    {
        char buffer[4096] = {};
#if defined(_WIN32)
        if (_getcwd(buffer, sizeof(buffer)) == nullptr)
#else
        if (getcwd(buffer, sizeof(buffer)) == nullptr)
#endif
        {
            return ".";
        }

        return buffer;
    }

    inline std::string projectRoot()
    {
        const char* root = std::getenv("HELLOMINE3D_ROOT");
        if (root != nullptr && *root != '\0')
        {
            return root;
        }

        std::string current = currentPath();
        for (int i = 0; i < 4; ++i)
        {
            if (exists(join(current, "media")) && exists(join(current, "bin")))
            {
                return current;
            }

            const auto parent = parentPath(current);
            if (parent == current)
            {
                break;
            }
            current = parent;
        }

        return currentPath();
    }

    inline std::string media(const std::string& relative)
    {
        return join(join(projectRoot(), "media"), relative);
    }

    inline std::string bin(const std::string& relative)
    {
        return join(join(projectRoot(), "bin"), relative);
    }
} // namespace ResourcePaths
