#pragma once
#include "state.h"
#include "referee.h"
#include "mechanics.h"
#include "../player_controller.h"
#include "../engine/dictionary.h"

#include <string>

struct MatchResult {
    int scoreP1;
    int scoreP2;
    int winner; // 0, 1, or -1 (Draw)
};

// >>> THE DECOUPLING INTERFACE <<<
// The Engine broadcasts events here. It has no idea if the receiver is a Console, a GUI, or nothing.
class GameObserver {
public:
    virtual ~GameObserver() = default;
    virtual void onLogMessage(const std::string& msg) {}
    virtual void onTurnStart(int gameId, int pIdx, const GameState& state, const Board& bonusBoard, const std::string& pName) {}
    virtual void onGameOver(const GameState& state, const Board& bonusBoard) {}
};

class GameDirector {
public:
    struct Config {
        bool allowChallenge;
        bool sixPassEndsGame;
        int delayMs;
        GameObserver* observer; // Replaced 'verbose'

        Config() : allowChallenge(true), sixPassEndsGame(true), delayMs(0), observer(nullptr) {}
    };

    GameDirector(PlayerController* p1, PlayerController* p2,
                 const Board& bonusBoard,
                 Config cfg = Config());

    MatchResult run(int gameId = 1);

private:
    PlayerController* controllers[2];
    Board bonusBoard;
    Config config;

    GameState state;
    GameState snapshot;
    LastMoveInfo lastMove;
    bool canChallenge;

    void initGame();
    bool processTurn(int pIdx);
    void executePlay(int pIdx, Move& move);
    bool executeChallenge(int challengerIdx);
};