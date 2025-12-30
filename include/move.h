#pragma once

#include <string>
#include <vector>
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

    // [CRITICAL FIX] Added tiles vector to match AIPlayer usage
    std::vector<Tile> tiles;

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