#pragma once

#include "engine/board.h"
#include "engine/rack.h"
#include "move.h"
#include "player_controller.h"
#include "kernel/move_generator.h"
#include "kernel/greedy_engine.h"
#include <string>
#include <memory>

// =========================================================
// FORWARD DECLARATIONS (Spectre types - NOT included here)
// =========================================================
// GreedyPlayer has ZERO knowledge of spectre/.
// SpectrePlayer forward-declares what it needs.
// =========================================================

namespace spectre {
    class OpponentModel;
    class LeaveEvaluator;
    class SearchStrategy;
    class Treasurer;
    class Vanguard;
    class General;
    struct EngineConfig;
}

// =========================================================
// AI STYLE ENUM (For menu compatibility)
// =========================================================
enum class AIStyle {
    SPEEDI_PI,
    CUTIE_PI
};

// Factory to spawn isolated players per match
std::unique_ptr<PlayerController> create_ai_player(AIStyle style);

// ---------------------------------------------------------
// THE CONTROL GROUP (Stateless, Pure Greedy)
// ---------------------------------------------------------
// Dependencies: kernel/greedy_engine.h ONLY.
// No spectre headers. No intelligence. Pure speed.
// ---------------------------------------------------------
class GreedyPlayer : public PlayerController {
public:
    GreedyPlayer(std::string name = "Speedi_Pi");

    Move getMove(const GameState& state, const Board& bonusBoard,
                 const LastMoveInfo& lastMove, bool canChallenge) override;
    Move getEndGameResponse(const GameState& state,
                            const LastMoveInfo& lastMove) override;
    std::string getName() const override;

private:
    std::string playerName;
};

// ---------------------------------------------------------
// THE TEST GROUP (Stateful, Configurable Intelligence)
// ---------------------------------------------------------
// Dependencies: spectre/ interfaces ONLY (via forward decl).
// Composed from an EngineConfig at construction time.
// Different configs = different rows in the ablation table.
// ---------------------------------------------------------
class SpectrePlayer : public PlayerController {
public:
    // Construct with full config (for ablation)
    SpectrePlayer(spectre::EngineConfig config, std::string name = "Cutie_Pi");

    // Convenience: default config (current behavior)
    SpectrePlayer(std::string name = "Cutie_Pi");

    ~SpectrePlayer() override;

    Move getMove(const GameState& state, const Board& bonusBoard,
                 const LastMoveInfo& lastMove, bool canChallenge) override;
    Move getEndGameResponse(const GameState& state,
                            const LastMoveInfo& lastMove) override;
    void observeMove(const Move& move, const LetterBoard& preMoveBoard) override;
    std::string getName() const override;

private:
    std::string playerName;

    // Composed components (lifetime managed by shared_ptr in EngineConfig)
    std::shared_ptr<spectre::LeaveEvaluator> leaveEval;
    std::shared_ptr<spectre::OpponentModel> opponentModel;
    std::shared_ptr<spectre::SearchStrategy> searchStrategy;

    // Owned components (created from config, die with this instance)
    std::unique_ptr<spectre::Treasurer> treasurer;
    std::unique_ptr<spectre::Vanguard> vanguard;
};
