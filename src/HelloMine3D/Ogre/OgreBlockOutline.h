#pragma once

namespace Ogre
{
    class ManualObject;
    class SceneManager;
    class SceneNode;
}

struct BlockSelection;

class OgreBlockOutline
{
  public:
    explicit OgreBlockOutline(Ogre::SceneManager &sceneManager);
    ~OgreBlockOutline();

    OgreBlockOutline(const OgreBlockOutline &) = delete;
    OgreBlockOutline &operator=(const OgreBlockOutline &) = delete;

    void update(const BlockSelection *selection);

  private:
    Ogre::SceneManager *m_sceneManager = nullptr;
    Ogre::ManualObject *m_object = nullptr;
    Ogre::SceneNode *m_node = nullptr;
};
