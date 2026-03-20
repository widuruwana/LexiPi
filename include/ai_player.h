#pragma once

#include "engine/board.h"
#include "engine/rack.h"
#include "move.h"
#include "player_controller.h"
#include "kernel/move_generator.h"
// REMOVED: #include "spectre/spy.h"
#include <vector>
#include <string>
#include <memory>

// DEFINE THE BOTS
enum class AIStyle {
    SPEEDI_PI,  // Fast, Static Heuristics (The "Speedster")
    CUTIE_PI    // MCTS, Simulation (The "Champion")
};

class AIPlayer : public PlayerController {
public:
    AIPlayer(AIStyle style);

    Move getMove(const GameState& state,
                 const Board& bonusBoard,
                 const LastMoveInfo& lastMove,
                 bool canChallenge) override;

    Move getEndGameResponse(const GameState& state,
                            const LastMoveInfo& lastMove) override;

    void observeMove(const Move& move, const LetterBoard& board);

    std::string getName() const override;

private:
    AIStyle style;
    // REMOVED: std::unique_ptr<spectre::Spy> spy;

    std::vector<kernel::MoveCandidate> candidates;

    // Internal Helpers
    void findAllMoves(const LetterBoard& letters, const TileRack& rack);
    int calculateStaticScore(const kernel::MoveCandidate& move,
                             const LetterBoard& letters,
                             const Board& bonusBoard);

    struct DifferentialMove { int row, col; char word[16]; };
    DifferentialMove calculateDifferential(const LetterBoard& letters, const kernel::MoveCandidate& best);
    bool isRackBad(const TileRack& rack);
    std::string getTilesToExchange(const TileRack& rack);
};