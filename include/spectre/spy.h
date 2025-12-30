#pragma once
#include <vector>
#include <map>
#include <array>
#include <mutex>
#include "../../include/board.h"
#include "../../include/tile_tracker.h"
#include "../../include/dawg.h"
#include "../../include/move.h"

namespace spectre {

    class Spy {
    public:
        Spy();
        void reset();

        void updateGroundTruth(const LetterBoard& board, const std::vector<char>& myRack);
        void observeOpponentMove(const Move& move, int score, const LetterBoard& preMoveBoard, Dawg& dict);

        // [CRITICAL FIX] New signature accepts scratch buffers for Zero-Allocation generation.
        void generateWeightedRack(int* rackCounts, std::vector<char>& bagBuf, std::vector<double>& weightBuf);

        // Legacy method overload (for convenience outside hot loops)
        void generateWeightedRack(int* rackCounts);

    private:
        TileTracker tracker;
        std::array<float, 27> beliefMatrix;

        void applyNegativeInference(const LetterBoard& board, int opponentScore, Dawg& dict);
        void applyRetentionBias(int keptCount);
        void applyRackBalanceInference(const std::string& wordPlayed);
    };

}