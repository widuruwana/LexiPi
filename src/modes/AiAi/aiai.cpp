#include <iostream>
#include <vector>
#include <numeric>
#include <thread>
#include <chrono>
#include <future>
#include <mutex>

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

using namespace std;

// Wrapper for running a single game (Used only for singular execution if needed)
MatchResult runSingleGame(AIStyle s1, AIStyle s2, int id, bool verbose) {
    AIPlayer bot1(s1);
    AIPlayer bot2(s2);
    Board b = createBoard();
    GameDirector::Config cfg;
    cfg.verbose = verbose;
    cfg.allowChallenge = false;
    GameDirector director(&bot1, &bot2, b, cfg);
    return director.run(id);
}

// OPTIMIZED BATCH: Instantiates bots ONCE and reuses them
vector<MatchResult> runBatch(AIStyle s1, AIStyle s2, int startId, int count, bool verbose) {
    // 1. ALLOCATE ONCE (On Stack) - Massive performance gain
    AIPlayer bot1(s1);
    AIPlayer bot2(s2);
    Board b = createBoard(); // Check if createBoard allocates; reusing 'b' is safer.

    GameDirector::Config cfg;
    cfg.verbose = verbose;
    cfg.allowChallenge = false;

    // 2. Create Director ONCE
    GameDirector director(&bot1, &bot2, b, cfg);

    vector<MatchResult> results;
    results.reserve(count);

    // 3. REUSE EVERYTHING
    for(int i = 0; i < count; i++) {
        // director.run() resets the game state internally.
        // We do NOT call runSingleGame() here because that would re-allocate memory.
        results.push_back(director.run(startId + i));
    }
    return results;
}

void runAiAi() {
    int numGames;
    bool verbose;
    int matchType;

    cout << "\n=========================================\n";
    cout << "           AI vs AI SIMULATION\n";
    cout << "=========================================\n";
    cout << "Select Matchup:\n";
    cout << "1. Speedi_Pi vs Speedi_Pi (Pure Speed Test)\n";
    cout << "2. Cutie_Pi  vs Speedi_Pi (Smart vs Fast)\n";
    cout << "3. Cutie_Pi  vs Cutie_Pi  (Clash of Titans)\n";
    cout << "Choice: ";
    cin >> matchType;

    AIStyle styleP1, styleP2;
    if (matchType == 1) { styleP1 = AIStyle::SPEEDI_PI; styleP2 = AIStyle::SPEEDI_PI; }
    else if (matchType == 2) { styleP1 = AIStyle::CUTIE_PI; styleP2 = AIStyle::SPEEDI_PI; }
    else { styleP1 = AIStyle::CUTIE_PI; styleP2 = AIStyle::CUTIE_PI; }

    cout << "Enter number of games to simulate: ";
    cin >> numGames;
    cout << "Watch the games? (1 = Yes, 0 = No/Fast): ";
    cin >> verbose;

    if (!gDictionary.loadFromFile("csw24.txt")) { cout << "Error: Dictionary not found.\n"; return; }

    auto startTotal = chrono::high_resolution_clock::now();

    // =======================================================
    // PARALLEL EXECUTION
    // =======================================================
    int numThreads = thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;

    cout << "Launching " << numGames << " games across " << numThreads << " threads (Raw Threading)..." << endl;

    // DATA STRUCTURES FOR RAW THREADS
    vector<thread> threads;
    vector<vector<MatchResult>> allResults(numThreads); // Pre-allocate result buckets

    int gamesPerThread = numGames / numThreads;
    int currentId = 1;

    for (int t = 0; t < numThreads; t++) {
        int count = gamesPerThread + (t < (numGames % numThreads) ? 1 : 0);

        if (count > 0) {
            // Launch Thread and write to specific slot in 'allResults'
            // We use a lambda to wrap the work
            threads.emplace_back([&, t, currentId, count]() {
                allResults[t] = runBatch(styleP1, styleP2, currentId, count, verbose);
            });
            currentId += count;
        }
    }

    // WAIT FOR COMPLETION
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    // AGGREGATE RESULTS
    vector<MatchResult> results;
    results.reserve(numGames);
    for (const auto& batch : allResults) {
        results.insert(results.end(), batch.begin(), batch.end());
    }

    auto endTotal = chrono::high_resolution_clock::now();
    double elapsedMs = chrono::duration_cast<chrono::milliseconds>(endTotal-startTotal).count();

    // Statistics (Unchanged)
    long long totalP1 = 0, totalP2 = 0;
    int winsP1 = 0, winsP2 = 0, draws = 0;

    for (const auto& res: results) {
        totalP1 += res.scoreP1;
        totalP2 += res.scoreP2;
        if (res.winner == 0) winsP1++;
        else if (res.winner == 1) winsP2++;
        else draws++;
    }

    string nameP1 = (styleP1 == AIStyle::SPEEDI_PI ? "Speedi_Pi" : "Cutie_Pi");
    string nameP2 = (styleP2 == AIStyle::SPEEDI_PI ? "Speedi_Pi" : "Cutie_Pi");
    if (matchType == 1 || matchType == 3) nameP2 = "Evil " + nameP2;

    cout << "\n\n=========================================\n";
    cout << "           SIMULATION RESULTS            \n";
    cout << "=========================================\n";
    cout << "Matchup: " << nameP1 << " vs " << nameP2 << endl;
    cout << "Games played: " << numGames << endl;
    cout << "Time Elapsed: " << (elapsedMs / 1000.0) << " seconds" << endl;
    cout << "Throughput:   " << (numGames / (elapsedMs / 1000.0)) << " games/sec" << endl;
    cout << "=========================================\n";
    cout << nameP1 << " Wins: " << winsP1 << " (" << (winsP1 * 100.0 / numGames) << "%)\n";
    cout << nameP2 << " Wins: " << winsP2 << " (" << (winsP2 * 100.0 / numGames) << "%)\n";
    cout << "Draws: " << draws << "\n";
    cout << "Avg Score (" << nameP1 << "): " << (totalP1 / (double)numGames) << "\n";
    cout << "Avg Score (" << nameP2 << "): " << (totalP2 / (double)numGames) << "\n";
    cout << "Combined Avg: " << ((totalP1 + totalP2) / (double)numGames / 2.0) << "\n";
    cout << "=========================================\n";

    Renderer::waitForQuitKey();
    Renderer::clearScreen();
}