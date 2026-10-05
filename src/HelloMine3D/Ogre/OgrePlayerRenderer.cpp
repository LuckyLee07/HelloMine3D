#include "OgrePlayerRenderer.h"
#include "OgreItemGeometry.h"
#include "ManualMeshVertexAttributes.h"

#include <Ogre.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <string>

namespace
{
    using PlayerAvatarPresentation::PartDefinition;
    using PlayerAvatarPresentation::PartTransform;
    using PlayerAvatarPresentation::Vec3;

    std::atomic<unsigned long long> s_rendererSerial{0};

    bool finite(float value) noexcept
    {
        return std::isfinite(value);
    }

    bool finiteVector(const Vec3& value) noexcept
    {
        return finite(value.x) && finite(value.y) && finite(value.z);
    }

    bool sameVector(const Vec3& left, const Vec3& right) noexcept
    {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }

    bool sameTransform(const PartTransform& left,
                       const PartTransform& right) noexcept
    {
        return sameVector(left.rotationDegrees, right.rotationDegrees) &&
               sameVector(left.offset, right.offset) &&
               sameVector(left.scale, right.scale);
    }

    Ogre::Vector3 vector(const Vec3& value)
    {
        return {value.x, value.y, value.z};
    }

    Ogre::Quaternion localRotation(const Vec3& degrees)
    {
        return Ogre::Quaternion(Ogre::Degree(degrees.y),
                                Ogre::Vector3::UNIT_Y) *
               Ogre::Quaternion(Ogre::Degree(degrees.x),
                                Ogre::Vector3::UNIT_X) *
               Ogre::Quaternion(Ogre::Degree(degrees.z),
                                Ogre::Vector3::UNIT_Z);
    }

    bool validRole(PlayerAvatarPresentation::PartRole role) noexcept
    {
        return static_cast<int>(role) >=
                   static_cast<int>(PlayerAvatarPresentation::PartRole::Head) &&
               static_cast<int>(role) <=
                   static_cast<int>(PlayerAvatarPresentation::PartRole::Belt);
    }

    bool validMaterial(
        PlayerAvatarPresentation::MaterialRole material) noexcept
    {
        return static_cast<int>(material) >=
                   static_cast<int>(
                       PlayerAvatarPresentation::MaterialRole::Skin) &&
               static_cast<int>(material) <=
                   static_cast<int>(
                       PlayerAvatarPresentation::MaterialRole::Accent);
    }

