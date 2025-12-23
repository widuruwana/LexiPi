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
    string leave; // Remaining tiles
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

    // Helper struct for engine translation
    struct DifferentialMove {
        int row, col;
        string word;
    };

    // Calculate the correct engine input (skip existing tiles, shifts start coord)
    DifferentialMove calculateEngineMove(const LetterBoard &letters, const MoveCandidate &bestMove);

    // The solver functions
    void findAllMoves(const LetterBoard &letters, const TileRack &rack);

    void solveRow(int rowIdx, const LetterBoard &letters,
                  const TileRack &rack, bool isHorizontal, bool isEmptyBoard);

    // Recursive "Tunneling" function
    // nodeIdx: Current node in DAWG
    // col: Current column on board
    // currentWord: The word built so far
    // tilesUsed: which tiles from rack we used (to avoid re-using)
    // anchorFilled: Have we connected to existing board yet?
    void recursiveSearch(int nodeIdx,
                         int col,
                         const RowConstraint &constraints,
                         uint32_t rackMask,
                         string currentWord,
                         string currentRack,
                         bool anchorFilled,
                         const LetterBoard &letters,
                         bool isEmptyBoard,
                         bool tilesPlaced);
};