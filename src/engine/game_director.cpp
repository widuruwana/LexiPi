#include "../../include/engine/game_director.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/referee.h"
#include "../../include/choices.h"
#include <thread>
#include <chrono>

using namespace std;

GameDirector::GameDirector(PlayerController* p1, PlayerController* p2,
                           const Board& bBoard, Config cfg) : bonusBoard(bBoard), config(cfg) {
    controllers[0] = p1;
    controllers[1] = p2;
}

void GameDirector::initGame() {
    clearLetterBoard(state.board);
    clearBlankBoard(state.blanks);
    refillStandardTileBag(state.bag);
    shuffleTileBag(state.bag);

    for(int i=0; i<2; i++) {
        state.players[i].rack.clear();
        state.players[i].score = 0;
        state.players[i].passCount = 0;
        drawTiles(state.bag, state.players[i].rack, 7);
    }
    state.currentPlayerIndex = 0;
    lastMove.reset();
    snapshot = state;
    canChallenge = false;

    if (controllers[0]) controllers[0]->reset();
    if (controllers[1]) controllers[1]->reset();
}

MatchResult GameDirector::run(int gameId) {
    initGame();
    if (config.observer) config.observer->onLogMessage("\n[DIRECTOR] Game " + to_string(gameId) + " Started.");

    bool gameRunning = true;
    while (gameRunning) {
        int pIdx = state.currentPlayerIndex;

        // UI Broadcast: Notify the observer that a turn is starting
        if (config.observer) {
            config.observer->onTurnStart(gameId, pIdx, state, bonusBoard, controllers[pIdx]->getName());
        }

        // 1. END GAME: Empty Rack Check
        if (lastMove.exists && lastMove.emptiedRack && state.bag.empty()) {
            if (config.observer) config.observer->onLogMessage("\n[DIRECTOR] Player " + to_string(lastMove.playerIndex + 1) + " emptied rack!");

            Move response = controllers[pIdx]->getEndGameResponse(state, lastMove);

            if (response.type == MoveType::CHALLENGE && config.allowChallenge) {
                if (executeChallenge(pIdx)) {
                    if (config.observer) config.observer->onLogMessage("[DIRECTOR] Challenge Successful. Game Continues.");
                    continue;
                } else {
                    if (config.observer) config.observer->onLogMessage("[DIRECTOR] Challenge Failed. Game Over.");
                    gameRunning = false;
                    break;
                }
            } else {
                if (config.observer) config.observer->onLogMessage("[DIRECTOR] Opponent passed. Applying penalties...");
                Mechanics::applyEmptyRackBonus(state, lastMove.playerIndex);
                gameRunning = false;
                break;
            }
        }

        // 2. END GAME: Six Pass
        if (config.sixPassEndsGame && state.players[0].passCount >= 3 && state.players[1].passCount >= 3) {
            if (config.observer) config.observer->onLogMessage("\n[DIRECTOR] Game Over: 6 Consecutive Passes.");
            Mechanics::applySixPassPenalty(state);
            gameRunning = false;
            break;
        }

        // 3. Normal Turn
        gameRunning = processTurn(pIdx);

        if (config.delayMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(config.delayMs));
    }

    // UI Broadcast: Game Over Final State
    if (config.observer) config.observer->onGameOver(state, bonusBoard);

    MatchResult res;
    res.scoreP1 = state.players[0].score;
    res.scoreP2 = state.players[1].score;
    if (res.scoreP1 > res.scoreP2) res.winner = 0;
    else if (res.scoreP2 > res.scoreP1) res.winner = 1;
    else res.winner = -1;
    return res;
}

bool GameDirector::processTurn(int pIdx) {
    Move move = controllers[pIdx]->getMove(state, bonusBoard, lastMove, canChallenge);

    if (move.type == MoveType::QUIT) return false;

    if (move.type == MoveType::CHALLENGE) {
        if (config.allowChallenge && canChallenge) {
            if (executeChallenge(pIdx)) return true;
            state.currentPlayerIndex = 1 - pIdx;
            return true;
        }
    }

    if (move.type == MoveType::PASS) {
        state.players[pIdx].passCount++;
        if (config.observer) config.observer->onLogMessage("[DIRECTOR] Player " + to_string(pIdx+1) + " Passed.");
        canChallenge = false;
        lastMove.reset();
    }
    else if (move.type == MoveType::EXCHANGE) {
        if (Mechanics::attemptExchange(state, move)) {
            if (config.observer) config.observer->onLogMessage("[DIRECTOR] Player " + to_string(pIdx+1) + " Exchanged.");
            canChallenge = false;
            lastMove.reset();
        } else {
            state.players[pIdx].passCount++;
        }
    }
    else if (move.type == MoveType::PLAY) {
        executePlay(pIdx, move);
    }

    state.currentPlayerIndex = 1 - pIdx;
    return true;
}

void GameDirector::executePlay(int pIdx, Move& move) {
    MoveResult result = Referee::validateMove(state, move, bonusBoard, gDictionary);

    if (result.success) {
        Mechanics::commitSnapshot(snapshot, state);
        Mechanics::applyMove(state, move, result.score);

        state.players[0].passCount = 0;
        state.players[1].passCount = 0;

        lastMove.exists = true;
        lastMove.playerIndex = pIdx;
        lastMove.move = move;
        lastMove.score = result.score;
        lastMove.emptiedRack = state.players[pIdx].rack.empty();

        int opponentIdx = 1 - pIdx;
        controllers[opponentIdx]->observeMove(move, snapshot.board, bonusBoard);

        if (config.allowChallenge) {
            lastMove.formedWords.clear();
            string mainWord = extractMainWord(state.board, move.row, move.col, move.horizontal);
            lastMove.formedWords.push_back(mainWord);

            vector<string> crossWords = crossWordList(state.board, snapshot.board, move.row, move.col, move.horizontal);
            lastMove.formedWords.insert(lastMove.formedWords.end(), crossWords.begin(), crossWords.end());
            canChallenge = true;
        } else {
            canChallenge = false;
        }

        if (config.observer) config.observer->onLogMessage("[DIRECTOR] Player " + to_string(pIdx+1) + " Played: " + move.word + " (" + to_string(result.score) + ")");
    } else {
        if (config.observer) config.observer->onLogMessage(string("[DIRECTOR] Invalid Move: ") + result.message);
        state.players[pIdx].passCount++;
    }
}

bool GameDirector::executeChallenge(int challengerIdx) {
    bool invalid = false;

    if (!lastMove.formedWords.empty()) {
        for(const string& w : lastMove.formedWords) {
            if(!gDictionary.isValidWord(w)) { invalid = true; break; }
        }
    } else {
        string mainWord = extractMainWord(state.board, lastMove.move.row, lastMove.move.col, lastMove.move.horizontal);
        if(!gDictionary.isValidWord(mainWord)) invalid = true;
    }

    if (invalid) {
        if (config.observer) config.observer->onLogMessage(">>> CHALLENGE SUCCESSFUL! Invalid Word Found.");
        Mechanics::restoreSnapshot(state, snapshot);
        canChallenge = false;
        lastMove.reset();
        state.currentPlayerIndex = challengerIdx;
        return true;
    } else {
        if (config.observer) config.observer->onLogMessage(">>> CHALLENGE FAILED! Words Valid. (+5 pts)");
        state.players[lastMove.playerIndex].score += 5;
        canChallenge = false;
        lastMove.reset();
        return false;
    }
}