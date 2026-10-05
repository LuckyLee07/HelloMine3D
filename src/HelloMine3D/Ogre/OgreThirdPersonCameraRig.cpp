#include "OgreThirdPersonCameraRig.h"

#include "OgrePlayerRenderer.h"
#include "../Core/Camera.h"
#include "../Player/Player.h"
#include "../World/World.h"

#include <OgreCamera.h>
#include <OgreQuaternion.h>

#include <array>
#include <cstdint>
#include <limits>

namespace OgreThirdPersonCameraRig
{
    namespace
    {
        constexpr float MinimumRenderNear = .01f;
        constexpr float MaximumNearPolicyNominal = 1.f;
        constexpr std::size_t MaximumNearVoxelQueries = 128;

        void updateRenderNear(
            CameraPose& pose, Ogre::Camera& camera, World* world,
            const ThirdPersonCameraPresentation::Input& input,
            float nominalNear)
        {
            pose.nominalNearClipDistance = nominalNear;
            auto apply = [&](float near, NearClipStatus status,
                             bool unresolved)
            {
                pose.renderNearClipDistance = near;
                pose.nearClipStatus = status;
                pose.nearClipSafetyUnresolved = unresolved;
                camera.setNearClipDistance(near);
            };
            // Reject malformed source/projection before trig, casts or scans.
            // The nominal .2 unsupported-third control is valid for this cap.
            if (!ThirdPersonCameraPresentation::finite(input.eye) ||
                !ThirdPersonCameraPresentation::projectionSupported(
                    input.verticalFovDegrees, input.aspectRatio,
                    ThirdPersonCameraPresentation::DefaultNearClipDistance) ||
                !std::isfinite(nominalNear) ||
                nominalNear < MinimumRenderNear ||
                nominalNear > MaximumNearPolicyNominal)
            {
                apply(MinimumRenderNear, NearClipStatus::InvalidInput, true);
                return;
            }
            if (pose.effectiveMode ==
                ThirdPersonCameraPresentation::Mode::ThirdPersonRear)
            {
                apply(nominalNear, NearClipStatus::Nominal, false);
                return;
            }
            if (world == nullptr)
            {
                apply(nominalNear, NearClipStatus::NoWorld, true);
                return;
            }

            const double tangent = std::tan(
                static_cast<double>(input.verticalFovDegrees) *
                (3.14159265358979323846 / 360.0));
            const double aspect = input.aspectRatio;
            const double factor = std::sqrt(
                1.0 + tangent * tangent * (1.0 + aspect * aspect));
            const double margin =
                ThirdPersonCameraPresentation::ProjectionSafetyMargin;
            const double radius = nominalNear * factor + margin;
            if (!std::isfinite(factor) || !std::isfinite(radius) ||
                factor <= 0.0)
            {
                apply(MinimumRenderNear, NearClipStatus::InvalidInput, true);
                return;
            }

            const std::array<double, 3> eye{{input.eye.x, input.eye.y,
                                           input.eye.z}};
            std::array<int, 3> minimum{}, maximum{};
            std::size_t cells = 1;
            for (std::size_t axis = 0; axis < eye.size(); ++axis)
            {
                const double low = std::floor(eye[axis] - radius);
                const double high = std::floor(eye[axis] + radius);
                if (!std::isfinite(low) || !std::isfinite(high) ||
                    low < std::numeric_limits<int>::min() ||
                    high > std::numeric_limits<int>::max() - 1.0)
                {
                    apply(MinimumRenderNear,
                          NearClipStatus::InvalidInput, true);
                    return;
                }
                minimum[axis] = static_cast<int>(low);
                maximum[axis] = static_cast<int>(high);
                const auto width = static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(maximum[axis]) -
                    minimum[axis] + 1);
                if (width > MaximumNearVoxelQueries ||
                    cells > MaximumNearVoxelQueries / width)
                {
                    pose.collisionBudgetExhausted = true;
                    apply(MinimumRenderNear,
                          NearClipStatus::BudgetExhausted, true);
                    return;
                }
                cells *= static_cast<std::size_t>(width);
            }
            const std::size_t remaining =
                pose.collisionQueries <
                        ThirdPersonCameraPresentation::MaximumVoxelQueries
                    ? ThirdPersonCameraPresentation::MaximumVoxelQueries -
                        pose.collisionQueries
                    : 0;
            // Never use a partial scan as proof of a complete free sphere.
            if (cells > remaining)
            {
                pose.collisionBudgetExhausted = true;
                apply(MinimumRenderNear,
                      NearClipStatus::BudgetExhausted, true);
                return;
            }

