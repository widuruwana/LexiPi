#pragma once

#include "player_controller.h"

// Runs the generic game loop with two controllers.
// If shuffleBag is false, the tile bag is not shuffled (for deterministic testing).
void runGame(PlayerController* p1, PlayerController* p2, bool shuffleBag);