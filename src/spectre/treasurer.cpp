#include "../../include/spectre/treasurer.h"
#include "../../include/engine/mechanics.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <cstring>

namespace spectre {

    Treasurer::Treasurer(LeaveEvaluator* le) : leaveEval(le) {
        for (int i = 0; i < 27; i++) turn_deltas[i] = 0.0f;
        current_gamma = 0.0f;
        current_market_volatility = 0.0f;
    }

    void Treasurer::update_market_context(const TopologyReport& topo, int scoreDiff, const std::vector<Tile>& bag) {
        current_gamma = calculate_gamma(scoreDiff);
        current_market_volatility = calculate_market_volatility(bag);

        for (int i = 0; i < 27; i++) turn_deltas[i] = 0.0f;

        // Topology-driven adjustments
        if (topo.open_anchors < 5) {
            turn_deltas['S' - 'A'] -= 4.0f;
        } else if (topo.open_anchors > 20) {
            turn_deltas['S' - 'A'] += 2.0f;
        }

        if (current_gamma < 0.0f && topo.attackable_tws > 0) {
            turn_deltas[26] += 6.0f;
        }
        if (topo.open_anchors == 0) {
            turn_deltas[26] -= 5.0f;
        }

        // Junk tile penalty scaled by game phase
        const int junk_indices[] = {'Q'-'A', 'Z'-'A', 'J'-'A', 'X'-'A'};
        for (int idx : junk_indices) {
            turn_deltas[idx] -= (current_gamma * 3.0f);
        }

        // Bag composition adjustments
        int bagVowels = 0, bagConsonants = 0;
        for (const auto& t : bag) {
            if (t.letter == '?') continue;
            if (t.letter == 'A' || t.letter == 'E' || t.letter == 'I' ||
                t.letter == 'O' || t.letter == 'U') {
                bagVowels++;
            } else {
                bagConsonants++;
            }
        }

        if (!bag.empty()) {
            float vowelRatio = (float)bagVowels / bag.size();
            if (vowelRatio > 0.6f) {
                turn_deltas['A'-'A'] -= 1.5f; turn_deltas['E'-'A'] -= 1.5f;
                turn_deltas['I'-'A'] -= 1.5f; turn_deltas['O'-'A'] -= 1.5f;
                turn_deltas['U'-'A'] -= 1.5f;
            } else if (vowelRatio < 0.3f) {
                turn_deltas['A'-'A'] += 1.5f; turn_deltas['E'-'A'] += 1.5f;
                turn_deltas['I'-'A'] += 1.5f; turn_deltas['O'-'A'] += 1.5f;
                turn_deltas['U'-'A'] += 1.5f;
            }
        }
    }

    float Treasurer::evaluate_equity(const GameState& state, const Move& move, int moveScore) const {
        int leaveCounts[27] = {0};

        // Populate current rack
        for (const auto& t : state.players[state.currentPlayerIndex].rack) {
            if (t.letter == '?') leaveCounts[26]++;
            else if (t.letter >= 'A' && t.letter <= 'Z') leaveCounts[t.letter - 'A']++;
        }

        // Subtract placed tiles (only from empty squares)
        int r = move.row, c = move.col;
        int dr = move.horizontal ? 0 : 1;
        int dc = move.horizontal ? 1 : 0;

        for (int i = 0; i < 15 && move.word[i] != '\0' && r < 15 && c < 15; i++) {
            char letter = move.word[i];
            if (state.board[r][c] == ' ') {
                bool isBlank = (letter >= 'a' && letter <= 'z');
                if (isBlank) {
                    if (leaveCounts[26] > 0) leaveCounts[26]--;
                } else {
                    int idx = letter - 'A';
                    if (idx >= 0 && idx < 26 && leaveCounts[idx] > 0) {
                        leaveCounts[idx]--;
                    } else if (leaveCounts[26] > 0) {
                        leaveCounts[26]--;
                    }
                }
            }
            r += dr; c += dc;
        }

        // Baseline from plugged LeaveEvaluator
        float base_equity = leaveEval->evaluate(leaveCounts);

        // Dynamic context layer (Treasurer's unique contribution)
        float dynamic_equity = 0.0f;
        for (int i = 0; i < 26; i++) {
            dynamic_equity += (leaveCounts[i] * turn_deltas[i]);
        }
        dynamic_equity += (leaveCounts[26] * turn_deltas[26]);

        return (float)moveScore + base_equity + dynamic_equity;
    }

    float Treasurer::normalize_to_winprob(int currentScoreDiff, float moveNAV, int bagSize) const {
        float projected_spread = (float)currentScoreDiff + moveNAV;
        float temperature = 15.0f + ((float)bagSize * 0.5f);
        if (temperature < 1.0f) temperature = 1.0f;
        return 1.0f / (1.0f + std::exp(-projected_spread / temperature));
    }

    float Treasurer::calculate_gamma(int scoreDiff) const {
        if (scoreDiff > 60) return 1.5f;
        if (scoreDiff > 30) return 0.8f;
        if (scoreDiff < -60) return -1.2f;
        if (scoreDiff < -30) return -0.5f;
        return 0.2f;
    }

    float Treasurer::calculate_market_volatility(const std::vector<Tile>& bag) const {
        if (bag.empty()) return 0.0f;
        float sum = 0.0f, sq_sum = 0.0f;
        for (const auto& t : bag) {
            float val = (float)Mechanics::getPointValue(t.letter);
            sum += val; sq_sum += (val * val);
        }
        float n = (float)bag.size();
        float mean = sum / n;
        float variance = (sq_sum / n) - (mean * mean);
        return std::sqrt(variance) / 2.5f;
    }

    void Treasurer::report_equity(const GameState& state, const Move& move, int moveScore) const {
        float final_nav = evaluate_equity(state, move, moveScore);
        std::cout << "[TREASURER] Move: " << move.word
                  << " | Score: " << moveScore
                  << " | NAV: " << std::fixed << std::setprecision(2) << final_nav
                  << " | Gamma: " << current_gamma
                  << " | Leaves: " << leaveEval->name()
                  << std::endl;
    }

} // namespace spectre
