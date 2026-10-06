#pragma once

class World;
class Player;
class Camera;

// Explicit first-creation sample only. The bootstrap rejects non-empty save
// directories before constructing World; normal reopening never calls this.
bool buildReferenceVisualScene(World& world, Player& player, Camera& camera);
