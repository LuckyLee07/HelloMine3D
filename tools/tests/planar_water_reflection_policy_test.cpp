#include "../../src/HelloMine3D/Ogre/PlanarWaterReflection.h"
#include <climits>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void require(bool value, const char* message) {
    ++checks; if (!value) throw std::runtime_error(message);
}
}
int main() {
    try {
        using R = PlanarWaterReflection;
        const auto normal = R::targetSize(1280, 720), retina = R::targetSize(2560, 1440);
        require(normal.width == 640 && normal.height == 360, "Point-scale half size incorrect");
        require(retina.width == 1280 && retina.height == 720, "Physical Retina framebuffer was not halved");
        const auto odd = R::targetSize(1281, 721);
        require(odd.width == 641 && odd.height == 361, "Odd physical dimensions must cover all pixels");
        require(bool(R::targetSize(3840,2160)) && R::targetSize(3840,2160).pixels() == R::MaximumPixels, "Exact pixel budget rejected");
        require(!R::targetSize(3841,2161), "Over-budget physical size allowed");
        require(!R::targetSize(INT_MAX,INT_MAX), "Integer overflow permitted giant target");
        require(!R::targetSize(0,720) && !R::targetSize(-1,720) && !R::targetSize(1280,0), "Zero/negative target allowed");
        require(R::targetSize(1,1).pixels() == 1, "Small valid viewport rejected");
        require(R::excludes("HelloMine3D/Water",80), "Water queue8 was not rejected");
        require(!R::excludes("HelloMine3D/Transparent",80), "Glass sharing water queue8 was rejected");
        require(!R::excludes("HelloMine3D/Flora",60), "Resident flora was rejected");
        require(!R::excludes("HelloMine3D/Terrain",50), "Resident terrain was rejected");
        require(!R::excludes("HelloMine3D/ActorPlayer",50), "Third-person resident actor was rejected");
        require(!R::excludes("WorldSkyBoxPlane2",5), "Scene sky was rejected");
        require(R::excludes("HelloMine3D/BlockSurfaceFeedback",90), "Operation surface overlay leaked");
        require(R::excludes("HelloMine3D/FloraSurfaceFeedback",90), "Flora operation overlay leaked");
        require(R::excludes("HelloMine3D/BlockFragments",90), "Operation particles leaked");
        require(R::excludes("AnyHudMaterial",100) && R::excludes("UnknownHud",255), "UI queue leaked");
        require(R::MaximumPrivateMaterials <= 96 && R::MaximumPrivatePasses <= 384, "Private parameter cache is unbounded");
        std::cout << "[PLANAR_POLICY] checks=" << checks << " failures=0 scope=production-target-and-exclusion-policy\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[PLANAR_POLICY] " << error.what() << '\n'; return 1;
    }
}
