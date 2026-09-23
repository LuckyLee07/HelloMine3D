#include "../../src/HelloMine3D/World/Exploration/ExplorationMarkers.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* label)
{
    if (!condition) {
        throw std::runtime_error(label);
    }
}
}

int main()
{
    try {
        using Result = ExplorationMarkers::Result;
        using Kind = ExplorationMarkers::Kind;
        ExplorationMarkers markers;
        std::uint32_t first = 0;
        require(markers.create(-27, 40, "基地", Kind::Home, &first) ==
                    Result::Created && first == 1 &&
                    markers.home()->id == first,
                "localized home marker was not created");
        require(markers.rename(first, "河岸营地") == Result::Changed &&
                    markers.move(first, -31, 44) == Result::Changed &&
                    markers.home()->worldX == -31 &&
                    markers.home()->name == "河岸营地",
                "rename or move lost marker identity");
        require(markers.track(first) == Result::Changed &&
                    markers.tracked()->id == first &&
                    markers.track(first) == Result::Unchanged,
                "tracking was not stable");
        std::uint32_t second = 0;
        require(markers.create(8, -8, "Waystone", Kind::Note, &second) ==
                    Result::Created &&
                    markers.setHome(second) == Result::Changed &&
                    markers.home()->id == second &&
                    markers.find(first)->kind == Kind::Note &&
                    markers.tracked()->id == first,
                "home reassignment changed tracking or duplicated home");
        require(!ExplorationMarkers::validName(" bad") &&
                    !ExplorationMarkers::validName("bad ") &&
                    !ExplorationMarkers::validName("\xc0\xaf") &&
                    !ExplorationMarkers::validName("\xed\xa0\x80") &&
                    !ExplorationMarkers::validName(
                        std::string(25, 'a')) &&
                    markers.rename(second, "\xc0\xaf") == Result::Invalid &&
                    markers.home()->name == "Waystone",
                "invalid or oversized UTF-8 marker name was accepted");
        require(markers.erase(first) == Result::Removed &&
                    markers.tracked() == nullptr &&
                    markers.trackedId() == 0 &&
                    markers.erase(first) == Result::Missing,
                "marker deletion left stale tracking");
        while (markers.size() < ExplorationMarkers::Capacity) {
            require(markers.create(static_cast<int>(markers.size()), 0,
                                   "Point") == Result::Created,
                    "capacity fill failed before limit");
        }
        const auto count = markers.size();
        require(markers.create(100, 0, "Overflow") == Result::Full &&
                    markers.size() == count && markers.home()->id == second,
                "full marker set discarded existing history");
        markers.clear();
        std::uint32_t afterClear = 0;
        require(markers.size() == 0 && markers.home() == nullptr &&
                    markers.tracked() == nullptr &&
                    markers.create(0, 0, "Start", Kind::Note,
                                   &afterClear) == Result::Created &&
                    afterClear == 1,
                "world boundary did not clear marker identity");
        std::cout << "[EXPLORATION_MARKERS] checks=8 status=PASS\n";
        return 0;
    }
    catch (const std::exception& exception) {
        std::cerr << "[EXPLORATION_MARKERS] FAIL "
                  << exception.what() << '\n';
        return 1;
    }
}
