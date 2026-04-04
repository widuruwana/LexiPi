#pragma once

#include "../engine/board.h"
#include "../engine/rack.h"
#include "../engine/dictionary.h"
#include "../move.h"
#include <vector>
#include <chrono>
#include <unordered_map>

namespace kernel {
    struct MoveCandidate;
}

namespace spectre {

    class Spy; // forward declaration — full definition in spy.h

    // Transposition Table Entry
    struct TTEntry {
        int depth;
        int value;
        int flag; // 0=Exact, 1=LowerBound, 2=UpperBound
        char bestWord[16];
    };

    class Judge {
    public:
        // Perfect-information endgame solver (bag empty)
        static kernel::MoveCandidate solveEndgame(const LetterBoard& board, const Board& bonusBoard,
                                 const TileRack& myRack, const TileRack& oppRack, Dictionary& dict);

        // Stochastic pre-endgame solver (bag has <= 14 tiles).
        // Samples numSamples opponent racks and averages minimax scores to handle uncertainty.
        static kernel::MoveCandidate solvePreEndgame(const LetterBoard& board, const Board& bonusBoard,
                                 const TileRack& myRack, spectre::Spy& spy,
                                 Dictionary& dict, int numSamples = 8);

    private:
        // Hash Helpers
        static uint64_t computeHash(const LetterBoard& board, int* myRack, int* oppRack, bool myTurn);

        // Core Minimax
        static int minimax(LetterBoard& board, // Passed by value-copy in recursion, optimized via reference here
                           const Board& bonusBoard,
                           int* currentRackCounts,
                           int* otherRackCounts,
                           Dictionary& dict,
                           int alpha, int beta,
                           bool maximizingPlayer,
                           int passesInARow,
                           int depth,
                           const std::chrono::steady_clock::time_point& startTime,
                           int timeBudgetMs,
                           std::unordered_map<uint64_t, TTEntry>& transTable);

        static bool applyMove(LetterBoard& board, const kernel::MoveCandidate& move, int* rackCounts);
        static void undoMove(LetterBoard& board, const kernel::MoveCandidate& move, int* rackCounts); // For backtracking if needed, or we just copy
    };
}