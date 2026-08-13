#ifndef CHESTCONTAINER_H_INCLUDED
#define CHESTCONTAINER_H_INCLUDED

#include <optional>

#include "../../Item/ContainerInventory.h"
#include "../../Maths/glm.h"

class Player;
class World;

struct ChestContainerView {
    glm::ivec3 position{0};
    ContainerInventory inventory;
};

class ChestContainer {
  public:
    static constexpr int SlotCount = 9;
    static constexpr const char *BlockEntityType = "hellomine:chest";

    static bool initialize(World &world, const glm::ivec3 &position);
    static bool open(World &world, Player &player,
                     const glm::ivec3 &position);
    static void close(Player &player) noexcept;
    static std::optional<ChestContainerView>
    view(World &world, const Player &player);
    static bool transferFromPlayer(World &world, Player &player,
                                   int playerSlot, int amount);
    static bool transferToPlayer(World &world, Player &player,
                                 int containerSlot, int amount);
    static int spillContents(World &world, const glm::ivec3 &position);
};

#endif // CHESTCONTAINER_H_INCLUDED
