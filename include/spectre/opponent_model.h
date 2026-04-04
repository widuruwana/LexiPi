#pragma once

#include "../engine/types.h"
#include "../engine/board.h"
#include <vector>
#include <array>

namespace spectre {

    // =========================================================
    // OPPONENT MODEL INTERFACE
    // =========================================================
    // The Opponent Model answers one question:
    // "What tiles does my opponent likely hold?"
    //
    // This is the intelligence layer. Swapping implementations
    // here tests the value of opponent rack inference.
    // =========================================================

    class OpponentModel {
    public:
        virtual ~OpponentModel() = default;

        // Called once per turn: Sync the model with absolute ground truth.
        // unseenPool = all tiles not on the board and not on my rack.
        virtual void updateGroundTruth(const LetterBoard& board,
                                        const TileRack& myRack,
                                        const std::vector<Tile>& bag) = 0;

        // Called when opponent plays: Update beliefs based on observed tiles.
        virtual void observeOpponentMove(const Move& move,
                                          const LetterBoard& preMoveBoard) = 0;

        // Sample a hypothetical opponent rack for simulation.
        virtual std::vector<char> sampleOpponentRack() const = 0;

        // Access the unseen tile pool (bag + opponent rack from our perspective).
        virtual const std::vector<char>& getUnseenPool() const = 0;

        virtual const char* name() const = 0;
    };

    // ---------------------------------------------------------
    // IMPLEMENTATION 1: Uniform Random Model (Baseline)
    // ---------------------------------------------------------
    // Draws random 7-tile racks from the unseen pool.
    // No memory of opponent behavior. Maximum ignorance.
    // ---------------------------------------------------------
    class UniformOpponentModel : public OpponentModel {
    public:
        UniformOpponentModel();

        void updateGroundTruth(const LetterBoard& board,
                                const TileRack& myRack,
                                const std::vector<Tile>& bag) override;

        void observeOpponentMove(const Move& move,
                                  const LetterBoard& preMoveBoard) override;

        std::vector<char> sampleOpponentRack() const override;
        const std::vector<char>& getUnseenPool() const override;
        const char* name() const override { return "Uniform"; }

    private:
        std::vector<char> unseen_pool;
    };

    // ---------------------------------------------------------
    // IMPLEMENTATION 2: Particle Filter Model (The Spy)
    // ---------------------------------------------------------
    // Forward declaration - full definition stays in spy.h
    // This is the novel contribution for the paper.
    // ---------------------------------------------------------
    // (Spy class already implements this interface - we'll adapt it)

} // namespace spectre
