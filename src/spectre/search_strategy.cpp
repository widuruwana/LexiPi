#include "../../include/spectre/search_strategy.h"
#include "../../include/spectre/treasurer.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/dictionary.h"
#include "../../include/kernel/move_generator.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <chrono>
#include <random>

namespace spectre {

    // =========================================================
    // GreedySelect: Pick highest raw score
    // =========================================================

    kernel::MoveCandidate GreedySelect::selectMove(
        const GameState& state,
        const Board& bonusBoard,
        const std::vector<kernel::MoveCandidate>& candidates,
        OpponentModel* /*opponentModel*/)
    {
        if (candidates.empty()) {
            kernel::MoveCandidate pass;
            pass.word[0] = '\0';
            pass.score = 0;
            return pass;
        }

        auto best = std::max_element(candidates.begin(), candidates.end(),
            [](const kernel::MoveCandidate& a, const kernel::MoveCandidate& b) {
                return a.score < b.score;
            });

        return *best;
    }

    // =========================================================
    // EquitySelect: Pick highest score + leave equity
    // =========================================================

    EquitySelect::EquitySelect(LeaveEvaluator* le) : leaveEval(le) {}

    void EquitySelect::computeLeave(const GameState& state,
                                     const kernel::MoveCandidate& cand,
                                     int outLeaveCounts[27]) const {
        // Start with current rack
        std::memset(outLeaveCounts, 0, 27 * sizeof(int));
        for (const auto& t : state.players[state.currentPlayerIndex].rack) {
            if (t.letter == '?') outLeaveCounts[26]++;
            else if (t.letter >= 'A' && t.letter <= 'Z') outLeaveCounts[t.letter - 'A']++;
            else if (t.letter >= 'a' && t.letter <= 'z') outLeaveCounts[t.letter - 'a']++;
        }

        // Subtract tiles placed by this candidate
        int r = cand.row;
        int c = cand.col;
        int dr = cand.isHorizontal ? 0 : 1;
        int dc = cand.isHorizontal ? 1 : 0;

        for (int i = 0; cand.word[i] != '\0' && r < 15 && c < 15; i++) {
            char letter = cand.word[i];
            if (state.board[r][c] == ' ') {
                bool isBlank = (letter >= 'a' && letter <= 'z');
                if (isBlank) {
                    if (outLeaveCounts[26] > 0) outLeaveCounts[26]--;
                } else {
                    int idx = letter - 'A';
                    if (idx >= 0 && idx < 26 && outLeaveCounts[idx] > 0) {
                        outLeaveCounts[idx]--;
                    } else if (outLeaveCounts[26] > 0) {
                        outLeaveCounts[26]--;
                    }
                }
            }
            r += dr;
            c += dc;
        }
    }

    kernel::MoveCandidate EquitySelect::selectMove(
        const GameState& state,
        const Board& bonusBoard,
        const std::vector<kernel::MoveCandidate>& candidates,
        OpponentModel* /*opponentModel*/)
    {
        if (candidates.empty()) {
            kernel::MoveCandidate pass;
            pass.word[0] = '\0';
            pass.score = 0;
            return pass;
        }

        kernel::MoveCandidate best;
        best.word[0] = '\0';
        float bestEquity = -1e9f;

        int leaveCounts[27];

        for (const auto& cand : candidates) {
            computeLeave(state, cand, leaveCounts);
            float equity = (float)cand.score + leaveEval->evaluate(leaveCounts);

            if (equity > bestEquity) {
                bestEquity = equity;
                best = cand;
            }
        }

        return best;
    }

    // =========================================================
    // SimulationSelect: N-game rollout per candidate
    // =========================================================

    SimulationSelect::SimulationSelect(LeaveEvaluator* le, int sc, int tb)
        : leaveEval(le), simCount(sc), timeBudgetMs(tb) {}

