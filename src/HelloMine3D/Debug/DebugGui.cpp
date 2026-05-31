#include "DebugGui.h"

#include "../Util/ResourcePaths.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_sfml/imgui-SFML.h>
#include <imgui_sfml/imgui_impl_opengl3.h>

namespace DebugGui
{
    bool init(sf::Window* window)
    {
        const bool initialized =
            ImGui::SFML::Init(*window, sf::Vector2f{window->getSize()}) && ImGui_ImplOpenGL3_Init();
        if (initialized)
        {
            static std::string imguiIniPath;
            imguiIniPath = ResourcePaths::bin("imgui.ini");
            ImGui::GetIO().IniFilename = imguiIniPath.c_str();
        }
        return initialized;
    }

    void begin_frame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui::NewFrame();
    }

    void shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::SFML::Shutdown();
    }

    void render()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void event(const sf::Window& window, sf::Event& e)
    {
        ImGui::SFML::ProcessEvent(window, e);
    }

} // namespace DebugGui
