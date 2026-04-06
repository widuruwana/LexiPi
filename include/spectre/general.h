#pragma once

#include "../engine/board.h"
#include "spectre_types.h"
#include "../move.h"
#include "../engine/state.h"

namespace spectre {
    class General {
    public:
        General();

        // --- THE STRATEGIC MANDATE ---
        // Input: The current game state and a candidate move.
        // Output: A "Topological Utility" score (0.0 to 1.0).
        //
        // Logic:
        // 1. If we are Winning (>40 pts): PUNISH Openness and TWS Access.
        // 2. If we are Losing: REWARD Openness and TWS Access (Variance).
        float evaluate_topology(const GameState& state, const Move& move);

        // Generates the "Market Report" for the board.
        // Public so the Treasurer can use it for Context-Aware Pricing.
        TopologyReport scan_topology(const LetterBoard& board);

        void report_topology(const GameState& state, const Move& move);

    private:
        // Helper to apply move to a temporary board for simulation
        void simulate_move_on_board(LetterBoard& board, const Move& move);
    };

}