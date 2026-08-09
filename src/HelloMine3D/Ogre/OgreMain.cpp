#include "OgreBootstrap.h"

#include <cstdlib>
#include <string>

namespace
{
    bool isTruthy(const char* value)
    {
        if (value == nullptr)
        {
            return false;
        }

        const std::string text(value);
        return text == "1" || text == "true" || text == "TRUE" ||
               text == "on" || text == "ON";
    }
}

int main()
{
    return runOgreBootstrap(
        isTruthy(std::getenv("HELLOMINE3D_OGRE_VALIDATE_ONLY")));
}
