#include "../include/ai_player.h"
#include "../include/spectre/judge.h"
#include "../include/engine/mechanics.h"
#include "../include/engine/dictionary.h"
#include <cstring>
#include <iostream>
#include <random>
#include <algorithm>

std::unique_ptr<PlayerController> create_ai_player(AIStyle style) {
    if (style == AIStyle::SPEEDI_PI) {
        return std::make_unique<GreedyPlayer>();
    }
    return std::make_unique<SpectrePlayer>();
}

// =========================================================
// GREEDY PLAYER IMPLEMENTATION (Stateless Control Group)
// =========================================================

GreedyPlayer::GreedyPlayer(std::string n) : playerName(n) {}

std::string GreedyPlayer::getName() const {
    return playerName;
}

Move GreedyPlayer::getMove(const GameState& state, const Board& bonusBoard, const LastMoveInfo& lastMove, bool canChallenge) {
    const Player& me = state.players[state.currentPlayerIndex];

    // Pure stateless generation
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
// SPECTRE PLAYER IMPLEMENTATION (Stateful Test Group)
// =========================================================

SpectrePlayer::SpectrePlayer(std::string n) : playerName(n) {}

std::string SpectrePlayer::getName() const {
    return playerName;
}

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
    bestCand.word[0] = '\0';
    bestCand.score = 0;

    // 2. DECISION FORK
    if (state.bag.empty()) {
        // --- ENDGAME (The Judge) ---
        std::vector<char> inferredOppChars;
        const std::vector<char>& rawPool = spy.getUnseenPool();

        if (rawPool.size() <= 7) {
            inferredOppChars = rawPool;
        } else {
            inferredOppChars = rawPool;
            static thread_local std::mt19937 rng(std::random_device{}());
            std::shuffle(inferredOppChars.begin(), inferredOppChars.end(), rng);
            if (inferredOppChars.size() > 7) {
                inferredOppChars.resize(7);
            }
        }

        TileRack oppRack;
        for(char c : inferredOppChars) {
            Tile t;
            t.letter = c;
            t.points = Mechanics::getPointValue(c);
            oppRack.push_back(t);
        }

        bestCand = spectre::Judge::solveEndgame(state.board, bonusBoard, me.rack, oppRack, gDictionary);
    }
    else {
        // --- MIDGAME (The Vanguard + Zero Allocation Root Generator) ---
        std::vector<kernel::MoveCandidate> candidates;
        candidates.reserve(300); // Prevent reallocation during generation

        int myRackCounts[27] = {0};
        for (const auto& t : me.rack) {
            if (t.letter == '?') {
                myRackCounts[26]++;
            } else if (isalpha(t.letter)) {
                myRackCounts[toupper(t.letter) - 'A']++;
            }
        }

        // The Zero-Allocation Consumer Pipeline
        auto rootConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
            cand.score = Mechanics::calculateTrueScore(cand, state.board, bonusBoard);
            candidates.push_back(cand);
            return true;
        };

        kernel::MoveGenerator::generate_raw(state.board, myRackCounts, gDictionary, rootConsumer);

        // Pass the pre-evaluated candidates to Vanguard
        bestCand = vanguard.select_best_move(state, bonusBoard, candidates, spy);
    }

    if (bestCand.word[0] == '\0') {
        return Move(MoveType::PASS);
    }

    // 3. FULL STRING SEMANTICS (Fixes Spy/Treasurer tracking bugs)
    Move m(MoveType::PLAY);
    m.row = bestCand.row;
    m.col = bestCand.col;
    m.horizontal = bestCand.isHorizontal;
    std::strncpy(m.word, bestCand.word, 16);
    m.word[15] = '\0';

    return m;
}