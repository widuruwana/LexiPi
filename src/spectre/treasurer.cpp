#include "../../include/spectre/treasurer.h"
#include "../../include/engine/mechanics.h"
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <map>
#include <iostream>
#include <numeric>

using namespace std;

namespace spectre {

    Treasurer::Treasurer() {}

    float Treasurer::evaluate_equity(const GameState& state, const Move& move, int moveScore) {
        // --- 1. RECONSTRUCT THE PORTFOLIO (The Leave) ---

        /*
        std::cout << "[TREASURER] Eval Move: " << move.word
                  << " | Raw Score: " << moveScore
                  << std::endl;
        */

        // A. Snapshot the Rack (Asset Ledger)
        // Using map to track counts of available tiles
        std::map<char, int> rackLedger;
        for (const auto& t : state.players[state.currentPlayerIndex].rack) {
            rackLedger[t.letter]++;
        }

        // B. Identify Placed Tiles (The "Sales")
        int r = move.row;
        int c = move.col;
        int dr = move.horizontal ? 0 : 1;
        int dc = move.horizontal ? 1 : 0;

        // Iterate through the word provided in the Move struct
        for (int i = 0; i < 15; i++) {
            char letter = move.word[i];
            if (letter == '\0') break;
            if (r >= 15 || c >= 15) break;

            if (state.board[r][c] == ' ') {
                // --- FIX START ---
                // Check if the move string explicitly indicates a blank (lowercase)
                bool isBlank = (letter >= 'a' && letter <= 'z');

                if (isBlank) {
                    // Explicit Blank Usage
                    if (rackLedger['?'] > 0) rackLedger['?']--;
                } else {
                    // Normal Letter Usage
                    char upperC = letter; // It's already uppercase
                    if (rackLedger[upperC] > 0) {
                        rackLedger[upperC]--;
                    } else if (rackLedger['?'] > 0) {
                        // Fallback: We used a blank as a letter but didn't notate it as lowercase?
                        // (Safety catch, but the isBlank check above handles standard notation)
                        rackLedger['?']--;
                    }
                }
                // --- FIX END ---
            }

            // Advance cursor
            r += dr;
            c += dc;
        }

        // C. Construct the Leave Vector
        std::vector<char> leave;
        for (auto const& [key, val] : rackLedger) {
            for (int k = 0; k < val; k++) leave.push_back(key);
        }

        //std::cout << "[TREASURER] Leave: ";
        //for (char c : leave) std::cout << c;
        //std::cout << std::endl;

        // --- 2. MARKET CONTEXT ---
        int bagSize = (int)state.bag.size();

        // Score Diff Calculation
        int myScore = state.players[state.currentPlayerIndex].score;
        int oppScore = state.players[1 - state.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        // --- 3. FINANCIAL ANALYSIS ---

        // A. Fundamental Value (Sum of Parts)
        float fundamentals = 0.0f;
        for (char c : leave) {
            fundamentals += get_fundamental_value(c, bagSize);
        }

        // B. Synergy (Correlation Bonus)
        float synergy = calculate_synergy(leave);

        // C. Risk Calculation (Rack Volatility * Market Volatility)
        float rackRisk = calculate_rack_volatility(leave);
        float marketRisk = calculate_market_volatility(state.bag);

        // Amplification: Risky rack in a risky market is dangerous.
        float totalRisk = rackRisk * (1.0f + marketRisk);

        // D. Gamma Adjustment (Risk Preference)
        float gamma = calculate_gamma(scoreDiff);

        // --- 4. NET ASSET VALUE (NAV) ---
        // Equity = (Fundamentals + Synergy) - (RiskAversion * Risk)
        float equity = (fundamentals + synergy) - (gamma * totalRisk);

        /*
        std::cout << "[TREASURER] Financial Report:"
                  << "\n\t Fundamentals: " << fundamentals
                  << "\n\t Synergy: " << synergy
                  << "\n\t Total Risk: " << totalRisk
                  << "\n\t Gamma (Risk Aversion): " << gamma
                  << "\n\t Equity Adjustment: " << equity
                  << "\n\t Final NAV: " << ((float)moveScore + equity)
                  << std::endl;
        */

        // Return Total NAV (Immediate Cash + Future Equity)
        return (float)moveScore + equity;
    }

    float Treasurer::get_fundamental_value(char tile, int bagSize) {
        // 1. DERIVATIVES (The Blank)
        if (tile == '?') {
            if (bagSize > 40) return 35.0f; // High Option Value
            if (bagSize > 10) return 25.0f; // Stable
            return 8.0f;   // Expiry
        }

        // 2. BLUE CHIP (S)
        if (tile == 'S') return 10.0f;

        // 3. TOXIC ASSETS (Liabilities)
        if (tile == 'Q') return -12.0f;
        if (tile == 'V') return -6.0f;
        if (tile == 'U') return -4.0f;

        // 4. TREASURY BONDS (E, R, I, N, A, T)
        const std::string bonds = "ERINAT";
        if (bonds.find(tile) != std::string::npos) return 4.0f;

        // 5. JUNK BONDS (Z, X, J, K)
        const std::string junk = "ZXJK";
        if (junk.find(tile) != std::string::npos) return -2.0f;

        // Default Commodity
        return 1.0f;
    }

    float Treasurer::calculate_synergy(const std::vector<char>& leave) {
        if (leave.empty()) return 0.0f;

        float synergy = 0.0f;
        std::string s = "";
        for (char c : leave) s += c;
        std::sort(s.begin(), s.end());

        // A. HEDGING (Q-U Bond)
        bool hasQ = (s.find('Q') != std::string::npos);
        bool hasU = (s.find('U') != std::string::npos);
        if (hasQ && hasU) synergy += 10.0f;

        // B. LINGUISTIC CLUSTERS
        if (s.find("ING") != std::string::npos) synergy += 12.0f;
        else if (s.find("IN") != std::string::npos) synergy += 4.0f;

        if (s.find("ER") != std::string::npos) synergy += 5.0f;
        if (s.find("EST") != std::string::npos) synergy += 6.0f;

        // C. CANNIBALIZATION (Duplicates)
        std::map<char, int> counts;
        for (char c : leave) counts[c]++;

        for (auto const& [key, val] : counts) {
            if (val > 1) {
                // Non-linear penalty
                synergy -= std::pow((float)val, 2.0f);
            }
        }

        // D. PORTFOLIO BALANCE
        int vowels = 0;
        const std::string vChars = "AEIOU";
        for (char c : leave) {
            if (vChars.find(c) != std::string::npos) vowels++;
        }
        int consonants = (int)leave.size() - vowels;

        if (vowels == 0 && consonants > 0) synergy -= 6.0f;
        if (consonants == 0 && vowels > 0) synergy -= 6.0f;

        return synergy;
    }

    float Treasurer::calculate_rack_volatility(const std::vector<char>& leave) {
        float vol = 0.0f;
        const std::string volChars = "QZJX";
        for (char c : leave) {
            if (c == '?') vol += 10.0f;
            if (volChars.find(c) != std::string::npos) vol += 8.0f;
        }
        return vol;
    }

    float Treasurer::calculate_market_volatility(const std::vector<Tile>& bag) {
        if (bag.empty()) return 0.0f;

        double sum = 0.0;
        double sq_sum = 0.0;

        // Iterating standard vector<Tile>
        for (const auto& t : bag) {
            double val = (double)Mechanics::getPointValue(t.letter);
            sum += val;
            sq_sum += (val * val);
        }

        double n = (double)bag.size();
        double mean = sum / n;
        double variance = (sq_sum / n) - (mean * mean);

        // Normalize (StdDev 0-3 range -> 0-1 range approx)
        return (float)(std::sqrt(variance) / 2.5);
    }

    float Treasurer::calculate_gamma(int scoreDiff) {
        // LEADING -> Risk Averse
        if (scoreDiff > 60) return 1.5f;
        if (scoreDiff > 30) return 0.8f;

        // TRAILING -> Risk Seeking
        if (scoreDiff < -60) return -1.2f;
        if (scoreDiff < -30) return -0.5f;

        return 0.2f;
    }

void Treasurer::report_equity(const GameState& state, const Move& move, int moveScore) const {
        // RECONSTRUCT LOGIC FOR REPORTING
        // (We duplicate the read-only logic to avoid altering the core calculation signature)

        std::map<char, int> rackLedger;
        for (const auto& t : state.players[state.currentPlayerIndex].rack) rackLedger[t.letter]++;

        int r = move.row; int c = move.col;
        int dr = move.horizontal ? 0 : 1;
        int dc = move.horizontal ? 1 : 0;

        for (int i = 0; i < 15; i++) {
            char letter = move.word[i];
            if (letter == '\0') break;
            if (state.board[r][c] == ' ') {
                bool isBlank = (letter >= 'a' && letter <= 'z');
                if (isBlank) {
                    if (rackLedger['?'] > 0) rackLedger['?']--;
                } else {
                    char upperC = letter;
                    if (rackLedger[upperC] > 0) rackLedger[upperC]--;
                    else if (rackLedger['?'] > 0) rackLedger['?']--;
                }
            }
            r += dr; c += dc;
        }

        std::vector<char> leave;
        for (auto const& [key, val] : rackLedger) {
            for (int k = 0; k < val; k++) leave.push_back(key);
        }

        auto* mutThis = const_cast<Treasurer*>(this);

        int bagSize = (int)state.bag.size();
        int scoreDiff = state.players[state.currentPlayerIndex].score - state.players[1 - state.currentPlayerIndex].score;

        float fundamentals = 0.0f;
        for (char tile : leave) fundamentals += mutThis->get_fundamental_value(tile, bagSize);

        float synergy = mutThis->calculate_synergy(leave);
        float rackRisk = mutThis->calculate_rack_volatility(leave);
        float marketRisk = mutThis->calculate_market_volatility(state.bag);
        float totalRisk = rackRisk * (1.0f + marketRisk);
        float gamma = mutThis->calculate_gamma(scoreDiff);
        float equity = (fundamentals + synergy) - (gamma * totalRisk);

        std::cout << "[TREASURER REPORT] Move: " << move.word << " (" << moveScore << " pts)"
                  << "\n  > Leave: ";
        for(char c : leave) std::cout << c;
        std::cout << "\n  > Fundamentals: " << std::fixed << std::setprecision(2) << fundamentals
                  << " | Synergy: " << synergy
                  << "\n  > Risk (Rack/Mkt): " << rackRisk << " / " << marketRisk
                  << " | Gamma: " << gamma
                  << "\n  > EQUITY ADJUSTMENT: " << equity
                  << " (NAV: " << (moveScore + equity) << ")"
                  << std::endl;
    }
}