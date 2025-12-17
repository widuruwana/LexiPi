#pragma once

#include "player_controller.h"
#include "dawg.h"
#include "fast_constraints.h"
#include "rack.h"
#include <vector>
#include <string>

//using namespace std;

struct MoveCandidate {
    int row, col;
    string word;
    int score;
    bool isHorizontal;
};

class AIPlayer : public PlayerController {
public:
    AIPlayer() = default;
    virtual ~AIPlayer() = default;

    // Main entry point called by the game loop
    Move getMove(const Board &bonusBoard,
                 const LetterBoard &letters,
                 const BlankBoard &blankBoard,
                 const TileBag &bag,
                 const Player &me,
                 const Player &opponent,
                 int PlayerNum) override;

    // Placeholder for now (required by pure virtual)
    Move getEndGameDecision() override;

private:
    vector <MoveCandidate> candidates;

    // Recursion state helpers
    int currentRow;
    bool currentIsHorizontal;

    // The solver functions
    void findAllMoves(const LetterBoard &letters, const TileRack &rack);

    void solveRow(int rowIdx, const LetterBoard &letters, const TileRack &rack, bool isHorizontal);

    // Recursive "Tunneling" function
    // nodeIdx: Current node in DAWG
    // col: Current column on board
    // currentWord: The word built so far
    // tilesUsed: which tiles from rack we used (to avoid re-using)
    // anchorFilled: Have we connected to existing board yet?
    void recursiveSearch(int nodeIdx,
                         int col,
                         const RowConstraint &constraints,
                         string currentWord,
                         string currentRack,
                         bool anchorFilled,
                         const LetterBoard &letters);
};