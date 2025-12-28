#pragma once

#include "../../include/board.h"
#include "move_generator.h"
#include "../../include/dawg.h"

namespace spectre {

class Spy {
public:
    // Analyzes a board state to determine how dangerous it is.
    // Returns the Max Score the opponent could get with a "Killer Rack" (S A T I R E ?)
    static int getRiskScore(const LetterBoard& board, const Board& bonusBoard, Dawg& dict);

private:
    // Helper to calculate score quickly for the Spy's hypothetical moves
    static int calculateSpyScore(const LetterBoard& board, const Board& bonusBoard, const MoveCandidate& move);
};

}