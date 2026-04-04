#pragma once
#include <cctype>

// =========================================================
// KERNEL HEURISTICS (Greedy Engine ONLY)
// =========================================================
// These are fast, stateless lookup tables for the greedy engine.
// They have ZERO dependency on anything in spectre/.
//
// Spectre's leave evaluation uses the LeaveEvaluator interface
// in spectre/leave_evaluator.h instead.
// =========================================================

namespace kernel {

    class Heuristics {
    public:
        // Point values for A-Z (standard Scrabble)
        static constexpr int TILE_VALUES[26] = {
            1, 3, 3, 2, 1, 4, 2, 4, 1, 8, 5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10
        };

        // Simple per-tile leave heuristic for greedy engine.
        // These are NOT the trained values — they exist only to make
        // the greedy player slightly smarter than pure score maximizing.
        static constexpr float LEAVE_VALUES[26] = {
            1.0f, -2.0f, -1.0f, 0.0f, 3.0f, -2.0f, -2.0f, 2.0f, 1.0f, -3.0f,
            -2.5f, 0.5f, -1.0f, 1.0f, -1.5f, -1.5f, -6.0f, 3.5f, 8.0f, 1.5f,
            -3.0f, -5.0f, -3.5f, -3.0f, -2.5f, -2.0f
        };

        // Unsafe: Assumes input is 0-25 (Use inside optimized loops)
        static inline int getValueFast(int charIdx0to25) {
            return TILE_VALUES[charIdx0to25];
        }

        // Safe wrapper
        static int getTileValue(char letter) {
            if (letter == '?' || (letter >= 'a' && letter <= 'z')) return 0;
            if (letter >= 'A' && letter <= 'Z') return TILE_VALUES[letter - 'A'];
            return 0;
        }

        // Simple per-tile leave score for greedy engine
        static float getLeaveValue(char letter) {
            if (letter == '?') return 25.0f;
            int idx = (letter & 31) - 1;
            if (idx >= 0 && idx < 26) return LEAVE_VALUES[idx];
            return 0.0f;
        }
    };

} // namespace kernel
