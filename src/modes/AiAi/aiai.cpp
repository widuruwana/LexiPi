#include <iostream>
#include <vector>
#include <numeric>
#include <thread>
#include <chrono>
#include <future>
#include <mutex>
#include <functional>
#include <iomanip>

#include "../../../include/modes/AiAi/aiai.h"
#include "../../../include/engine/board.h"
#include "../../../include/engine/tiles.h"
#include "../../../include/engine/rack.h"
#include "../../../include/engine/dictionary.h"
#include "../../../include/engine/game_director.h"
#include "../../../include/ai_player.h"
#include "../../../include/interface/renderer.h"
#include "../../../include/spectre/engine_config.h"

using namespace std;

// =========================================================
// BATCH RUNNER (Reuses bots across games — zero reallocation)
// =========================================================

static vector<MatchResult> runBatch(
    std::function<std::unique_ptr<PlayerController>()> factory1,
    std::function<std::unique_ptr<PlayerController>()> factory2,
    int startId, int count, bool verbose)
{
    auto bot1 = factory1();
    auto bot2 = factory2();
    Board b = createBoard();

    GameDirector::Config cfg;
    cfg.verbose = verbose;
    cfg.allowChallenge = false;
    cfg.sixPassEndsGame = true;

    GameDirector director(bot1.get(), bot2.get(), b, cfg);

    vector<MatchResult> results;
    results.reserve(count);

    for (int i = 0; i < count; i++) {
        results.push_back(director.run(startId + i));
    }
    return results;
}

// =========================================================
// STATISTICS HELPER
// =========================================================

struct MatchupStats {
    string labelP1;
    string labelP2;
    int totalGames = 0;
    int winsP1 = 0;
    int winsP2 = 0;
    int draws = 0;
    long long totalScoreP1 = 0;
    long long totalScoreP2 = 0;
    double elapsedSec = 0.0;

    void aggregate(const vector<MatchResult>& results) {
        for (const auto& r : results) {
            totalGames++;
            totalScoreP1 += r.scoreP1;
            totalScoreP2 += r.scoreP2;
            if (r.winner == 0) winsP1++;
            else if (r.winner == 1) winsP2++;
            else draws++;
        }
    }

    void print() const {
        cout << "\n+--------------------------------------------------+\n";
        cout << "| " << left << setw(22) << labelP1 << " vs " << setw(22) << labelP2 << "|\n";
        cout << "+--------------------------------------------------+\n";
        cout << "| Games:      " << setw(37) << totalGames << "|\n";
        cout << "| Time:       " << setw(34) << fixed << setprecision(2) << elapsedSec << " s |\n";
        if (elapsedSec > 0)
            cout << "| Throughput: " << setw(30) << fixed << setprecision(1) << (totalGames / elapsedSec) << " games/s |\n";
        cout << "+--------------------------------------------------+\n";
        cout << "| " << setw(12) << labelP1 << " Wins: " << setw(5) << winsP1
             << " (" << setw(5) << fixed << setprecision(1) << (winsP1 * 100.0 / totalGames) << "%)           |\n";
        cout << "| " << setw(12) << labelP2 << " Wins: " << setw(5) << winsP2
             << " (" << setw(5) << fixed << setprecision(1) << (winsP2 * 100.0 / totalGames) << "%)           |\n";
        cout << "| Draws:            " << setw(5) << draws
             << "                              |\n";
        cout << "+--------------------------------------------------+\n";
        cout << "| Avg Score " << setw(10) << labelP1 << ": " << setw(7) << fixed << setprecision(1)
             << (totalScoreP1 / (double)totalGames) << "                    |\n";
        cout << "| Avg Score " << setw(10) << labelP2 << ": " << setw(7) << fixed << setprecision(1)
             << (totalScoreP2 / (double)totalGames) << "                    |\n";
        cout << "| Avg Spread:       " << setw(7) << fixed << setprecision(1)
             << ((totalScoreP1 - totalScoreP2) / (double)totalGames) << "                    |\n";
        cout << "+--------------------------------------------------+\n";
    }
};

// =========================================================
// PARALLEL RUNNER (Distributes games across CPU cores)
// =========================================================

