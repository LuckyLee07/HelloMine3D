#include "Player.h"

#include "../Renderer/RenderMaster.h"

#include <imgui.h>

void Player::draw(RenderMaster &master)
{
    (void)master;

    if (ImGui::Begin("Player")) {
        for (int i = 0; i < m_inventory.getSlotCount(); i++) {
            const auto &slot = m_inventory.getSlot(i);
            const char *selected =
                i == m_inventory.getSelectedSlot() ? ">" : " ";
            ImGui::Text("%s %s %d", selected, slot.getMaterial().name.c_str(),
                        slot.getNumInStack());
        }
        ImGui::Text("X: %.2f, Y: %.2f, z: %.2f", position.x, position.y,
                    position.z);
    }
    ImGui::End();
}
