#include "../../include/spectre/vanguard.h"
#include "../../include/kernel/move_generator.h"
#include "../../include/kernel/greedy_engine.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/dictionary.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>

using namespace std::chrono;

namespace spectre {

    static bool is_game_over(const GameState& state) {
        if (!state.bag.empty()) return false;
        for (const auto& p : state.players) {
            if (p.rack.empty()) return true;
        }
        return false;
    }

    Vanguard::Vanguard() {}

    kernel::MoveCandidate Vanguard::select_best_move(
        const GameState& state,
        const Board& bonusBoard,
        const std::vector<kernel::MoveCandidate>& candidates,
        const spectre::Spy& spy)
    {
        if (candidates.empty()) {
            kernel::MoveCandidate pass;
            pass.word[0] = '\0'; pass.score = 0;
            return pass;
        }

        std::vector<EvaluatedMove> initial_untried;
        initial_untried.reserve(candidates.size());

        for (size_t i = 0; i < candidates.size(); ++i) {
            // FIX: We pass the pure, unadulterated candidate.
            // cand.score remains its actual point value!
            EvaluatedMove em = {candidates[i], consult_council(state, candidates[i])};
            initial_untried.push_back(em);
        }

        std::sort(initial_untried.begin(), initial_untried.end(),
            [](const EvaluatedMove& a, const EvaluatedMove& b) {
                return a.win_prob < b.win_prob;
            });

        int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 6;

        std::vector<std::unique_ptr<MCTSNode>> thread_roots(num_threads);
        std::vector<std::thread> workers;
        std::atomic<int> total_mcts_iterations{0};

        auto startTime = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < num_threads; ++t) {
            workers.emplace_back([&, t]() {
                thread_roots[t] = std::make_unique<MCTSNode>(state, kernel::MoveCandidate(), nullptr);
                thread_roots[t]->untriedMoves = initial_untried;

                int local_iters = 0;
                while (true) {
                    if ((local_iters & 31) == 0) {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                        if (duration >= TIME_BUDGET_MS) break;
                    }

                    MCTSNode* leaf = select_node(thread_roots[t].get());

                    if (!is_game_over(leaf->state) && !leaf->untriedMoves.empty()) {
                        leaf = expand_node(leaf, bonusBoard);
                    }

                    double result = simulate_rollout(leaf->state, spy, bonusBoard);
                    backpropagate(leaf, result);
                    local_iters++;
                }
                total_mcts_iterations += local_iters;
            });
        }

        for (auto& w : workers) {
            if (w.joinable()) w.join();
        }

        struct MergedStats { int visits = 0; double score = 0.0; };
        std::vector<MergedStats> merged(candidates.size());

        for (int t = 0; t < num_threads; ++t) {
            if (!thread_roots[t]) continue;

            for (const auto& child : thread_roots[t]->children) {
                // Safely match the child back to the original candidates list
                for (size_t i = 0; i < initial_untried.size(); ++i) {
                    if (child->moveLeadingTo.row == initial_untried[i].cand.row &&
                        child->moveLeadingTo.col == initial_untried[i].cand.col &&
                        child->moveLeadingTo.isHorizontal == initial_untried[i].cand.isHorizontal &&
                        std::strcmp(child->moveLeadingTo.word, initial_untried[i].cand.word) == 0) {

                        merged[i].visits += child->visits;
                        merged[i].score += child->totalScore;
                        break;
                    }
                }
            }
        }

        int bestIdx = initial_untried.size() - 1;
        int maxVisits = -1;

        for (size_t i = 0; i < merged.size(); ++i) {
            if (merged[i].visits > maxVisits) {
                maxVisits = merged[i].visits;
                bestIdx = static_cast<int>(i);
            }
        }

        kernel::MoveCandidate bestMove = initial_untried[bestIdx].cand;

        std::cout << "\n======================================================\n";
        std::cout << "[VANGUARD] MCTS Complete | Total Iterations: " << total_mcts_iterations
                  << " | Time: " << TIME_BUDGET_MS << "ms\n";
        std::cout << "------------------------------------------------------\n";
        std::cout << " SELECTED MOVE (ROOT PARALLELISM CONSENSUS)\n";
        std::cout << "------------------------------------------------------\n";

        double child_win_prob = (maxVisits > 0) ? (merged[bestIdx].score / maxVisits) : 0.0;
        double true_win_prob = 1.0 - child_win_prob;
        char dir = bestMove.isHorizontal ? 'H' : 'V';

