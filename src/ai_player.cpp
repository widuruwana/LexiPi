#include "../include/ai_player.h"
#include "../include/spectre/judge.h"
#include "../include/engine/mechanics.h"
#include "../include/engine/dictionary.h" // For gDictionary
#include <cstring>
#include <iostream>

std::unique_ptr<PlayerController> create_ai_player(AIStyle style) {
    if (style == AIStyle::SPEEDI_PI) return std::make_unique<GreedyPlayer>();
    return std::make_unique<SpectrePlayer>();
}

// =========================================================
// GREEDY PLAYER IMPLEMENTATION
// =========================================================
GreedyPlayer::GreedyPlayer(std::string n) : playerName(n) {}

std::string GreedyPlayer::getName() const { return playerName; }

Move GreedyPlayer::getMove(const GameState& state, const Board& bonusBoard, const LastMoveInfo& lastMove, bool canChallenge) {
    // Fixed: Call generate_best_move instead of deliberate
    kernel::MoveCandidate bestCand = kernel::GreedyEngine::generate_best_move(state, bonusBoard);

    if (bestCand.word[0] == '\0') return Move(MoveType::PASS);

    Move m(MoveType::PLAY);
    m.row = bestCand.row; m.col = bestCand.col; m.horizontal = bestCand.isHorizontal;
    std::strncpy(m.word, bestCand.word, 16); m.word[15] = '\0';
    return m;
}

Move GreedyPlayer::getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) {
    return Move(MoveType::PASS);
}


// =========================================================
// SPECTRE PLAYER IMPLEMENTATION
// =========================================================
SpectrePlayer::SpectrePlayer(std::string n) : playerName(n) {}

std::string SpectrePlayer::getName() const { return playerName; }

void SpectrePlayer::observeMove(const Move& move, const LetterBoard& preMoveBoard) {
    spy.observeOpponentMove(move, preMoveBoard);
}

Move SpectrePlayer::getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) {
    return Move(MoveType::PASS);
}

Move SpectrePlayer::getMove(const GameState& state, const Board& bonusBoard, const LastMoveInfo& lastMove, bool canChallenge) {
    const Player& me = state.players[state.currentPlayerIndex];

    // 1. UPDATE INTELLIGENCE
    spy.updateGroundTruth(state.board, me.rack, state.bag);

    kernel::MoveCandidate bestCand;

    // 2. DECISION FORK
    if (state.bag.empty()) {
        std::vector<char> inferredOppChars;
        const std::vector<char>& rawPool = spy.getUnseenPool();

        if (rawPool.size() <= 7) {
            inferredOppChars = rawPool;
        } else {
            inferredOppChars = rawPool;
            static thread_local std::mt19937 rng(std::random_device{}());
            std::shuffle(inferredOppChars.begin(), inferredOppChars.end(), rng);
            if (inferredOppChars.size() > 7) inferredOppChars.resize(7);
        }

        TileRack oppRack;
        for(char c : inferredOppChars) {
            Tile t; t.letter = c; t.points = Mechanics::getPointValue(c);
            oppRack.push_back(t);
        }

        // Using global dictionary as required by judge
        bestCand = spectre::Judge::solveEndgame(state.board, bonusBoard, me.rack, oppRack, gDictionary);
    }
    else {
        std::vector<kernel::MoveCandidate> candidates = kernel::MoveGenerator::generate(state.board, me.rack, gDictionary);
        for(auto& cand : candidates) {
            cand.score = Mechanics::calculateTrueScore(cand, state.board, bonusBoard);
        }

        bestCand = vanguard.select_best_move(state, bonusBoard, candidates, spy);
    }

    if (bestCand.word[0] == '\0') return Move(MoveType::PASS);

    return calculateDifferential(state, bestCand);
}

Move SpectrePlayer::calculateDifferential(const GameState& state, const kernel::MoveCandidate& cand) {
    Move m(MoveType::PLAY);
    m.row = cand.row; m.col = cand.col; m.horizontal = cand.isHorizontal;

    int r = cand.row, c = cand.col;
    int dr = cand.isHorizontal ? 0 : 1, dc = cand.isHorizontal ? 1 : 0;
    int writeIdx = 0;

    for (int i = 0; cand.word[i] != '\0'; i++) {
        if (state.board[r][c] == ' ') m.word[writeIdx++] = cand.word[i];
        r += dr; c += dc;
    }
    m.word[writeIdx] = '\0';
    return m;
}