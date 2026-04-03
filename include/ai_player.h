#pragma once

#include "engine/board.h"
#include "engine/rack.h"
#include "move.h"
#include "player_controller.h"
#include "kernel/move_generator.h"
#include "kernel/greedy_engine.h"
#include "spectre/vanguard.h"
#include "spectre/spy.h"
#include <string>
#include <memory>

// Restored to keep PvE and AiAi menus compiling
enum class AIStyle {
    SPEEDI_PI,
    CUTIE_PI
};

// Factory to spawn isolated players per match
std::unique_ptr<PlayerController> create_ai_player(AIStyle style);

// ---------------------------------------------------------
// THE CONTROL GROUP (Stateless)
// ---------------------------------------------------------
class GreedyPlayer : public PlayerController {
public:
    GreedyPlayer(std::string name = "Speedi_Pi");

    Move getMove(const GameState& state, const Board& bonusBoard, const LastMoveInfo& lastMove, bool canChallenge) override;
    Move getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) override;
    std::string getName() const override;

private:
    std::string playerName;
};

// ---------------------------------------------------------
// THE TEST GROUP (Stateful)
// ---------------------------------------------------------
class SpectrePlayer : public PlayerController {
public:
    SpectrePlayer(std::string name = "Cutie_Pi");

    Move getMove(const GameState& state, const Board& bonusBoard, const LastMoveInfo& lastMove, bool canChallenge) override;
    Move getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) override;
    void observeMove(const Move& move, const LetterBoard& preMoveBoard) override;
    std::string getName() const override;

private:
    std::string playerName;

    // Memory Encapsulation: Born and die with this instance.
    spectre::Spy spy;
    spectre::Vanguard vanguard;

};