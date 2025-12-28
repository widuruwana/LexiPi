#pragma once

#include <vector>
#include <string>
#include <future>
#include "../../include/board.h"
#include "../../include/rack.h"
#include "../../include/spectre/move_generator.h"

using namespace std;

namespace spectre {

class Vanguard {
public:
    // Main Entry Point: Selects the best move using Monte Carlo Simulation
    // timeLimitMs: How long to think (e.g., 500ms)
    static MoveCandidate search(
        const LetterBoard& board,
        const Board& bonusBoard,
        const TileRack& rack,
        const vector<char>& unseenBag,
        Dawg& dict,
        int timeLimitMs = 1000
    );

private:
    // Run a single random game from this state to the end
    // Returns: The Point Spread (MyScore - OppScore)
    static int playout(
        LetterBoard board,
        const Board& bonusBoard,
        int* myRackCounts,
        int* oppRackCounts, // Opponent rack (inferred or random)
        vector<char> bag,
        bool myTurn,
        Dawg& dict
    );

    // Calculates the score of a move (Main Word + Cross Words)
    static int calculateScore(
        const LetterBoard& board,
        const Board& bonusBoard,
        const MoveCandidate& move
    );

    // Fast helper to apply a move to a board and return score
    static int applyMove(
        LetterBoard& board,
        const MoveCandidate& move,
        int* rackCounts
    );

    // Fast helper to fill a rack from the bag
    static void fillRack(int* rackCounts, vector<char>& bag);
};

}