#include "../include/ai_player.h"
#include "../include/kernel/heuristics.h"
#include "../include/tile_tracker.h"
#include "../include/kernel/move_generator.h"
#include "../include/spectre/vanguard.h"
#include "../include/engine/dictionary.h"
#include "../include/modes/PvE/pve.h"
#include "../include/spectre/judge.h"
#include "../include/engine/mechanics.h"

#include "../include/spectre/spectre_engine.h"
#include "../include/kernel/greedy_engine.h"

#include "../include/kernel/fast_constraints.h"

#include <cstring>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace spectre;
using namespace std;
using namespace std::chrono;

// --- CONSTRUCTOR & IDENTITY ---
AIPlayer::AIPlayer(AIStyle style) : style(style) {
    // REMOVED: Spy initialization. It is now handled statically in SpectreEngine.
}

string AIPlayer::getName() const {
    return (style == AIStyle::SPEEDI_PI) ? "Speedi_Pi" : "Cutie_Pi";
}

// --- NEW: WIRE THE BRAIN ---
void AIPlayer::observeMove(const Move& move, const LetterBoard& board) {
    if (style == AIStyle::CUTIE_PI) {
        // Delegate to Engine
        SpectreEngine::notify_move(move, board);
    }
}

// Optimization: Prune the search space
struct SearchRange {
    int minRow, maxRow, minCol, maxCol;
    bool isEmpty;
};

AIPlayer::DifferentialMove AIPlayer::calculateDifferential(const LetterBoard &letters, const kernel::MoveCandidate &bestMove) {
    DifferentialMove diff;
    diff.row = -1;
    diff.col = -1;
    diff.word[0] = '\0';

    int r = bestMove.row;
    int c = bestMove.col;
    int dr = bestMove.isHorizontal ? 0 : 1;
    int dc = bestMove.isHorizontal ? 1 : 0;
    int len = 0;

    for (char letter : bestMove.word) {
        if (letter == '\0') break;

        // If board is empty at this position, it's a new tile -> add to word
        if (letters[r][c] == ' ') {
            if (diff.row == -1 && diff.col == -1) {
                diff.row = r;
                diff.col = c;
            }
            if (len < 15) {
                diff.word[len++] = letter;
            }
        }
        r += dr;
        c += dc;
    }
    diff.word[len] = '\0'; // Null terminate
    return diff;
}

bool AIPlayer::isRackBad(const TileRack& rack) {
    int v=0, c=0;
    for (auto t : rack) {
        if (strchr("AEIOU", t.letter)) {
            v++;
        }else if (t.letter != '?') {
            c++;
        }
    }
    return (v == 0 || c == 0) && rack.size() >= 5;
}

string AIPlayer::getTilesToExchange(const TileRack& rack) {
    string s; for (auto t : rack) if (t.letter != '?' && t.letter != 'S') s += t.letter;
    return s.empty() ? "A" : s;
}

Move AIPlayer::getMove(const GameState& state,
                       const Board& bonusBoard,
                       const LastMoveInfo& lastMove,
                       bool canChallenge)
{
    // ---------------------------------------------------------
    // 1. THE VALIDATION CHECK (Runs for BOTH Bots)
    // ---------------------------------------------------------
    // Note: canChallenge is ONLY true in PvE (Human vs Bot).
    // It is FALSE in AiAi (Bot vs Bot), ensuring bots never challenge each other.
    if (canChallenge && lastMove.exists) {
        bool foundInvalid = false;
        string invalidWord = "";

        for (const auto& word : lastMove.formedWords) {
            if (!gDictionary.isValidWord(word)) {
                foundInvalid = true;
                invalidWord = word;
                break;
            }
        }

        if (foundInvalid) {
            cout << getName() << "\nDETECTED INVALID WORD: " << invalidWord << endl;

            // THE RULE OF COOL
            challengePhrase();
            std::this_thread::sleep_for(std::chrono::seconds(3));

            return Move(MoveType::CHALLENGE);
        }
    }

    candidates.clear();

    kernel::MoveCandidate bestMove;
    bestMove.word[0] = '\0';
    bestMove.score = -10000;

    const Player& me = state.players[state.currentPlayerIndex];
    // ---------------------------------------------------------
    // ENGINE SWITCH
    // ---------------------------------------------------------
    if (style == AIStyle::SPEEDI_PI) {
        bestMove = kernel::GreedyEngine::generate_best_move(state, bonusBoard);
    } else {
        bestMove = SpectreEngine::deliberate(state, bonusBoard);
    }

    // ---------------------------------------------------------
    // EXECUTION & TRANSLATION
    // ---------------------------------------------------------
    bool shouldExchange = (bestMove.word[0] == '\0') ||
                          (bestMove.score < 14 && isRackBad(me.rack) && state.bag.size() >= 7);

    if (shouldExchange) {
        if (state.bag.size() < 7) return Move(MoveType::PASS);
        Move ex; ex.type = MoveType::EXCHANGE;

        string s = getTilesToExchange(me.rack);
        strncpy(ex.exchangeLetters, s.c_str(), 7);
        ex.exchangeLetters[7] = '\0';

        return ex;
    }

    DifferentialMove diff = calculateDifferential(state.board, bestMove);
    if (diff.row == -1) return Move(MoveType::PASS);

    Move result;
    result.type = MoveType::PLAY;
    result.row = diff.row;
    result.col = diff.col;

    // Copy from stack buffer
    strncpy(result.word, diff.word, 15);
    result.word[15] = '\0';

    result.horizontal = bestMove.isHorizontal;
    return result;
}

Move AIPlayer::getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) {
    // If opponent played an invalid word, CHALLENGE.
    for (const auto& word : lastMove.formedWords) {
        if (!gDictionary.isValidWord(word)) return Move(MoveType::CHALLENGE);
    }
    // Speedi_Pi accepts fate. Cutie_Pi *might* challenge if it thinks it can win, but for now PASS.
    return Move(MoveType::PASS);
}

void AIPlayer::findAllMoves(const LetterBoard &letters, const TileRack &rack) {
    candidates = kernel::MoveGenerator::generate(letters, rack, gDictionary);
}