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
    constexpr const char* ItemMaterial = "HelloMine3D/ActorItem";
    constexpr float FirstPersonNearPlanePadding = 0.15f;

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
        if (snapshot.type == "hellomine:stalker")
        {
            return StalkerMaterial;
        }
        if (snapshot.type == "hellomine:brute")
        {
            return BruteMaterial;
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
               FirstPersonNearPlanePadding * FirstPersonNearPlanePadding;
    }

    void buildUnitCube(Ogre::ManualObject& object,
                       const Ogre::String& material)
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
        object.setCastShadows(false);
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
    for (const ActorSnapshot& snapshot : snapshots)
    {
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

void OgreActorRenderer::clear()
{
    for (auto& entry : m_visuals)
    {
        destroyVisual(entry.second);
    }
    m_visuals.clear();
}

OgreActorRenderer::ActorVisual OgreActorRenderer::createVisual(
    const ActorSnapshot& snapshot)
{
    const Ogre::String baseName =
        "Actor_" + std::to_string(snapshot.id);
    ActorVisual visual;
    visual.type = snapshot.type;
    visual.object = m_sceneManager->createManualObject(baseName + "_Mesh");
    buildUnitCube(*visual.object, materialFor(snapshot));
    visual.node = m_sceneManager->getRootSceneNode()->createChildSceneNode(
        baseName + "_Node");
    visual.node->attachObject(visual.object);
    return visual;
}

void OgreActorRenderer::updateVisual(
    ActorVisual& visual, const ActorSnapshot& snapshot,
    const glm::vec3& cameraPosition)
{
    visual.node->setVisible(
        !intersectsFirstPersonNearPlane(snapshot, cameraPosition));
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
}

void OgreActorRenderer::destroyVisual(ActorVisual& visual)
{
    if (m_sceneManager == nullptr)
    {
        return;
    }
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
