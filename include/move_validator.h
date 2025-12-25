#pragma once

#include "board.h"
#include "move.h"
#include <vector>
#include <string>

// Forward declaration
struct MoveCandidate;

// Structured placement with explicit blank tracking
struct TilePlacement {
    int row;
    int col;
    char letter;      // Always uppercase (A-Z)
    bool isBlank;    // True if this tile is a blank
};

// Structured move representation
struct StructuredMove {
    std::vector<TilePlacement> placements;
    bool isHorizontal;
    
    // Optional display word (with lowercase for blanks)
    std::string displayWord;
    
    // Computed fields
    int row;
    int col;
    int score;
};

// Result of move validation
struct ValidationResult {
    bool isValid;
    int score;
    std::string errorMessage;
    std::vector<std::string> formedWords;  // All words formed (main + crosses)
    std::vector<std::string> invalidWords; // Words not in dictionary
};

// Move validator with final legality + score recompute
class MoveValidator {
public:
    // Validate a move and recompute score from scratch
    static ValidationResult validateMove(
        const Board& bonusBoard,
        const LetterBoard& letters,
        const BlankBoard& blanks,
        const TileRack& rack,
        const StructuredMove& move
    );
    
    // Convert MoveCandidate to StructuredMove
    static StructuredMove fromCandidate(const MoveCandidate& candidate);
    
    // Convert StructuredMove to MoveCandidate (for compatibility)
    static MoveCandidate toCandidate(const StructuredMove& move);
    
    // Extract all words formed by a move
    static std::vector<std::string> extractFormedWords(
        const LetterBoard& letters,
        const StructuredMove& move
    );
    
    // Recompute score from scratch
    static int recomputeScore(
        const Board& bonusBoard,
        const LetterBoard& letters,
        const BlankBoard& blanks,
        const StructuredMove& move
    );
    
private:
    // Check if all formed words are in dictionary
    static bool validateWords(const std::vector<std::string>& words);
    
    // Check connectivity rules
    static bool validateConnectivity(
        const LetterBoard& letters,
        const StructuredMove& move
    );
    
    // Check that move uses tiles from rack correctly
    static bool validateRackUsage(
        const TileRack& rack,
        const StructuredMove& move
    );
};
