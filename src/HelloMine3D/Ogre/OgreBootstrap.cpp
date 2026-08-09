#include "OgreBootstrap.h"

#include <OIS.h>
#include <Ogre.h>
#include <OgreGL3PlusPlugin.h>
#include <OgreWindowEventUtilities.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
    constexpr const char* ConfigFileName = "Mine.cfg";
    constexpr const char* ResourceFileName = "MineResources.cfg";
    constexpr const char* LogFileName = "MineOgre.log";
    constexpr const char* WindowTitle = "HelloMine3D - Ogre Bootstrap";
    constexpr const char* SkyboxMaterial = "HelloMine3D/Skybox";

    class OgreBootstrap final : public Ogre::FrameListener,
                                public Ogre::WindowEventListener,
                                public OIS::KeyListener,
                                public OIS::MouseListener
    {
      public:
        ~OgreBootstrap() override
        {
            shutdown();
        }

        bool validate()
        {
            createRoot();
            const std::size_t resourceLocations = configureResources();
            Ogre::RenderSystem* renderSystem = configureRenderSystem();

            std::cout << "[OGRE_VALIDATION] renderer="
                      << renderSystem->getName() << '\n';
            std::cout << "[OGRE_VALIDATION] resource_locations="
                      << resourceLocations << '\n';
            std::cout << "[OGRE_VALIDATION] ois_version="
                      << OIS::InputManager::getVersionNumber() << '\n';

            return resourceLocations > 0;
        }

        int run()
        {
            createRoot();
            configureResources();
            configureRenderSystem();
            createWindowAndScene();
            createInput();

            m_root->addFrameListener(this);
            Ogre::WindowEventUtilities::addWindowEventListener(m_window, this);
            m_listenersInstalled = true;
            m_root->startRendering();
            return EXIT_SUCCESS;
        }

      private:
        void createRoot()
        {
            m_root = std::make_unique<Ogre::Root>(
                "", ConfigFileName, LogFileName);
            m_gl3PlusPlugin = std::make_unique<Ogre::GL3PlusPlugin>();
            m_root->installPlugin(m_gl3PlusPlugin.get());
        }

        std::size_t configureResources()
        {
            Ogre::ConfigFile resourceConfig;
            resourceConfig.load(ResourceFileName);

            std::size_t locationCount = 0;
            Ogre::ConfigFile::SectionIterator sections =
                resourceConfig.getSectionIterator();
            while (sections.hasMoreElements())
            {
                const Ogre::String group = sections.peekNextKey();
                const Ogre::ConfigFile::SettingsMultiMap* settings =
                    sections.getNext();
                for (const auto& setting : *settings)
                {
                    Ogre::ResourceGroupManager::getSingleton()
                        .addResourceLocation(setting.second, setting.first,
                                             group, true);
                    ++locationCount;
                }
            }
            return locationCount;
        }

        Ogre::RenderSystem* configureRenderSystem()
        {
            Ogre::RenderSystem* selected = nullptr;
            if (m_root->restoreConfig())
            {
                selected = m_root->getRenderSystem();
            }

            if (selected == nullptr)
            {
                const Ogre::RenderSystemList& renderers =
                    m_root->getAvailableRenderers();
                for (Ogre::RenderSystem* renderer : renderers)
                {
                    if (renderer != nullptr &&
                        renderer->getName().find("OpenGL 3+") !=
                            Ogre::String::npos)
                    {
                        selected = renderer;
                        break;
                    }
                }
            }

            if (selected == nullptr)
            {
                throw std::runtime_error(
                    "OpenGL 3+ render system was not registered.");
            }

            m_root->setRenderSystem(selected);
            setOptionIfAvailable(*selected, "Full Screen", "No");
            setOptionIfAvailable(*selected, "VSync", "Yes");
            setOptionIfAvailable(*selected, "FSAA", "0");
            selectWindowSize(*selected, "1280 x 720");
            return selected;
        }

        static void setOptionIfAvailable(Ogre::RenderSystem& renderSystem,
                                         const Ogre::String& name,
                                         const Ogre::String& value)
        {
            const Ogre::ConfigOptionMap& options =
                renderSystem.getConfigOptions();
            if (options.find(name) != options.end())
            {
                renderSystem.setConfigOption(name, value);
            }
        }

        static void selectWindowSize(Ogre::RenderSystem& renderSystem,
                                     const Ogre::String& preferred)
        {
            const Ogre::ConfigOptionMap& options =
                renderSystem.getConfigOptions();
            const auto option = options.find("Video Mode");
            if (option == options.end())
            {
                return;
            }

            const Ogre::StringVector& values = option->second.possibleValues;
            if (std::find(values.begin(), values.end(), preferred) !=
                values.end())
            {
                renderSystem.setConfigOption("Video Mode", preferred);
            }
        }

        void createWindowAndScene()
        {
            m_window = m_root->initialise(true, WindowTitle);
            if (m_window == nullptr)
            {
                throw std::runtime_error("Ogre failed to create a window.");
            }

            m_window->setDeactivateOnFocusChange(false);
            m_sceneManager = m_root->createSceneManager(
                Ogre::ST_GENERIC, "HelloMine3DScene");
            m_camera = m_sceneManager->createCamera("PlayerCamera");
            m_camera->setPosition(0.0f, 1.0f, 5.0f);
            m_camera->lookAt(0.0f, 1.0f, 0.0f);
            m_camera->setNearClipDistance(0.1f);
            m_camera->setFarClipDistance(10000.0f);
            m_camera->setFOVy(Ogre::Degree(90.0f));
            m_camera->setFixedYawAxis(true, Ogre::Vector3::UNIT_Y);

            Ogre::Viewport* viewport = m_window->addViewport(m_camera);
            viewport->setBackgroundColour(
                Ogre::ColourValue(0.2f, 0.55f, 0.85f));
            updateAspectRatio();

            Ogre::ResourceGroupManager::getSingleton()
                .initialiseAllResourceGroups();
            m_sceneManager->setAmbientLight(
                Ogre::ColourValue(0.7f, 0.7f, 0.7f));
            m_sceneManager->setSkyBox(
                true, SkyboxMaterial, 5000.0f, true);

            const char* exitFrames =
                std::getenv("HELLOMINE3D_OGRE_EXIT_AFTER_FRAMES");
            if (exitFrames != nullptr)
            {
                m_exitAfterFrames = std::max(0, std::atoi(exitFrames));
            }
        }

        void createInput()
        {
            std::size_t windowHandle = 0;
            m_window->getCustomAttribute("WINDOW", &windowHandle);

            std::ostringstream handleText;
            handleText << windowHandle;
            OIS::ParamList parameters;
            parameters.insert({"WINDOW", handleText.str()});
#if defined(OIS_WIN32_PLATFORM)
            parameters.insert({"w32_mouse", "DISCL_FOREGROUND"});
            parameters.insert({"w32_mouse", "DISCL_NONEXCLUSIVE"});
            parameters.insert({"w32_keyboard", "DISCL_FOREGROUND"});
            parameters.insert({"w32_keyboard", "DISCL_NONEXCLUSIVE"});
#endif

            m_inputManager =
                OIS::InputManager::createInputSystem(parameters);
            m_keyboard = static_cast<OIS::Keyboard*>(
                m_inputManager->createInputObject(OIS::OISKeyboard, true));
            m_mouse = static_cast<OIS::Mouse*>(
                m_inputManager->createInputObject(OIS::OISMouse, true));
            m_keyboard->setEventCallback(this);
            m_mouse->setEventCallback(this);
            updateMouseBounds();
        }

        bool frameRenderingQueued(const Ogre::FrameEvent& event) override
        {
            Ogre::WindowEventUtilities::messagePump();
            if (m_shutdownRequested || m_window == nullptr ||
                m_window->isClosed())
            {
                return false;
            }

            m_keyboard->capture();
            m_mouse->capture();
            updateCamera(event.timeSinceLastFrame);

            ++m_frameCount;
            return m_exitAfterFrames <= 0 ||
                   m_frameCount < m_exitAfterFrames;
        }

        void updateCamera(float deltaSeconds)
        {
            Ogre::Vector3 movement = Ogre::Vector3::ZERO;
            if (m_keyboard->isKeyDown(OIS::KC_W))
            {
                movement.z -= 1.0f;
            }
            if (m_keyboard->isKeyDown(OIS::KC_S))
            {
                movement.z += 1.0f;
            }
            if (m_keyboard->isKeyDown(OIS::KC_A))
            {
                movement.x -= 1.0f;
            }
            if (m_keyboard->isKeyDown(OIS::KC_D))
            {
                movement.x += 1.0f;
            }

            if (movement.squaredLength() > 0.0f)
            {
                movement.normalise();
                m_camera->moveRelative(
                    movement * (m_moveSpeed * deltaSeconds));
            }
            if (m_keyboard->isKeyDown(OIS::KC_SPACE))
            {
                m_camera->move(Ogre::Vector3::UNIT_Y *
                               (m_moveSpeed * deltaSeconds));
            }
            if (m_keyboard->isKeyDown(OIS::KC_C) ||
                m_keyboard->isKeyDown(OIS::KC_LCONTROL))
            {
                m_camera->move(Ogre::Vector3::NEGATIVE_UNIT_Y *
                               (m_moveSpeed * deltaSeconds));
            }
        }

        bool keyPressed(const OIS::KeyEvent& event) override
        {
            if (event.key == OIS::KC_ESCAPE)
            {
                m_shutdownRequested = true;
            }
            return true;
        }

        bool keyReleased(const OIS::KeyEvent&) override
        {
            return true;
        }

        bool mouseMoved(const OIS::MouseEvent& event) override
        {
            m_camera->yaw(
                Ogre::Degree(-event.state.X.rel * m_lookSensitivity));
            m_camera->pitch(
                Ogre::Degree(-event.state.Y.rel * m_lookSensitivity));
            return true;
        }

        bool mousePressed(const OIS::MouseEvent&,
                          OIS::MouseButtonID) override
        {
            return true;
        }

        bool mouseReleased(const OIS::MouseEvent&,
                           OIS::MouseButtonID) override
        {
            return true;
        }

        void windowResized(Ogre::RenderWindow*) override
        {
            updateAspectRatio();
            updateMouseBounds();
        }

        void windowClosed(Ogre::RenderWindow* window) override
        {
            if (window == m_window)
            {
                m_shutdownRequested = true;
            }
        }

        void updateAspectRatio()
        {
            if (m_window == nullptr || m_camera == nullptr ||
                m_window->getNumViewports() == 0)
            {
                return;
            }

            const Ogre::Viewport* viewport = m_window->getViewport(0);
            if (viewport->getActualHeight() > 0)
            {
                m_camera->setAspectRatio(
                    static_cast<Ogre::Real>(viewport->getActualWidth()) /
                    static_cast<Ogre::Real>(viewport->getActualHeight()));
            }
        }

        void updateMouseBounds()
        {
            if (m_window == nullptr || m_mouse == nullptr)
            {
                return;
            }

            unsigned int width = 0;
            unsigned int height = 0;
            unsigned int colourDepth = 0;
            int left = 0;
            int top = 0;
            m_window->getMetrics(width, height, colourDepth, left, top);
            const OIS::MouseState& state = m_mouse->getMouseState();
            state.width = static_cast<int>(width);
            state.height = static_cast<int>(height);
        }

        void shutdown()
        {
            if (m_listenersInstalled && m_root != nullptr)
            {
                m_root->removeFrameListener(this);
                Ogre::WindowEventUtilities::removeWindowEventListener(
                    m_window, this);
                m_listenersInstalled = false;
            }

            if (m_inputManager != nullptr)
            {
                if (m_mouse != nullptr)
                {
                    m_inputManager->destroyInputObject(m_mouse);
                    m_mouse = nullptr;
                }
                if (m_keyboard != nullptr)
                {
                    m_inputManager->destroyInputObject(m_keyboard);
                    m_keyboard = nullptr;
                }
                OIS::InputManager::destroyInputSystem(m_inputManager);
                m_inputManager = nullptr;
            }

            m_camera = nullptr;
            m_sceneManager = nullptr;
            m_window = nullptr;
            m_root.reset();
            m_gl3PlusPlugin.reset();
        }

        std::unique_ptr<Ogre::Root> m_root;
        std::unique_ptr<Ogre::GL3PlusPlugin> m_gl3PlusPlugin;
        Ogre::RenderWindow* m_window = nullptr;
        Ogre::SceneManager* m_sceneManager = nullptr;
        Ogre::Camera* m_camera = nullptr;
        OIS::InputManager* m_inputManager = nullptr;
        OIS::Keyboard* m_keyboard = nullptr;
        OIS::Mouse* m_mouse = nullptr;
        bool m_listenersInstalled = false;
        bool m_shutdownRequested = false;
        int m_exitAfterFrames = 0;
        int m_frameCount = 0;
        float m_moveSpeed = 12.0f;
        float m_lookSensitivity = 0.12f;
    };
}

int runOgreBootstrap(bool validateOnly)
{
    try
    {
        OgreBootstrap bootstrap;
        if (validateOnly)
        {
            return bootstrap.validate() ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        return bootstrap.run();
    }
    catch (const Ogre::Exception& exception)
    {
        std::cerr << "Ogre bootstrap failed: "
                  << exception.getFullDescription() << '\n';
    }
    catch (const OIS::Exception& exception)
    {
        std::cerr << "OIS bootstrap failed: " << exception.eText << '\n';
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Ogre bootstrap failed: " << exception.what() << '\n';
    }
    return EXIT_FAILURE;
}
