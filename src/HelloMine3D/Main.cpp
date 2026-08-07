#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Window.hpp>
#include <SFML/Window/Context.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <glad/glad.h>
#include <iostream>

#include "Application.h"

#include "Diagnostics/RuntimePerformanceCapture.h"
#include "Diagnostics/RuntimeRenderCapture.h"
#include "GL/GLUtils.h"
#include "Debug/DebugGui.h"
#include "Input/Keyboard.h"
#include "Util/Profiler.h"
#include "Util/ResourcePaths.h"
#include "Util/TimeStep.h"

#include <SFML/GpuPreference.hpp>

namespace
{
    using PerfClock = std::chrono::steady_clock;

    void handle_event(const sf::Event& event, sf::Window& window, bool& show_debug_info,
                      bool& close_requested);
    void loadConfig(Config& config);
    void displayInfo();
    double elapsedMilliseconds(PerfClock::time_point start,
                               PerfClock::time_point end);

} // namespace

int main()
{
    Config config;
    loadConfig(config);
    displayInfo();

    sf::ContextSettings context_settings;
    context_settings.depthBits = 24;
    context_settings.stencilBits = 8;
    context_settings.antiAliasingLevel = 4;
#if defined(__APPLE__)
    context_settings.majorVersion = 4;
    context_settings.minorVersion = 1;
#else
    context_settings.majorVersion = 3;
    context_settings.minorVersion = 3;
#endif
#ifndef NDEBUG
    context_settings.attributeFlags = sf::ContextSettings::Debug;
#endif

    sf::Window window;
    if (config.isFullscreen)
    {
        window.create(sf::VideoMode::getDesktopMode(), "HelloMine3D", sf::Style::None,
                      sf::State::Fullscreen, context_settings);
    }
    else
    {
        sf::VideoMode winMode({(unsigned)config.windowX, (unsigned)config.windowY});
        window.create(winMode, "HelloMine3D", sf::State::Windowed, context_settings);
    }

    const bool render_capture_enabled = RuntimeRenderCapture::isEnabled();
    const bool perf_capture_enabled = RuntimePerformanceCapture::isEnabled();
    if (render_capture_enabled || perf_capture_enabled)
    {
        window.setSize({static_cast<unsigned>(config.windowX),
                        static_cast<unsigned>(config.windowY)});
    }

    window.setVerticalSyncEnabled(true);
    if (!window.setActive(true))
    {
        std::cerr << "Failed to activate the window.\n";
        return EXIT_FAILURE;
    }

    auto load_gl_function = [](const char* name) -> void* {
        return reinterpret_cast<void*>(sf::Context::getFunction(name));
    };
    if (!gladLoadGLLoader(load_gl_function))
    {
        std::cerr << "Failed to initialise OpenGL - Is OpenGL linked correctly?\n";
        return EXIT_FAILURE;
    }
    glClearColor(50.0f / 255.0f, 205.0f / 255.0f, 250.0f / 255.0f, 0);
    glViewport(0, 0, window.getSize().x, window.getSize().y);
    glCullFace(GL_BACK);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gl::enable_debugging();

    TimeStep updater{50};
    Profiler profiler;
    bool show_debug_info = false;

    Application app{window, config};

    if (!DebugGui::init(&window))
    {
        std::cerr << "Failed to initialise Imgui.\n";
        return EXIT_FAILURE;
    }

    Keyboard keyboard;

    // -------------------
    // ==== Main Loop ====
    // -------------------
    sf::Clock clock;
    while (window.isOpen())
    {
        RuntimePerformanceCapture::FrameTimings frameTimings;
        const auto frameStart = PerfClock::now();
        const auto debugBeginStart = PerfClock::now();
        DebugGui::begin_frame();
        const auto debugBeginEnd = PerfClock::now();

        bool close_requested = false;
        const auto eventStart = debugBeginEnd;
        while (auto event = window.pollEvent())
        {
            DebugGui::event(window, *event);
            keyboard.update(*event);
            app.on_event(*event);
            handle_event(*event, window, show_debug_info, close_requested);
        }
        const auto eventEnd = PerfClock::now();

        auto dt = clock.restart();
        g_timeElapsed += dt.asSeconds();
        frameTimings.deltaMs =
            static_cast<double>(dt.asSeconds()) * 1000.0;
        frameTimings.eventMs = elapsedMilliseconds(eventStart, eventEnd);

        // Update
        {
            const auto updateStart = PerfClock::now();
            auto& update_profiler = profiler.begin_section("Update");
            app.on_update(keyboard, dt);
            update_profiler.end_section();
            frameTimings.updateMs =
                elapsedMilliseconds(updateStart, PerfClock::now());
        }

        // Render
        {
            const auto renderStart = PerfClock::now();
            auto& render_profiler = profiler.begin_section("Render");
            app.on_render(show_debug_info);
            render_profiler.end_section();
            frameTimings.renderMs =
                elapsedMilliseconds(renderStart, PerfClock::now());
        }

        // Show profiler
        profiler.end_frame();
        if (show_debug_info)
        {
            profiler.gui();
        }

        // --------------------------
        // ==== End Frame ====
        // --------------------------
        const auto debugRenderStart = PerfClock::now();
        DebugGui::render();
        const auto debugRenderEnd = PerfClock::now();
        frameTimings.debugGuiMs =
            elapsedMilliseconds(debugBeginStart, debugBeginEnd) +
            elapsedMilliseconds(debugRenderStart, debugRenderEnd);

        const auto renderCaptureStart = PerfClock::now();
        RuntimeRenderCapture::update(window, dt);
        frameTimings.renderCaptureMs =
            elapsedMilliseconds(renderCaptureStart, PerfClock::now());

        const auto displayStart = PerfClock::now();
        window.display();
        const auto displayEnd = PerfClock::now();
        frameTimings.displayMs =
            elapsedMilliseconds(displayStart, displayEnd);
        frameTimings.frameMs = elapsedMilliseconds(frameStart, displayEnd);

        if (perf_capture_enabled) {
            RuntimePerformanceCapture::recordFrame(frameTimings,
                                                   app.collectDebugStats());
        }

        if (RuntimeRenderCapture::shouldCloseWindow())
        {
            window.close();
        }
        if (RuntimePerformanceCapture::shouldCloseWindow())
        {
            window.close();
        }
        if (close_requested)
        {
            window.close();
        }
    }

    // --------------------------
    // ==== Graceful Cleanup ====
    // --------------------------
    RuntimePerformanceCapture::shutdown();
    DebugGui::shutdown();
    return EXIT_SUCCESS;
}

