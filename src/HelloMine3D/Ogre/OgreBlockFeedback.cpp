#include "OgreBlockFeedback.h"

#include <Ogre.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "../Feedback/ActionFeedback.h"
#include "../Feedback/BlockSurfaceGeometry.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockTextureCoordinates.h"
#include "../World/Interaction/BlockSelection.h"
#include "../World/World.h"

namespace
{
    Ogre::Vector3 vector(const glm::vec3 &v) { return {v.x, v.y, v.z}; }

    Ogre::MaterialPtr createMaterial(const std::string &name,
                                    const std::string &vertex,
                                    const std::string &fragment, bool decal)
    {
        const bool array = runtimeTerrainMaterialProfile().usesTextureArray();
        const auto &profile = runtimeTerrainMaterialProfile().parameters();
        auto material = Ogre::MaterialManager::getSingleton().create(
            name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        auto *pass = material->getTechnique(0)->getPass(0);
        pass->setLightingEnabled(false);
        pass->setDepthCheckEnabled(true);
        pass->setDepthWriteEnabled(false);
        pass->setCullingMode(Ogre::CULL_NONE);
        pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
        if (decal) pass->setDepthBias(1.f, 1.f);
        pass->setVertexProgram(vertex);
        pass->setFragmentProgram(fragment + (array ? "ArrayFragment" : "Fragment"));
        auto *base = Ogre::MaterialManager::getSingleton().getByName("HelloMine3D/Terrain")
                         ->getTechnique(0)->getPass(0)->getTextureUnitState(0);
        auto *texture = pass->createTextureUnitState();
        texture->setTextureName(base->getTextureName(), base->getTextureType());
        texture->setTextureAddressingMode(array ? Ogre::TextureUnitState::TAM_WRAP
                                               : Ogre::TextureUnitState::TAM_CLAMP);
        texture->setTextureFiltering(Ogre::FT_MIN, Ogre::FO_POINT);
        texture->setTextureFiltering(Ogre::FT_MAG, Ogre::FO_POINT);
        texture->setTextureFiltering(Ogre::FT_MIP, array ? Ogre::FO_LINEAR : Ogre::FO_NONE);
        auto parameters = pass->getFragmentProgramParameters();
        parameters->setNamedConstant("tilesPerRow", static_cast<float>(profile.tilesPerRow));
        if (!array)
        {
            parameters->setNamedConstant("atlasPixels", static_cast<float>(profile.atlasPixels));
            parameters->setNamedConstant("tilePixels", static_cast<float>(profile.tilePixels));
        }
        material->load();
        if (!pass->getFragmentProgram()->isSupported() ||
            pass->getFragmentProgram()->hasCompileError())
            throw std::runtime_error("Invalid block feedback shader: " + fragment);
        return material;
    }

    glm::ivec2 particleTile(World &world, const ActionFeedbackParticle &particle)
    {
        const auto &definition = BlockDatabase::get().getDefinition(particle.blockId);
        const auto &render = definition.render;
        const bool resource = render.meshType == BlockMeshType::Resource;
        auto &chunks = world.getChunkManager();
        const auto &position = particle.blockPosition;
        return TerrainAppearance::select(particle.blockId,
            resource ? TerrainFaceKind::Resource : TerrainFaceKind::Side,
            resource ? render.texTopCoord : render.texSideCoord,
            chunks.getTerrainGenerator().getBiomeAtWorld(position.x, position.z),
            chunks.getTerrainSeed(), position).coordinates;
    }
}

struct OgreBlockFeedback::Impl
{
    Ogre::SceneManager &scene;
    Ogre::ManualObject *surface = nullptr;
    Ogre::ManualObject *particles = nullptr;
    Ogre::SceneNode *surfaceNode = nullptr;
    Ogre::SceneNode *particleNode = nullptr;
    Ogre::MaterialPtr surfaceMaterial;
    Ogre::MaterialPtr floraMaterial;
    Ogre::MaterialPtr particleMaterial;
    glm::ivec3 selectedPosition{0};
    ChunkBlock selectedBlock;
    bool hasSelection = false;

