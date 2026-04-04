#pragma once

#include "../engine/state.h"
#include "../move.h"
#include "general.h"
#include <vector>

namespace spectre {

    class Treasurer {
    public:
        Treasurer();
        ~Treasurer() = default;

        // Called EXACTLY ONCE at the start of the turn, before MCTS begins
        void update_market_context(const TopologyReport& topo, int scoreDiff, const std::vector<Tile>& bag);

        // Called thousands of times per second. Zero allocations. Pure O(1) math.
        float evaluate_equity(const GameState& state, const Move& move, int moveScore) const;

        // Converts raw projected points into a strict [0.0, 1.0] win probability
        float normalize_to_winprob(int currentScoreDiff, float moveNAV, int bagSize) const;

        // Debug/CLI reporting
        void report_equity(const GameState& state, const Move& move, int moveScore) const;

    private:
        float turn_deltas[27];
        float current_gamma;
        float current_market_volatility;

        float calculate_gamma(int scoreDiff) const;
        float calculate_market_volatility(const std::vector<Tile>& bag) const;
        float get_quackle_baseline(const int* leaveCounts) const;
    };

}