#include "OgreActorRenderer.h"

#include <Ogre.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace
{
    constexpr const char* MobMaterial = "HelloMine3D/ActorMob";
    constexpr const char* StalkerMaterial = "HelloMine3D/ActorStalker";
    constexpr const char* BruteMaterial = "HelloMine3D/ActorBrute";
    constexpr const char* SpitterMaterial = "HelloMine3D/ActorSpitter";
    constexpr const char* ItemMaterial = "HelloMine3D/ActorItem";
    constexpr const char* ProjectileMaterial =
        "HelloMine3D/CombatProjectile";
    // Hostile mobs stop 0.55 m from the player centre. With a 0.35 m
    // half-width that leaves their nearest face only 0.20 m from the
    // first-person camera, where it can fill the viewport before crossing
    // the 0.10 m near plane. Hide visuals once they enter the player's
    // contact envelope; simulation, selection and collision remain active.
    constexpr float FirstPersonContactPadding = 0.30f;

    bool finiteVector(const glm::vec3& value)
    {
        return std::isfinite(value.x) && std::isfinite(value.y) &&
               std::isfinite(value.z);
    }

    const char* materialFor(const ActorSnapshot& snapshot)
    {
        if (snapshot.type == "item")
        {
            return ItemMaterial;
        }
        if (snapshot.type == "hellomine:stalker" ||
            snapshot.type == "hellomine:waystone_stalker")
        {
            return StalkerMaterial;
        }
        if (snapshot.type == "hellomine:brute" ||
            snapshot.type == "hellomine:waystone_brute")
        {
            return BruteMaterial;
        }
        if (snapshot.type == "hellomine:spitter")
        {
            return SpitterMaterial;
        }
        return MobMaterial;
    }

    bool intersectsFirstPersonNearPlane(
        const ActorSnapshot& snapshot, const glm::vec3& cameraPosition)
    {
        const glm::vec3 separation = glm::max(
            glm::abs(cameraPosition - snapshot.position) -
                snapshot.dimensions,
            glm::vec3(0.0f));
        return glm::dot(separation, separation) <=
               FirstPersonContactPadding * FirstPersonContactPadding;
    }

    void buildUnitCube(Ogre::ManualObject& object,
                       const Ogre::String& material,
                       bool castShadows)
    {
        object.begin(material, Ogre::RenderOperation::OT_TRIANGLE_LIST);

        object.position(-0.5f, -0.5f, -0.5f);
        object.position(0.5f, -0.5f, -0.5f);
        object.position(0.5f, 0.5f, -0.5f);
        object.position(-0.5f, 0.5f, -0.5f);
        object.position(-0.5f, -0.5f, 0.5f);
        object.position(0.5f, -0.5f, 0.5f);
        object.position(0.5f, 0.5f, 0.5f);
        object.position(-0.5f, 0.5f, 0.5f);

        const Ogre::uint32 indices[] = {
            0, 2, 1, 0, 3, 2,
            4, 5, 6, 4, 6, 7,
            0, 4, 7, 0, 7, 3,
            1, 2, 6, 1, 6, 5,
            3, 7, 6, 3, 6, 2,
            0, 1, 5, 0, 5, 4
        };
        for (Ogre::uint32 index : indices)
        {
            object.index(index);
        }
        object.end();
        object.setCastShadows(castShadows);
        object.setRenderQueueGroup(Ogre::RENDER_QUEUE_MAIN);
    }
}

OgreActorRenderer::OgreActorRenderer(Ogre::SceneManager& sceneManager)
    : m_sceneManager(&sceneManager)
{
}

OgreActorRenderer::~OgreActorRenderer()
{
    clear();
}

