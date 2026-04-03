#pragma once

#include "../kernel/move_generator.h"
#include "../engine/state.h"
#include "../engine/board.h"
#include "spy.h"
#include "general.h"
#include "treasurer.h"
#include <vector>
#include <memory>
#include <chrono>

namespace spectre {

    // MCTS Node structure for the search tree
    struct MCTSNode {
        GameState state;
        kernel::MoveCandidate moveLeadingTo;
        MCTSNode* parent = nullptr;
        std::vector<std::unique_ptr<MCTSNode>> children;

        // Stats
        int visits = 0;
        double totalScore = 0.0;

        // The "Council's Bias" (Topology + Equity)
        double heuristicBias = 0.0;

        // Unexplored moves from this state
        std::vector<kernel::MoveCandidate> untriedMoves;

        MCTSNode(const GameState& s, const kernel::MoveCandidate& m, MCTSNode* p)
            : state(s), moveLeadingTo(m), parent(p) {}
    };

    class Vanguard {
    public:
        Vanguard();

        // THE TACTICAL MANDATE
        // Orchestrates the Council to select the best move within the time budget.
        // Returns the best kernel::MoveCandidate found.
        kernel::MoveCandidate select_best_move(
            const GameState& state,
            const Board& bonusBoard,
            const std::vector<kernel::MoveCandidate>& candidates,
            spectre::Spy& spy
        );

    private:
        // Strategic Advisors
        // These must be fully defined in the header includes
        spectre::General general;
        spectre::Treasurer treasurer;

        // Configuration
        const int TIME_BUDGET_MS = 18000; // 18 seconds (leave 2s buffer for other agents)
        const float UCT_C = 1.41421356f;       // Exploration constant
        const double BIAS_WEIGHT = 1.0;  // Trust in Council

        // MCTS Phases
        MCTSNode* select_node(MCTSNode* root);
        MCTSNode* expand_node(MCTSNode* node, const Board& bonusBoard);
        double simulate_rollout(const GameState& startState, const spectre::Spy& spy, const Board& bonusBoard);
        void backpropagate(MCTSNode* node, double score);

        // Calculations
        double calculate_uct_value(const MCTSNode* node) const;

        // The "Council Meeting"
        // Calculates the composite Heuristic Bias (General + Treasurer) for a move
        double consult_council(const GameState& state, const kernel::MoveCandidate& move);
    };
}