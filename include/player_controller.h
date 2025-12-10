#pragma once

#include <string>
#include "board.h"
#include "rack.h"
#include "tiles.h"
#include "move.h"

using namespace std;

class PlayerController {
public:
    virtual ~PlayerController() = default;

    // Returns a turn-ending move
    // playerNum is 1-based (1 or 2)
    virtual Move getMove( const Board &bonusBoard, // The bonus board
                          const LetterBoard &letters, // Current letters on the board
                          const BlankBoard &blankBoard,
                          const TileBag &Bag,
                          const Player &me,
                          const Player &opponent,
                          int playerNum) = 0;

    virtual Move getEndGameDecision() = 0;
};