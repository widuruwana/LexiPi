#pragma once

#include "../engine/state.h"
#include "../engine/board.h"
#include "../kernel/move_generator.h"
#include "opponent_model.h"
#include "leave_evaluator.h"
#include <vector>

namespace spectre {

    // Forward declarations
    class Treasurer;
    class General;

    // =========================================================
    // SEARCH STRATEGY INTERFACE
    // =========================================================
    // The Search Strategy answers one question:
    // "Given a list of legal moves, which one do I play?"
    //
    // This is the decision-making core. Swapping implementations
    // here tests the value of search depth and simulation.
    // =========================================================

    class SearchStrategy {
    public:
        virtual ~SearchStrategy() = default;

        // Select the best move from a list of scored candidates.
        // Returns the chosen candidate.
        virtual kernel::MoveCandidate selectMove(
            const GameState& state,
            const Board& bonusBoard,
            const std::vector<kernel::MoveCandidate>& candidates,
            OpponentModel* opponentModel) = 0;

        virtual const char* name() const = 0;
    };

    // ---------------------------------------------------------
    // IMPLEMENTATION 1: Greedy Selection (Control Baseline)
    // ---------------------------------------------------------
    // Picks the highest-scoring move. Zero intelligence.
    // Used to establish the floor for ablation.
    // ---------------------------------------------------------
    class GreedySelect : public SearchStrategy {
    public:
        kernel::MoveCandidate selectMove(
            const GameState& state,
            const Board& bonusBoard,
            const std::vector<kernel::MoveCandidate>& candidates,
            OpponentModel* opponentModel) override;

        const char* name() const override { return "Greedy"; }
    };

    // ---------------------------------------------------------
    // IMPLEMENTATION 2: Equity Selection (Leave-Aware)
    // ---------------------------------------------------------
    // Picks the move with highest score + leave equity.
    // Tests the value of leave evaluation in isolation.
    // ---------------------------------------------------------
    class EquitySelect : public SearchStrategy {
    public:
        explicit EquitySelect(LeaveEvaluator* leaveEval);

        kernel::MoveCandidate selectMove(
            const GameState& state,
            const Board& bonusBoard,
            const std::vector<kernel::MoveCandidate>& candidates,
            OpponentModel* opponentModel) override;

        const char* name() const override { return "Equity"; }

    private:
        LeaveEvaluator* leaveEval;

        // Compute leave counts for a candidate by subtracting placed tiles from rack
        void computeLeave(const GameState& state, const kernel::MoveCandidate& cand,
                          int outLeaveCounts[27]) const;
    };

    // ---------------------------------------------------------
    // IMPLEMENTATION 3: Flat Simulation (The Real Evaluator)
    // ---------------------------------------------------------
    // For each candidate: play N full games using greedy rollouts
    // with randomized opponent racks. Pick the highest avg spread.
    // This is what Quackle's championship player essentially does.
    // ---------------------------------------------------------
    class SimulationSelect : public SearchStrategy {
    public:
        SimulationSelect(LeaveEvaluator* leaveEval, int simCount = 100, int timeBudgetMs = 500);

        kernel::MoveCandidate selectMove(
            const GameState& state,
            const Board& bonusBoard,
            const std::vector<kernel::MoveCandidate>& candidates,
            OpponentModel* opponentModel) override;

        const char* name() const override { return "Simulation"; }

    private:
        LeaveEvaluator* leaveEval;
        int simCount;
        int timeBudgetMs;

        // Run one full game simulation from a given state using greedy play
        int simulateGame(const GameState& startState, const Board& bonusBoard) const;
    };

} // namespace spectre