    kernel::MoveCandidate SimulationSelect::selectMove(
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

        // Pre-filter: Only simulate top candidates by equity (saves time)
        const int MAX_CANDIDATES_TO_SIM = 15;

        std::vector<std::pair<float, int>> equityRanked;
        equityRanked.reserve(candidates.size());

        int leaveCounts[27];
        for (int i = 0; i < (int)candidates.size(); i++) {
            // Quick equity estimate for filtering
            int rackCounts[27] = {0};
            for (const auto& t : state.players[state.currentPlayerIndex].rack) {
                if (t.letter == '?') rackCounts[26]++;
                else if (t.letter >= 'A' && t.letter <= 'Z') rackCounts[t.letter - 'A']++;
            }

            // Subtract placed tiles
            int r = candidates[i].row, c = candidates[i].col;
            int dr = candidates[i].isHorizontal ? 0 : 1;
            int dc = candidates[i].isHorizontal ? 1 : 0;
            for (int j = 0; candidates[i].word[j] != '\0' && r < 15 && c < 15; j++) {
                if (state.board[r][c] == ' ') {
                    char letter = candidates[i].word[j];
                    bool isBlank = (letter >= 'a' && letter <= 'z');
                    if (isBlank) { if (rackCounts[26] > 0) rackCounts[26]--; }
                    else {
                        int idx = letter - 'A';
                        if (idx >= 0 && idx < 26 && rackCounts[idx] > 0) rackCounts[idx]--;
                        else if (rackCounts[26] > 0) rackCounts[26]--;
                    }
                }
                r += dr; c += dc;
            }

            float eq = (float)candidates[i].score + leaveEval->evaluate(rackCounts);
            equityRanked.push_back({eq, i});
        }

        std::sort(equityRanked.begin(), equityRanked.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        int numToSim = std::min(MAX_CANDIDATES_TO_SIM, (int)equityRanked.size());

        // Simulation loop
        auto startTime = std::chrono::high_resolution_clock::now();
        kernel::MoveCandidate bestMove = candidates[equityRanked[0].second];
        float bestAvgSpread = -1e9f;

        int simsPerCandidate = std::max(1, simCount / numToSim);

        for (int ci = 0; ci < numToSim; ci++) {
            // Time check
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
            if (elapsed >= timeBudgetMs) break;

            int candIdx = equityRanked[ci].second;
            const kernel::MoveCandidate& cand = candidates[candIdx];

            float totalSpread = 0.0f;
            int completedSims = 0;

            for (int s = 0; s < simsPerCandidate; s++) {
                // Time check inside inner loop
                now = std::chrono::high_resolution_clock::now();
                elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
                if (elapsed >= timeBudgetMs) break;

                // 1. Clone state and apply our move
                GameState simState = state;
                Move m;
                m.type = MoveType::PLAY;
                m.row = cand.row; m.col = cand.col;
                m.horizontal = cand.isHorizontal;
                std::strncpy(m.word, cand.word, 16); m.word[15] = '\0';
                Mechanics::applyMove(simState, m, cand.score);

                // 2. Sample opponent rack
                std::vector<char> oppRack;
                if (opponentModel) {
                    oppRack = opponentModel->sampleOpponentRack();
                }

                // 3. Simulate opponent's best greedy reply
                int oppRackCounts[27] = {0};
                for (char ch : oppRack) {
                    if (ch == '?') oppRackCounts[26]++;
                    else if (ch >= 'A' && ch <= 'Z') oppRackCounts[ch - 'A']++;
                }

                int oppBestScore = 0;
                auto oppConsumer = [&](kernel::MoveCandidate& oppCand, int* leave) -> bool {
                    int score = Mechanics::calculateTrueScore(oppCand, simState.board, bonusBoard);
                    if (score > oppBestScore) oppBestScore = score;
                    return true;
                };

                kernel::MoveGenerator::generate_raw(simState.board, oppRackCounts, gDictionary, oppConsumer);

                // 4. Record spread (my score gain - opponent's best reply)
                totalSpread += (float)(cand.score - oppBestScore);
                completedSims++;
            }

            if (completedSims > 0) {
                float avgSpread = totalSpread / completedSims;
                if (avgSpread > bestAvgSpread) {
                    bestAvgSpread = avgSpread;
                    bestMove = cand;
                }
            }
        }

        return bestMove;
    }

} // namespace spectre
