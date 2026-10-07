#pragma once

class World;
class Player;
class Camera;

// Explicit first-creation sample only. The bootstrap rejects non-empty save
// directories before constructing World; normal reopening never calls this.
// v2 source: media/sources/reference-visual-scene-v2.json, exported deterministically
// to finite regions covering a 48m site and two distinct reusable kit layouts.
bool buildReferenceVisualScene(World& world, Player& player, Camera& camera);
