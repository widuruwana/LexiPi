#pragma once

#include "../engine/state.h"
#include "../move.h"
#include <vector>
#include <string>

namespace spectre {

    class Treasurer {
    public:
        Treasurer();

        // THE FINANCIAL MANDATE: Net Asset Value (NAV)
        // Calculates: Immediate Score + Future Equity.
        //
        // INPUTS:
        // - state: needed for Rack, Bag, and Board (to calc leave)
        // - move: the candidate move (coords + word)
        // - moveScore: passed explicitly to avoid re-calculation overhead
        float evaluate_equity(const GameState& state, const Move& move, int moveScore);

        // [NEW] Detailed Console Report
        void report_equity(const GameState& state, const Move& move, int moveScore) const;

    private:
        // --- ASSET PRICING KERNEL ---

        // 1. RISK TOLERANCE (Gamma)
        // Derived from the score spread.
        float calculate_gamma(int scoreDiff);

        // 2. FUNDAMENTAL VALUE (Alpha)
        // The intrinsic value of a tile (with Theta Decay for blanks).
        float get_fundamental_value(char tile, int bagSize);

        // 3. COVARIANCE / SYNERGY
        // Bonus for linguistic clusters and hedging.
        float calculate_synergy(const std::vector<char>& leave);

        // 4. RISK ASSESSMENT (Volatility)
        // Rack Volatility + Market Volatility.
        float calculate_rack_volatility(const std::vector<char>& leave);
        float calculate_market_volatility(const std::vector<Tile>& bag);
    };

}