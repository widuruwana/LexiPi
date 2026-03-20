#pragma once

#include "../kernel/move_generator.h"
#include "../engine/state.h"
#include "../engine/board.h"
#include "../move.h"

namespace SpectreEngine {

    // Main entry point for the Multi-Agent System
    // Returns a MoveCandidate to maintain compatibility with AIPlayer's data pipeline
    kernel::MoveCandidate deliberate(const GameState& state, const Board& bonusBoard);

    // Updates the Spy's probability models
    void notify_move(const Move& move, const LetterBoard& board);

}