OgreActorRendererValidation OgreActorRenderer::validateSnapshots(
    const std::vector<ActorSnapshot>& snapshots)
{
    OgreActorRendererValidation validation;
    std::unordered_set<ActorId> ids;
    ids.reserve(snapshots.size());

    for (const ActorSnapshot& snapshot : snapshots)
    {
        if (snapshot.id == InvalidActorId)
        {
            validation.message = "actor id is invalid";
            return validation;
        }
        if (!ids.insert(snapshot.id).second)
        {
            validation.message = "actor id is duplicated";
            return validation;
        }
        if (snapshot.type.empty())
        {
            validation.message = "actor type is empty";
            return validation;
        }
        if (!finiteVector(snapshot.position) ||
            !finiteVector(snapshot.rotation) ||
            !finiteVector(snapshot.dimensions) ||
            snapshot.dimensions.x <= 0.0f ||
            snapshot.dimensions.y <= 0.0f ||
            snapshot.dimensions.z <= 0.0f)
        {
            validation.message = "actor transform is invalid";
            return validation;
        }
        if (!std::isfinite(snapshot.hitFeedback) ||
            snapshot.hitFeedback < 0.f || snapshot.hitFeedback > 1.f ||
            snapshot.combatStateTicksRemaining < 0 ||
            snapshot.combatStateTicksTotal < 0 ||
            snapshot.combatStateTicksRemaining >
                snapshot.combatStateTicksTotal)
        {
            validation.message = "actor combat snapshot is invalid";
            return validation;
        }
        if ((snapshot.deathPresentation &&
             (snapshot.type == "item" || !snapshot.combatant ||
              snapshot.deathPresentationTicksTotal !=
                  EnemyPresentation::DeathPoseTicks ||
              snapshot.deathPresentationTicksRemaining <= 0 ||
              snapshot.deathPresentationTicksRemaining >
                  snapshot.deathPresentationTicksTotal)) ||
            (!snapshot.deathPresentation &&
             (snapshot.deathPresentationTicksRemaining != 0 ||
              snapshot.deathPresentationTicksTotal != 0)))
        {
            validation.message = "actor death presentation is invalid";
            return validation;
        }
        if (snapshot.combatant)
        {
            switch (snapshot.combatMode)
            {
                case EnemyCombatMode::Melee:
                case EnemyCombatMode::Ranged:
                    break;
                default:
                    validation.message = "actor combat mode is invalid";
                    return validation;
            }
            switch (snapshot.combatState)
            {
                case MobCombatState::Idle:
                case MobCombatState::Chase:
                case MobCombatState::Windup:
                case MobCombatState::Recover:
                    break;
                default:
                    validation.message = "actor combat state is invalid";
                    return validation;
            }
        }

        ++validation.actorCount;
        if (snapshot.type == "item")
        {
            ++validation.itemCount;
        }
        else
        {
            ++validation.mobCount;
        }
    }

    validation.valid = true;
    validation.visibleItemCount = std::min(
        validation.itemCount, OgreActorRenderer::MaxVisibleItems);
    validation.message = "ok";
    return validation;
}

OgreProjectileRendererValidation
OgreActorRenderer::validateProjectileSnapshots(
    const std::vector<CombatProjectileSnapshot>& snapshots)
{
    OgreProjectileRendererValidation validation;
    std::unordered_set<CombatProjectileId> ids;
    ids.reserve(snapshots.size());
    for (const CombatProjectileSnapshot& snapshot : snapshots)
    {
        if (snapshot.id == InvalidCombatProjectileId ||
            snapshot.ownerId == InvalidActorId)
        {
            validation.message = "projectile identity is invalid";
            return validation;
        }
        if (!ids.insert(snapshot.id).second)
        {
            validation.message = "projectile id is duplicated";
            return validation;
        }
        if (!finiteVector(snapshot.position) ||
            !finiteVector(snapshot.velocity) ||
            glm::length(snapshot.velocity) <= 0.000001f ||
            snapshot.radius <= 0.0f || snapshot.radius > 0.5f ||
            snapshot.ticksRemaining <= 0 ||
            !std::isfinite(snapshot.distanceTravelled) ||
            !std::isfinite(snapshot.maximumDistance) ||
            snapshot.distanceTravelled < 0.0f ||
            snapshot.maximumDistance <= 0.0f ||
            snapshot.distanceTravelled > snapshot.maximumDistance)
        {
            validation.message = "projectile state is invalid";
            return validation;
        }
        ++validation.projectileCount;
    }
    validation.valid = true;
    validation.message = "ok";
    return validation;
}

void OgreActorRenderer::sync(
    const std::vector<ActorSnapshot>& snapshots,
    const glm::vec3& cameraPosition)
{
    if (m_sceneManager == nullptr)
    {
        return;
    }

    const OgreActorRendererValidation validation =
        validateSnapshots(snapshots);
    if (!validation.valid)
    {
        throw std::runtime_error(
            "Actor snapshot validation failed: " + validation.message);
    }

    std::unordered_set<ActorId> liveIds;
    liveIds.reserve(snapshots.size());
    std::size_t visibleItems = 0;
    for (const ActorSnapshot& snapshot : snapshots)
    {
        if (snapshot.type == "item")
        {
            if (visibleItems >= MaxVisibleItems)
            {
                continue;
            }
            ++visibleItems;
        }
        liveIds.insert(snapshot.id);
        auto existing = m_visuals.find(snapshot.id);
        if (existing != m_visuals.end() &&
            existing->second.type != snapshot.type)
        {
            destroyVisual(existing->second);
            m_visuals.erase(existing);
            existing = m_visuals.end();
        }

        if (existing == m_visuals.end())
        {
            existing = m_visuals.emplace(
                snapshot.id, createVisual(snapshot)).first;
        }
        updateVisual(existing->second, snapshot, cameraPosition);
    }

    for (auto it = m_visuals.begin(); it != m_visuals.end();)
    {
        if (liveIds.find(it->first) != liveIds.end())
        {
            ++it;
            continue;
        }
        destroyVisual(it->second);
        it = m_visuals.erase(it);
    }
}

