#include "../../include/spectre/vanguard.h"
#include "../../include/kernel/move_generator.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/dictionary.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

using namespace std::chrono;

namespace spectre {

    static bool is_game_over(const GameState& state) {
        if (!state.bag.empty()) return false;
        for (const auto& p : state.players) {
            if (p.rack.empty()) return true;
        }
        return false;
    }

    Vanguard::Vanguard(Treasurer* t) : treasurer(t) {}

    kernel::MoveCandidate Vanguard::select_best_move(
        const GameState& state,
        const Board& bonusBoard,
        const std::vector<kernel::MoveCandidate>& candidates,
        OpponentModel* opponentModel)
    {
        if (candidates.empty()) {
            kernel::MoveCandidate pass;
            pass.word[0] = '\0';
            pass.score = 0;
            return pass;
        }

        // 1. UPDATE THE JIT CONTEXT
        int myScore = state.players[state.currentPlayerIndex].score;
        int oppScore = state.players[1 - state.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        TopologyReport topo = general.scan_topology(state.board);
        treasurer->update_market_context(topo, scoreDiff, state.bag);

        auto root = std::make_unique<MCTSNode>(state, kernel::MoveCandidate(), nullptr);
        root->untriedMoves.reserve(candidates.size());

        // 2. EVALUATE ONCE & CACHE
        for (const auto& c : candidates) {
            root->untriedMoves.push_back({c, consult_council(state, c)});
        }

        // 3. SORT BY CACHED WIN PROBABILITY
        std::sort(root->untriedMoves.begin(), root->untriedMoves.end(),
            [](const EvaluatedMove& a, const EvaluatedMove& b) {
                return a.win_prob < b.win_prob;
            });

        // 4. MCTS LOOP
        auto startTime = high_resolution_clock::now();
        int mcts_iterations = 0;

        while (true) {
            auto now = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(now - startTime).count();
            if (duration >= TIME_BUDGET_MS) break;

            MCTSNode* leaf = select_node(root.get());

            if (!is_game_over(leaf->state) && !leaf->untriedMoves.empty()) {
                leaf = expand_node(leaf, bonusBoard);
            }

            double result = simulate_rollout(leaf->state, opponentModel, bonusBoard);
            backpropagate(leaf, result);
            mcts_iterations++;
        }

        // 5. POST-SIMULATION ANALYSIS
        std::vector<MCTSNode*> sorted_children;
        for (const auto& child : root->children) {
            sorted_children.push_back(child.get());
        }

        std::sort(sorted_children.begin(), sorted_children.end(),
            [](MCTSNode* a, MCTSNode* b) { return a->visits > b->visits; });

        if (!sorted_children.empty()) {
            std::cout << "\n======================================================\n";
            std::cout << "[VANGUARD] MCTS Complete | Iterations: " << mcts_iterations
                      << " | Time: " << TIME_BUDGET_MS << "ms\n";
            std::cout << "------------------------------------------------------\n";

            for (size_t i = 0; i < std::min((size_t)3, sorted_children.size()); i++) {
                MCTSNode* child = sorted_children[i];
                kernel::MoveCandidate& mc = child->moveLeadingTo;
                char dir = mc.isHorizontal ? 'H' : 'V';
                double win_prob = (child->visits > 0) ? (child->totalScore / child->visits) : 0.0;

                std::cout << " #" << (i+1) << ": " << mc.word
                          << " [" << mc.row << "," << mc.col << " " << dir << "] "
                          << "| Raw: " << mc.score
                          << " | Visits: " << child->visits
                          << " | Win Prob: " << (win_prob * 100.0) << "%\n";
            }
            std::cout << "======================================================\n\n";
        }

        return !sorted_children.empty() ? sorted_children[0]->moveLeadingTo : candidates.back();
    }

    double Vanguard::consult_council(const GameState& state, const kernel::MoveCandidate& cand) {
        Move m;
        m.row = cand.row; m.col = cand.col; m.horizontal = cand.isHorizontal;
        m.type = MoveType::PLAY;
        std::strncpy(m.word, cand.word, 16); m.word[15] = '\0';

        float topoScore = general.evaluate_topology(state, m);
        double topoBias = (topoScore - 0.5) * 40.0;
        float nav = treasurer->evaluate_equity(state, m, cand.score);

        int myScore = state.players[state.currentPlayerIndex].score;
        int oppScore = state.players[1 - state.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        return treasurer->normalize_to_winprob(scoreDiff, nav + topoBias, state.bag.size());
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

        Move m;
        m.row = emove.cand.row; m.col = emove.cand.col; m.horizontal = emove.cand.isHorizontal;
        m.type = MoveType::PLAY;
        std::strncpy(m.word, emove.cand.word, 16); m.word[15] = '\0';

        Mechanics::applyMove(newState, m, emove.cand.score);

        auto child = std::make_unique<MCTSNode>(newState, emove.cand, node);
        child->heuristicBias = emove.win_prob;

        MCTSNode* childPtr = child.get();
        node->children.push_back(std::move(child));
        return childPtr;
    }

    double Vanguard::simulate_rollout(const GameState& startState,
                                       OpponentModel* opponentModel,
                                       const Board& bonusBoard) {
        std::vector<char> oppRackChars;
        if (opponentModel) {
            oppRackChars = opponentModel->sampleOpponentRack();
        }

        int oppRackCounts[27] = {0};
        for (char c : oppRackChars) {
            if (c == '?') oppRackCounts[26]++;
            else if (isalpha(c)) oppRackCounts[toupper(c) - 'A']++;
        }

        int bestScore = 0;
        auto opponentConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
            int score = Mechanics::calculateTrueScore(cand, startState.board, bonusBoard);
            if (score > bestScore) bestScore = score;
            return true;
        };

        kernel::MoveGenerator::generate_raw(startState.board, oppRackCounts, gDictionary, opponentConsumer);

        int myScore = startState.players[startState.currentPlayerIndex].score;
        int oppScore = startState.players[1 - startState.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        return treasurer->normalize_to_winprob(scoreDiff, -(float)bestScore, startState.bag.size());
    }

    void Vanguard::backpropagate(MCTSNode* node, double score) {
        while (node) {
            node->visits++;
            node->totalScore += score;
            node = node->parent;
        }
    }

    double Vanguard::calculate_uct_value(const MCTSNode* node) const {
        if (node->visits == 0) return 1e9;

        double avgOutcome = node->totalScore / node->visits;
        double exploration = UCT_C * std::sqrt(std::log(node->parent->visits) / node->visits);
        double bias = (node->heuristicBias * BIAS_WEIGHT) / (1 + node->visits);

        return avgOutcome + exploration + bias;
    }

} // namespace spectre