    void buildUnitCube(Ogre::ManualObject& object, bool castShadows)
    {
        object.begin(OgrePlayerRenderer::MaterialName,
                     Ogre::RenderOperation::OT_TRIANGLE_LIST);

        const Ogre::Vector3 positions[] = {
            {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
            {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
            {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
            {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}};
        for (const Ogre::Vector3 &position : positions)
        {
            object.position(position);
            appendOrdinaryManualVertexAttributes(object);
        }

        const Ogre::uint32 indices[] = {
            0, 2, 1, 0, 3, 2,
            4, 5, 6, 4, 6, 7,
            0, 4, 7, 0, 7, 3,
            1, 2, 6, 1, 6, 5,
            3, 7, 6, 3, 6, 2,
            0, 1, 5, 0, 5, 4
        };
        for (const Ogre::uint32 index : indices)
        {
            object.index(index);
        }
        object.end();
        object.getSection(0)->setCustomParameter(
            1, Ogre::Vector4(0.f, 0.f, 0.f,
                             OgrePlayerRenderer::PlayerSurfaceMarker));
        object.setCastShadows(castShadows);
        object.setRenderQueueGroup(Ogre::RENDER_QUEUE_MAIN);
    }

    Ogre::Vector3 componentProduct(const Ogre::Vector3& left,
                                   const Ogre::Vector3& right)
    {
        return {left.x * right.x, left.y * right.y,
                left.z * right.z};
    }

    struct HeldItemGrip
    {
        Ogre::Vector3 scale{Ogre::Vector3::ZERO};
        Ogre::Vector3 itemSpaceGrip{Ogre::Vector3::ZERO};
        Ogre::Quaternion rotation{Ogre::Quaternion::IDENTITY};
    };

    HeldItemGrip heldItemGrip(Material::ID material)
    {
        if (itemVisualUsesCube(material))
        {
            // A block is supported below its lower face and turned just enough
            // for the rear camera to read two voxel faces.
            return {{.38f, .38f, .38f},
                    {.04f, -.54f, .20f},
                    Ogre::Quaternion(Ogre::Degree(-24.f),
                                     Ogre::Vector3::UNIT_Y)};
        }

        const Material& definition = Material::toMaterial(material);
        const float size = definition.isTool ? .54f : .43f;
        // Extruded icons use the same lower-left grip region as the existing
        // first-person hand. The texture already contains the diagonal tool
        // pose, so it only needs a small turn to expose its real side wall.
        return {{size, size, size},
                {-.28f, -.32f, .09f},
                Ogre::Quaternion(Ogre::Degree(-7.f),
                                 Ogre::Vector3::UNIT_Y)};
    }
}

OgrePlayerRenderer::OgrePlayerRenderer(Ogre::SceneManager& sceneManager)
    : m_sceneManager(&sceneManager),
      m_baseName("PlayerAvatar_" +
                 std::to_string(s_rendererSerial.fetch_add(1)))
{
    createVisuals();
}

OgrePlayerRenderer::~OgrePlayerRenderer()
{
    clear();
}

OgrePlayerRendererValidation OgrePlayerRenderer::validate(
    const PlayerAvatarPresentation::Profile& profile,
    const PlayerAvatarPresentation::Pose& pose) noexcept
{
    OgrePlayerRendererValidation result;
    result.partCount = profile.partCount;
    if (profile.partCount > profile.parts.size())
    {
        result.message = "player avatar part count exceeds budget";
        return result;
    }
    if (!finiteVector(pose.worldPosition) ||
        !finite(pose.facingYawDegrees) || !finiteVector(pose.rootOffset) ||
        !finiteVector(pose.rootRotationDegrees))
    {
        result.message = "player avatar root transform is invalid";
        return result;
    }
    if (!finite(pose.weights.idle) || !finite(pose.weights.walk) ||
        !finite(pose.weights.airborne) || !finite(pose.weights.land) ||
        !finite(pose.weights.tool) || !finite(pose.weights.hurt) ||
        pose.weights.idle < 0.f || pose.weights.idle > 1.f ||
        pose.weights.walk < 0.f || pose.weights.walk > 1.f ||
        pose.weights.airborne < 0.f || pose.weights.airborne > 1.f ||
        pose.weights.land < 0.f || pose.weights.land > 1.f ||
        pose.weights.tool < 0.f || pose.weights.tool > 1.f ||
        pose.weights.hurt < 0.f || pose.weights.hurt > 1.f)
    {
        result.message = "player avatar pose weights are invalid";
        return result;
    }

    std::array<bool, PlayerAvatarPresentation::Profile::MaximumParts>
        rolePresent{};
    std::array<std::size_t,
               PlayerAvatarPresentation::Profile::MaximumParts>
        roleIndices{};
    roleIndices.fill(profile.parts.size());
    for (std::size_t index = 0; index < profile.partCount; ++index)
    {
        const PartDefinition& definition = profile.parts[index];
        const PartTransform& transform = pose.parts[index];
        if (!validRole(definition.role))
        {
            result.message = "player avatar part role is invalid";
            return result;
        }
        if (!validMaterial(definition.material))
        {
            result.message = "player avatar part material is invalid";
            return result;
        }
        const std::size_t roleIndex =
            static_cast<std::size_t>(definition.role);
        if (rolePresent[roleIndex])
        {
            result.message = "player avatar part role is duplicated";
            return result;
        }
        rolePresent[roleIndex] = true;
        roleIndices[roleIndex] = index;
        if (!finiteVector(definition.centre) ||
            !finiteVector(definition.size) ||
            !finiteVector(definition.pivot) || definition.size.x <= 0.f ||
            definition.size.y <= 0.f || definition.size.z <= 0.f)
        {
            result.message = "player avatar part definition is invalid";
            return result;
        }
        if (!finiteVector(transform.rotationDegrees) ||
            !finiteVector(transform.offset) || !finiteVector(transform.scale) ||
            transform.scale.x <= 0.f || transform.scale.y <= 0.f ||
            transform.scale.z <= 0.f)
        {
            result.message = "player avatar part transform is invalid";
            return result;
        }
    }

    constexpr PlayerAvatarPresentation::PartRole requiredRoles[] = {
        PlayerAvatarPresentation::PartRole::Head,
        PlayerAvatarPresentation::PartRole::Torso,
        PlayerAvatarPresentation::PartRole::LeftArm,
        PlayerAvatarPresentation::PartRole::RightArm,
        PlayerAvatarPresentation::PartRole::LeftLeg,
        PlayerAvatarPresentation::PartRole::RightLeg};
    for (const PlayerAvatarPresentation::PartRole role : requiredRoles)
    {
        if (!rolePresent[static_cast<std::size_t>(role)])
        {
            result.message = "player avatar required body part is missing";
            return result;
        }
    }

    const auto linkedTransformMatches =
        [&](PlayerAvatarPresentation::PartRole child,
            PlayerAvatarPresentation::PartRole parent) noexcept
        {
            const std::size_t childRole = static_cast<std::size_t>(child);
            if (!rolePresent[childRole])
            {
                return true;
            }
            const std::size_t parentRole = static_cast<std::size_t>(parent);
            return rolePresent[parentRole] &&
                   sameTransform(pose.parts[roleIndices[childRole]],
                                 pose.parts[roleIndices[parentRole]]);
        };
    if (!linkedTransformMatches(PlayerAvatarPresentation::PartRole::Hair,
                                PlayerAvatarPresentation::PartRole::Head) ||
        !linkedTransformMatches(PlayerAvatarPresentation::PartRole::Belt,
                                PlayerAvatarPresentation::PartRole::Torso))
    {
        result.message = "player avatar cosmetic transform is detached";
        return result;
    }

    result.valid = true;
    result.message = "ok";
    return result;
}

void OgrePlayerRenderer::createVisuals()
{
    if (m_sceneManager == nullptr || m_rootNode != nullptr)
    {
        return;
    }

    try
    {
        m_rootNode =
            m_sceneManager->getRootSceneNode()->createChildSceneNode(
                m_baseName + "_RootNode");
        for (std::size_t index = 0; index < m_parts.size(); ++index)
        {
            PartVisual& part = m_parts[index];
            part.object = m_sceneManager->createManualObject(
                m_baseName + "_PartMesh_" + std::to_string(index));
            buildUnitCube(*part.object, m_castShadows);
            part.node = m_rootNode->createChildSceneNode(
                m_baseName + "_PartNode_" + std::to_string(index));
            part.node->attachObject(part.object);
            part.node->setVisible(false);
        }
        m_heldItem.object = m_sceneManager->createManualObject(
            m_baseName + "_HeldItemMesh");
        m_heldItem.object->setCastShadows(m_castShadows);
        m_heldItem.object->setRenderQueueGroup(Ogre::RENDER_QUEUE_MAIN);
        m_heldItem.node = m_rootNode->createChildSceneNode(
            m_baseName + "_HeldItemNode");
        m_heldItem.node->attachObject(m_heldItem.object);
        m_heldItem.node->setVisible(false);
    }
    catch (...)
    {
        clear();
        throw;
    }
}

void OgrePlayerRenderer::rebuildHeldItem(Material::ID material)
{
    if (m_heldItem.object == nullptr ||
        (m_heldItem.geometryInitialized &&
         m_heldItem.material == material))
    {
        return;
    }

    m_heldItem.object->clear();
    m_heldItem.material = material;
    m_heldItem.geometryInitialized = true;
    m_heldItem.geometryAvailable = false;
    if (material <= Material::Nothing || material >= Material::Count)
    {
        return;
    }

    const ItemVisualGeometry::Mesh& geometry =
        itemVisualGeometry(material);
    if (geometry.empty())
    {
        return;
    }

    const char* materialName =
        material == Material::Glass ||
                material == Material::GlassBorderless
            ? HeldTransparentMaterialName
            : HeldMaterialName;
    m_heldItem.object->begin(
        materialName, Ogre::RenderOperation::OT_TRIANGLE_LIST);
    Ogre::uint32 vertex = 0;
    for (const ItemVisualGeometry::Face& face : geometry)
    {
        const glm::vec2 tileOrigin = itemVisualTileOrigin(face.tile);
        for (int corner = 0; corner < 4; ++corner)
        {
            const glm::vec3& position = face.positions[corner];
            m_heldItem.object->position(
                position.x, position.y, position.z);
            appendOrdinaryManualVertexAttributes(*m_heldItem.object,
                tileOrigin.x, tileOrigin.y,
                face.uv[corner].x, face.uv[corner].y,
                .72f + .28f * std::max(face.normal.y, 0.f));
        }
        m_heldItem.object->quad(
            vertex, vertex + 1, vertex + 2, vertex + 3);
        vertex += 4;
    }
    m_heldItem.object->end();
    m_heldItem.object->setCastShadows(m_castShadows);
    m_heldItem.object->setRenderQueueGroup(Ogre::RENDER_QUEUE_MAIN);
    m_heldItem.geometryAvailable = true;
}

void OgrePlayerRenderer::syncHeldItem(
    const PlayerAvatarPresentation::Profile& profile,
    const PlayerAvatarPresentation::Pose& pose,
    Material::ID material)
{
    rebuildHeldItem(material);
    if (m_heldItem.node == nullptr || !m_heldItem.geometryAvailable)
    {
        if (m_heldItem.node != nullptr)
        {
            m_heldItem.node->setVisible(false);
        }
        return;
    }

    std::size_t rightArmIndex = profile.partCount;
    for (std::size_t index = 0; index < profile.partCount; ++index)
    {
        if (profile.parts[index].role ==
            PlayerAvatarPresentation::PartRole::RightArm)
        {
            rightArmIndex = index;
            break;
        }
    }
    if (rightArmIndex >= profile.partCount)
    {
        m_heldItem.node->setVisible(false);
        return;
    }

    const PartDefinition& armDefinition = profile.parts[rightArmIndex];
    const PartTransform& armTransform = pose.parts[rightArmIndex];
    const PartVisual& armVisual = m_parts[rightArmIndex];
    const Ogre::Quaternion armRotation = armVisual.node->getOrientation();
    const Ogre::Vector3 armHalfExtent(
        0.f,
        -.5f * armDefinition.size.y * armTransform.scale.y,
        0.f);
    const Ogre::Vector3 handPosition =
        armVisual.node->getPosition() + armRotation * armHalfExtent;

    const HeldItemGrip grip = heldItemGrip(material);
    const Ogre::Quaternion itemRotation = armRotation * grip.rotation;
    m_heldItem.node->setPosition(
        handPosition - itemRotation * componentProduct(
            grip.itemSpaceGrip, grip.scale));
    m_heldItem.node->setOrientation(itemRotation);
    m_heldItem.node->setScale(grip.scale);
    m_heldItem.node->setVisible(m_visible);
}

void OgrePlayerRenderer::sync(
    const PlayerAvatarPresentation::Profile& profile,
    const PlayerAvatarPresentation::Pose& pose, bool visible,
    Material::ID heldMaterial)
{
    if (m_sceneManager == nullptr)
    {
        return;
    }
    const OgrePlayerRendererValidation validation = validate(profile, pose);
    if (!validation.valid)
    {
        throw std::runtime_error(
            std::string("Player avatar validation failed: ") +
            validation.message);
    }
    createVisuals();
    m_activePartCount = profile.partCount;
    m_visible = visible;

    // Player yaw follows the simulation convention where +90 degrees faces
    // +X. Ogre rotates a local -Z facing model the opposite way, hence the
    // sign conversion at this boundary.
    const Ogre::Quaternion facing(
        Ogre::Degree(-pose.facingYawDegrees), Ogre::Vector3::UNIT_Y);
    const Ogre::Vector3 worldPosition = vector(pose.worldPosition);
    m_rootNode->setPosition(worldPosition + facing * vector(pose.rootOffset));
    m_rootNode->setOrientation(
        facing * localRotation(pose.rootRotationDegrees));
    m_rootNode->setScale(Ogre::Vector3::UNIT_SCALE);

    for (std::size_t index = 0; index < m_parts.size(); ++index)
    {
        PartVisual& part = m_parts[index];
        const bool partVisible = visible && index < profile.partCount;
        part.node->setVisible(partVisible);
        if (index >= profile.partCount)
        {
            continue;
        }

        const PartDefinition& definition = profile.parts[index];
        const PartTransform& transform = pose.parts[index];
        const Ogre::Quaternion rotation =
            localRotation(transform.rotationDegrees);
        const Ogre::Vector3 centre = vector(definition.centre);
        const Ogre::Vector3 pivot = vector(definition.pivot);
        part.node->setPosition(
            pivot + vector(transform.offset) + rotation * (centre - pivot));
        part.node->setOrientation(rotation);
        part.node->setScale(
            definition.size.x * transform.scale.x,
            definition.size.y * transform.scale.y,
            definition.size.z * transform.scale.z);

        // Layout consumed by ActorPlayer shaders:
        // x = PartRole, y = MaterialRole, z = hurt envelope,
        // w = negative player marker.
        part.object->getSection(0)->setCustomParameter(
            1, Ogre::Vector4(static_cast<float>(definition.role),
                             static_cast<float>(definition.material),
                             pose.weights.hurt, PlayerSurfaceMarker));
    }
    syncHeldItem(profile, pose, heldMaterial);
}

void OgrePlayerRenderer::setLighting(float exposure)
{
    const float boundedExposure =
        std::isfinite(exposure) ? std::clamp(exposure, 0.f, 1.f) : .12f;
    for (const char* name :
         {MaterialName, HeldMaterialName, HeldTransparentMaterialName})
    {
        const auto material = Ogre::MaterialManager::getSingleton().getByName(name);
        if (material.isNull() || material->getNumTechniques() == 0 ||
            material->getTechnique(0)->getNumPasses() == 0)
        {
            throw std::runtime_error(std::string("Missing player material: ") + name);
        }
        // Reapply each frame: switching shadow/array programs replaces params.
        material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()
            ->setNamedConstant("playerExposure", boundedExposure);
    }
}

void OgrePlayerRenderer::setCastShadows(bool enabled) noexcept
{
    m_castShadows = enabled;
    for (PartVisual& part : m_parts)
    {
        if (part.object != nullptr)
        {
            part.object->setCastShadows(enabled);
        }
    }
    if (m_heldItem.object != nullptr)
    {
        m_heldItem.object->setCastShadows(enabled);
    }
}

void OgrePlayerRenderer::setVisible(bool visible) noexcept
{
    m_visible = visible;
    for (std::size_t index = 0; index < m_parts.size(); ++index)
    {
        if (m_parts[index].node != nullptr)
        {
            m_parts[index].node->setVisible(
                visible && index < m_activePartCount);
        }
    }
    if (m_heldItem.node != nullptr)
    {
        m_heldItem.node->setVisible(
            visible && m_heldItem.geometryAvailable);
    }
}

void OgrePlayerRenderer::clear()
{
    if (m_sceneManager == nullptr)
    {
        return;
    }
    if (m_heldItem.object != nullptr)
    {
        if (m_heldItem.object->isAttached())
        {
            m_heldItem.object->detachFromParent();
        }
        m_sceneManager->destroyManualObject(m_heldItem.object);
        m_heldItem.object = nullptr;
    }
    if (m_heldItem.node != nullptr)
    {
        m_sceneManager->destroySceneNode(m_heldItem.node);
        m_heldItem.node = nullptr;
    }
    m_heldItem.material = Material::Nothing;
    m_heldItem.geometryAvailable = false;
    m_heldItem.geometryInitialized = false;
    for (PartVisual& part : m_parts)
    {
        if (part.object != nullptr)
        {
            if (part.object->isAttached())
            {
                part.object->detachFromParent();
            }
            m_sceneManager->destroyManualObject(part.object);
            part.object = nullptr;
        }
        if (part.node != nullptr)
        {
            m_sceneManager->destroySceneNode(part.node);
            part.node = nullptr;
        }
    }
    if (m_rootNode != nullptr)
    {
        m_sceneManager->destroySceneNode(m_rootNode);
        m_rootNode = nullptr;
    }
    m_activePartCount = 0;
    m_visible = false;
}
