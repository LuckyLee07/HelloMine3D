#pragma once

#include <memory>

namespace Ogre { class SceneManager; class Camera; }
class World;
struct BlockSelection;
struct MiningProgressSnapshot;
struct ActionFeedbackSnapshot;
struct WorldEnvironmentState;

class OgreBlockFeedback
{
  public:
    explicit OgreBlockFeedback(Ogre::SceneManager &sceneManager);
    ~OgreBlockFeedback();
    OgreBlockFeedback(const OgreBlockFeedback &) = delete;
    OgreBlockFeedback &operator=(const OgreBlockFeedback &) = delete;

    void update(World &world, const BlockSelection *selection,
                const MiningProgressSnapshot &mining,
                const ActionFeedbackSnapshot &feedback, const Ogre::Camera &camera);
    void setEnvironment(const WorldEnvironmentState &environment);
    void hideSelection();
    void clear();

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