namespace
{
    void handle_event(const sf::Event& event, sf::Window& window, bool& show_debug_info,
                      bool& close_requested)
    {
        (void)window;

        if (event.is<sf::Event::Closed>())
        {
            close_requested = true;
        }
        else if (auto* key = event.getIf<sf::Event::KeyPressed>())
        {

            switch (key->code)
            {
                case sf::Keyboard::Key::Escape:
                    close_requested = true;
                    break;

                case sf::Keyboard::Key::F1:
                    show_debug_info = !show_debug_info;
                    break;

                default:
                    break;
            }
        }
    }

    /// @brief Self declared function that loads in configuration files as needed.
    /// @param config
    void loadConfig(Config& config)
    {
        const auto configPath = ResourcePaths::bin("config.txt");
        std::ifstream configFile(configPath);
        std::string key;

        // If the config file is missing or "bad"
        if (!configFile.good())
        {
            std::cout << "Configuration file invalid,\n";
            std::cout << "writing 'new' configuration." << "\n";
            std::cout << "\n";

            std::ofstream outfile(configPath);

            if (outfile.is_open())
            {
                outfile << "renderdistance 8\n";
                outfile << "fullscreen 0\n";
                outfile << "windowsize 1600 900\n";
                outfile << "fov 105\n";

                outfile.close();
                configFile.close(); // Close so it can be reopened safely.
            }

            std::cout << "\n";
            std::cout << "New configuration file created." << "\n";
        }

        try
        {
            // Open 'new' config file.
            if (!configFile.is_open())
            {
                configFile.open(configPath);
            }

            // If the file is still creating errors
            if (configFile.fail())
            {
                std::cout << "Error: The program failed to load the configuration files." << "\n";
                std::cout << "To understand why this error may have occured,\n";
                std::cout << "please examine your 'bin/config.txt' file. Thank you." << "\n";

                // Because this is thrown before runtime, no memory needs to be freed.
                throw "Unable to load configuration file.";
            }

            if (configFile.is_open())
            {
                while (configFile >> key)
                {
                    if (key == "renderdistance")
                    {
                        configFile >> config.renderDistance;
                        std::cout << "Config: Render Distance: " << config.renderDistance << '\n';
                    }
                    else if (key == "fullscreen")
                    {
                        configFile >> config.isFullscreen;
                        std::cout << "Config: Full screen mode: " << std::boolalpha
                                  << config.isFullscreen << '\n';
                    }
                    else if (key == "windowsize")
                    {
                        configFile >> config.windowX >> config.windowY;
                        std::cout << "Config: Window Size: " << config.windowX << " x "
                                  << config.windowY << '\n';
                    }
                    else if (key == "fov")
                    {
                        configFile >> config.fov;
                        std::cout << "Config: Field of Vision: " << config.fov << '\n';
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what();
        }
    }

    void displayInfo()
    {
        std::ifstream inFile(ResourcePaths::bin("info.txt"));
        std::string line;
        while (std::getline(inFile, line))
        {
            std::cout << line << "\n";
        }
    }

    double elapsedMilliseconds(PerfClock::time_point start,
                               PerfClock::time_point end)
    {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
} // namespace
