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
    std::size_t visibleItemCount = 0;
    std::string message;
};

struct OgreProjectileRendererValidation
{
    bool valid = false;
    std::size_t projectileCount = 0;
    std::string message;
};

class OgreActorRenderer
{
  public:
    static constexpr std::size_t MaxVisibleItems = 256;

    explicit OgreActorRenderer(Ogre::SceneManager& sceneManager);
    ~OgreActorRenderer();

    void sync(const std::vector<ActorSnapshot>& snapshots,
              const glm::vec3& cameraPosition);
    void syncProjectiles(
        const std::vector<CombatProjectileSnapshot>& snapshots);
    void setCastShadows(bool enabled) noexcept;
    void clear();

    static OgreActorRendererValidation validateSnapshots(
        const std::vector<ActorSnapshot>& snapshots);
    static OgreProjectileRendererValidation validateProjectileSnapshots(
        const std::vector<CombatProjectileSnapshot>& snapshots);

  private:
    struct ActorVisual
    {
        Ogre::ManualObject* object = nullptr;
        Ogre::SceneNode* node = nullptr;
        std::string type;
    };

    ActorVisual createVisual(const ActorSnapshot& snapshot);
    void updateVisual(ActorVisual& visual,
                      const ActorSnapshot& snapshot,
                      const glm::vec3& cameraPosition);
    void destroyVisual(ActorVisual& visual);
    ActorVisual createProjectileVisual(CombatProjectileId id);
    void updateProjectileVisual(
        ActorVisual& visual, const CombatProjectileSnapshot& snapshot);

    Ogre::SceneManager* m_sceneManager = nullptr;
    std::unordered_map<ActorId, ActorVisual> m_visuals;
    std::unordered_map<CombatProjectileId, ActorVisual>
        m_projectileVisuals;
    bool m_castShadows = false;
};

#endif // OGREACTORRENDERER_H_INCLUDED
