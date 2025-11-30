#pragma once

#include <string>
#include "board.h"
#include "tiles.h"
#include "rack.h"

using namespace std;

// Result of trying to play a word
struct MoveResult {
    bool success;
    int score;
    string errorMessage;
};

// Play a word using the tiles in the rack.
//
// - bonusBoard: fixed DL/TL/DW/TW layout
// - letters:    board letters (A–Z or ' ')
// - blanks:     which board cells are blanks
// - bag:        tile bag (to refill rack)
// - rack:       player's rack (tiles will be consumed/refilled)
// - startRow:   0-based (A=0, B=1, ...)
// - startCol:   0-based (1=>0, 2=>1, ...)
// - horizontal: true = left→right, false = top→bottom
// - rackWord:   ONLY the letters the player is placing from their rack
//               (do NOT include letters already on the board)
//
// Example for FANSITE over an existing S on the board:
//   Board already has S in the path.
//   Player types just "FANSITE".
//   playWord will fill empty cells with those letters and "skip" the S.

MoveResult playWord(
    const Board &bonusBoard,
    LetterBoard &letters,
    BlankBoard &blanks,
    TileBag &bag,
    TileRack &rack,
    int startRow,
    int startCol,
    bool horizontal,
    const string &rackWord
);