            double closest = radius;
            bool obstruction = false;
            bool embedded = false;
            for (int x = minimum[0]; x <= maximum[0]; ++x)
                for (int y = minimum[1]; y <= maximum[1]; ++y)
                    for (int z = minimum[2]; z <= maximum[2]; ++z)
                    {
                        ++pose.nearClipQueries;
                        ++pose.collisionQueries;
                        if (!world->getBlock(x, y, z).getData().isCollidable)
                            continue;
                        const std::array<double, 3> low{{double(x), double(y),
                                                        double(z)}};
                        double squared = 0.0;
                        bool inside = true;
                        for (std::size_t axis = 0; axis < eye.size(); ++axis)
                        {
                            const double high = low[axis] + 1.0;
                            const double d = std::max(
                                {low[axis] - eye[axis], 0.0,
                                 eye[axis] - high});
                            squared += d * d;
                            inside = inside && eye[axis] > low[axis] &&
                                eye[axis] < high;
                        }
                        embedded = embedded || inside;
                        const double distance = std::sqrt(squared);
                        if (distance < closest)
                        {
                            closest = distance;
                            obstruction = true;
                        }
                    }
            if (embedded)
            {
                apply(MinimumRenderNear, NearClipStatus::EyeEmbedded, true);
                return;
            }
            if (!obstruction)
            {
                apply(nominalNear, NearClipStatus::Nominal, false);
                return;
            }
            const double candidate = std::min(
                double(nominalNear), (closest - margin) / factor);
            if (candidate < MinimumRenderNear)
            {
                apply(MinimumRenderNear,
                      NearClipStatus::InsufficientClearance, true);
                return;
            }
            apply(static_cast<float>(candidate),
                  candidate < nominalNear ? NearClipStatus::Reduced
                                          : NearClipStatus::Nominal,
                  false);
        }
    }

    CameraPose updateCameraPose(
        const ::Camera& logicCamera, Ogre::Camera& renderCamera,
        float nominalNearClipDistance, World* world, const Player* player,
        CameraPerspective requestedPerspective,
        ThirdPersonCameraPresentation::State& state, float deltaSeconds,
        const glm::vec3* selectedHitPoint)
    {
        ThirdPersonCameraPresentation::Input input;
        input.eye = logicCamera.position;
        input.rotation = logicCamera.rotation;
        input.verticalFovDegrees = static_cast<float>(
            renderCamera.getFOVy().valueDegrees());
        input.aspectRatio = static_cast<float>(
            renderCamera.getAspectRatio());
        const float nearClipDistance = nominalNearClipDistance;
        input.desiredDistance =
            ThirdPersonCameraPresentation::distanceForVerticalFov(
                input.verticalFovDegrees);
        input.radius =
            ThirdPersonCameraPresentation::radiusForProjection(
                input.verticalFovDegrees, input.aspectRatio,
                nearClipDistance);
        input.shoulderOffset =
            ThirdPersonCameraPresentation::DefaultShoulderOffset;
        input.verticalOffset =
            ThirdPersonCameraPresentation::DefaultVerticalOffset;
        input.aimTargetDistance =
            ThirdPersonCameraPresentation::DefaultAimConvergenceDistance;
        if (selectedHitPoint != nullptr &&
            ThirdPersonCameraPresentation::finite(*selectedHitPoint))
        {
            input.aimTargetDistance =
                glm::dot(
                    *selectedHitPoint - input.eye,
                    ThirdPersonCameraPresentation::forward(input.rotation));
            input.aimTargetVisible = true;
        }
        if (player != nullptr)
        {
            const glm::vec3 playerCentre(
                logicCamera.position.x,
                logicCamera.position.y - .6f,
                logicCamera.position.z);
            input.subjectBoundsEnabled = true;
            input.subjectMinimum = playerCentre - player->box.dimensions;
            input.subjectMaximum = playerCentre + player->box.dimensions;
        }
        const ThirdPersonCameraPresentation::Mode requestedMode =
            requestedPerspective == CameraPerspective::ThirdPerson &&
                ThirdPersonCameraPresentation::projectionSupported(
                    input.verticalFovDegrees, input.aspectRatio,
                    nearClipDistance)
                ? ThirdPersonCameraPresentation::Mode::ThirdPersonRear
                : ThirdPersonCameraPresentation::Mode::FirstPerson;
        CameraPose pose;
        static_cast<ThirdPersonCameraPresentation::Pose&>(pose) =
            ThirdPersonCameraPresentation::update(
            state,
            world != nullptr
                ? requestedMode
                : ThirdPersonCameraPresentation::Mode::FirstPerson,
            input, deltaSeconds,
            [world](int x, int y, int z)
            {
                return world != nullptr &&
                       world->getBlock(x, y, z).getData().isCollidable;
            });
        updateRenderNear(pose, renderCamera, world, input,
                         nominalNearClipDistance);
        return pose;
    }

    void applyCameraPose(Ogre::Camera& renderCamera,
                         const glm::vec3& position,
                         const glm::vec3& rotation)
    {
        renderCamera.setPosition(position.x, position.y, position.z);
        renderCamera.setOrientation(Ogre::Quaternion::IDENTITY);
        renderCamera.yaw(Ogre::Degree(-rotation.y));
        renderCamera.pitch(Ogre::Degree(-rotation.x));
    }

    AvatarVisibility visibilityForCamera(
        ThirdPersonCameraPresentation::Mode effectiveMode,
        bool diagnosticSweepEnabled) noexcept
    {
        const bool visible = !diagnosticSweepEnabled &&
            effectiveMode == ThirdPersonCameraPresentation::Mode::ThirdPersonRear;
        return {visible, !visible};
    }

    void syncAvatarForCamera(
        OgrePlayerRenderer& renderer,
        const PlayerAvatarPresentation::Profile& profile,
        const PlayerAvatarPresentation::Pose& pose,
        ThirdPersonCameraPresentation::Mode effectiveMode,
        bool diagnosticSweepEnabled, Material::ID heldMaterial)
    {
        const auto visibility = visibilityForCamera(
            effectiveMode, diagnosticSweepEnabled);
        renderer.sync(profile, pose, visibility.avatarVisible, heldMaterial);
    }
}
