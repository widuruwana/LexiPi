#include "../../include/spectre/treasurer.h"
#include "../../include/engine/mechanics.h"
#include <cmath>
#include <iostream>
#include <iomanip>
#include <cstring>

namespace spectre {

    Treasurer::Treasurer() {
        memset(turn_deltas, 0, sizeof(turn_deltas));
        current_gamma = 0.0f;
        current_market_volatility = 0.0f;
    }

    void Treasurer::update_market_context(const TopologyReport& topo, int scoreDiff, const std::vector<Tile>& bag) {
        // 1. Establish Macro Variables
        current_gamma = calculate_gamma(scoreDiff);
        current_market_volatility = calculate_market_volatility(bag);

        // 2. Reset Deltas
        for (int i = 0; i < 27; i++) turn_deltas[i] = 0.0f;

        // 3. Generate Turn-Specific Asset Pricing (The "JIT" Logic)

        // A. The 'S' Utility (Hook Liquidity)
        // If the board is choking, 'S' loses its premium.
        if (topo.open_anchors < 5) {
            turn_deltas['S' - 'A'] -= 4.0f;
        } else if (topo.open_anchors > 20) {
            turn_deltas['S' - 'A'] += 2.0f;
        }

        // B. The Blank Option ('?')
        // Blanks gain immense value when trailing (variance seeking) AND TWS are open.
        if (current_gamma < 0.0f && topo.attackable_tws > 0) {
            turn_deltas[26] += 6.0f;
        }
        // Blanks lose value if the board is completely locked and we just need points now.
        if (topo.open_anchors == 0) {
            turn_deltas[26] -= 5.0f;
        }

        // C. Volatility Adjustment for "Junk" (Q, Z, J, X)
        // If we are desperate (gamma < 0), we don't penalize high-variance tiles as much.
        // If we are winning (gamma > 0), we want safe, high-liquidity leaves.
        const int junk_indices[] = {'Q'-'A', 'Z'-'A', 'J'-'A', 'X'-'A'};
        for (int idx : junk_indices) {
            turn_deltas[idx] -= (current_gamma * 3.0f);
        }

        // D. Bag Composition (Supply & Demand)
        // If the bag is heavily skewed towards vowels, discount vowels on the rack.
        int bagVowels = 0;
        int bagConsonants = 0;
        const std::string vChars = "AEIOU";
        for (const auto& t : bag) {
            if (t.letter == '?') continue;
            if (vChars.find(t.letter) != std::string::npos) bagVowels++;
            else bagConsonants++;
        }

        if (!bag.empty()) {
            float vowelRatio = (float)bagVowels / bag.size();
            if (vowelRatio > 0.6f) {
                // Oversupply of vowels in market -> discount vowels on rack
                turn_deltas['A'-'A'] -= 1.5f; turn_deltas['E'-'A'] -= 1.5f;
                turn_deltas['I'-'A'] -= 1.5f; turn_deltas['O'-'A'] -= 1.5f;
                turn_deltas['U'-'A'] -= 1.5f;
            } else if (vowelRatio < 0.3f) {
                // Drought of vowels -> premium on vowels
                turn_deltas['A'-'A'] += 1.5f; turn_deltas['E'-'A'] += 1.5f;
                turn_deltas['I'-'A'] += 1.5f; turn_deltas['O'-'A'] += 1.5f;
                turn_deltas['U'-'A'] += 1.5f;
            }
        }
    }

    float Treasurer::evaluate_equity(const GameState& state, const Move& move, int moveScore) const {
        // --- ZERO ALLOCATION LEAVE CALCULATION ---
        int leaveCounts[27] = {0};

        // 1. Populate current rack
        for (const auto& t : state.players[state.currentPlayerIndex].rack) {
            if (t.letter == '?') leaveCounts[26]++;
            else leaveCounts[t.letter - 'A']++;
        }

        // 2. Subtract placed tiles
        int r = move.row; int c = move.col;
        int dr = move.horizontal ? 0 : 1; int dc = move.horizontal ? 1 : 0;

        for (int i = 0; i < 15 && move.word[i] != '\0' && r < 15 && c < 15; i++) {
            char letter = move.word[i];
            if (state.board[r][c] == ' ') {
                bool isBlank = (letter >= 'a' && letter <= 'z');
                if (isBlank) {
                    leaveCounts[26]--;
                } else {
                    int idx = letter - 'A';
                    if (leaveCounts[idx] > 0) leaveCounts[idx]--;
                    else leaveCounts[26]--; // Fallback to blank if notation is weird
                }
            }
            r += dr; c += dc;
        }

        // 3. Calculate Base Equity (Quackle Static Table)
        float base_equity = get_quackle_baseline(leaveCounts);

        // 4. Apply JIT Context Deltas
        float dynamic_equity = 0.0f;
        for (int i = 0; i < 26; i++) {
            dynamic_equity += (leaveCounts[i] * turn_deltas[i]);
        }
        dynamic_equity += (leaveCounts[26] * turn_deltas[26]); // Blanks

        return (float)moveScore + base_equity + dynamic_equity;
    }

    float Treasurer::normalize_to_winprob(int currentScoreDiff, float moveNAV, int bagSize) const {
        // 1. Total Projected Spread
        float projected_spread = (float)currentScoreDiff + moveNAV;

        // 2. Dynamic Temperature (Variance Decay)
        // Midgame (Bag = 80) -> Temp = 55 (Flatter curve, high uncertainty)
        // Endgame (Bag = 10) -> Temp = 20 (Steep curve, low uncertainty)
        float temperature = 15.0f + ((float)bagSize * 0.5f);

        // Prevent division by zero just in case
        if (temperature < 1.0f) temperature = 1.0f;

        // 3. Logistic Sigmoid Mapping
        return 1.0f / (1.0f + std::exp(-projected_spread / temperature));
    }

    // --- MOCK QUACKLE BASELINE ---
    // A temporary stand-in until the 12MB hash table is loaded.
    float Treasurer::get_quackle_baseline(const int* leaveCounts) const {
        float val = 0.0f;
        // Standard high-level approximations
        val += leaveCounts[26] * 25.0f; // Blank
        val += leaveCounts['S'-'A'] * 8.0f;
        val -= leaveCounts['Q'-'A'] * 12.0f;
        val -= leaveCounts['V'-'A'] * 5.0f;

        // Vowel/Consonant balance heuristic
        int v = leaveCounts['A'-'A'] + leaveCounts['E'-'A'] + leaveCounts['I'-'A'] + leaveCounts['O'-'A'] + leaveCounts['U'-'A'];
        int c = 0;
        for(int i=0; i<26; i++) c += leaveCounts[i];
        c -= v;

        if (v == 0 && c > 0) val -= 5.0f;
        if (c == 0 && v > 0) val -= 5.0f;

        return val;
    }

    float Treasurer::calculate_gamma(int scoreDiff) const {
        if (scoreDiff > 60) return 1.5f;   // Highly Averse
        if (scoreDiff > 30) return 0.8f;
        if (scoreDiff < -60) return -1.2f; // Highly Seeking
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
        return std::sqrt(variance) / 2.5f; // Normalized roughly to 0-1
    }

    void Treasurer::report_equity(const GameState& state, const Move& move, int moveScore) const {
        // For CLI printing, we just run the fast evaluator and print the result.
        float final_nav = evaluate_equity(state, move, moveScore);
        std::cout << "[TREASURER JIT] Move: " << move.word
                  << " | Score: " << moveScore
                  << " | NAV: " << final_nav
                  << " | Gamma: " << current_gamma << std::endl;
    }
}