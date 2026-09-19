#include "World/Block/WetlandGrassGeometry.h"
#include <iostream>
#include <stdexcept>

int main()
{
    try {
        using namespace WetlandGrassGeometry;
        int checks = 0;
        const auto require = [&](bool ok, const char *label) {
            if (!ok) throw std::runtime_error(label);
            ++checks;
        };
        BlockShape cross{"Cross", {{0,0,0, 1,0,1, 1,1,1, 0,1,0},
                                    {0,0,1, 1,0,0, 1,1,0, 0,1,1}}};
        require(applies(BlockId::TallGrass, TerrainBiome::Wetland, cross), "wetland tall grass enabled");
        for (int biome = 0; biome <= 7; ++biome)
            require(applies(BlockId::TallGrass, static_cast<TerrainBiome>(biome), cross) == (biome == 6), "other biomes preserved");
        require(!applies(BlockId::WheatCrop, TerrainBiome::Wetland, cross), "crop preserved");
        cross.faces[0][0] = .1f;
        require(!applies(BlockId::TallGrass, TerrainBiome::Wetland, cross), "custom cross preserved");
        for (unsigned variant = 0; variant < 3; ++variant) {
            for (bool mature : {false, true}) {
                const auto model = build(variant, mature, 1.f);
                const auto half = build(variant, mature, .5f);
                require(model.count == (mature ? 8u : 6u) && model.count <= MaxFaces, "bounded geometry");
                for (std::size_t f = 0; f < model.count; ++f) {
                    const auto &face = model.faces[f];
                    const auto point = [&](int v) { return glm::vec3(face.positions[v*3], face.positions[v*3+1], face.positions[v*3+2]); };
                    require(glm::length(glm::cross(point(1)-point(0),point(2)-point(0))) > .00001f &&
                            glm::length(glm::cross(point(2)-point(0),point(3)-point(0))) > .00001f, "nondegenerate triangles");
                    for (std::size_t v = 0; v < 4; ++v) {
                        for (std::size_t axis = 0; axis < 3; ++axis) {
                            const float value = face.positions[v*3+axis];
                            require(std::isfinite(value) && value >= 0.f && value <= 1.f, "inside block bounds");
                        }
                        require(std::abs(face.repeat[v*2+1] - (1.f-face.positions[v*3+1])) < .000001f, "height-consistent wind weight");
                        require(half.faces[f].positions[v*3+1] == face.positions[v*3+1]*.5f &&
                                half.faces[f].repeat == face.repeat, "metadata scale keeps root and texture");
                    }
                    require(face.seedHead == (mature && f >= 6), "seed-head maturity");
                }
                if (mature)
                    require(model.faces[4].positions[7] == model.faces[6].positions[1] &&
                            model.faces[4].repeat[5] == model.faces[6].repeat[1], "stem to seed-head joint shares height and bend");
            }
        }
        require(build(0,true,1.f).faces[0].positions != build(1,true,1.f).faces[0].positions &&
                build(1,true,1.f).faces[0].positions != build(2,true,1.f).faces[0].positions, "three distinct silhouettes");
        std::cout << "PASS " << checks << " wetland grass geometry assertions\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
