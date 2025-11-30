#pragma once

#include <array>
#include <string>

using namespace std;

// Board Constants
constexpr int BOARD_SIZE = 15; //15 x 15 scrabble board
constexpr int CELL_INNER_WIDTH = 5; //Characters inside each cell

enum class CellType {
    Normal,
    DLS, //Double Letter Score
    TLS, //Triple Letter Score
    DWS, //Double Word Score
    TWS, //Triple Word Score
};

// 2D board type
using Board = array < array < CellType, BOARD_SIZE >, BOARD_SIZE>;

//Holds the actual letters placed on the board
using LetterBoard = array < array < char, BOARD_SIZE >, BOARD_SIZE >;

// Track whether a letter on the board came from a blank tile.
// true = this square is a blank (score 0)
// false = normal tile
using BlankBoard = array < array < bool, BOARD_SIZE >, BOARD_SIZE >;

//initialize all letters to ' '
void clearLetterBoard(LetterBoard &letters);

//initialize all blank flags to false
void clearBlankBoard(BlankBoard &blanks);

// Place a word on the board (0-based row/col
// horizontal = true -> left to right
// horizontal = false -> top to bottom
// Returns a false if the word doesn't fit or conflicts with existing letters.
bool placeWordOnBoard(LetterBoard &letters, int row, int col, bool horizontal, const string &word);

// Create and return a board with all Scrabble bonus squares set.
Board createBoard();

// Print the board with letters if present, otherwise with bonuses.
void printBoard(const Board &bonusBoard, const LetterBoard &letters);