#pragma once

#include "board.h"
#include "rack.h"
#include "move.h"
#include "ai_player.h"
#include <vector>
#include <string>

using namespace std;

class EndgameSolver {
public:
    EndgameSolver(const Board& bonusBoard) : bonusBoard(bonusBoard) {}

    // Main entry point for endgame solving
    Move solveEndgame(const LetterBoard& letters, 
                      const BlankBoard& blanks,
                      const TileRack& myRack, 
                      const TileRack& oppRack, 
                      int myScore, 
                      int oppScore);

private:
    const Board& bonusBoard;
    
    // Simplified game state for recursion
    struct GameState {
        LetterBoard letters;
        BlankBoard blanks;
        TileRack myRack;
        TileRack oppRack;
        int myScore;
        int oppScore;
        bool isMyTurn;
        Move lastMove;
    };

    // Minimax with alpha-beta pruning
    int minimax(GameState& state, int depth, int alpha, int beta, bool isMaximizing);

    // Generate all legal moves for the current player in the given state
    // This will need to reuse some logic from AIPlayer, or we can pass a helper
    vector<MoveCandidate> generateMoves(const GameState& state);

    // Evaluate the terminal state or leaf node
    int evaluateState(const GameState& state);

    // Check if the game is over (one rack empty or no moves possible)
    bool isGameOver(const GameState& state);
    
    // Helper to apply a move to a state
    GameState applyMove(const GameState& currentState, const MoveCandidate& move);
    
    // Helper to calculate score (reusing AIPlayer logic if possible, or simplified)
    int calculateScore(const MoveCandidate& move, const LetterBoard& letters);
};