#pragma once

#include "../engine/board.h"
#include "../../include/move.h"
#include "../../include/engine/rack.h"
#include "../engine/tiles.h"
#include <vector>
#include <random>

namespace spectre {

    struct Particle {
        std::vector<char> rack;
        double weight;
    };

    class Spy {
    public:
        Spy();

        TileRack inferOpponentRack() const;
        void observeOpponentMove(const Move& move, const LetterBoard& preMoveBoard, const Board& bonusBoard);
        void updateGroundTruth(const LetterBoard& board, const TileRack& myRack, const TileBag& bag);
        std::vector<char> generateWeightedRack() const;

        // [FIX] Expose the raw pool for direct Endgame access
        const std::vector<char>& getUnseenPool() const;

    private:
        std::vector<char> unseenPool;
        std::vector<Particle> particles;
        const int PARTICLE_COUNT = 1000;

        // Internal Logic
        int findBestPossibleScore(const std::vector<char>& rack, const LetterBoard& board);

        void initParticles();

        void resample_particles();
    };

}