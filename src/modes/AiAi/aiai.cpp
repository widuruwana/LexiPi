#include <iostream>
#include <vector>
#include <numeric>
#include <thread>
#include <chrono>
#include <future>

#include "../../../include/modes/AiAi/aiai.h"
#include "../../../include/engine/board.h"
#include "../../../include/move.h"
#include "../../../include/engine/tiles.h"
#include "../../../include/engine/rack.h"
#include "../../../include/engine/dictionary.h"
#include "../../../include/engine/game_director.h"
#include "../../../include/choices.h"
#include "../../../include/ai_player.h"
#include "../../../include/modes/Home/home.h"
#include "../../../include/interface/renderer.h"
#include "../../../include/interface/console_observer.h"

using namespace std;

// Wrapper for running a single game (Used only for singular execution if needed)
MatchResult runSingleGame(AIStyle s1, AIStyle s2, int id, bool verbose) {
    AIPlayer bot1(s1);
    AIPlayer bot2(s2);
    Board b = createBoard();
    GameDirector::Config cfg;
    ConsoleObserver obs;

    if (verbose) {
        cfg.observer = &obs;
        cfg.delayMs = 1500; // Optional: adds a slight delay so you can read the board
    } else {
        cfg.observer = nullptr; // Pure RAM speed, no I/O bottleneck
    }

    cfg.allowChallenge = true;
    cfg.sixPassEndsGame = true;
    GameDirector director(&bot1, &bot2, b, cfg);
    return director.run(id);
}

// OPTIMIZED BATCH: Instantiates bots ONCE and reuses them
vector<MatchResult> runBatch(AIStyle s1, AIStyle s2, int startId, int count, bool verbose) {
    AIPlayer bot1(s1);
    AIPlayer bot2(s2);
    Board b = createBoard();

    GameDirector::Config cfg;
    ConsoleObserver obs;

    cfg.observer = verbose ? &obs : nullptr;
    cfg.allowChallenge = true;
    cfg.sixPassEndsGame = true;

    GameDirector director(&bot1, &bot2, b, cfg);

    vector<MatchResult> results;
    results.reserve(count);

    for(int i = 0; i < count; i++) {
        // Clear memory banks before game
        bot1.reset();
        bot2.reset();

        MatchResult res = director.run(startId + i);
        results.push_back(res);
    }
    return results;
}