void OgreActorRenderer::syncProjectiles(
    const std::vector<CombatProjectileSnapshot>& snapshots)
{
    if (m_sceneManager == nullptr)
    {
        return;
    }
    const OgreProjectileRendererValidation validation =
        validateProjectileSnapshots(snapshots);
    if (!validation.valid)
    {
        throw std::runtime_error(
            "Projectile snapshot validation failed: " +
            validation.message);
    }

    std::unordered_set<CombatProjectileId> liveIds;
    liveIds.reserve(snapshots.size());
    for (const CombatProjectileSnapshot& snapshot : snapshots)
    {
        liveIds.insert(snapshot.id);
        auto existing = m_projectileVisuals.find(snapshot.id);
        if (existing == m_projectileVisuals.end())
        {
            existing = m_projectileVisuals.emplace(
                snapshot.id, createProjectileVisual(snapshot.id)).first;
        }
        updateProjectileVisual(existing->second, snapshot);
    }
    for (auto iterator = m_projectileVisuals.begin();
         iterator != m_projectileVisuals.end();)
    {
        if (liveIds.find(iterator->first) != liveIds.end())
        {
            ++iterator;
            continue;
        }
        destroyVisual(iterator->second);
        iterator = m_projectileVisuals.erase(iterator);
    }
}

void OgreActorRenderer::clear()
{
    for (auto& entry : m_visuals)
    {
        destroyVisual(entry.second);
    }
    m_visuals.clear();
    for (auto& entry : m_projectileVisuals)
    {
        destroyVisual(entry.second);
    }
    m_projectileVisuals.clear();
}

void OgreActorRenderer::setCastShadows(bool enabled) noexcept
{
    m_castShadows = enabled;
    for (auto& entry : m_visuals)
    {
        if (entry.second.object != nullptr)
        {
            entry.second.object->setCastShadows(enabled);
        }
        for (ActorPartVisual& part : entry.second.parts)
        {
            if (part.object != nullptr)
            {
                part.object->setCastShadows(enabled);
            }
        }
    }
    for (auto& entry : m_projectileVisuals)
    {
        entry.second.object->setCastShadows(enabled);
    }
}

OgreActorRenderer::ActorVisual OgreActorRenderer::createProjectileVisual(
    CombatProjectileId id)
{
    const Ogre::String baseName =
        "CombatProjectile_" + std::to_string(id);
    ActorVisual visual;
    visual.type = "combat_projectile";
    visual.object = m_sceneManager->createManualObject(baseName + "_Mesh");
    buildUnitCube(*visual.object, ProjectileMaterial, m_castShadows);
    visual.node = m_sceneManager->getRootSceneNode()->createChildSceneNode(
        baseName + "_Node");
    visual.node->attachObject(visual.object);
    return visual;
}

void OgreActorRenderer::updateProjectileVisual(
    ActorVisual& visual, const CombatProjectileSnapshot& snapshot)
{
    visual.node->setVisible(true);
    visual.node->setPosition(snapshot.position.x, snapshot.position.y,
                             snapshot.position.z);
    const float diameter = snapshot.radius * 2.0f;
    visual.node->setScale(diameter, diameter, diameter * 1.8f);
    const glm::vec3 direction = glm::normalize(snapshot.velocity);
    const float yaw = glm::degrees(std::atan2(direction.x, -direction.z));
    const float horizontal = std::sqrt(
        direction.x * direction.x + direction.z * direction.z);
    const float pitch = glm::degrees(std::atan2(direction.y, horizontal));
    visual.node->setOrientation(
        Ogre::Quaternion(Ogre::Degree(yaw), Ogre::Vector3::UNIT_Y) *
        Ogre::Quaternion(Ogre::Degree(pitch), Ogre::Vector3::UNIT_X));
}

