#pragma once

#include "../kernel/move_generator.h"
#include "../engine/state.h"
#include "../engine/board.h"
#include "../move.h"
#include "spy.h"
#include "vanguard.h"
#include <memory>

namespace spectre {

    class SpectreEngine {
    public:
        SpectreEngine();

        // Wipes the particle filters and MCTS trees for a new game
        void reset();

        kernel::MoveCandidate deliberate(const GameState& state, const Board& bonusBoard, bool isSpeediPi);
        void notify_move(const Move& move, const LetterBoard& board, const Board& bonusBoard);

    private:
        // Each engine instance physically owns its own agents
        std::unique_ptr<Spy> spy;
        std::unique_ptr<Vanguard> vanguard;
    };

}