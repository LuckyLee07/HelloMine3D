#pragma once

#include <cstddef>

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

    void update(const BlockSelection *selection, int crackStage = -1);
    static std::size_t crackSegmentCount(int crackStage) noexcept;

  private:
    void rebuildCracks(int crackStage);

    Ogre::SceneManager *m_sceneManager = nullptr;
    Ogre::ManualObject *m_object = nullptr;
    Ogre::ManualObject *m_crackObject = nullptr;
    Ogre::SceneNode *m_node = nullptr;
    int m_crackStage = -2;
};
