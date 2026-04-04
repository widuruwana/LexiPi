#pragma once

#include "../engine/state.h"
#include "../move.h"
#include "general.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace spectre {

    class Treasurer {
    public:
        Treasurer();
        ~Treasurer() = default;

        // Called EXACTLY ONCE at the start of the turn, before MCTS begins
        void update_market_context(const TopologyReport& topo, int scoreDiff, const std::vector<Tile>& bag);

        // Called thousands of times per second. Zero allocations. Pure O(1) math.
        float evaluate_equity(const GameState& state, const Move& move, int moveScore) const;

        // Converts raw projected points into a strict [0.0, 1.0] win probability
        float normalize_to_winprob(int currentScoreDiff, float moveNAV, int bagSize) const;

        // Debug/CLI reporting
        void report_equity(const GameState& state, const Move& move, int moveScore) const;

        // Returns true if the Magpie combinatorial leave table is loaded and active
        bool isMagpieLoaded() const { return magpie_loaded; }

    private:
        float turn_deltas[27];
        float current_gamma;
        float current_market_volatility;

        // Magpie combinatorial leave table (loaded from data/magpie.bin if present)
        std::unordered_map<uint32_t, float> magpie_table;
        bool magpie_loaded = false;

        // Loads magpie.bin from the given path. Returns true on success.
        bool load_magpie(const std::string& path);

        float calculate_gamma(int scoreDiff) const;
        float calculate_market_volatility(const std::vector<Tile>& bag) const;
        float get_quackle_baseline(const int* leaveCounts) const;
    };

}