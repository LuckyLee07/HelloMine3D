#include "SandboxRuntime.h"

#include <chrono>

#include <SFML/Window/Mouse.hpp>
#include <imgui.h>

#include "../Diagnostics/RuntimePerformanceCapture.h"
#include "../Diagnostics/RuntimeRenderCapture.h"
#include "../Maths/Ray.h"
#include "../Renderer/RenderMaster.h"
#include "../World/Block/BlockDatabase.h"
#include "../World/Block/BlockId.h"
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
    if (!RuntimeRenderCapture::isEnabled() &&
        !RuntimePerformanceCapture::isEnabled()) {
        m_player.handleInput(m_window, keyboard);
        handlePlayerInteraction();
    }

    runFixedTicks(dt);
    m_camera.update();

    World *world = m_worldManager.getActiveWorld();
    if (world != nullptr) {
        world->update(m_camera);
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

void SandboxRuntime::handlePlayerInteraction()
{
    World *world = m_worldManager.getActiveWorld();
    if (world == nullptr) {
        return;
    }

    glm::vec3 lastPosition = m_player.position;

    for (Ray ray({m_player.position.x, m_player.position.y + 0.6f,
                  m_player.position.z},
                 m_player.rotation);
         ray.getLength() < 6.f; ray.step(0.05f)) {
        int x = World::toBlockCoord(ray.getEnd().x);
        int y = World::toBlockCoord(ray.getEnd().y);
        int z = World::toBlockCoord(ray.getEnd().z);

        auto block = world->getBlock(x, y, z);
        auto id = static_cast<BlockId>(block.id);

        if (id != BlockId::Air && id != BlockId::Water) {
            if (m_interactionTimer.getElapsedTime().asSeconds() > 0.2f) {
                if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                    m_interactionTimer.restart();
                    world->addEvent<PlayerDigEvent>(sf::Mouse::Button::Left,
                                                    ray.getEnd(), m_player);
                    break;
                }
                else if (sf::Mouse::isButtonPressed(
                             sf::Mouse::Button::Right)) {
                    m_interactionTimer.restart();
                    world->addEvent<PlayerDigEvent>(sf::Mouse::Button::Right,
                                                    lastPosition, m_player);
                    break;
                }
            }
        }

        lastPosition = ray.getEnd();
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
    }
    ImGui::End();
}