static MatchupStats runParallelMatchup(
    const string& labelP1,
    const string& labelP2,
    std::function<std::unique_ptr<PlayerController>()> factory1,
    std::function<std::unique_ptr<PlayerController>()> factory2,
    int numGames, bool verbose)
{
    auto startTime = chrono::high_resolution_clock::now();

    int numThreads = thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    if (verbose) numThreads = 1; // Verbose mode = single thread for readable output

    vector<thread> threads;
    vector<vector<MatchResult>> allResults(numThreads);

    int gamesPerThread = numGames / numThreads;
    int currentId = 1;

    for (int t = 0; t < numThreads; t++) {
        int count = gamesPerThread + (t < (numGames % numThreads) ? 1 : 0);
        if (count > 0) {
            threads.emplace_back([&, t, currentId, count]() {
                allResults[t] = runBatch(factory1, factory2, currentId, count, verbose);
            });
            currentId += count;
        }
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    auto endTime = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count() / 1000.0;

    MatchupStats stats;
    stats.labelP1 = labelP1;
    stats.labelP2 = labelP2;
    stats.elapsedSec = elapsed;

    for (const auto& batch : allResults) {
        stats.aggregate(batch);
    }

    return stats;
}

// =========================================================
// ABLATION TABLE DEFINITION
// =========================================================

struct AblationRow {
    string label;
    std::function<std::unique_ptr<PlayerController>()> factory;
};

static vector<AblationRow> buildAblationTable() {
    vector<AblationRow> rows;

    // ROW 0: Greedy Baseline (Score + Simple Leave)
    rows.push_back({"Greedy", []() {
        return std::make_unique<GreedyPlayer>("Greedy");
    }});

    // ROW 1: Equity Selection with Hand-Tuned Leaves (No simulation)
    rows.push_back({"Equity_HT", []() {
        auto cfg = spectre::EngineConfig::equity_handtuned();
        return std::make_unique<SpectrePlayer>(cfg, "Equity_HT");
    }});

    // ROW 2: Simulation + Uniform Opponent Model (No inference)
    rows.push_back({"Sim_Uniform", []() {
        auto cfg = spectre::EngineConfig::simulation_uniform();
        return std::make_unique<SpectrePlayer>(cfg, "Sim_Uniform");
    }});

    // ROW 3: Full Spectre (Simulation + Particle Filter)
    rows.push_back({"Full_Spectre", []() {
        auto cfg = spectre::EngineConfig::full_spectre();
        return std::make_unique<SpectrePlayer>(cfg, "Full_Spectre");
    }});

    // ROW 4: Default Spectre (MCTS path — current Cutie_Pi behavior)
    rows.push_back({"MCTS_Default", []() {
        return std::make_unique<SpectrePlayer>("MCTS_Default");
    }});

    return rows;
}

// =========================================================
// ABLATION RUNNER (The Paper's Evaluation Section)
// =========================================================

static void runAblation() {
    if (!gDictionary.loadFromFile("csw24.txt")) { cout << "Error: Dictionary not found.\n"; return; }

    int numGames;
    cout << "\n=========================================\n"
         << "        ABLATION STUDY FRAMEWORK\n"
         << "=========================================\n"
         << "This runs each engine configuration against\n"
         << "the Greedy baseline to measure marginal gains.\n"
         << "=========================================\n\n";

    cout << "Games per matchup (recommend 200+): ";
    cin >> numGames;
    if (numGames <= 0) numGames = 100;

    auto table = buildAblationTable();

    cout << "\nAblation configurations:\n";
    for (size_t i = 0; i < table.size(); i++) {
        cout << "  [" << i << "] " << table[i].label << "\n";
    }
    cout << "\nBaseline: " << table[0].label << "\n";
    cout << "Running " << (table.size() - 1) << " matchups x " << numGames << " games each...\n\n";

    vector<MatchupStats> allStats;

    // Run each non-baseline config against the baseline
    for (size_t i = 1; i < table.size(); i++) {
        cout << ">>> Starting: " << table[0].label << " vs " << table[i].label
             << " (" << numGames << " games)...\n";

        auto stats = runParallelMatchup(
            table[0].label, table[i].label,
            table[0].factory, table[i].factory,
            numGames, false);

        stats.print();
        allStats.push_back(stats);
    }

    // Also run baseline vs itself (sanity check — should be ~50/50)
    cout << "\n>>> Sanity Check: " << table[0].label << " vs " << table[0].label << "...\n";
    auto sanity = runParallelMatchup(
        table[0].label, "Evil_" + table[0].label,
        table[0].factory, table[0].factory,
        numGames, false);
    sanity.print();

    // Summary Table
    cout << "\n\n=========================================================\n";
    cout << "                  ABLATION SUMMARY\n";
    cout << "=========================================================\n";
    cout << left << setw(18) << "Configuration" << setw(10) << "Win%" << setw(12) << "Avg Spread"
         << setw(12) << "Games/s" << "\n";
    cout << "---------------------------------------------------------\n";

    cout << setw(18) << "Greedy (base)" << setw(10) << "50.0"
         << setw(12) << "0.0" << setw(12) << "-" << "\n";

    for (const auto& s : allStats) {
        double winPct = (s.winsP2 * 100.0 / s.totalGames); // P2 is the test config
        double avgSpread = -((s.totalScoreP1 - s.totalScoreP2) / (double)s.totalGames); // Positive = test wins
        double throughput = s.totalGames / s.elapsedSec;

        cout << setw(18) << s.labelP2
             << setw(10) << fixed << setprecision(1) << winPct
             << setw(12) << fixed << setprecision(1) << avgSpread
             << setw(12) << fixed << setprecision(1) << throughput << "\n";
    }
    cout << "=========================================================\n";
    cout << "\nFirst-move advantage expected: ~54.5% for P1 in mirror matches.\n";
    cout << "A delta > 5% from baseline indicates genuine improvement.\n\n";

    Renderer::waitForQuitKey();
    Renderer::clearScreen();
}

// =========================================================
// LEGACY AI vs AI (Original Menu - Preserved for Compat)
// =========================================================

void runAiAi() {
    int mode;
    cout << "\n=========================================\n"
         << "           AI vs AI SIMULATION\n"
         << "=========================================\n"
         << "1. Classic Matchup (Speedi vs Speedi / Cutie vs Speedi / etc)\n"
         << "2. Ablation Study (Systematic component evaluation)\n"
         << "Choice: ";
    cin >> mode;

    if (mode == 2) {
        runAblation();
        return;
    }

    // --- Original classic mode ---
    int numGames;
    bool verbose;
    int matchType;

    cout << "\nSelect Matchup:\n"
         << "1. Speedi_Pi vs Speedi_Pi (Pure Speed Test)\n"
         << "2. Cutie_Pi  vs Speedi_Pi (Smart vs Fast)\n"
         << "3. Cutie_Pi  vs Cutie_Pi  (Clash of Titans)\n"
         << "Choice: ";
    cin >> matchType;

    AIStyle styleP1, styleP2;
    if (matchType == 1) { styleP1 = AIStyle::SPEEDI_PI; styleP2 = AIStyle::SPEEDI_PI; }
    else if (matchType == 2) { styleP1 = AIStyle::CUTIE_PI; styleP2 = AIStyle::SPEEDI_PI; }
    else { styleP1 = AIStyle::CUTIE_PI; styleP2 = AIStyle::CUTIE_PI; }

    cout << "Number of games: "; cin >> numGames;
    cout << "Watch games? (1=Yes, 0=No): "; cin >> verbose;

    if (!gDictionary.loadFromFile("csw24.txt")) { cout << "Error: Dictionary not found.\n"; return; }

    // Factory lambdas for the parallel runner
    auto factoryP1 = [styleP1]() { return create_ai_player(styleP1); };
    auto factoryP2 = [styleP2]() { return create_ai_player(styleP2); };

    string nameP1 = (styleP1 == AIStyle::SPEEDI_PI ? "Speedi_Pi" : "Cutie_Pi");
    string nameP2 = (styleP2 == AIStyle::SPEEDI_PI ? "Speedi_Pi" : "Cutie_Pi");
    if (matchType == 1 || matchType == 3) nameP2 = "Evil_" + nameP2;

    auto stats = runParallelMatchup(nameP1, nameP2, factoryP1, factoryP2, numGames, verbose);
    stats.print();

    Renderer::waitForQuitKey();
    Renderer::clearScreen();
}
