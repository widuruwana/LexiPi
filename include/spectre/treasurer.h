#pragma once

#include "../engine/state.h"
#include "../move.h"
#include "general.h"
#include "leave_evaluator.h"
#include <vector>

namespace spectre {

    // =========================================================
    // THE TREASURER: Context-Aware Equity Evaluator
    // =========================================================
    // Composes:
    //   1. A LeaveEvaluator (pluggable) for baseline rack quality
    //   2. Dynamic turn deltas based on topology and game phase
    //   3. Sigmoid normalization to win probability
    //
    // The Treasurer's unique contribution is the DYNAMIC LAYER.
    // The baseline leave quality comes from the plugged evaluator.
    // =========================================================

    class Treasurer {
    public:
        explicit Treasurer(LeaveEvaluator* leaveEval);
        ~Treasurer() = default;

        // Called EXACTLY ONCE at the start of the turn, before search begins.
        void update_market_context(const TopologyReport& topo, int scoreDiff, const std::vector<Tile>& bag);

        // Called thousands of times per second. Zero allocations. Pure O(1) math.
        float evaluate_equity(const GameState& state, const Move& move, int moveScore) const;

        // Converts raw projected points into a strict [0.0, 1.0] win probability.
        float normalize_to_winprob(int currentScoreDiff, float moveNAV, int bagSize) const;

        // Debug/CLI reporting.
        void report_equity(const GameState& state, const Move& move, int moveScore) const;

        // Access the underlying leave evaluator (for SearchStrategy implementations)
        LeaveEvaluator* getLeaveEvaluator() const { return leaveEval; }

    private:
        LeaveEvaluator* leaveEval;  // NOT owned. Lifetime managed by EngineConfig.

        float turn_deltas[27];
        float current_gamma;
        float current_market_volatility;

        float calculate_gamma(int scoreDiff) const;
        float calculate_market_volatility(const std::vector<Tile>& bag) const;
    };

} // namespace spectre
