#include "../include/test_player.h"

TestPlayer::TestPlayer() {}

Move TestPlayer::getMove(const Board &bonusBoard,
                         const LetterBoard &letters,
                         const BlankBoard &blankBoard,
                         const TileBag &Bag,
                         const Player &me,
                         const Player &opponent,
                         int playerNum) {
    if (moveScript.empty()) {
        return Move::Pass();
    }

    Move m = moveScript.front();
    moveScript.pop();
    return m;
}

Move TestPlayer::getEndGameDecision() {
    return Move::Pass();
}

void TestPlayer::addMove(Move m) {
    moveScript.push(m);
}