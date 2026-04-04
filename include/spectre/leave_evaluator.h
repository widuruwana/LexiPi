#pragma once

#include "../engine/types.h"
#include <string>

namespace spectre {

    // =========================================================
    // LEAVE EVALUATOR INTERFACE
    // =========================================================
    // The Leave Evaluator answers one question:
    // "How good are the tiles remaining on my rack after a play?"
    //
    // This is the single most impactful component for playing strength.
    // Swapping implementations here is the primary ablation axis.
    // =========================================================

    class LeaveEvaluator {
    public:
        virtual ~LeaveEvaluator() = default;

        // Core evaluation: Given rack counts AFTER a play, return leave equity.
        // leaveCounts[0..25] = A-Z counts, leaveCounts[26] = blank count.
        // Positive = good leave, Negative = bad leave.
        virtual float evaluate(const int leaveCounts[27]) const = 0;

        // Human-readable name for telemetry/ablation logging
        virtual const char* name() const = 0;
    };

    // ---------------------------------------------------------
    // IMPLEMENTATION 1: Hand-Tuned Static Leaves (Current)
    // ---------------------------------------------------------
    // Per-tile linear sum with vowel/consonant balance penalties.
    // No training data. Pure heuristic.
    // ---------------------------------------------------------
    class HandTunedLeaves : public LeaveEvaluator {
    public:
        float evaluate(const int leaveCounts[27]) const override;
        const char* name() const override { return "HandTuned"; }

    private:
        static constexpr float STATIC_LEAVES[27] = {
             1.0f,  -3.5f, -0.5f,  0.0f,  4.0f,
            -2.0f,  -2.0f,  0.5f, -0.5f, -3.0f,
            -2.5f,  -1.0f, -1.0f,  0.5f, -1.5f,
            -1.5f, -11.5f,  1.5f,  7.5f, -1.0f,
            -3.0f,  -5.5f, -4.0f, -3.5f, -2.0f,
            -2.0f,  24.0f
        };
    };

    // ---------------------------------------------------------
    // IMPLEMENTATION 2: Trained Linear Leaves (Future)
    // ---------------------------------------------------------
    // Per-tile weights learned from self-play regression.
    // Loaded from a binary file at startup.
    // ---------------------------------------------------------
    class TrainedLinearLeaves : public LeaveEvaluator {
    public:
        bool loadFromFile(const std::string& filename);
        float evaluate(const int leaveCounts[27]) const override;
        const char* name() const override { return "TrainedLinear"; }

    private:
        float weights[27] = {0};
        bool loaded = false;
    };

} // namespace spectre
