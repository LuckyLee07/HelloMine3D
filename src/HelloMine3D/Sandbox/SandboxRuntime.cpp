#include "SandboxRuntime.h"

#include <chrono>

#include <SFML/Window/Mouse.hpp>
#include <imgui.h>

#include "../Diagnostics/RuntimePerformanceCapture.h"
#include "../Diagnostics/RuntimeRenderCapture.h"
#include "../Input/ToggleKey.h"
#include "../Renderer/RenderMaster.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Event/PlayerDigEvent.h"
#include "../World/World.h"

SandboxRuntime::SandboxRuntime(sf::Window &window, const Config &config,
                               Camera &camera)
    : m_window(window)
    , m_camera(camera)
    , m_worldManager(config, camera, m_player)
{
    BlockDatabase::get();
    m_camera.hookEntity(m_player);
    m_worldManager.loadWorld(WorldManager::MainWorldId);
    m_camera.update();
}

void SandboxRuntime::onEvent(const sf::Event &event)
{
    (void)event;
}

void SandboxRuntime::update(const Keyboard &keyboard, sf::Time dt)
{
    const bool acceptsPlayerInput = !RuntimeRenderCapture::isEnabled() &&
                                    !RuntimePerformanceCapture::isEnabled();
    if (acceptsPlayerInput) {
        m_player.handleInput(m_window, keyboard);
    }

    runFixedTicks(dt);
    m_camera.update();

    World *world = m_worldManager.getActiveWorld();
    if (world != nullptr) {
        m_blockSelection = BlockSelectionSystem::pick(
            *world, m_camera.position, m_player.rotation);
        if (acceptsPlayerInput) {
            handlePlayerInteraction(*world);
        }

        static ToggleKey resetMeshesKey(sf::Keyboard::Key::C);
        if (resetMeshesKey.isKeyPressed()) {
            world->resetChunkMeshes();
        }
        world->update(m_camera);
    }
    else {
        m_blockSelection.reset();
    }
}

void SandboxRuntime::render(RenderMaster &renderer, bool showDebugInfo)
{
    if (showDebugInfo) {
        m_player.draw(renderer);
    }

    World *world = m_worldManager.getActiveWorld();
    if (world != nullptr) {
        world->renderWorld(renderer, m_camera);
    }
    if (m_blockSelection.has_value()) {
        renderer.drawBlockOutline(m_blockSelection->blockPosition);
    }

    renderer.finishRender(m_window, m_camera);

    if (showDebugInfo && world != nullptr) {
        drawSandboxDebug(*world);
    }
}

WorldDebugStats SandboxRuntime::collectDebugStats()
{
    World *world = m_worldManager.getActiveWorld();
    if (world == nullptr) {
        return WorldDebugStats();
    }

    return world->collectDebugStats();
}

Player &SandboxRuntime::getPlayer()
{
    return m_player;
}

WorldManager &SandboxRuntime::getWorldManager()
{
    return m_worldManager;
}

void SandboxRuntime::handlePlayerInteraction(World &world)
{
    if (!m_blockSelection.has_value() ||
        m_interactionTimer.getElapsedTime().asSeconds() <= 0.2f) {
        return;
    }

    const BlockSelection &selection = *m_blockSelection;
    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        m_interactionTimer.restart();
        world.addEvent<PlayerDigEvent>(PlayerDigAction::Break,
                                       glm::vec3(selection.blockPosition),
                                       m_player);
    }
    else if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
        m_interactionTimer.restart();
        world.addEvent<PlayerDigEvent>(PlayerDigAction::Place,
                                       glm::vec3(selection.placementPosition),
                                       m_player);
    }
}

void SandboxRuntime::runFixedTicks(sf::Time dt)
{
    const auto ticksThisFrame = m_tickScheduler.advance(
        std::chrono::microseconds(dt.asMicroseconds()));
    for (std::size_t tick = 0; tick < ticksThisFrame; ++tick) {
        World *world = m_worldManager.getActiveWorld();
        if (world != nullptr) {
            m_player.update(m_fixedTickStep.asSeconds(), *world);
        }

        m_worldManager.tick();
    }

    RuntimePerformanceCapture::recordSimulationTicks(ticksThisFrame);
}

void SandboxRuntime::drawSandboxDebug(World &world)
{
    const WorldDebugStats stats = world.collectDebugStats();

    if (ImGui::Begin("Sandbox")) {
        ImGui::Text("Seed: %d", stats.terrainSeed);
        ImGui::Text("World time: %.0f", stats.worldTime);
        ImGui::Text("Actors: %llu",
                    static_cast<unsigned long long>(stats.actorCount));

        ImGui::Separator();
        ImGui::Text("Chunks: %llu existing / %llu loaded",
                    static_cast<unsigned long long>(
                        stats.chunks.existingChunks),
                    static_cast<unsigned long long>(stats.chunks.loadedChunks));
        ImGui::Text("Dirty chunks: %llu",
                    static_cast<unsigned long long>(
                        stats.chunks.saveDirtyChunks));
        ImGui::Text("Queued chunk updates: %llu",
                    static_cast<unsigned long long>(
                        stats.queuedChunkUpdates));

        ImGui::Separator();
        ImGui::Text("Sections: %llu",
                    static_cast<unsigned long long>(stats.chunks.sections));
        ImGui::Text("Mesh dirty / CPU / GPU: %llu / %llu / %llu",
                    static_cast<unsigned long long>(
                        stats.chunks.meshDirtySections),
                    static_cast<unsigned long long>(
                        stats.chunks.cpuReadySections),
                    static_cast<unsigned long long>(
                        stats.chunks.gpuBufferedSections));
        ImGui::Text("Mesh rebuilds: %llu",
                    static_cast<unsigned long long>(
                        stats.chunks.meshRebuilds));

        ImGui::Separator();
        if (m_blockSelection.has_value()) {
            const auto &selection = *m_blockSelection;
            const auto &definition =
                BlockDatabase::get().getDefinition(selection.blockId);
            ImGui::Text("Selected: %s (%d, %d, %d)",
                        definition.name.c_str(), selection.blockPosition.x,
                        selection.blockPosition.y,
                        selection.blockPosition.z);
        }
        else {
            ImGui::TextUnformatted("Selected: none");
        }
    }
    ImGui::End();
}
