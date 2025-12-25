#pragma once

#include <string>
#include <cstdint>
#include "board.h"
#include "move.h"

// Centralized ruleset configuration
class Ruleset {
public:
    static Ruleset& getInstance();
    
    // Initialize with configuration
    void initialize(
        const std::string& lexiconId = "CSW24",
        int bingoBonus = 50,
        int rackSize = 7,
        int boardSize = 15
    );
    
    // Getters
    std::string getLexiconId() const { return lexiconId; }
    int getBingoBonus() const { return bingoBonus; }
    int getRackSize() const { return rackSize; }
    int getBoardSize() const { return boardSize; }
    
    // Validation
    bool isInitialized() const { return initialized; }
    
    // Calculate score for a move
    int scoreMove(const Board& bonusBoard, const LetterBoard& letters, const BlankBoard& blanks, const Move& move);

    // Prevent copying
    Ruleset(const Ruleset&) = delete;
    Ruleset& operator=(const Ruleset&) = delete;

private:
    Ruleset() = default;
    ~Ruleset() = default;
    
    std::string lexiconId;
    int bingoBonus;
    int rackSize;
    int boardSize;
    bool initialized = false;
};
