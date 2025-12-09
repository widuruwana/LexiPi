#include <iostream>
#include "../../../include/modes/Test/test_mode.h"
#include "../../../include/game_engine.h"
#include "../../../include/test_player.h"
#include "../../../include/move.h"

void runTestMode() {
    TestPlayer* p1 = new TestPlayer();
    TestPlayer* p2 = new TestPlayer();

    // P1 plays "HELLO" at 7,7 (Horizontal)
    // Board is 15x15. 7,7 is center.
    // Word: HELLO
    p1->addMove(Move::Play(7, 7, true, "HELLO"));

    // P2 plays "WORLD" at 6,11 (Vertical)
    // Intersects 'O' at 7,11.
    // Rack letters needed: W, R, L, D.
    // Word on board: WORLD.
    // Move::Play takes the rack word.
    p2->addMove(Move::Play(6, 11, false, "WRLD"));

    // After these moves, players will pass or quit.
    // TestPlayer returns Pass() when script is empty.
    // Game ends after 6 passes.

    runGame(p1, p2, false);

    std::cout << "\nTest finished. Press Enter to continue...";
    std::cin.ignore();
    std::cin.get();
}