#pragma once

#include "board.h"
#include <vector>
#include <stack>

// Structured placement with explicit blank tracking
struct TilePlacement {
    int row;
    int col;
    char letter;      // Always uppercase (A-Z)
    bool isBlank;    // True if this tile is a blank
};

// Track a single board modification
struct BoardChange {
    int row;
    int col;
    char oldLetter;
    bool oldBlank;
    bool wasEmpty;
};

// Board state manager with make/unmove support
class BoardStateManager {
public:
    BoardStateManager();
    
    // Apply a move to the board (make)
    void makeMove(
        LetterBoard& letters,
        BlankBoard& blanks,
        const std::vector<TilePlacement>& placements
    );
    
    // Undo the last move (unmake)
    void unmakeMove(
        LetterBoard& letters,
        BlankBoard& blanks
    );
    
    // Get current depth (number of moves made)
    int getDepth() const { return static_cast<int>(changeStack.size()); }
    
    // Clear all changes
    void clear();
    
    // Get Zobrist hash (for transposition table)
    uint64_t getHash(const LetterBoard& letters, const BlankBoard& blanks) const;
    
private:
    std::stack<BoardChange> changeStack;
    
    // Zobrist hashing tables
    uint64_t pieceHash[15][15][27];  // [row][col][letter+blank]
    uint64_t blankHash[15][15];           // [row][col]
    uint64_t hash;
    
    void initializeZobrist();
    uint64_t random64();
};
