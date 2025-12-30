#include "../../include/spectre/spy.h"
#include "../../include/spectre/move_generator.h"
#include "../../include/heuristics.h"
#include <algorithm>
#include <random>
#include <iostream>
#include <cstring>
#include <cmath>

using namespace std;

namespace spectre {

static thread_local mt19937 rng_spy(random_device{}());

Spy::Spy() {
    reset();
}

void Spy::reset() {
    tracker.reset();
    for(int i=0; i<27; ++i) beliefMatrix[i] = 1.0f;
}

void Spy::updateGroundTruth(const LetterBoard& board, const std::vector<char>& myRack) {
    tracker.reset();

    // 1. Remove tiles visible on the board
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (board[r][c] != ' ') tracker.remove(board[r][c]);
        }
    }

    // 2. Remove tiles in my current rack
    for (char c : myRack) tracker.remove(c);

    // Note: We do NOT reset the beliefMatrix here entirely, because beliefs
    // about the *opponent's* rack (e.g. "They have an S") might persist across turns
    // if they didn't play. However, for safety in this version, we reset to neutral
    // and re-run inference freshly each turn to avoid compounding errors.
    for(int i=0; i<27; ++i) beliefMatrix[i] = 1.0f;
}

// =============================================================================
// THE GHOST SEARCH (Negative Inference)
// =============================================================================
// This function analyzes the board to see if the opponent missed any obvious
// high-scoring opportunities in their last turn. If they did, we infer they
// likely do not possess the tiles required for those moves.
// =============================================================================
void Spy::performInference(const LetterBoard& board, int lastOpponentScore, Dawg& dict) {
    // We assume the board state hasn't changed significantly since the opponent's move.
    // We check specific "Power Tiles" that usually dictate high-scoring moves.
    const char powerTiles[] = {'S', 'Z', 'Q', 'J', 'X', '?'};

    for (char target : powerTiles) {
        int idx = (target == '?') ? 26 : toupper(target) - 'A';

        // OPTIMIZATION: If the tile is physically gone from the unseen pool,
        // we don't need to simulate. Belief is 0.0.
        if (tracker.getUnseenCount(target) == 0) {
            beliefMatrix[idx] = 0.0f;
            continue;
        }

        // GHOST SIMULATION:
        // We ask: "Could the opponent have scored > 35 points if they held this tile?"
        if (checkHighScoringPotential(target, board, dict)) {

            // LOGIC:
            // If a 35+ point move existed using 'S', and they played a move scoring
            // 'lastOpponentScore' (e.g., 12 points), it is highly probable they
            // do NOT hold an 'S'.

            // PENALTY:
            // We slash the probability weight to 5% (0.05).
            // This ensures the Monte Carlo simulation rarely gives this tile to the opponent.
            beliefMatrix[idx] *= 0.05f;
        }
    }
}

bool Spy::checkHighScoringPotential(char tile, const LetterBoard& board, Dawg& dict) {
    // 1. Construct a "Dream Rack" containing the target tile + optimal fillers.
    int ghostRack[27] = {0};
    int tileIdx = (tile == '?') ? 26 : toupper(tile) - 'A';
    ghostRack[tileIdx]++;

    // Fillers: E, A, I, R, N, T (The "RETINA" set)
    // We construct a standard 7-tile rack to give the generator freedom.
    ghostRack['E' - 'A']++;
    ghostRack['A' - 'A']++;
    ghostRack['R' - 'A']++;
    ghostRack['I' - 'A']++;
    ghostRack['N' - 'A']++;
    ghostRack['T' - 'A']++;

    int maxScoreFound = 0;

    // 2. Run Move Generator with the Ghost Rack
    auto consumer = [&](MoveCandidate& cand, int* rack) -> bool {
        // Does this move actually use the target tile?
        bool usesTarget = false;
        for(int i=0; cand.word[i]!='\0'; ++i) {
            char c = cand.word[i];
            if (toupper(c) == tile || (tile=='?' && islower(c))) {
                usesTarget = true;
                break;
            }
        }
        if (!usesTarget) return true;

        // Approximate Score Calculation
        // We do a fast heuristic score (Letter Values + Length Bonus)
        // because full board scoring is expensive inside this loop.
        int score = 0;
        for(int i=0; cand.word[i]!='\0'; ++i) {
            score += Heuristics::getTileValue(cand.word[i]);
        }

        // Multiplier Heuristic:
        // If length > 4, likely hits a DLS/DWS or covers enough board to be valuable.
        if (strlen(cand.word) >= 4) score = (int)(score * 1.5);

        // "Z" or "Q" on a TL/TW is huge.
        // We assume the generator found a good spot.

        if (score > maxScoreFound) maxScoreFound = score;

        // Short-circuit: If we found a massive play, we can stop looking.
        if (maxScoreFound > 45) return false;
        return true;
    };

    MoveGenerator::generate_raw(board, ghostRack, dict, consumer);

    // 3. Verdict
    // If we found a move worth ~35 heuristic points (likely 40-50 real points), return true.
    return (maxScoreFound > 35);
}

