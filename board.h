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

// Create and return a board with all Scrabble bonus squares set.
Board createBoard();

// Print the board (with bonuses) to the terminal.
void printBoard(const Board &board);