OgreActorRenderer::ActorVisual OgreActorRenderer::createVisual(
    const ActorSnapshot& snapshot)
{
    const Ogre::String baseName =
        "Actor_" + std::to_string(snapshot.id);
    ActorVisual visual;
    visual.type = snapshot.type;
    visual.node = m_sceneManager->getRootSceneNode()->createChildSceneNode(
        baseName + "_Node");
    if (snapshot.type == "item")
    {
        visual.object = m_sceneManager->createManualObject(
            baseName + "_Mesh");
        buildUnitCube(*visual.object, materialFor(snapshot), m_castShadows);
        visual.node->attachObject(visual.object);
        return visual;
    }

    const EnemyVisualProfile profile =
        EnemyPresentation::profileForType(snapshot.type);
    visual.parts.reserve(profile.partCount);
    for (std::size_t index = 0; index < profile.partCount; ++index)
    {
        ActorPartVisual part;
        part.definition = profile.parts[index];
        part.object = m_sceneManager->createManualObject(
            baseName + "_PartMesh_" + std::to_string(index));
        buildUnitCube(*part.object, materialFor(snapshot), m_castShadows);
        part.node = visual.node->createChildSceneNode(
            baseName + "_PartNode_" + std::to_string(index));
        part.node->attachObject(part.object);
        visual.parts.push_back(part);
    }
    return visual;
}

void OgreActorRenderer::updateVisual(
    ActorVisual& visual, const ActorSnapshot& snapshot,
    const glm::vec3& cameraPosition)
{
    visual.node->setVisible(
        !intersectsFirstPersonNearPlane(snapshot, cameraPosition));
    if (snapshot.type == "item")
    {
        visual.node->setPosition(snapshot.position.x, snapshot.position.y,
                                 snapshot.position.z);
        visual.node->setScale(snapshot.dimensions.x * 2.0f,
                              snapshot.dimensions.y * 2.0f,
                              snapshot.dimensions.z * 2.0f);
        const Ogre::Quaternion pitch(
            Ogre::Degree(snapshot.rotation.x), Ogre::Vector3::UNIT_X);
        const Ogre::Quaternion yaw(
            Ogre::Degree(snapshot.rotation.y), Ogre::Vector3::UNIT_Y);
        const Ogre::Quaternion roll(
            Ogre::Degree(snapshot.rotation.z), Ogre::Vector3::UNIT_Z);
        visual.node->setOrientation(yaw * pitch * roll);
        return;
    }

    const EnemyVisualProfile profile =
        EnemyPresentation::profileForType(snapshot.type);
    const EnemyVisualPose pose =
        EnemyPresentation::poseFor(snapshot, profile);
    visual.node->setPosition(
        snapshot.position.x,
        snapshot.position.y + pose.rootYOffset *
            snapshot.dimensions.y * 2.0f,
        snapshot.position.z);
    visual.node->setScale(
        snapshot.dimensions.x * 2.0f * pose.rootScale,
        snapshot.dimensions.y * 2.0f * pose.rootScale,
        snapshot.dimensions.z * 2.0f * pose.rootScale);
    const Ogre::Quaternion pitch(
        Ogre::Degree(snapshot.rotation.x + pose.rootPitch),
        Ogre::Vector3::UNIT_X);
    const Ogre::Quaternion yaw(
        Ogre::Degree(snapshot.rotation.y), Ogre::Vector3::UNIT_Y);
    const Ogre::Quaternion roll(
        Ogre::Degree(snapshot.rotation.z + pose.rootRoll),
        Ogre::Vector3::UNIT_Z);
    visual.node->setOrientation(yaw * pitch * roll);

    const std::size_t count = std::min(
        visual.parts.size(), profile.partCount);
    for (std::size_t index = 0; index < count; ++index)
    {
        ActorPartVisual& part = visual.parts[index];
        const EnemyVisualPartDefinition& definition =
            profile.parts[index];
        const glm::vec3 offset = definition.offset + pose.offsets[index];
        part.node->setPosition(offset.x, offset.y, offset.z);
        const float partScale = pose.scales[index];
        part.node->setScale(definition.scale.x * partScale,
                            definition.scale.y * partScale,
                            definition.scale.z * partScale);
        const glm::vec3 rotation = pose.rotations[index];
        part.node->setOrientation(
            Ogre::Quaternion(Ogre::Degree(rotation.y),
                             Ogre::Vector3::UNIT_Y) *
            Ogre::Quaternion(Ogre::Degree(rotation.x),
                             Ogre::Vector3::UNIT_X) *
            Ogre::Quaternion(Ogre::Degree(rotation.z),
                             Ogre::Vector3::UNIT_Z));
    }
}

void OgreActorRenderer::destroyVisual(ActorVisual& visual)
{
    if (m_sceneManager == nullptr)
    {
        return;
    }
    for (ActorPartVisual& part : visual.parts)
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
    visual.parts.clear();
    if (visual.object != nullptr)
    {
        if (visual.object->isAttached())
        {
            visual.object->detachFromParent();
        }
        m_sceneManager->destroyManualObject(visual.object);
        visual.object = nullptr;
    }
    if (visual.node != nullptr)
    {
        m_sceneManager->destroySceneNode(visual.node);
        visual.node = nullptr;
    }
}
