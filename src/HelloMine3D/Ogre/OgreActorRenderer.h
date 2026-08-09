#ifndef OGREACTORRENDERER_H_INCLUDED
#define OGREACTORRENDERER_H_INCLUDED

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "../Actor/Actor.h"

namespace Ogre
{
    class ManualObject;
    class SceneManager;
    class SceneNode;
}

struct OgreActorRendererValidation
{
    bool valid = false;
    std::size_t actorCount = 0;
    std::size_t mobCount = 0;
    std::size_t itemCount = 0;
    std::string message;
};

class OgreActorRenderer
{
  public:
    explicit OgreActorRenderer(Ogre::SceneManager& sceneManager);
    ~OgreActorRenderer();

    void sync(const std::vector<ActorSnapshot>& snapshots);
    void clear();

    static OgreActorRendererValidation validateSnapshots(
        const std::vector<ActorSnapshot>& snapshots);

  private:
    struct ActorVisual
    {
        Ogre::ManualObject* object = nullptr;
        Ogre::SceneNode* node = nullptr;
        std::string type;
    };

    ActorVisual createVisual(const ActorSnapshot& snapshot);
    void updateVisual(ActorVisual& visual,
                      const ActorSnapshot& snapshot);
    void destroyVisual(ActorVisual& visual);

    Ogre::SceneManager* m_sceneManager = nullptr;
    std::unordered_map<ActorId, ActorVisual> m_visuals;
};

#endif // OGREACTORRENDERER_H_INCLUDED