    explicit Impl(Ogre::SceneManager &manager) : scene(manager)
    {
        surfaceMaterial = createMaterial("HelloMine3D/BlockSurfaceFeedback",
            "HelloMine3D/TerrainVertex", "HelloMine3D/BlockFeedback", true);
        floraMaterial = createMaterial("HelloMine3D/FloraSurfaceFeedback",
            "HelloMine3D/FloraVertex", "HelloMine3D/BlockFeedback", true);
        particleMaterial = createMaterial("HelloMine3D/BlockFragments",
            "HelloMine3D/BlockParticleVertex", "HelloMine3D/BlockParticle", false);
        surface = scene.createManualObject("BlockSurfaceFeedback");
        particles = scene.createManualObject("BlockFragments");
        for (auto *object : {surface, particles})
        {
            object->setCastShadows(false);
            object->setDynamic(true);
            object->setRenderQueueGroup(Ogre::RENDER_QUEUE_9);
            object->setVisible(false);
        }
        surfaceNode = scene.getRootSceneNode()->createChildSceneNode("BlockSurfaceFeedbackNode");
        particleNode = scene.getRootSceneNode()->createChildSceneNode("BlockFragmentsNode");
        surfaceNode->attachObject(surface);
        particleNode->attachObject(particles);
    }

    ~Impl()
    {
        for (auto *object : {surface, particles})
        {
            object->detachFromParent();
            scene.destroyManualObject(object);
        }
        scene.destroySceneNode(surfaceNode);
        scene.destroySceneNode(particleNode);
        for (const auto &material : {surfaceMaterial, floraMaterial, particleMaterial})
            Ogre::MaterialManager::getSingleton().remove(material->getName());
    }

    void updateSelection(World &world, const BlockSelection *selection,
                         const MiningProgressSnapshot &mining)
    {
        if (selection == nullptr)
        {
            surface->setVisible(false);
            hasSelection = false;
            return;
        }
        const auto &position = selection->blockPosition;
        const auto block = world.getBlock(position.x, position.y, position.z);
        // Selection is computed before world commands commit; hide stale targets.
        if (block.id != static_cast<Block_t>(selection->blockId) || block == BlockId::Air)
        {
            surface->setVisible(false);
            hasSelection = false;
            return;
        }
        const auto &definition = BlockDatabase::get().getDefinition(selection->blockId);
        const bool flora = definition.render.shaderType == BlockShaderType::Flora;
        const auto &material = flora ? floraMaterial : surfaceMaterial;
        if (!hasSelection || position != selectedPosition || block != selectedBlock)
        {
            auto &chunks = world.getChunkManager();
            const auto faces = blockSurfaceGeometry(definition, block, position,
                chunks.getTerrainGenerator().getBiomeAtWorld(position.x, position.z),
                chunks.getTerrainSeed());
            surface->clear();
            surface->begin(material->getName(), Ogre::RenderOperation::OT_TRIANGLE_LIST);
            const std::array<glm::vec2, 4> repeat{{{1,1}, {0,1}, {0,0}, {1,0}}};
            unsigned int first = 0;
            for (const auto &face : faces)
            {
                const auto uv = BlockTextureCoordinates::get(face.tile.x, face.tile.y);
                for (std::size_t vertex = 0; vertex < 4; ++vertex)
                {
                    surface->position(face.positions[vertex * 3], face.positions[vertex * 3 + 1],
                                      face.positions[vertex * 3 + 2]);
                    surface->textureCoord(uv[vertex * 2], uv[vertex * 2 + 1]);
                    surface->textureCoord(repeat[vertex].x, repeat[vertex].y);
                    surface->textureCoord(1.f);
                }
                surface->quad(first, first + 1, first + 2, first + 3);
                first += 4;
            }
            surface->end();
            surfaceNode->setPosition(vector(glm::vec3(position)));
            selectedPosition = position;
            selectedBlock = block;
            hasSelection = true;
        }
        auto parameters = material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
        const bool matchingProgress = mining.active && mining.target == position &&
                                      mining.blockId == selection->blockId;
        parameters->setNamedConstant("crackStage", matchingProgress ?
            static_cast<float>(mining.crackStage()) : -1.f);
        parameters->setNamedConstant("highlightStrength", flora ? 0.27f : 0.15f);
        const float seedX = static_cast<float>(World::floorMod(position.x, 97));
        const float seedZ = static_cast<float>(World::floorMod(position.z, 89));
        parameters->setNamedConstant("crackSeed", Ogre::Vector2(seedX * 1.7f, seedZ * 2.3f));
        parameters->setNamedConstant("crackStretch", definition.miningClass == MiningClass::Axe
            ? Ogre::Vector2(1.8f, 0.65f) : Ogre::Vector2(1.f, 1.f));
        const bool cutout = definition.render.shaderType != BlockShaderType::Transparent;
        parameters->setNamedConstant("alphaCutoff",
            runtimeTerrainMaterialProfile().usesTextureArray() && cutout ? 0.4999f : 0.f);
        surface->setVisible(true);
    }

