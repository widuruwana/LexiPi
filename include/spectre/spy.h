#pragma once
#include <vector>
#include <map>
#include <array>
#include <mutex>
#include "../../include/board.h"
#include "../../include/tile_tracker.h"
#include "../../include/dawg.h"
#include "../../include/move.h"

using namespace std;

namespace spectre {

    class Spy {
    public:
        Spy();
        void reset();

        // [NEW] Export the current belief state (Brain Dump)
        array<float, 27> getBeliefs() const { return beliefMatrix; }

        // [NEW] Import a belief state (Brain Transplant)
        void setBeliefs(const array<float, 27>& beliefs) { beliefMatrix = beliefs; }

        // =========================================================================
        // THE OBSERVATION LOOP (Main Thread)
        // =========================================================================

        // 1. Syncs the Spy with the current reality (Board + My Rack)
        // This resets the tracker to the absolute "Unseen" pool.
        void updateGroundTruth(const LetterBoard& board, const vector<char>& myRack);

        // 2. THE GHOST SEARCH (Negative Inference)
        // Runs a simulation on the *current board* to see if the opponent missed
        // any high-scoring opportunities in their *last turn*.
        // If a massive 'S' hook exists now, it existed then. If they didn't take it,
        // we infer they do not have an 'S'.
        void performInference(const LetterBoard& board, int lastOpponentScore, Dawg& dict);

        // 3. RETENTION BIAS (Positive Inference)
        // If the opponent exchanged tiles, we infer the kept tiles are valuable.
        void applyExchangeInference(int tilesExchanged);

        // =========================================================================
        // THE GENERATOR (Worker Threads)
        // =========================================================================

        // Generates a rack for Monte Carlo simulation using Weighted Reservoir Sampling
        // based on the inferred Belief Matrix.
        void generateWeightedRack(int* rackCounts);

    private:
        TileTracker tracker;
        array<float, 27> beliefMatrix; // Probability weights (1.0 = Neutral)

        // Helper: Runs a simulation to see if a specific tile could score high points
        bool checkHighScoringPotential(char tile, const LetterBoard& board, Dawg& dict);
    };

}