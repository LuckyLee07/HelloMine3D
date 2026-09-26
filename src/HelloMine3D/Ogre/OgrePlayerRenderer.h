#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "../Item/Material.h"
#include "../Presentation/PlayerAvatarPresentation.h"

namespace Ogre
{
    class ManualObject;
    class SceneManager;
    class SceneNode;
}

struct OgrePlayerRendererValidation
{
    bool valid = false;
    std::size_t partCount = 0;
    const char* message = "not validated";
};

// Owns only the Ogre representation of the local player. Simulation, camera,
// input, Player/World lifetime, and save state remain with their existing
// owners. All eight part objects and the single held-item object are allocated
// once and reused by sync(). Held geometry is rebuilt only when its material
// identity changes.
class OgrePlayerRenderer
{
  public:
    static constexpr const char* MaterialName =
        "HelloMine3D/ActorPlayer";
    static constexpr const char* HeldMaterialName =
        "HelloMine3D/PlayerHeld";
    static constexpr const char* HeldTransparentMaterialName =
        "HelloMine3D/PlayerHeldTransparent";
    static constexpr float PlayerSurfaceMarker = -1.f;

    explicit OgrePlayerRenderer(Ogre::SceneManager& sceneManager);
    ~OgrePlayerRenderer();

    OgrePlayerRenderer(const OgrePlayerRenderer&) = delete;
    OgrePlayerRenderer& operator=(const OgrePlayerRenderer&) = delete;

    void sync(const PlayerAvatarPresentation::Profile& profile,
              const PlayerAvatarPresentation::Pose& pose, bool visible,
              Material::ID heldMaterial = Material::Nothing);
    void setVisible(bool visible) noexcept;
    void setCastShadows(bool enabled) noexcept;
    // Updates only the local player's material instances, never shared terrain
    // or other actors, and does not invalidate held-item geometry.
    void setLighting(float exposure);
    void clear();

    static OgrePlayerRendererValidation validate(
        const PlayerAvatarPresentation::Profile& profile,
        const PlayerAvatarPresentation::Pose& pose) noexcept;

  private:
    struct PartVisual
    {
        Ogre::ManualObject* object = nullptr;
        Ogre::SceneNode* node = nullptr;
    };

    struct HeldItemVisual
    {
        Ogre::ManualObject* object = nullptr;
        Ogre::SceneNode* node = nullptr;
        Material::ID material = Material::Nothing;
        bool geometryAvailable = false;
        bool geometryInitialized = false;
    };

    void createVisuals();
    void rebuildHeldItem(Material::ID material);
    void syncHeldItem(const PlayerAvatarPresentation::Profile& profile,
                      const PlayerAvatarPresentation::Pose& pose,
                      Material::ID material);

    Ogre::SceneManager* m_sceneManager = nullptr;
    Ogre::SceneNode* m_rootNode = nullptr;
    std::array<PartVisual,
               PlayerAvatarPresentation::Profile::MaximumParts>
        m_parts{};
    HeldItemVisual m_heldItem;
    std::string m_baseName;
    std::size_t m_activePartCount = 0;
    bool m_visible = false;
    bool m_castShadows = false;
};
