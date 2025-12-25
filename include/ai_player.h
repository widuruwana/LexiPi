#pragma once

#include "player_controller.h"
#include "dawg.h"
#include "fast_constraints.h"
#include "rack.h"
#include "board.h"
#include <vector>
#include <string>

using namespace std;

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

    bool shouldChallenge(const Move &opponentMove, const LetterBoard &board) const;

private:
    vector <MoveCandidate> candidates;

    // Recursion state helpers
    int currentRow;
    bool currentIsHorizontal;

    /*
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
    */

    // The solver functions
    void findAllMoves(const LetterBoard &letters, const TileRack &rack);

    // The GADDAG Entry Point: Starts searches at "Anchor" squares
    void genMovesGADDAG(int row, const LetterBoard &board, const TileRack &rack,
                        const RowConstraint &constraints, bool isHorizontal);

    // GADDAG Traversal: Going "Backwards" (Left/Up) through the graph
    void goLeft(int row, int col, int node, const RowConstraint &constraints,
                uint32_t rackMask, uint32_t pruningMask, string currentRack, string wordSoFar,
                const LetterBoard &board, bool isHoriz, int anchorCol);

    // GADDAG Traversal: Going "Forwards" (Right/Down) after hitting a separator
    void goRight(int row, int col, int node, const RowConstraint &constraints,
                 uint32_t rackMask, uint32_t pruningMask, string currentRack, string wordSoFar,
                 const LetterBoard &board, bool isHoriz, int anchorCol);

    // Helper struct for engine translation
    // Covert "formed word" -> "tiles playing"
    struct DifferentialMove {
        int row, col;
        string word;
    };

    // Calculate the correct engine input (skip existing tiles, shifts start coord)
    DifferentialMove calculateEngineMove(const LetterBoard &letters, const MoveCandidate &bestMove);

    void solveRow(int rowIdx, const LetterBoard &letters,
                  const TileRack &rack, bool isHorizontal, bool isEmptyBoard);

    bool isRackBad(const TileRack &rack);
    std::string getTilesToExchange(const TileRack &rack);
};