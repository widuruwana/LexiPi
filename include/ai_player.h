#pragma once

#include "spectre/spectre_engine.h"
#include "engine/board.h"
#include "engine/rack.h"
#include "move.h"
#include "player_controller.h"
#include "kernel/move_generator.h"
#include <vector>
#include <string>
#include <memory>

// --- DEFINE THE BOTS ---
enum class AIStyle {
    SPEEDI_PI,  // Fast, Static Heuristics (The "Speedster")
    CUTIE_PI    // MCTS, Guided Simulation (The "Champion")
};

/**
 * @brief The bridge between the AI Agents and the Game Director.
 * @invariant This controller MUST submit moves adhering to the Full String Doctrine.
 */
class AIPlayer : public PlayerController {

public:
    AIPlayer(AIStyle style);

    void reset() override;

    Move getMove(const GameState& state,
                 const Board& bonusBoard,
                 const LastMoveInfo& lastMove,
                 bool canChallenge) override;

    Move getEndGameResponse(const GameState& state,
                            const LastMoveInfo& lastMove) override;

    void observeMove(const Move& move, const LetterBoard& board, const Board& bonusBoard) override;

    std::string getName() const override;

private:
    spectre::SpectreEngine engine;
    AIStyle style;
    // Exchange Heuristics
    bool isRackBad(const TileRack& rack);
    std::string getTilesToExchange(const TileRack& rack);
};