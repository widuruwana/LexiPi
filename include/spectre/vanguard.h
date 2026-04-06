#pragma once

#include "spectre_types.h"
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
    // MCTS tree node — used exclusively inside Vanguard
    struct MCTSNode {
        GameState state;
        kernel::MoveCandidate moveLeadingTo;
        MCTSNode* parent = nullptr;
        std::vector<std::unique_ptr<MCTSNode>> children;

        int visits = 0;
        double totalScore = 0.0;
        double heuristicBias = 0.0;

        std::vector<EvaluatedMove> untriedMoves;

        MCTSNode(const GameState& s, const kernel::MoveCandidate& m, MCTSNode* p)
            : state(s), moveLeadingTo(m), parent(p) {}
    };
    /**
     * @brief The Orchestrator for the Guided Monte Carlo Tree Search.
     */
    class Vanguard {
    public:
        Vanguard();

        /**
         * @brief Orchestrates the Council to select the best move within the time budget.
         * @pre `candidates` contains all legal moves. `spy` particle filter is fully updated.
         * @post Returns the mathematically optimal MoveCandidate. Blocks thread for exactly `TIME_BUDGET_MS`.
         */
        kernel::MoveCandidate select_best_move(
            const GameState& state,
            const Board& bonusBoard,
            const std::vector<kernel::MoveCandidate>& candidates,
            const spectre::Spy& spy
        );

    private:
        // Strategic Advisors
        spectre::General general;
        spectre::Treasurer treasurer;

        // Engine Parameters
        const int TIME_BUDGET_MS = 18000; // 18 seconds (leaves 2s buffer for UI/Referee boundaries)
        const float UCT_C = 1.41421356f;  // Exploration constant (sqrt(2)) for [0,1] reward space
        const double BIAS_WEIGHT = 1.0;   // Decay multiplier for the initial Council heuristic

        // MCTS Pipeline
        MCTSNode* select_node(MCTSNode* root);

        /**
         * @pre `node->untriedMoves` must not be empty.
         * @post Pops the best heuristically ranked move, mutates state via applyCandidateMove, returns new child.
         */
        MCTSNode* expand_node(MCTSNode* node, const Board& bonusBoard);

        /**
         * @brief Simulates an opponent response using the Spy's particle filter.
         * @invariant Zero heap allocations inside the rollout evaluation loop.
         */
        double simulate_rollout(const GameState& startState, const spectre::Spy& spy, const Board& bonusBoard);

        void backpropagate(MCTSNode* node, double score);

        // Core Math
        double calculate_uct_value(const MCTSNode* node) const;

        /**
         * @brief Queries General and Treasurer to evaluate a specific board state.
         * @post Returns a strict win probability bound [0.0, 1.0].
         */
        double consult_council(const GameState& state, const kernel::MoveCandidate& move);
    };
}