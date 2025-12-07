#pragma once

#include <string>
#include "board.h"
#include "rack.h"
#include "tiles.h"
#include "move.h"

using namespace std;

class PlayerController {
public:
    virtual ~PlayerController() = default;

    // Returns a command string:
    // "PLAY <row> <col> <h/v> <word>"
    // "EXCHANGE <tiles>" (or "EXCHANGE" to indicate intent, handled by caller)
    // "PASS"
    // "CHALLENGE"
    // "QUIT"
    virtual string getMove(const Board& bonusBoard, 
                           const LetterBoard& letters, 
                           const BlankBoard& blanks, 
                           const TileBag& bag, 
                           Player& player, 
                           int playerIndex, 
                           bool canChallenge) = 0;

    virtual string getEndGameDecision() = 0;
};

class HumanController : public PlayerController {
public:
    string getMove(const Board& bonusBoard, 
                   const LetterBoard& letters, 
                   const BlankBoard& blanks, 
                   const TileBag& bag, 
                   Player& player, 
                   int playerIndex, 
                   bool canChallenge) override;

    string getEndGameDecision() override;
};