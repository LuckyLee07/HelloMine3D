#pragma once

#include "../Config.h"
#include "../Presentation/ThirdPersonCameraPresentation.h"
#include <OgreRenderObjectListener.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

class World;
class Player;
class Camera;
namespace Ogre { class Camera; class SceneManager; }

// Optional independent observer of the ordinary client scene. It never owns
// a World/Player/Camera or changes a pose, projection, resource or world block.
// Keep this object alive through the draw and destroy it BEFORE SceneManager.
class OgreCameraDiagnostics final : public Ogre::RenderObjectListener {
public:
    // Must name a fresh directory. Strict bounds:12 distinct phase names,
    // 24 frames,512 copied nearby voxels/frame,9 actual player objects/frame,
    // 64 selected main-camera notifications/frame,64MiB stored evidence.
    OgreCameraDiagnostics(const std::string& newOutputDirectory,
                          Ogre::SceneManager& scene);
    ~OgreCameraDiagnostics() override;
    OgreCameraDiagnostics(const OgreCameraDiagnostics&) = delete;
    OgreCameraDiagnostics& operator=(const OgreCameraDiagnostics&) = delete;

    // Call after NORMAL camera/avatar/mesh/environment sync, before rendering.
    // No VISUAL_CAMERA_SWEEP, MI fixture scheduler or direct diagnostic pose.
    void beginFrame(const std::string& phase, World& world,
                    const Player& player, const ::Camera& logic,
                    const Ogre::Camera& camera,
                    ThirdPersonCameraPresentation::Mode effectiveMode,
                    CameraPerspective requestedPerspective,
                    std::uint64_t frameId,
                    float nominalNearClipDistance = .1f,
                    std::size_t nearClipQueries = 0,
                    bool nearClipSafetyUnresolved = false,
                    int nearClipStatus = 0);
    // Root's actual normal backend writes this PNG after drawing/before swap.
    // This module does not read GL/frame pixels or call writeContentsToFile.
    std::string framePngPath() const;
    void finishFrame(bool actualFirstPersonHandVisible);
    bool isFrameOpen() const noexcept;
    std::size_t frameCount() const noexcept;

    void notifyRenderSingleObject(
        Ogre::Renderable* renderable, const Ogre::Pass* pass,
        const Ogre::AutoParamDataSource* source,
        const Ogre::LightList* lights,
        bool suppressRenderStateChanges) override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
