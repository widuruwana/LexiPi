#include "../../include/spectre/vanguard.h"
#include "../../include/kernel/move_generator.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/dictionary.h"
#include <cmath>
#include <limits>
#include <algorithm>
#include <chrono>
#include <cstring>

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
        spectre::Spy& spy)
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
        treasurer.update_market_context(topo, scoreDiff, state.bag);

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
        const int myPlayerIndex = state.currentPlayerIndex;
        auto startTime = high_resolution_clock::now();
        int mcts_iterations = 0; // We will track exactly how many simulations we run

        while (true) {
            auto now = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(now - startTime).count();
            if (duration >= TIME_BUDGET_MS) break;

            MCTSNode* leaf = select_node(root.get());

            if (!is_game_over(leaf->state) && !leaf->untriedMoves.empty()) {
                leaf = expand_node(leaf, bonusBoard);
            }

            double result = simulate_rollout(leaf->state, spy, bonusBoard, myPlayerIndex);
            backpropagate(leaf, result);
            mcts_iterations++;
        }

        // 5. POST-SIMULATION ANALYSIS & REPORTING
        // Sort children by the number of visits (The standard MCTS "Robust Child" selection)
        std::vector<MCTSNode*> sorted_children;
        for (const auto& child : root->children) {
            sorted_children.push_back(child.get());
        }

        std::sort(sorted_children.begin(), sorted_children.end(), [](MCTSNode* a, MCTSNode* b) {
            return a->visits > b->visits;
        });

        // The Clean Telemetry Output
        if (!sorted_children.empty()) {
            std::cout << "\n======================================================\n";
            std::cout << "[VANGUARD] MCTS Complete | Iterations: " << mcts_iterations
                      << " | Time: " << TIME_BUDGET_MS << "ms\n";
            std::cout << "------------------------------------------------------\n";
            std::cout << " TOP 3 EVALUATED LINES (POST-SIMULATION)\n";
            std::cout << "------------------------------------------------------\n";

            for (size_t i = 0; i < std::min((size_t)3, sorted_children.size()); i++) {
                MCTSNode* child = sorted_children[i];
                kernel::MoveCandidate& mc = child->moveLeadingTo;

                // Decode orientation so you know exactly WHERE the move is
                char dir = mc.isHorizontal ? 'H' : 'V';

                // Calculate actual MCTS win probability
                double win_prob = (child->visits > 0) ? (child->totalScore / child->visits) : 0.0;

                std::cout << " #" << (i+1) << ": " << mc.word
                          << " [" << mc.row << "," << mc.col << " " << dir << "] "
                          << "| Raw: " << mc.score
                          << " | Visits: " << child->visits
                          << " | Win Prob: " << (win_prob * 100.0) << "%\n";
            }
            std::cout << "======================================================\n\n";
        }

        // Return the most visited robust child
        return !sorted_children.empty() ? sorted_children[0]->moveLeadingTo : candidates.back();
    }

    double Vanguard::consult_council(const GameState& state, const kernel::MoveCandidate& cand) {
        Move m;
        m.row = cand.row; m.col = cand.col; m.horizontal = cand.isHorizontal;
        m.type = MoveType::PLAY;
        std::strncpy(m.word, cand.word, 16); m.word[15] = '\0';

        float topoScore = general.evaluate_topology(state, m);
        double topoBias = (topoScore - 0.5) * 40.0;
        float nav = treasurer.evaluate_equity(state, m, cand.score);

        int myScore = state.players[state.currentPlayerIndex].score;
        int oppScore = state.players[1 - state.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        return treasurer.normalize_to_winprob(scoreDiff, nav + topoBias, state.bag.size());
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

        // O(1) Bias Transfer. Zero re-evaluation.
        child->heuristicBias = emove.win_prob;

        MCTSNode* childPtr = child.get();
        node->children.push_back(std::move(child));
        return childPtr;
    }

    double Vanguard::simulate_rollout(const GameState& startState, const spectre::Spy& spy, const Board& bonusBoard, int myPlayerIndex) {
        // 4-PLY GREEDY ROLLOUT — zero heap allocation after the initial rack sample.
        // Constraints: consumer-pattern move generation, stack-local board/rack copies, no I/O.

        // Step 1: sample opponent rack (one std::vector alloc at entry — acceptable per architecture)
        std::vector<char> oppChars = spy.sampleParticleRack();

        // Step 2: stack-local rack counts (A-Z = indices 0-25, blank = index 26)
        int myCounts[27] = {0};
        int oppCounts[27] = {0};

        for (const auto& tile : startState.players[myPlayerIndex].rack) {
            if (tile.letter == '?') myCounts[26]++;
            else if (tile.letter >= 'A' && tile.letter <= 'Z') myCounts[tile.letter - 'A']++;
        }
        for (char c : oppChars) {
            if (c == '?') oppCounts[26]++;
            else if (c >= 'A' && c <= 'Z') oppCounts[c - 'A']++;
        }

        // Step 3: stack-local board copy (LetterBoard is a fixed-size char array — trivial copy)
        LetterBoard rolloutBoard = startState.board;

        // Step 4: running score differential
        int myRolloutScore  = startState.players[myPlayerIndex].score;
        int oppRolloutScore = startState.players[1 - myPlayerIndex].score;
        int currentPlayer   = startState.currentPlayerIndex;

        // Step 5: MAX_ROLLOUT_DEPTH plies of greedy play
        for (int ply = 0; ply < MAX_ROLLOUT_DEPTH; ply++) {
            int* activeCounts = (currentPlayer == myPlayerIndex) ? myCounts : oppCounts;

            // Consumer-pattern best-move search — zero vector allocation
            int bestScore = 0;
            kernel::MoveCandidate bestCand{};
            auto consumer = [&](kernel::MoveCandidate& cand, int* /*leave*/) -> bool {
                int s = Mechanics::calculateTrueScore(cand, rolloutBoard, bonusBoard);
                if (s > bestScore) { bestScore = s; bestCand = cand; }
                return true;
            };
            kernel::MoveGenerator::generate_raw(rolloutBoard, activeCounts, gDictionary, consumer);

            if (bestScore == 0) break; // no legal move — implicit pass, stop rollout

            // Apply best move inline: place tiles on board, deduct from rack
            {
                int r = bestCand.row, c = bestCand.col;
                int dr = bestCand.isHorizontal ? 0 : 1, dc = bestCand.isHorizontal ? 1 : 0;
                for (int i = 0; bestCand.word[i] != '\0'; i++, r += dr, c += dc) {
                    if (rolloutBoard[r][c] == ' ') {
                        char letter = bestCand.word[i];
                        bool isBlank = (letter >= 'a' && letter <= 'z');
                        rolloutBoard[r][c] = isBlank ? (char)(letter - 32) : letter;
                        if (isBlank) {
                            if (activeCounts[26] > 0) activeCounts[26]--;
                        } else {
                            int idx = letter - 'A';
                            if (activeCounts[idx] > 0) activeCounts[idx]--;
                            else if (activeCounts[26] > 0) activeCounts[26]--;
                        }
                    }
                }
            }

            if (currentPlayer == myPlayerIndex) myRolloutScore  += bestScore;
            else                                 oppRolloutScore += bestScore;

            currentPlayer = 1 - currentPlayer;
        }

        // Step 6: convert final score differential to win probability
        int finalDiff = myRolloutScore - oppRolloutScore;
        return treasurer.normalize_to_winprob(finalDiff, 0.0f, (int)startState.bag.size());
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

}