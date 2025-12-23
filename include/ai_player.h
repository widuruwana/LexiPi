#pragma once

#include "player_controller.h"
#include "dawg.h"
#include "fast_constraints.h"
#include "rack.h"
#include "tile_tracker.h"
#include "evaluation_model.h"
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
    friend class EndgameSolver; // Allow EndgameSolver to access private move generation
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

    // Allow injecting a shared evaluation model (for training)
    void setEvaluationModel(EvaluationModel* model);

private:
    vector <MoveCandidate> candidates;
    
    // 2-ply evaluation parameters - TUNED AGGRESSION
    static constexpr int TOP_K_CANDIDATES = 30;  // Evaluate even more options
    static constexpr float LEAVE_WEIGHT = 2.5f;  // Slightly more conservative to ensure better racks

    // Recursion state helpers
    int currentRow;
    bool currentIsHorizontal;
    
    // Tile tracking for strategic play
    TileTracker tileTracker;
    EvaluationModel evalModel;
    EvaluationModel* externalModel = nullptr; // Pointer to external model (if any)
    
    // Helper functions for strategic depth
    bool isEndgame(const TileBag &bag, const TileRack &myRack, const TileRack &oppRack) const;
    bool isCriticalPosition(const Player &me, const Player &opponent, const TileBag &bag) const;
    float evaluateWithTileTracking(float baseEval, const TileRack &oppRack) const;

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

    // GADDAG Move Generation
    void genMovesGaddag(int anchorRow, int anchorCol, const LetterBoard &letters, const TileRack &rack, bool isHorizontal);
    
    // GADDAG Helper: Go Left (Reversed Prefix)
    void goLeft(int nodeIdx, int currRow, int currCol,
                const LetterBoard &letters, string currentRack,
                string currentWord, int anchorCol, bool isHorizontal);
                
    // GADDAG Helper: Go Right (Suffix)
    void goRight(int nodeIdx, int currRow, int currCol,
                 const LetterBoard &letters, string currentRack,
                 string currentWord, int startCol, bool isHorizontal);
    
    // 2-ply evaluation: simulate a move and get opponent's best reply
    float evaluate2Ply(const MoveCandidate &myMove,
                       const Board &bonusBoard,
                       const LetterBoard &letters,
                       const BlankBoard &blanks,
                       const TileBag &bag,
                       const TileRack &myRack,
                       const TileRack &oppRack);

    // 3-ply evaluation: simulate my move -> opp best reply -> my best counter-reply
    float evaluate3Ply(const MoveCandidate &myMove,
                       const Board &bonusBoard,
                       const LetterBoard &letters,
                       const BlankBoard &blanks,
                       const TileBag &bag,
                       const TileRack &myRack,
                       const TileRack &oppRack);

    // Monte Carlo Simulation: Simulate N random games from this position
    float evaluateMonteCarlo(const MoveCandidate &myMove,
                             const Board &bonusBoard,
                             const LetterBoard &letters,
                             const BlankBoard &blanks,
                             const TileBag &bag,
                             const TileRack &myRack,
                             const TileRack &oppRack,
                             int numSimulations);
    
    // Helper: simulate a move and return new game state
    struct SimulatedState {
        LetterBoard letters;
        BlankBoard blanks;
        TileBag bag;
        TileRack myRack;
        int scoreGained;
        bool success;
    };
    
    SimulatedState simulateMove(const MoveCandidate &move,
                                const Board &bonusBoard,
                                const LetterBoard &letters,
                                const BlankBoard &blanks,
                                const TileBag &bag,
                                const TileRack &rack);
};