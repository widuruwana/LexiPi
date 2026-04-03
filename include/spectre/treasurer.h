#pragma once

#include "../engine/state.h"
#include "../move.h"
#include "general.h" // For TopologyReport
#include <vector>

namespace spectre {

    class Treasurer {
    public:
        Treasurer();

        // --- THE JIT CONTEXT GENERATOR ---
        // Called EXACTLY ONCE at the start of the turn, before MCTS begins.
        // Calculates the premiums and discounts for all 27 tiles.
        void update_market_context(const TopologyReport& topo, int scoreDiff, const std::vector<Tile>& bag);

        // --- THE HOT LOOP EVALUATOR ---
        // Called thousands of times per second by Vanguard.
        // Zero allocations. Pure O(1) math.
        float evaluate_equity(const GameState& state, const Move& move, int moveScore) const;

        // Debug/CLI reporting
        void report_equity(const GameState& state, const Move& move, int moveScore) const;

        // --- NORMALIZATION (MCTS UCT Fix) ---
        // Converts raw projected points into a strict [0.0, 1.0] win probability.
        float normalize_to_winprob(int currentScoreDiff, float moveNAV, int bagSize) const;

    private:
        // The dynamic JIT modifiers (A-Z + Blank)
        float turn_deltas[27];

        // MPT Variables stored for reporting
        float current_gamma;
        float current_market_volatility;

        // Internal Math Helpers (used only during update_market_context)
        float calculate_gamma(int scoreDiff) const;
        float calculate_market_volatility(const std::vector<Tile>& bag) const;

        // --- THE BASELINE (Quackle Mock) ---
        // Until we load the physical 12MB .bin file, this acts as our static fallback.
        float get_quackle_baseline(const int* leaveCounts) const;

    };
}