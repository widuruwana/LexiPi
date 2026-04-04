#pragma once

#include "../engine/state.h"
#include "../engine/board.h"
#include "../kernel/move_generator.h"
#include "opponent_model.h"
#include "treasurer.h"
#include "general.h"
#include <vector>
#include <memory>

namespace spectre {

    struct EvaluatedMove {
        kernel::MoveCandidate cand;
        double win_prob;
    };

    class MCTSNode {
    public:
        GameState state;
        kernel::MoveCandidate moveLeadingTo;
        MCTSNode* parent;
        std::vector<std::unique_ptr<MCTSNode>> children;
        std::vector<EvaluatedMove> untriedMoves;

        int visits;
        double totalScore;
        double heuristicBias;

        MCTSNode(const GameState& s, const kernel::MoveCandidate& m, MCTSNode* p)
            : state(s), moveLeadingTo(m), parent(p), visits(0), totalScore(0.0), heuristicBias(0.0) {}
    };

    // =========================================================
    // THE VANGUARD: MCTS-Based Move Selection
    // =========================================================
    // Dependencies are INJECTED, not owned:
    //   - OpponentModel* for rack sampling in rollouts
    //   - Treasurer* for equity evaluation in the council
    //   - General is owned (lightweight, always needed)
    // =========================================================

    class Vanguard {
    public:
        static constexpr int TIME_BUDGET_MS = 500;
        static constexpr double BIAS_WEIGHT = 1.0;
        static constexpr double UCT_C = 1.41421356;

        explicit Vanguard(Treasurer* treasurer);
        ~Vanguard() = default;

        kernel::MoveCandidate select_best_move(
            const GameState& state,
            const Board& bonusBoard,
            const std::vector<kernel::MoveCandidate>& candidates,
            OpponentModel* opponentModel);

    private:
        Treasurer* treasurer;   // NOT owned
        General general;        // Owned (lightweight)

        double consult_council(const GameState& state, const kernel::MoveCandidate& cand);

        MCTSNode* select_node(MCTSNode* node);
        MCTSNode* expand_node(MCTSNode* node, const Board& bonusBoard);
        double simulate_rollout(const GameState& startState, OpponentModel* opponentModel, const Board& bonusBoard);
        void backpropagate(MCTSNode* node, double score);
        double calculate_uct_value(const MCTSNode* node) const;
    };

} // namespace spectre
