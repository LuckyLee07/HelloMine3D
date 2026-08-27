#pragma once

#include <functional>
#include <string>

class ResourcePackResolver;

struct TerrainMaterialParameters
{
    static constexpr int ContractVersion = 1;
    static constexpr const char *LogicalPath =
        "media/materials/Base.terrain-material";
    static constexpr const char *TerrainShaderLogicalPath =
        "media/ogre/HelloMine3DTerrain.frag";
    static constexpr const char *DefaultAtlasLogicalPath =
        "media/textures/DefaultPack.png";

    std::string atlasTexture = DefaultAtlasLogicalPath;
    int atlasPixels = 256;
    int tilePixels = 16;
    int tilesPerRow = 16;
    float colourSaturation = 0.62f;
    float greenSuppression = 0.22f;
    float greenRedShift = 0.07f;
    float toneGamma = 1.05f;

    bool containsTile(int x, int y) const noexcept;
};

using TerrainResourceResolver =
    std::function<std::string(const std::string &)>;

TerrainMaterialParameters loadTerrainMaterialParameters(
    const std::string &parameterPath,
    const TerrainResourceResolver &resolveResource);

/// Process-wide immutable profile. Headless users keep the compatibility
/// defaults; the Ogre bootstrap freezes the effective resource view before
/// any block database or GPU material is constructed.
class RuntimeTerrainMaterialProfile
{
  public:
    void freezeFromResourceView(const ResourcePackResolver &resolver);
    bool isFrozen() const noexcept;
    const TerrainMaterialParameters &parameters() const noexcept;

  private:
    TerrainMaterialParameters m_parameters;
    bool m_frozen = false;
};

RuntimeTerrainMaterialProfile &runtimeTerrainMaterialProfile();