void runAiAi() {
    int numGames = 0;
    bool verbose = false;
    int matchType = 0;

    std::cout << "\n=========================================\n";
    std::cout << "           AI vs AI SIMULATION\n";
    std::cout << "=========================================\n";
    std::cout << "Select Matchup:\n";
    std::cout << "1. Speedi_Pi vs Speedi_Pi (Pure Speed Test)\n";
    std::cout << "2. Cutie_Pi  vs Speedi_Pi (Smart vs Fast)\n";
    std::cout << "3. Cutie_Pi  vs Cutie_Pi  (Clash of Titans)\n";
    std::cout << "Choice: ";

    // Error Handling: Validate numeric input
    if (!(std::cin >> matchType) || matchType < 1 || matchType > 3) {
        std::cout << "[ERROR] Invalid matchup selection. Aborting.\n";
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return;
    }

    AIStyle styleP1 = (matchType == 1) ? AIStyle::SPEEDI_PI : AIStyle::CUTIE_PI;
    AIStyle styleP2 = (matchType == 3) ? AIStyle::CUTIE_PI : AIStyle::SPEEDI_PI;

    std::cout << "Enter number of games to simulate: ";
    if (!(std::cin >> numGames) || numGames <= 0) {
        std::cout << "[ERROR] Invalid number of games. Aborting.\n";
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return;
    }

    std::cout << "Watch the games? (1 = Yes, 0 = No/Fast): ";
    if (!(std::cin >> verbose)) {
        std::cout << "[ERROR] Invalid verbose selection. Aborting.\n";
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        return;
    }

    if (!gMutableDictionary.loadFromFile("csw24.txt")) {
        std::cout << "[ERROR] Dictionary 'csw24.txt' not found. Aborting.\n";
        return;
    }

    auto startTotal = std::chrono::high_resolution_clock::now();

    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 6; // Fallback for undetected hardware

    std::vector<MatchResult> results;

    // =======================================================
    // BIFURCATED EXECUTION PIPELINE
    // =======================================================
    if (matchType == 1) {
        // PIPELINE A: GAME-LEVEL PARALLELISM (Greedy vs Greedy)
        // Spawns multiple independent games concurrently.
        // Note: With verbose=true, std::cout from different threads will interleave.
        std::cout << "Launching " << numGames << " games across " << numThreads << " threads (Game-Level Threading)..." << std::endl;

        std::vector<std::thread> threads;
        std::vector<std::vector<MatchResult>> allResults(numThreads);

        int baseGames = numGames / numThreads;
        int remainder = numGames % numThreads;
        int currentId = 1;

        for (int t = 0; t < numThreads; t++) {
            int count = baseGames + (t < remainder ? 1 : 0);
            if (count > 0) {
                threads.emplace_back([&, t, currentId, count, verbose]() {
                    allResults[t] = runBatch(styleP1, styleP2, currentId, count, verbose);
                });
                currentId += count;
            }
        }

        // Block until all game threads finish
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }

        // Aggregate results sequentially
        results.reserve(numGames);
        for (const auto& batch : allResults) {
            results.insert(results.end(), batch.begin(), batch.end());
        }
    } else {
        // PIPELINE B: SEARCH-LEVEL PARALLELISM (MCTS Involved)
        // Runs games sequentially here, allowing Vanguard to monopolize all threads for Root Parallelism.
        std::cout << "Running " << numGames << " sequential games (MCTS will use " << numThreads << " threads internally)..." << std::endl;
        results = runBatch(styleP1, styleP2, 1, numGames, verbose);
    }

    auto endTotal = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTotal - startTotal).count();

    // =======================================================
    // STATISTICS AGGREGATION
    // =======================================================
    long long totalP1 = 0, totalP2 = 0;
    int winsP1 = 0, winsP2 = 0, draws = 0;

    for (const auto& res : results) {
        totalP1 += res.scoreP1;
        totalP2 += res.scoreP2;
        if (res.winner == 0) winsP1++;
        else if (res.winner == 1) winsP2++;
        else draws++;
    }

    std::string nameP1 = (styleP1 == AIStyle::SPEEDI_PI ? "Speedi_Pi" : "Cutie_Pi");
    std::string nameP2 = (styleP2 == AIStyle::SPEEDI_PI ? "Speedi_Pi" : "Cutie_Pi");
    if (matchType == 1 || matchType == 3) nameP2 = "Evil " + nameP2;

    std::cout << "\n\n=========================================\n";
    std::cout << "           SIMULATION RESULTS            \n";
    std::cout << "=========================================\n";
    std::cout << "Matchup: " << nameP1 << " vs " << nameP2 << "\n";
    std::cout << "Games played: " << numGames << "\n";
    std::cout << "Time Elapsed: " << (elapsedMs / 1000.0) << " seconds\n";
    if (matchType == 1) {
        std::cout << "Throughput:   " << (numGames / (elapsedMs / 1000.0)) << " games/sec\n";
    }
    std::cout << "=========================================\n";
    std::cout << nameP1 << " Wins: " << winsP1 << " (" << (winsP1 * 100.0 / numGames) << "%)\n";
    std::cout << nameP2 << " Wins: " << winsP2 << " (" << (winsP2 * 100.0 / numGames) << "%)\n";
    std::cout << "Draws: " << draws << "\n";
    std::cout << "Avg Score (" << nameP1 << "): " << (totalP1 / (double)numGames) << "\n";
    std::cout << "Avg Score (" << nameP2 << "): " << (totalP2 / (double)numGames) << "\n";
    std::cout << "Combined Avg: " << ((totalP1 + totalP2) / (double)numGames / 2.0) << "\n";
    std::cout << "=========================================\n";

    Renderer::waitForQuitKey();
    Renderer::clearScreen();
}