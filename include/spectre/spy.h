#pragma once

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

    class Spy {
    public:
        // Changed to constexpr to avoid copy-assignment deletion issues during refactoring
        static constexpr int PARTICLE_COUNT = 1000;

        Spy();
        ~Spy() = default;

        // Updates the unseen pool based on absolute knowns (Board + My Rack + Bag)
        void updateGroundTruth(const LetterBoard& board, const TileRack& myRack, const std::vector<Tile>& bag);

        // Observes the opponent's move, comparing it to the pre-move board to find exactly which tiles left their rack
        void observeOpponentMove(const Move& move, const LetterBoard& preMoveBoard);

        // Samples a random particle uniformly to generate a hypothetical opponent rack for Vanguard
        std::vector<char> sampleParticleRack() const;

        const std::vector<char>& getUnseenPool() const;

    private:
        std::vector<Particle> particles;
        std::vector<char> unseen_pool;

        // Internal Helpers
        void initialize_particles();
        void resample_particles();
        bool particle_contains_tiles(const Particle& p, const std::string& required_tiles) const;

        // Softer check: allows one tile in the particle to act as a wildcard blank.
        // Used as a recovery pass before full reboot when all particles die.
        bool particle_contains_tiles_with_wildcard(const Particle& p, const std::string& required_tiles) const;
    };

}