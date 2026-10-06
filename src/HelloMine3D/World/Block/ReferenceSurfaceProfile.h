#pragma once
#include <array>
#include <cstdint>
#include <string>
#include "TerrainMaterialProfile.h"

class ResourcePackResolver;

/// Independent versioned interface; legacy terrain profiles v1/v2 stay strict.
/// Three complete arrays share one semantic layer/mip map and one pack owner.
struct ReferenceSurfaceProfile
{
    static constexpr const char *LogicalPath = "media/materials/Reference.surface-material";
    static constexpr unsigned Edge = 64, Layers = 256, Mips = 7;
    std::array<std::string, 3> textures;
    std::array<std::uint64_t, 3> contentHashes{};
};
ReferenceSurfaceProfile loadReferenceSurfaceProfile(
    const std::string &path, const TerrainResourceResolver &resolve);
class RuntimeReferenceSurfaceProfile
{
  public:
    void freezeFromResourceView(const ResourcePackResolver &resolver);
    const ReferenceSurfaceProfile &parameters() const noexcept { return m_profile; }
    bool frozen() const noexcept { return m_frozen; }
    bool available() const noexcept { return m_available; }
    bool usableWithEffectiveTerrain() const noexcept { return m_available && !m_legacyOverride; }
    const std::string &selectionReason() const noexcept { return m_selectionReason; }
  private:
    ReferenceSurfaceProfile m_profile;
    bool m_frozen = false;
    bool m_available = false;
    bool m_legacyOverride = false;
    std::string m_selectionReason = "not-frozen";
};
RuntimeReferenceSurfaceProfile &runtimeReferenceSurfaceProfile();