// =============================================================================
// RETENTION INFERENCE (Exchange Logic)
// =============================================================================
void Spy::applyExchangeInference(int tilesExchanged) {
    int kept = 7 - tilesExchanged;
    if (kept <= 0) return;

    // Logic: If they kept tiles, those tiles are likely: S, Blank, or High Synergy (Q+U).
    // They are UNLIKELY to be: V, W, J, Duplicate Consonants.

    beliefMatrix['S' - 'A'] *= 3.0f; // Strong suspicion of S
    beliefMatrix[26]        *= 3.0f; // Strong suspicion of Blank

    beliefMatrix['Q' - 'A'] *= 1.2f;
    beliefMatrix['U' - 'A'] *= 1.2f;

    // Slash Clunkers
    beliefMatrix['V' - 'A'] *= 0.2f;
    beliefMatrix['W' - 'A'] *= 0.3f;
    beliefMatrix['J' - 'A'] *= 0.4f;
}

// =============================================================================
// WEIGHTED GENERATOR (Optimized for Heap Safety)
// =============================================================================
// Generates a random rack for the opponent based on the Bayesian Belief Matrix.
// [FIX] Uses static thread_local buffers to prevent Heap Fragmentation/Corruption
// inside hot simulation loops.
// =============================================================================
void Spy::generateWeightedRack(int* rackCounts) {
    // 1. Reset Output
    memset(rackCounts, 0, 27 * sizeof(int));

    // 2. Get Remaining Tiles
    // (This vector copy is unavoidable but safe since it's outside the inner sampling loop)
    std::vector<char> bag = tracker.getRemainingTiles();
    if (bag.empty()) return;

    int tilesNeeded = 7;
    if (bag.size() < 7) tilesNeeded = bag.size();

    // 3. Prepare Reuseable Memory Buffers
    // static thread_local ensures these are allocated ONCE per thread and reused forever.
    static thread_local std::vector<double> weights;
    weights.clear();
    weights.reserve(bag.size());

    double totalWeight = 0.0;

    // 4. Construct Distribution
    for (char c : bag) {
        int idx = (c == '?') ? 26 : toupper(c) - 'A';
        double w = beliefMatrix[idx];

        // Clamp to prevent math errors
        if (w < 0.001) w = 0.001;
        if (w > 100.0) w = 100.0;

        weights.push_back(w);
        totalWeight += w;
    }

    // 5. Weighted Sampling (Linear Scan)
    // Replaces std::discrete_distribution which allocates internal heap memory.
    for (int i = 0; i < tilesNeeded; ++i) {
        if (totalWeight <= 0.0001) break;

        // Pick a random value in the weight range
        std::uniform_real_distribution<double> dist(0.0, totalWeight);
        double r = dist(rng_spy);

        int pickedIndex = -1;
        double currentSum = 0.0;

        // Scan to find the tile
        for (size_t k = 0; k < weights.size(); ++k) {
            currentSum += weights[k];
            if (currentSum >= r) {
                pickedIndex = k;
                break;
            }
        }

        // Fallback for float precision edge cases
        if (pickedIndex == -1) pickedIndex = weights.size() - 1;

        // Add to Rack
        char c = bag[pickedIndex];
        if (c == '?') rackCounts[26]++;
        else if (isalpha(c)) rackCounts[toupper(c) - 'A']++;

        // "Remove" from pool by zeroing weight
        totalWeight -= weights[pickedIndex];
        weights[pickedIndex] = 0.0;
    }
}

}