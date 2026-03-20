#pragma once

#include "move_generator.h"
#include "../engine/state.h"
#include "../engine/board.h"

namespace kernel {

    class GreedyEngine {
    public:
        // Pure generation logic. Returns the best candidate based on score + leave.
        static kernel::MoveCandidate generate_best_move(const GameState& state, const Board& bonusBoard);
    };

}