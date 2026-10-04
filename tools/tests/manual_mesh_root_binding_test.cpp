#include "Ogre/ManualMeshVertexAttributes.h"

#include <OgreDefaultHardwareBufferManager.h>
#include <OgreMaterialManager.h>
#include <OgreRoot.h>
#include <OgreVertexIndexData.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
int failures = 0;

void check(const char *name, bool passed)
{
    ++checks;
    failures += !passed;
    std::cout << "[MANUAL_ROOT_TEST] " << (passed ? "PASS " : "FAIL ") << name << '\n';
}

void build(Ogre::ManualObject &object, bool ordinary, bool textured)
{
    object.begin("ManualRootFixture", Ogre::RenderOperation::OT_TRIANGLE_LIST);
    for (int vertex = 0; vertex < 3; ++vertex) {
        object.position(float(vertex), float(vertex + 1), float(vertex + 2));
        if (ordinary) {
            if (textured)
                appendOrdinaryManualVertexAttributes(object, .125f, .375f, .25f, .75f, .8f);
            else
                appendOrdinaryManualVertexAttributes(object);
        }
    }
    object.triangle(0, 1, 2);
    object.end();
}

float read(const Ogre::RenderOperation &operation, unsigned short uv, std::size_t vertex,
           unsigned component = 0)
{
    const auto *element = operation.vertexData->vertexDeclaration->findElementBySemantic(
        Ogre::VES_TEXTURE_COORDINATES, uv);
    if (!element || component >= Ogre::VertexElement::getTypeCount(element->getType()))
        throw std::runtime_error("Missing real ManualObject UV element");
    const auto buffer = operation.vertexData->vertexBufferBinding->getBuffer(element->getSource());
    float result = -1.f;
    buffer->readData(vertex * buffer->getVertexSize() + element->getOffset() + component * sizeof(float),
                     sizeof(result), &result);
    return result;
}

// Match the existing GL3Plus contract: only declared attributes replace
// previously bound pointers. Read the actual Ogre software VBO, without GL.
struct RootBinding {
    Ogre::HardwareVertexBufferSharedPtr buffer;
    std::size_t offset = 0;
    void bind(const Ogre::RenderOperation &operation)
    {
        const auto *element = operation.vertexData->vertexDeclaration->findElementBySemantic(
            Ogre::VES_TEXTURE_COORDINATES, 3);
        if (!element) return;
        buffer = operation.vertexData->vertexBufferBinding->getBuffer(element->getSource());
        offset = element->getOffset();
    }
    float fetch(std::size_t vertex) const
    {
        float result = -1.f;
        buffer->readData(vertex * buffer->getVertexSize() + offset, sizeof(result), &result);
        return result;
    }
};
} // namespace

int main(int argc, char **argv)
{
    try {
        // No renderer, plugin, render window or graphics context is created.
        Ogre::Root root("", "", argc > 1 ? argv[1] : "manual-root-ogre.log");
        Ogre::DefaultHardwareBufferManager buffers;
        auto material = Ogre::MaterialManager::getSingleton().create(
            "ManualRootFixture", Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        Ogre::ManualObject tree("TreeRootFixture");
        tree.begin("ManualRootFixture", Ogre::RenderOperation::OT_TRIANGLE_LIST);
        for (int vertex = 0; vertex < 3; ++vertex) {
            tree.position(float(vertex), 0.f, 0.f);
            tree.textureCoord(0.f, 0.f);
            tree.textureCoord(0.f, 0.f);
            tree.textureCoord(1.f);
            tree.textureCoord(float(331 + vertex * 32));
        }
        tree.triangle(0, 1, 2);
        tree.end();
        Ogre::ManualObject legacy("LegacyPositionOnly"), actor("OrdinaryPositionOnly"),
            item("OrdinaryTextured");
        build(legacy, false, false);
        build(actor, true, false);
        build(item, true, true);
        const auto &treeOp = *tree.getSection(0)->getRenderOperation();
        const auto &legacyOp = *legacy.getSection(0)->getRenderOperation();
        const auto &actorOp = *actor.getSection(0)->getRenderOperation();
        const auto &itemOp = *item.getSection(0)->getRenderOperation();
        const auto *rootElement = actorOp.vertexData->vertexDeclaration->findElementBySemantic(
            Ogre::VES_TEXTURE_COORDINATES, 3);
        check("position-only-object-has-explicit-float-root-element",
            rootElement && rootElement->getType() == Ogre::VET_FLOAT1 &&
            actorOp.vertexData->vertexCount == 3);
        bool zero = true, preserved = true;
        for (std::size_t vertex = 0; vertex < 3; ++vertex) {
            zero &= read(actorOp, 3, vertex) == 0.f && read(itemOp, 3, vertex) == 0.f;
            preserved &= read(itemOp, 0, vertex) == .125f && read(itemOp, 0, vertex, 1) == .375f &&
                read(itemOp, 1, vertex) == .25f && read(itemOp, 1, vertex, 1) == .75f &&
                read(itemOp, 2, vertex) == .8f;
        }
        check("every-ordinary-vbo-vertex-has-zero-owner", zero);
        check("textured-object-keeps-tile-repeat-and-light", preserved);
        RootBinding binding;
        binding.bind(treeOp);
        binding.bind(legacyOp);
        check("negative-old-missing-uv3-retains-positive-tree-buffer",
            binding.fetch(0) == 331.f && binding.fetch(2) == 395.f);
        binding.bind(actorOp);
        check("tree-then-position-only-actor-overwrites-stale-root-binding",
            binding.fetch(0) == 0.f && binding.fetch(2) == 0.f);
        binding.bind(treeOp);
        binding.bind(itemOp);
        check("tree-then-textured-item-overwrites-stale-root-binding",
            binding.fetch(0) == 0.f && binding.fetch(2) == 0.f);
        item.clear();
        build(item, true, true);
        binding.bind(treeOp);
        binding.bind(*item.getSection(0)->getRenderOperation());
        check("geometry-rebuild-replaces-tree-root-with-zero-again",
            binding.fetch(0) == 0.f && binding.fetch(2) == 0.f);
        check("software-validation-creates-no-render-system", root.getRenderSystem() == nullptr);
    }
    catch (const std::exception &error) {
        check("unexpected-exception", false);
        std::cerr << error.what() << '\n';
    }
    std::cout << "[MANUAL_ROOT_TEST] checks=" << checks << " failures=" << failures << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
