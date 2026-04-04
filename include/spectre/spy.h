#pragma once

#include "opponent_model.h"
#include "../engine/board.h"
#include "../engine/rack.h"
#include "../engine/tiles.h"
#include "../move.h"
#include <vector>
#include <array>
#include <string>

namespace spectre {

    struct Particle {
        std::array<char, 7> rack;
        int rack_size;
        double weight;
    };

    // =========================================================
    // THE SPY: Particle Filter Opponent Model
    // =========================================================
    // Novel contribution: Uses Sequential Monte Carlo to maintain
    // a belief distribution over opponent rack compositions.
    //
    // Implements OpponentModel interface for clean ablation:
    //   Spy vs UniformOpponentModel measures the value of inference.
    // =========================================================

    class Spy : public OpponentModel {
    public:
        static constexpr int PARTICLE_COUNT = 1000;

        Spy();
        ~Spy() override = default;

        // --- OpponentModel Interface ---
        void updateGroundTruth(const LetterBoard& board,
                                const TileRack& myRack,
                                const std::vector<Tile>& bag) override;

        void observeOpponentMove(const Move& move,
                                  const LetterBoard& preMoveBoard) override;

        std::vector<char> sampleOpponentRack() const override;
        const std::vector<char>& getUnseenPool() const override;
        const char* name() const override { return "ParticleFilter"; }

    private:
        std::vector<Particle> particles;
        std::vector<char> unseen_pool;

        void initialize_particles();
        void resample_particles();
        bool particle_contains_tiles(const Particle& p, const std::string& required_tiles) const;
    };

} // namespace spectre
