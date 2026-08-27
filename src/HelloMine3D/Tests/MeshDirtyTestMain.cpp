#include "../World/Chunk/ChunkUpdatePlanner.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string toString(const ChunkUpdateKey &key)
    {
        std::ostringstream out;
        out << "(" << key.x << "," << key.y << "," << key.z << ")";
        return out.str();
    }

    bool expectPlan(const char *name, int blockX, int blockY, int blockZ,
                    const std::vector<ChunkUpdateKey> &expected)
    {
        const auto actual =
            ChunkUpdatePlanner::planForBlockEdit(blockX, blockY, blockZ);
        if (actual == expected) {
            return true;
        }

        std::cerr << name << " failed\nexpected:";
        for (const auto &key : expected) {
            std::cerr << " " << toString(key);
        }

        std::cerr << "\nactual:";
        for (const auto &key : actual) {
            std::cerr << " " << toString(key);
        }
        std::cerr << "\n";
        return false;
    }
}

int main()
{
    bool ok = true;

    ok &= expectPlan("interior block", 5, 18, 7, {{0, 1, 0}});
    ok &= expectPlan("positive x boundary", 15, 18, 7,
                     {{0, 1, 0}, {1, 1, 0}});
    ok &= expectPlan("negative x boundary", -16, 18, 7,
                     {{-1, 1, 0}, {-2, 1, 0}});
    ok &= expectPlan("positive z boundary", 5, 18, 15,
                     {{0, 1, 0}, {0, 1, 1}});
    ok &= expectPlan("negative z boundary", 5, 18, -16,
                     {{0, 1, -1}, {0, 1, -2}});
    ok &= expectPlan("section bottom", 5, 16, 7,
                     {{0, 1, 0}, {0, 0, 0}});
    ok &= expectPlan("section top", 5, 31, 7,
                     {{0, 1, 0}, {0, 2, 0}});
    ok &= expectPlan("corner boundaries", 15, 31, -16,
                     {{0, 1, -1}, {1, 1, -1}, {0, 2, -1}, {1, 2, -1},
                      {0, 1, -2}, {1, 1, -2}, {0, 2, -2}, {1, 2, -2}});
    ok &= expectPlan("horizontal AO corner", 16, 18, 16,
                     {{1, 1, 1}, {0, 1, 1}, {1, 1, 0}, {0, 1, 0}});
    ok &= expectPlan("negative y ignored", 5, -1, 7, {});

    if (!ok) {
        return EXIT_FAILURE;
    }

    std::cout << "Mesh dirty planner tests passed.\n";
    return EXIT_SUCCESS;
}
