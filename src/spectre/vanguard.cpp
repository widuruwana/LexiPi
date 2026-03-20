#include "../../include/spectre/vanguard.h"
#include "../../include/kernel/move_generator.h" // Needed for generate_raw
#include "../../include/kernel/greedy_engine.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/dictionary.h" // Needed for gDictionary
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>
#include <cstring>

using namespace std::chrono;

namespace spectre {

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
                double scoreA = consult_council(state, a);
                double scoreB = consult_council(state, b);
                return scoreA < scoreB;
            });

        // Debug Output
        std::cout << "\n[VANGUARD] --- HEURISTIC RANKING (PRE-SIMULATION) ---" << std::endl;
        int count = 0;

        // Iterate backwards because we sorted ascending (best is at end)
        for (auto it = root->untriedMoves.rbegin(); it != root->untriedMoves.rend(); ++it) {
            if (count >= 5) break;

            // Re-calculate components for display
            Move m;
            m.row = it->row; m.col = it->col; m.horizontal = it->isHorizontal;
            m.type = MoveType::PLAY;
            std::strncpy(m.word, it->word, 16);
            m.word[15] = '\0';

            float topoScore = general.evaluate_topology(state, m);
            double topoBias = (topoScore - 0.5) * 40.0;
            float nav = treasurer.evaluate_equity(state, m, it->score);
            float equity = nav - it->score; // Extract just the future value

            std::cout << "#" << (count + 1) << ": " << it->word
                      << " | Raw: " << it->score
                      << " | Equity: " << (equity >= 0 ? "+" : "") << equity
                      << " | Topo: " << (topoBias >= 0 ? "+" : "") << topoBias
                      << " | FINAL: " << (nav + topoBias)
                      << std::endl;

            // PRINT FULL REPORT FOR THE #1 MOVE
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

        // 1. THE GENERAL (Topology)
        float topoScore = general.evaluate_topology(state, m);
        // Scaling: 0.5 is neutral. Range approx -20 to +20.
        double topoBias = (topoScore - 0.5) * 40.0;

        // 2. THE TREASURER (Finance)
        // NAV = RawScore + Equity (Future Value)
        float nav = treasurer.evaluate_equity(state, m, cand.score);

        // FIX: Do NOT subtract cand.score.
        // We want the Council to rank moves by Total Value (Score + Equity + Topology)
        return nav + topoBias;
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
        child->heuristicBias = consult_council(node->state, move);

        MCTSNode* childPtr = child.get();
        node->children.push_back(std::move(child));
        return childPtr;
    }

    double Vanguard::simulate_rollout(const GameState& startState, const spectre::Spy& spy, const Board& bonusBoard) {
        // 1. SETUP
        GameState simState = startState.clone();

        // 2. GUESS OPPONENT RACK (Spy)
        std::vector<char> oppRackChars = spy.generateWeightedRack();
        int oppRackCounts[27] = {0};

        for(char c : oppRackChars) {
            // Build counts for generator
            if (c == '?') oppRackCounts[26]++;
            else if (isalpha(c)) oppRackCounts[toupper(c) - 'A']++;
        }

        // Update sim state rack for Treasurer consistency
        std::vector<Tile> oppRackTiles;
        for(char c : oppRackChars) { Tile t; t.letter = c; t.points = 0; oppRackTiles.push_back(t); }
        simState.players[simState.currentPlayerIndex].rack = oppRackTiles;

        // 3. OPPONENT RESPONSE (Rational Actor)
        // We use generate_raw to find the move with the best NAV (Score + Equity)

        double bestNav = -10000.0;

        auto opponentConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
            Move m;
            m.row = cand.row; m.col = cand.col; m.horizontal = cand.isHorizontal;
            strncpy(m.word, cand.word, 16);
            m.word[15] = '\0';

            int score = Mechanics::calculateTrueScore(cand, simState.board, bonusBoard);

            // ASK THE TREASURER: What is this move really worth?
            // Note: We evaluate equity on the state *before* the move, assuming the move is candidate 'm'
            float nav = treasurer.evaluate_equity(simState, m, score);

            if (nav > bestNav) {
                bestNav = nav;
            }
            return true;
        };

        // Use Raw Generator directly for speed (Zero Allocation)
        kernel::MoveGenerator::generate_raw(simState.board, oppRackCounts, gDictionary, opponentConsumer);

        // 4. EVALUATE OUTCOME
        // If opponent can't move, NAV is 0 (or strictly negative score equivalent).
        // We return negative of their gain, because Minimax: My Gain = -(Opponent Gain)
        if (bestNav == -10000.0) return 0.0; // Pass

        return -(double)bestNav;
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

        double avgOutcome = (node->totalScore / node->visits) + node->moveLeadingTo.score;
        double exploration = UCT_C * std::sqrt(std::log(node->parent->visits) / node->visits);
        double bias = (node->heuristicBias * BIAS_WEIGHT) / (1 + node->visits);

        return avgOutcome + exploration + bias;
    }

}