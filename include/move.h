#pragma once

#include <string>
#include "board.h"
#include "tiles.h"
#include "rack.h"

using namespace std;

// Game can have 2-4 players
struct Player {
    TileRack rack;
    int score = 0;
    int passCount = 0;
};

// Result of trying to play a word
struct MoveResult {
    bool success;
    int score;
    string errorMessage;
};

enum class MoveType {
    PLAY, // Placing a word
    PASS, // Passing a turn
    EXCHANGE, // Swap tiles with bag
    CHALLENGE, // Challenge previous move
    QUIT, // Resign
    NONE // (Internal Use)
};

// To hold the move data
struct Move {
    MoveType type = MoveType::NONE;

    // For playing
    string word; // Playing word/exchanging tiles
    int row = -1;
    int col = -1;
    bool horizontal = true;

    // For exchange
    string exchangeLetters;

    static Move Pass() { return {MoveType::PASS}; }
    static Move Quit() { return {MoveType::QUIT}; }
    static Move Challenge() { return {MoveType::CHALLENGE}; }
    static Move Play(int r, int c, bool h, string w) {
        return {MoveType::PLAY, w, r, c, h};
    }
    static Move Exchange(string tiles) {
        return {MoveType::EXCHANGE, " ", -1, -1, true, tiles};
    }
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