#pragma once

#include "board.h"
#include "move.h"
#include <vector>
#include <string>

// Forward declaration
struct MoveCandidate;

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
        const Move& move
    );
    
    // Convert MoveCandidate to Move
    static Move fromCandidate(const MoveCandidate& candidate);
    
    // Extract all words formed by a move
    static std::vector<std::string> extractFormedWords(
        const LetterBoard& letters,
        const Move& move
    );
    
private:
    // Check if all formed words are in dictionary
    static bool validateWords(const std::vector<std::string>& words);
    
    // Check connectivity rules
    static bool validateConnectivity(
        const LetterBoard& letters,
        const Move& move
    );
    
    // Check that move uses tiles from rack correctly
    static bool validateRackUsage(
        const TileRack& rack,
        const Move& move
    );
};