        std::cout << " > " << bestMove.word
                  << " [" << bestMove.row << "," << bestMove.col << " " << dir << "] "
                  << "| Raw Score: " << bestMove.score
                  << " | Total Visits: " << maxVisits
                  << " | Win Prob: " << (true_win_prob * 100.0) << "%\n";
        std::cout << "======================================================\n\n";

        return bestMove;
    }

    MCTSNode* Vanguard::select_node(MCTSNode* node) {
        while (node->untriedMoves.empty() && !node->children.empty()) {
            MCTSNode* bestChild = nullptr;
            double bestValue = -std::numeric_limits<double>::infinity();

            for (const auto& child : node->children) {
                double uct = calculate_uct_value(child.get());
                if (uct > bestValue) {
                    bestValue = uct;
                    bestChild = child.get();
                }
            }
            if (!bestChild) break;
            node = bestChild;
        }
        return node;
    }

    MCTSNode* Vanguard::expand_node(MCTSNode* node, const Board& bonusBoard) {
        EvaluatedMove emove = node->untriedMoves.back();
        node->untriedMoves.pop_back();

        GameState newState = node->state.clone();
        int activePlayer = newState.currentPlayerIndex;
        int originalScore = newState.players[activePlayer].score;

        Mechanics::applyCandidateMove(newState, emove.cand);

        // FIX: Force the simulation to register the points, even if applyCandidateMove
        // assumes the GameDirector normally handles it.
        if (newState.players[activePlayer].score == originalScore) {
            newState.players[activePlayer].score += emove.cand.score;
        }

        // FIX: Force the turn to flip so the rollout simulates the correct opponent.
        if (newState.currentPlayerIndex == activePlayer) {
            newState.currentPlayerIndex = 1 - activePlayer;
        }

        auto child = std::make_unique<MCTSNode>(newState, emove.cand, node);
        child->heuristicBias = emove.win_prob;

        MCTSNode* childPtr = child.get();
        node->children.push_back(std::move(child));
        return childPtr;
    }

    double Vanguard::simulate_rollout(const GameState& startState, const spectre::Spy& spy, const Board& bonusBoard) {
        std::vector<char> oppRackChars = spy.generateWeightedRack();
        int oppRackCounts[27] = {0};

        for(char c : oppRackChars) {
            if (c == '?') oppRackCounts[26]++;
            else if (isalpha(c)) oppRackCounts[toupper(c) - 'A']++;
        }

        double bestNav = -10000.0;

        // Zero-allocation lambda: Evaluates the board directly without cloning GameState
        auto opponentConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
            Move m;
            m.row = cand.row; m.col = cand.col; m.horizontal = cand.isHorizontal; m.type = MoveType::PLAY;
            std::strncpy(m.word, cand.word, 16); m.word[15] = '\0';

            int score = Mechanics::calculateTrueScore(cand, startState.board, bonusBoard);
            float nav = treasurer.evaluate_equity(startState, m, score);

            if (nav > bestNav) bestNav = nav;
            return true;
        };

        kernel::MoveGenerator::generate_raw(startState.board, oppRackCounts, gDictionary, opponentConsumer);

        if (bestNav == -10000.0) bestNav = 0.0;

        int myScore = startState.players[startState.currentPlayerIndex].score;
        int oppScore = startState.players[1 - startState.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        // Passed as +bestNav so the leaf player's equity is correctly added to their standing
        return treasurer.normalize_to_winprob(scoreDiff, bestNav, startState.bag.size());
    }

    void Vanguard::backpropagate(MCTSNode* node, double score) {
        while (node) {
            node->visits++;
            node->totalScore += score;
            score = 1.0 - score; // NEGAMAX INVERSION: Flips probability for alternating turns
            node = node->parent;
        }
    }

    double Vanguard::calculate_uct_value(const MCTSNode* node) const {
        if (node->visits == 0) return 1e9;

        // Evaluate child from the parent's perspective (minimizing the child's win probability)
        double parentPerspectiveScore = 1.0 - (node->totalScore / node->visits);
        double exploration = UCT_C * std::sqrt(std::log(node->parent->visits) / node->visits);
        double bias = (node->heuristicBias * BIAS_WEIGHT) / (1 + node->visits);

        return parentPerspectiveScore + exploration + bias;
    }
}