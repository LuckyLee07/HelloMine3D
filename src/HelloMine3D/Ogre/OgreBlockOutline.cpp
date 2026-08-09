#include "OgreBlockOutline.h"

#include <Ogre.h>

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

    m_node = sceneManager.getRootSceneNode()->createChildSceneNode(
        "SelectedBlockOutlineNode");
    m_node->attachObject(m_object);
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
    if (m_node != nullptr) {
        m_sceneManager->destroySceneNode(m_node);
        m_node = nullptr;
    }
}

void OgreBlockOutline::update(const BlockSelection *selection)
{
    if (selection == nullptr) {
        m_node->setVisible(false);
        return;
    }

    const glm::ivec3 &position = selection->blockPosition;
    m_node->setPosition(static_cast<Ogre::Real>(position.x),
                        static_cast<Ogre::Real>(position.y),
                        static_cast<Ogre::Real>(position.z));
    m_node->setVisible(true);
}