    void updateParticles(World &world, const ActionFeedbackSnapshot &feedback,
                         const Ogre::Camera &camera)
    {
        std::vector<const ActionFeedbackParticle *> visible;
        for (const auto &particle : feedback.particles)
        {
            if (particle.worldSpace && particle.alpha > 0.f &&
                visible.size() < ActionFeedbackTimeline::MaxParticles)
                visible.push_back(&particle);
        }
        // Transparent fragments share one bounded draw and are ordered back to front.
        const auto eye = camera.getDerivedPosition();
        std::stable_sort(visible.begin(), visible.end(), [&](const auto *a, const auto *b)
        {
            return (vector(a->worldPosition) - eye).squaredLength() >
                   (vector(b->worldPosition) - eye).squaredLength();
        });
        if (visible.empty())
        {
            particles->setVisible(false);
            return;
        }
        particles->clear();
        particles->begin(particleMaterial->getName(), Ogre::RenderOperation::OT_TRIANGLE_LIST);
        const auto cameraRight = camera.getDerivedOrientation() * Ogre::Vector3::UNIT_X;
        const auto cameraUp = camera.getDerivedOrientation() * Ogre::Vector3::UNIT_Y;
        const std::array<Ogre::Vector2, 4> corners{{{-1,-1}, {1,-1}, {1,1}, {-1,1}}};
        struct TileEntry { BlockId id; glm::ivec3 position; glm::ivec2 tile; };
        std::vector<TileEntry> tiles;
        unsigned int first = 0;
        for (const auto *particle : visible)
        {
            const float c = std::cos(particle->rotation), s = std::sin(particle->rotation);
            const auto right = (cameraRight * c + cameraUp * s) * particle->size * 0.5f;
            const auto up = (cameraUp * c - cameraRight * s) * particle->size * 0.5f;
            auto cached = std::find_if(tiles.begin(), tiles.end(), [&](const TileEntry &entry)
            {
                return entry.id == particle->blockId && entry.position == particle->blockPosition;
            });
            if (cached == tiles.end())
            {
                tiles.push_back({particle->blockId, particle->blockPosition, particleTile(world, *particle)});
                cached = tiles.end() - 1;
            }
            const auto tile = cached->tile;
            const auto uv = BlockTextureCoordinates::get(tile.x, tile.y);
            const float shade = 0.72f + 0.23f * std::abs(c);
            for (const auto &corner : corners)
            {
                particles->position(vector(particle->worldPosition) + right * corner.x + up * corner.y);
                particles->colour(shade, shade, shade, particle->alpha);
                particles->textureCoord(uv[0], uv[1]);
                particles->textureCoord(particle->textureOffset.x + (corner.x + 1.f) * 0.125f,
                                        particle->textureOffset.y + (1.f - corner.y) * 0.125f);
            }
            particles->quad(first, first + 1, first + 2, first + 3);
            first += 4;
        }
        particles->end();
        particles->setVisible(true);
    }
};

OgreBlockFeedback::OgreBlockFeedback(Ogre::SceneManager &sceneManager)
    : m_impl(std::make_unique<Impl>(sceneManager)) {}
OgreBlockFeedback::~OgreBlockFeedback() = default;

void OgreBlockFeedback::update(World &world, const BlockSelection *selection,
    const MiningProgressSnapshot &mining, const ActionFeedbackSnapshot &feedback,
    const Ogre::Camera &camera)
{
    m_impl->updateSelection(world, selection, mining);
    m_impl->updateParticles(world, feedback, camera);
}

void OgreBlockFeedback::setEnvironment(const WorldEnvironmentState &environment)
{
    auto parameters = m_impl->particleMaterial->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
    parameters->setNamedConstant("environmentLight", environment.daylight);
    parameters->setNamedConstant("fogColour", vector(environment.fogColour));
    parameters->setNamedConstant("fogDensity", environment.fogDensity);
}

void OgreBlockFeedback::hideSelection()
{
    m_impl->surface->setVisible(false);
    m_impl->hasSelection = false;
}

void OgreBlockFeedback::clear()
{
    hideSelection();
    m_impl->particles->setVisible(false);
    m_impl->particles->clear();
    m_impl->surface->clear();
}
