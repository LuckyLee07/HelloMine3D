#pragma once

#include "../Config.h"
#include "../Item/Material.h"
#include "../Presentation/PlayerAvatarPresentation.h"
#include "../Presentation/ThirdPersonCameraPresentation.h"

class Camera;
class OgrePlayerRenderer;
class Player;
class World;

namespace Ogre
{
    class Camera;
}

// Ordinary client presentation boundary. Bootstrap retains frame ownership,
// selection priority, UI calls, diagnostic sweeps and avatar pose history.
// These functions neither own nor retain any Camera, Player or World pointer.
namespace OgreThirdPersonCameraRig
{
    enum class NearClipStatus
    {
        Nominal,
        Reduced,
        NoWorld,
        InvalidInput,
        BudgetExhausted,
        EyeEmbedded,
        InsufficientClearance
    };

    // Adapter facts extend the pure pose without changing its contract.
    // collisionQueries includes nearClipQueries; unresolved never means safe.
    struct CameraPose : ThirdPersonCameraPresentation::Pose
    {
        float nominalNearClipDistance = .1f;
        float renderNearClipDistance = .1f;
        std::size_t nearClipQueries = 0;
        bool nearClipSafetyUnresolved = false;
        NearClipStatus nearClipStatus = NearClipStatus::Nominal;
    };

    // Projection support and the third-person sweep always use nominal near.
    // Actual Ogre near is updated after the pure pose, immediately even at dt0;
    // authoritative Camera/Player are read-only. Pose apply remains separate.
    CameraPose updateCameraPose(
        const ::Camera& logicCamera, Ogre::Camera& renderCamera,
        float nominalNearClipDistance, World* world, const Player* player,
        CameraPerspective requestedPerspective,
        ThirdPersonCameraPresentation::State& state, float deltaSeconds,
        const glm::vec3* selectedHitPoint = nullptr);

    // Also used after the existing diagnostic sweep. Preserve Ogre's original
    // identity/yaw/pitch composition, including its sign convention.
    void applyCameraPose(Ogre::Camera& renderCamera,
                         const glm::vec3& position,
                         const glm::vec3& rotation);

    struct AvatarVisibility
    {
        bool avatarVisible = false;
        bool firstPersonHandVisible = true;
    };

    // Effective mode includes the existing transient near-wall fallback.
    AvatarVisibility visibilityForCamera(
        ThirdPersonCameraPresentation::Mode effectiveMode,
        bool diagnosticSweepEnabled) noexcept;

    // Bootstrap supplies its already-derived/smoothed pose and actual held ID.
    // This only delegates visibility and pose to the existing renderer; it does
    // not add a held-item collision rule or change its joint/geometry policy.
    void syncAvatarForCamera(
        OgrePlayerRenderer& renderer,
        const PlayerAvatarPresentation::Profile& profile,
        const PlayerAvatarPresentation::Pose& pose,
        ThirdPersonCameraPresentation::Mode effectiveMode,
        bool diagnosticSweepEnabled,
        Material::ID heldMaterial = Material::Nothing);
}
