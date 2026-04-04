#include "../include/ai_player.h"

// =========================================================
// GREEDY PLAYER INCLUDES (kernel + engine ONLY)
// =========================================================
#include "../include/kernel/greedy_engine.h"
#include "../include/engine/mechanics.h"
#include "../include/engine/dictionary.h"

// =========================================================
// SPECTRE PLAYER INCLUDES (spectre/ interfaces)
// =========================================================
#include "../include/spectre/engine_config.h"
#include "../include/spectre/leave_evaluator.h"
#include "../include/spectre/opponent_model.h"
#include "../include/spectre/search_strategy.h"
#include "../include/spectre/spy.h"
#include "../include/spectre/treasurer.h"
#include "../include/spectre/vanguard.h"
#include "../include/spectre/judge.h"

#include <cstring>
#include <iostream>
#include <random>
#include <algorithm>

// =========================================================
// FACTORY
// =========================================================

std::unique_ptr<PlayerController> create_ai_player(AIStyle style) {
    if (style == AIStyle::SPEEDI_PI) {
        return std::make_unique<GreedyPlayer>();
    }
    return std::make_unique<SpectrePlayer>();
}

// =========================================================
// GREEDY PLAYER (Stateless Control Group)
// =========================================================
// This implementation has ZERO dependency on spectre/.
// It uses kernel::GreedyEngine and nothing else.
// =========================================================

GreedyPlayer::GreedyPlayer(std::string n) : playerName(std::move(n)) {}

std::string GreedyPlayer::getName() const { return playerName; }

Move GreedyPlayer::getMove(const GameState& state, const Board& bonusBoard,
                            const LastMoveInfo& lastMove, bool canChallenge) {
    kernel::MoveCandidate bestCand = kernel::GreedyEngine::generate_best_move(state, bonusBoard);

    if (bestCand.word[0] == '\0') {
        return Move(MoveType::PASS);
    }

    Move m(MoveType::PLAY);
    m.row = bestCand.row;
    m.col = bestCand.col;
    m.horizontal = bestCand.isHorizontal;
    std::strncpy(m.word, bestCand.word, 16);
    m.word[15] = '\0';
    return m;
}

Move GreedyPlayer::getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) {
    return Move(MoveType::PASS);
}

// =========================================================
// SPECTRE PLAYER (Stateful Test Group)
// =========================================================
// Composed from EngineConfig interfaces.
// Different configs = different ablation configurations.
// =========================================================

// Default constructor: builds the "current behavior" config
SpectrePlayer::SpectrePlayer(std::string n) : playerName(std::move(n)) {
    // Wire up default Spectre configuration
    leaveEval = std::make_shared<spectre::HandTunedLeaves>();
    opponentModel = std::make_shared<spectre::Spy>();
    treasurer = std::make_unique<spectre::Treasurer>(leaveEval.get());
    vanguard = std::make_unique<spectre::Vanguard>(treasurer.get());
    // searchStrategy not used when vanguard is active (legacy path)
}

// Config constructor: builds from an ablation configuration
SpectrePlayer::SpectrePlayer(spectre::EngineConfig config, std::string n)
    : playerName(std::move(n)),
      leaveEval(config.leaveEval),
      opponentModel(config.opponentModel),
      searchStrategy(config.searchStrategy)
{
    // Create Treasurer if we have a leave evaluator
    if (leaveEval) {
        treasurer = std::make_unique<spectre::Treasurer>(leaveEval.get());
        vanguard = std::make_unique<spectre::Vanguard>(treasurer.get());
    }
}

SpectrePlayer::~SpectrePlayer() = default;

std::string SpectrePlayer::getName() const { return playerName; }

void SpectrePlayer::observeMove(const Move& move, const LetterBoard& preMoveBoard) {
    if (opponentModel) {
        opponentModel->observeOpponentMove(move, preMoveBoard);
    }
}

Move SpectrePlayer::getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) {
    return Move(MoveType::PASS);
}

Move SpectrePlayer::getMove(const GameState& state, const Board& bonusBoard,
                             const LastMoveInfo& lastMove, bool canChallenge) {
    const Player& me = state.players[state.currentPlayerIndex];

    // 1. UPDATE INTELLIGENCE
    if (opponentModel) {
        opponentModel->updateGroundTruth(state.board, me.rack, state.bag);
    }

    kernel::MoveCandidate bestCand;
    bestCand.word[0] = '\0';
    bestCand.score = 0;

    // 2. DECISION FORK: Endgame vs Midgame
    if (state.bag.empty()) {
        // --- ENDGAME (The Judge) ---
        std::vector<char> inferredOppChars;
        if (opponentModel) {
            const std::vector<char>& rawPool = opponentModel->getUnseenPool();
            if (rawPool.size() <= 7) {
                inferredOppChars = rawPool;
            } else {
                inferredOppChars = rawPool;
                static thread_local std::mt19937 rng(std::random_device{}());
                std::shuffle(inferredOppChars.begin(), inferredOppChars.end(), rng);
                if (inferredOppChars.size() > 7) inferredOppChars.resize(7);
            }
        }

        TileRack oppRack;
        for (char c : inferredOppChars) {
            Tile t;
            t.letter = c;
            t.points = Mechanics::getPointValue(c);
            oppRack.push_back(t);
        }

        bestCand = spectre::Judge::solveEndgame(state.board, bonusBoard, me.rack, oppRack, gDictionary);
    }
    else {
        // --- MIDGAME ---
        // Generate all legal moves
        std::vector<kernel::MoveCandidate> candidates;
        candidates.reserve(300);

        int myRackCounts[27] = {0};
        for (const auto& t : me.rack) {
            if (t.letter == '?') myRackCounts[26]++;
            else if (isalpha(t.letter)) myRackCounts[toupper(t.letter) - 'A']++;
        }

        auto rootConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
            cand.score = Mechanics::calculateTrueScore(cand, state.board, bonusBoard);
            candidates.push_back(cand);
            return true;
        };

        kernel::MoveGenerator::generate_raw(state.board, myRackCounts, gDictionary, rootConsumer);

        // DISPATCH: Use SearchStrategy if configured, else Vanguard (legacy)
        if (searchStrategy) {
            bestCand = searchStrategy->selectMove(state, bonusBoard, candidates,
                                                   opponentModel.get());
        } else if (vanguard) {
            bestCand = vanguard->select_best_move(state, bonusBoard, candidates,
                                                    opponentModel.get());
        } else {
            // Fallback: pure score greedy
            for (const auto& c : candidates) {
                if (c.score > bestCand.score) bestCand = c;
            }
        }
    }

    if (bestCand.word[0] == '\0') {
        return Move(MoveType::PASS);
    }

    Move m(MoveType::PLAY);
    m.row = bestCand.row;
    m.col = bestCand.col;
    m.horizontal = bestCand.isHorizontal;
    std::strncpy(m.word, bestCand.word, 16);
    m.word[15] = '\0';
    return m;
}
