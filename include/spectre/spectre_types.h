#pragma once

#include "../kernel/move_generator.h"
#include "../engine/state.h"
#include <vector>
#include <memory>

namespace spectre {

    // Board topology snapshot — produced by General, consumed by Treasurer
    struct TopologyReport {
        float openness;
        int open_anchors;
        int attackable_tws;
        bool is_fractured;
    };

    // Candidate + cached council evaluation — produced by council, consumed by Vanguard
    struct EvaluatedMove {
        kernel::MoveCandidate cand;
        double win_prob;
    };
}