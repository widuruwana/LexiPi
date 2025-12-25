#include <iostream>
#include <chrono>

#include "../include/dict.h"
#include "../include/modes/AiAi/aiai.h"
#include "../include/modes/PvP/pvp.h"
#include "../include/modes/Home/home.h"
#include "../include/modes/PvE/pve.h"
#include "../include/modes/Training/training_mode.h"
#include "../include/evaluator.h"
#include "../include/ruleset.h"
#include "../include/random.h"
#include "../include/logger.h"

using namespace std;

int main() {
    // Initialize logging
    Logger::getInstance().setLevel(LogLevel::INFO);
    LOG_INFO("=== LEXI_PI Starting ===");
    
    // Initialize RNG with current time (or fixed seed for determinism)
    uint64_t seed = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    Random::getInstance().seed(seed);
    LOG_INFO("RNG initialized with seed: ", seed);
    
    // Initialize ruleset
    Ruleset::getInstance().initialize("CSW24", 50, 7, 15);
    
    // Initialize evaluator (loads weights once)
    if (!Evaluator::getInstance().initialize("data/weights.txt")) {
        LOG_WARN("Failed to load weights, using defaults");
    }
    
    // Load dictionary (cached in DAWG)
    if (!loadDictionary("csw24.txt")) {
        LOG_ERROR("Failed to load dictionary!");
        return 1;
    }
    
    LOG_INFO("=== Initialization Complete ===");

    // selection menu
    while (true) {

        printTitleScreen();

        char mode;

        cout << "=========================================\n";
        cout << "           Welcome to LEXI_PI\n";
        cout << "         Terminal Crossword Game\n";
        cout << "               Version 0.2\n";
        cout << "=========================================\n\n";

        cout << "(1) Player vs Player\n"
             << "(2) Player vs AI\n"
             << "(3) AI vs AI (Simulation)\n"
             << "(4) Word Wizard\n"
             << "(5) How to play\n"
             << "(6) About\n"
             << "(7) Quit\n"
             << "(8) Train AI (RL)\n"
             << "Please select an option: ";
        cin >> mode;

        if (mode == '1') {
            runPvP();
        } else if (mode == '2') {
            runPvE();
        } else if (mode == '3') {
            runAiAi();
        } else if (mode == '4') {
            string word = " ";
            wordWizard(word);
        } else if (mode == '5') {
            showHowToPlayScreen();
        } else if (mode == '6') {
            showAboutScreen();
        } else if (mode == '7') {
            cout << " May your racks be fruitful and your bingos plentiful!\n" << endl;
            break;
        } else if (mode == '8') {
            TrainingMode trainer;
            trainer.run();
        }
    }

    return 0;
}