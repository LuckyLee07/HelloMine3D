#include "OgreBlockOutline.h"

#include <Ogre.h>

#include <algorithm>

#include "../World/Interaction/BlockSelection.h"

namespace
{
    constexpr Ogre::Real OutlineInset = 0.002f;

    void addEdge(Ogre::ManualObject &object,
                 Ogre::Real ax, Ogre::Real ay, Ogre::Real az,
                 Ogre::Real bx, Ogre::Real by, Ogre::Real bz)
    {
        object.position(ax, ay, az);
        object.position(bx, by, bz);
    }
}

OgreBlockOutline::OgreBlockOutline(Ogre::SceneManager &sceneManager)
    : m_sceneManager(&sceneManager)
{
    m_object = sceneManager.createManualObject("SelectedBlockOutline");
    m_object->setCastShadows(false);
    m_object->setDynamic(false);
    m_object->begin("HelloMine3D/Outline",
                    Ogre::RenderOperation::OT_LINE_LIST);

    const Ogre::Real low = -OutlineInset;
    const Ogre::Real high = 1.0f + OutlineInset;
    addEdge(*m_object, low, low, low, high, low, low);
    addEdge(*m_object, high, low, low, high, low, high);
    addEdge(*m_object, high, low, high, low, low, high);
    addEdge(*m_object, low, low, high, low, low, low);
    addEdge(*m_object, low, high, low, high, high, low);
    addEdge(*m_object, high, high, low, high, high, high);
    addEdge(*m_object, high, high, high, low, high, high);
    addEdge(*m_object, low, high, high, low, high, low);
    addEdge(*m_object, low, low, low, low, high, low);
    addEdge(*m_object, high, low, low, high, high, low);
    addEdge(*m_object, high, low, high, high, high, high);
    addEdge(*m_object, low, low, high, low, high, high);
    m_object->end();
    m_object->setRenderQueueGroup(Ogre::RENDER_QUEUE_9);

    m_crackObject = sceneManager.createManualObject("SelectedBlockCracks");
    m_crackObject->setCastShadows(false);
    m_crackObject->setDynamic(true);
    m_crackObject->setRenderQueueGroup(Ogre::RENDER_QUEUE_9);
    m_crackObject->setVisible(false);

    m_node = sceneManager.getRootSceneNode()->createChildSceneNode(
        "SelectedBlockOutlineNode");
    m_node->attachObject(m_object);
    m_node->attachObject(m_crackObject);
    m_node->setVisible(false);
}

OgreBlockOutline::~OgreBlockOutline()
{
    if (m_sceneManager == nullptr) {
        return;
    }

    if (m_object != nullptr) {
        if (m_object->isAttached()) {
            m_object->detachFromParent();
        }
        m_sceneManager->destroyManualObject(m_object);
        m_object = nullptr;
    }
    if (m_crackObject != nullptr) {
        if (m_crackObject->isAttached()) {
            m_crackObject->detachFromParent();
        }
        m_sceneManager->destroyManualObject(m_crackObject);
        m_crackObject = nullptr;
    }
    if (m_node != nullptr) {
        m_sceneManager->destroySceneNode(m_node);
        m_node = nullptr;
    }
}

std::size_t OgreBlockOutline::crackSegmentCount(int crackStage) noexcept
{
    return static_cast<std::size_t>(
        std::clamp(crackStage + 1, 0, 10)) * 12u;
}

void OgreBlockOutline::rebuildCracks(int crackStage)
{
    if (m_crackObject == nullptr || crackStage == m_crackStage) {
        return;
    }
    m_crackStage = crackStage;
    m_crackObject->clear();
    if (crackStage < 0) {
        m_crackObject->setVisible(false);
        return;
    }

    constexpr Ogre::Real low = -OutlineInset * 2.f;
    constexpr Ogre::Real high = 1.f + OutlineInset * 2.f;
    constexpr Ogre::Real center = 0.5f;
    m_crackObject->begin("HelloMine3D/Outline",
                         Ogre::RenderOperation::OT_LINE_LIST);
    const int arms = std::clamp(crackStage + 1, 1, 10);
    for (int arm = 0; arm < arms; ++arm) {
        const Ogre::Real u = 0.08f +
            static_cast<Ogre::Real>((arm * 37) % 83) / 100.f;
        const Ogre::Real v = 0.08f +
            static_cast<Ogre::Real>((arm * 53 + 19) % 83) / 100.f;
        const Ogre::Real midU = (center + u) * 0.5f +
                                (arm % 2 == 0 ? 0.035f : -0.035f);
        const Ogre::Real midV = (center + v) * 0.5f;
        addEdge(*m_crackObject, center, center, low,
                midU, midV, low);
        addEdge(*m_crackObject, midU, midV, low, u, v, low);
        addEdge(*m_crackObject, center, center, high,
                midU, midV, high);
        addEdge(*m_crackObject, midU, midV, high, u, v, high);
        addEdge(*m_crackObject, low, center, center,
                low, midU, midV);
        addEdge(*m_crackObject, low, midU, midV, low, u, v);
        addEdge(*m_crackObject, high, center, center,
                high, midU, midV);
        addEdge(*m_crackObject, high, midU, midV, high, u, v);
        addEdge(*m_crackObject, center, low, center,
                midU, low, midV);
        addEdge(*m_crackObject, midU, low, midV, u, low, v);
        addEdge(*m_crackObject, center, high, center,
                midU, high, midV);
        addEdge(*m_crackObject, midU, high, midV, u, high, v);
    }
    m_crackObject->end();
    m_crackObject->setVisible(true);
}

void OgreBlockOutline::update(const BlockSelection *selection,
                              int crackStage)
{
    if (selection == nullptr) {
        m_node->setVisible(false);
        rebuildCracks(-1);
        return;
    }

    const glm::ivec3 &position = selection->blockPosition;
    m_node->setPosition(static_cast<Ogre::Real>(position.x),
                        static_cast<Ogre::Real>(position.y),
                        static_cast<Ogre::Real>(position.z));
    m_node->setVisible(true);
    rebuildCracks(crackStage);
}
