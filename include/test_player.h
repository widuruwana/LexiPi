#pragma once

#include "player_controller.h"
#include <queue>

class TestPlayer : public PlayerController {
public:
    TestPlayer();
    virtual ~TestPlayer() = default;

    Move getMove(const Board &bonusBoard,
                 const LetterBoard &letters,
                 const BlankBoard &blankBoard,
                 const TileBag &Bag,
                 const Player &me,
                 const Player &opponent,
                 int playerNum) override;

    Move getEndGameDecision() override;

    void addMove(Move m);

private:
    std::queue<Move> moveScript;
};