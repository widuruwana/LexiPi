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

using namespace std::chrono;

namespace spectre {

    // --- THE MATHEMATICAL CONSTANT FIX ---
    // Scaled specifically for [0.0, 1.0] Win Probability bounds.
    static const double UCT_C = 1.41421356;

    // --- INTERNAL HELPERS ---
    bool is_game_over(const GameState& state) {
        if (!state.bag.empty()) return false;
        for (const auto& p : state.players) {
            if (p.rack.empty()) return true;
        }
        return false;
    }

    // --- VANGUARD IMPLEMENTATION ---

    Vanguard::Vanguard() {
        // Advisors (General, Treasurer) are auto-initialized
    }

    kernel::MoveCandidate Vanguard::select_best_move(
        const GameState& state,
        const Board& bonusBoard,
        const std::vector<kernel::MoveCandidate>& candidates,
        spectre::Spy& spy)
    {
        if (candidates.empty()) {
            kernel::MoveCandidate pass;
            pass.word[0] = '\0';
            pass.score = 0;
            return pass;
        }

        auto root = std::make_unique<MCTSNode>(state, kernel::MoveCandidate(), nullptr);
        root->untriedMoves = candidates;

        // COUNCIL SORT (Heuristic Pre-Sorting)
        std::sort(root->untriedMoves.begin(), root->untriedMoves.end(),
            [&](const kernel::MoveCandidate& a, const kernel::MoveCandidate& b) {
                double probA = consult_council(state, a);
                double probB = consult_council(state, b);
                return probA < probB;
            });

        // Debug Output
        std::cout << "\n[VANGUARD] --- HEURISTIC RANKING (PRE-SIMULATION) ---" << std::endl;
        int count = 0;

        for (auto it = root->untriedMoves.rbegin(); it != root->untriedMoves.rend(); ++it) {
            if (count >= 5) break;

            Move m;
            m.row = it->row; m.col = it->col; m.horizontal = it->isHorizontal;
            m.type = MoveType::PLAY;
            std::strncpy(m.word, it->word, 16);
            m.word[15] = '\0';

            float topoScore = general.evaluate_topology(state, m);
            double topoBias = (topoScore - 0.5) * 40.0;
            float nav = treasurer.evaluate_equity(state, m, it->score);

            int scoreDiff = state.players[state.currentPlayerIndex].score - state.players[1 - state.currentPlayerIndex].score;
            float win_prob = treasurer.normalize_to_winprob(scoreDiff, nav + topoBias, state.bag.size());

            std::cout << "#" << (count + 1) << ": " << it->word
                      << " | Raw: " << it->score
                      << " | NAV: " << nav
                      << " | TopoBias: " << (topoBias >= 0 ? "+" : "") << topoBias
                      << " | Prior WinProb: " << (win_prob * 100.0f) << "%"
                      << std::endl;

            if (count == 0) {
                std::cout << "------------------------------------------" << std::endl;
                treasurer.report_equity(state, m, it->score);
                general.report_topology(state, m);
                std::cout << "------------------------------------------" << std::endl;
            }

            count++;
        }
        std::cout << "------------------------------------------\n" << std::endl;

        // MCTS LOOP
        auto startTime = high_resolution_clock::now();

        while (true) {
            auto now = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(now - startTime).count();
            if (duration >= TIME_BUDGET_MS) break;

            MCTSNode* leaf = select_node(root.get());

            if (!is_game_over(leaf->state) && !leaf->untriedMoves.empty()) {
                leaf = expand_node(leaf, bonusBoard);
            }

            double result = simulate_rollout(leaf->state, spy, bonusBoard);
            backpropagate(leaf, result);
        }

        // SELECT ROBUST CHILD
        MCTSNode* bestChild = nullptr;
        int maxVisits = -1;

        for (const auto& child : root->children) {
            if (child->visits > maxVisits) {
                maxVisits = child->visits;
                bestChild = child.get();
            }
        }

        return bestChild ? bestChild->moveLeadingTo : candidates.back();
    }

