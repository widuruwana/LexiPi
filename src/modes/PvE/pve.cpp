#include "../../../include/modes/PvE/pve.h"

#include <iostream>
#include <string>
#include <sstream>
#include <chrono>

#include "../../../include/engine/board.h"
#include "../../../include/move.h"
#include "../../../include/engine/dictionary.h"
#include "../../../include/human_player.h"
#include "../../../include/ai_player.h"
#include "../../../include/engine/game_director.h"
#include "../../../include/interface/renderer.h"
#include "../../../include/interface/console_observer.h"

using namespace std;

void runPvE() {
    if (gDictionary.nodes.empty()) gMutableDictionary.loadFromFile("csw24.txt");

    cout << "\n=========================================\n";
    cout << "           SELECT OPPONENT\n";
    cout << "=========================================\n";
    cout << "1. Speedi_Pi (Fast, Heuristic)\n";
    cout << "2. Cutie_Pi  (Smart, Spectre Engine)\n";
    cout << "Choice: ";

    int choice;
    if (!(cin >> choice)) {
        cin.clear(); cin.ignore(1000, '\n');
        choice = 1;
    }
    AIStyle style = (choice == 2) ? AIStyle::CUTIE_PI : AIStyle::SPEEDI_PI;

    // 1. Setup Controllers
    HumanPlayer p1;
    AIPlayer p2(style);

    // 2. Setup Director
    Board b = createBoard();
    GameDirector::Config cfg;
    ConsoleObserver obs;
    cfg.observer = &obs;
    cfg.allowChallenge = true;
    cfg.sixPassEndsGame = true;

    GameDirector director(&p1, &p2, b, cfg);

    // 3. Run
    director.run();

    Renderer::waitForQuitKey();
    Renderer::clearScreen();
}





















