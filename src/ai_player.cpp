#include "../include/ai_player.h"
#include "../include/kernel/heuristics.h"
#include "../include/tile_tracker.h"
#include "../include/kernel/move_generator.h"
#include "../include/engine/dictionary.h"
#include "../include/modes/PvE/pve.h"
#include "../include/spectre/judge.h"
#include "../include/engine/mechanics.h"
#include "../include/kernel/fast_constraints.h"
#include "../include/spectre/spectre_engine.h"

#include <cstring>
#include <algorithm>
#include <iostream>

using namespace spectre;
using namespace std;

AIPlayer::AIPlayer(AIStyle style) : style(style) {}

void AIPlayer::reset() {
    engine.reset();
}

string AIPlayer::getName() const {
    return (style == AIStyle::SPEEDI_PI) ? "Speedi_Pi" : "Cutie_Pi";
}

void AIPlayer::observeMove(const Move& move, const LetterBoard& board, const Board& bonusBoard) {
    if (style == AIStyle::CUTIE_PI) {
        engine.notify_move(move, board, bonusBoard);
    }
}

Move AIPlayer::getMove(const GameState& state,
                       const Board& bonusBoard,
                       const LastMoveInfo& lastMove,
                       bool canChallenge)
{
    // 1. Procedural Challenge Logic
    if (canChallenge && !lastMove.formedWords.empty()) {
        for (const auto& word : lastMove.formedWords) {
            if (!gDictionary.isValidWord(word)) return Move(MoveType::CHALLENGE);
        }
    }

    // 2. THE CLEAN HANDOFF: Ask the Spectre Engine for a move, passing our Style
    kernel::MoveCandidate bestMove = engine.deliberate(state, bonusBoard, style == AIStyle::SPEEDI_PI);

    // 3. Fallback / Exchange Logic
    const Player& me = state.players[state.currentPlayerIndex];
    bool shouldExchange = (bestMove.word[0] == '\0') ||
                          (bestMove.score < 14 && isRackBad(me.rack) && state.bag.size() >= 7);

    if (shouldExchange) {
        if (state.bag.size() < 7) return Move(MoveType::PASS);
        Move ex(MoveType::EXCHANGE);
        string s = getTilesToExchange(me.rack);
        strncpy(ex.exchangeLetters, s.c_str(), 7);
        ex.exchangeLetters[7] = '\0';
        return ex;
    }

    // 4. Play Submission
    if (bestMove.word[0] == '\0') return Move(MoveType::PASS);

    Move result(MoveType::PLAY);
    result.row = bestMove.row;
    result.col = bestMove.col;
    result.horizontal = bestMove.isHorizontal;
    strncpy(result.word, bestMove.word, 15);
    result.word[15] = '\0';

    return result;
}

Move AIPlayer::getEndGameResponse(const GameState& state, const LastMoveInfo& lastMove) {
    for (const auto& word : lastMove.formedWords) {
        if (!gDictionary.isValidWord(word)) return Move(MoveType::CHALLENGE);
    }
    return Move(MoveType::PASS);
}

// --- HEURISTIC HELPERS ---
bool AIPlayer::isRackBad(const TileRack& rack) {
    int vowels = 0, consonants = 0, blanks = 0, dupes = 0;
    int counts[26] = {0};

    for (const auto& t : rack) {
        if (t.letter == '?') blanks++;
        else {
            char c = toupper(t.letter);
            if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') vowels++;
            else consonants++;
            if (++counts[c - 'A'] > 1) dupes++;
        }
    }

    if (blanks > 0) return false;
    if (vowels == 0 || consonants == 0) return true;
    if (vowels > 5 || consonants > 5) return true;
    if (dupes > 2) return true;

    return false;
}

string AIPlayer::getTilesToExchange(const TileRack& rack) {
    string keep = "", exchange = "";
    int counts[26] = {0};

    for (const auto& t : rack) {
        if (t.letter == '?') { keep += t.letter; continue; }
        char c = toupper(t.letter);
        counts[c - 'A']++;
        bool isGood = (c == 'S' || c == 'E' || c == 'A' || c == 'R' || c == 'T' || c == 'X' || c == 'Z');
        if (isGood && counts[c - 'A'] == 1) keep += t.letter;
        else exchange += t.letter;
    }

    if (exchange.empty()) {
        for (const auto& t : rack) if (t.letter != '?') exchange += t.letter;
    }
    return exchange;
}