    double Vanguard::consult_council(const GameState& state, const kernel::MoveCandidate& cand) {
        Move m;
        m.row = cand.row;
        m.col = cand.col;
        m.horizontal = cand.isHorizontal;
        m.type = MoveType::PLAY;
        std::strncpy(m.word, cand.word, 16);
        m.word[15] = '\0';

        float topoScore = general.evaluate_topology(state, m);
        double topoBias = (topoScore - 0.5) * 40.0;
        float nav = treasurer.evaluate_equity(state, m, cand.score);

        // NORMALIZATION FIX: Convert Council's raw point evaluation into a prior Win Probability
        int myScore = state.players[state.currentPlayerIndex].score;
        int oppScore = state.players[1 - state.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        return treasurer.normalize_to_winprob(scoreDiff, nav + topoBias, state.bag.size());
    }

    // --- MCTS PHASES ---

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
        kernel::MoveCandidate move = node->untriedMoves.back();
        node->untriedMoves.pop_back();

        GameState newState = node->state.clone();

        Move m;
        m.row = move.row; m.col = move.col; m.horizontal = move.isHorizontal;
        m.type = MoveType::PLAY;
        std::strncpy(m.word, move.word, 16);
        m.word[15] = '\0';

        Mechanics::applyMove(newState, m, move.score);

        auto child = std::make_unique<MCTSNode>(newState, move, node);
        child->heuristicBias = consult_council(node->state, move); // Bias is now [0.0, 1.0]

        MCTSNode* childPtr = child.get();
        node->children.push_back(std::move(child));
        return childPtr;
    }

    double Vanguard::simulate_rollout(const GameState& startState, const spectre::Spy& spy, const Board& bonusBoard) {
        GameState simState = startState.clone();

        std::vector<char> oppRackChars = spy.generateWeightedRack();
        int oppRackCounts[27] = {0};

        for(char c : oppRackChars) {
            if (c == '?') oppRackCounts[26]++;
            else if (isalpha(c)) oppRackCounts[toupper(c) - 'A']++;
        }

        std::vector<Tile> oppRackTiles;
        for(char c : oppRackChars) { Tile t; t.letter = c; t.points = 0; oppRackTiles.push_back(t); }
        simState.players[simState.currentPlayerIndex].rack = oppRackTiles;

        double bestNav = -10000.0;

        auto opponentConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
            Move m;
            m.row = cand.row; m.col = cand.col; m.horizontal = cand.isHorizontal;
            strncpy(m.word, cand.word, 16);
            m.word[15] = '\0';

            int score = Mechanics::calculateTrueScore(cand, simState.board, bonusBoard);
            float nav = treasurer.evaluate_equity(simState, m, score);

            if (nav > bestNav) {
                bestNav = nav;
            }
            return true;
        };

        kernel::MoveGenerator::generate_raw(simState.board, oppRackCounts, gDictionary, opponentConsumer);

        if (bestNav == -10000.0) bestNav = 0.0; // Pass

        // NORMALIZATION FIX: Convert projected point differential into a Win Probability
        int myScore = startState.players[startState.currentPlayerIndex].score;
        int oppScore = startState.players[1 - startState.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        // Spread = Current Diff - Opponent's expected gain
        return treasurer.normalize_to_winprob(scoreDiff, -bestNav, startState.bag.size());
    }

    void Vanguard::backpropagate(MCTSNode* node, double score) {
        while (node) {
            node->visits++;
            node->totalScore += score; // Score is now [0.0, 1.0] Win Probability
            node = node->parent;
        }
    }

    double Vanguard::calculate_uct_value(const MCTSNode* node) const {
        if (node->visits == 0) return 1e9;

        // FIX: Removed the raw '+ node->moveLeadingTo.score' addition.
        // avgOutcome is strictly [0.0, 1.0]
        double avgOutcome = node->totalScore / node->visits;

        double exploration = UCT_C * std::sqrt(std::log(node->parent->visits) / node->visits);

        // heuristicBias is also [0.0, 1.0]
        double bias = (node->heuristicBias * BIAS_WEIGHT) / (1 + node->visits);

        return avgOutcome + exploration + bias;
    }

}