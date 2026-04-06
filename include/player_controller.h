#pragma once

#include "engine/state.h"
#include "engine/board.h"
#include "move.h"
#include <string>

class PlayerController {
public:
    virtual ~PlayerController() = default;

    // Providing a default empty implementation so Human players don't break
    virtual void reset() {}

    virtual std::string getName() const = 0;

    virtual Move getMove(const GameState& state,
                         const Board& bonusBoard,
                         const LastMoveInfo& lastMove,
                         bool canChallenge) = 0;

    virtual Move getEndGameResponse(const GameState& state,
                                    const LastMoveInfo& lastMove) = 0;

    // Default empty implementation because human controllers don't need to observe silently
    virtual void observeMove(const Move& move, const LetterBoard& board, const Board& bonusBoard) {}
};