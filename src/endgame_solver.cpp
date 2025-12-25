#include "../include/endgame_solver.h"
#include "../include/dawg.h"
#include "../include/heuristics.h"
#include "../include/fast_constraints.h"
#include <algorithm>
#include <iostream>

using namespace std;

// Helper to calculate score (simplified version of AIPlayer::calculateTrueScore)
int EndgameSolver::calculateScore(const MoveCandidate& move, const LetterBoard& letters) {
    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMultiplier = 1;
    int tilesPlacedCount = 0;

    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    for (char letter : move.word) {
        int letterScore = getTileValue(letter);
        bool isNewlyPlaced = (letters[r][c] == ' ');

        if (isNewlyPlaced) {
            tilesPlacedCount++;
            CellType bonus = bonusBoard[r][c];
            if (bonus == CellType::DLS) letterScore *= 2;
            else if (bonus == CellType::TLS) letterScore *= 3;
            if (bonus == CellType::DWS) mainWordMultiplier *= 2;
            else if (bonus == CellType::TWS) mainWordMultiplier *= 3;
        }

        mainWordScore += letterScore;

        // Cross words
        if (isNewlyPlaced) {
            int pdr = move.isHorizontal ? 1 : 0;
            int pdc = move.isHorizontal ? 0 : 1;
            
            // Check neighbors
            bool hasNeighbour = false;
            if (r-pdr >= 0 && c-pdc >= 0 && letters[r-pdr][c-pdc] != ' ') hasNeighbour = true;
            if (r+pdr < 15 && c+pdc < 15 && letters[r+pdr][c+pdc] != ' ') hasNeighbour = true;

            if (hasNeighbour) {
                int currR = r;
                int currC = c;
                while (currR - pdr >= 0 && currC - pdc >= 0 && letters[currR-pdr][currC-pdc] != ' ') {
                    currR -= pdr;
                    currC -= pdc;
                }

                int crossScore = 0;
                int crossMult = 1;
                while (currR < 15 && currC < 15) {
                    char cellLetter = letters[currR][currC];
                    if (currR == r && currC == c) {
                        int crossLetterScore = getTileValue(letter);
                        CellType crossBonus = bonusBoard[currR][currC];
                        if (crossBonus == CellType::DLS) crossLetterScore *= 2;
                        else if (crossBonus == CellType::TLS) crossLetterScore *= 3;
                        if (crossBonus == CellType::DWS) crossMult *= 2;
                        else if (crossBonus == CellType::TWS) crossMult *= 3;
                        crossScore += crossLetterScore;
                    } else if (cellLetter != ' ') {
                        crossScore += getTileValue(cellLetter);
                    } else {
                        break;
                    }
                    currR += pdr;
                    currC += pdc;
                }
                totalScore += (crossScore * crossMult);
            }
        }
        r += dr;
        c += dc;
    }
    totalScore += (mainWordScore * mainWordMultiplier);
    if (tilesPlacedCount == 7) totalScore += 50;

    return totalScore;
}

Move EndgameSolver::solveEndgame(const LetterBoard& letters, 
                                 const BlankBoard& blanks,
                                 const TileRack& myRack, 
                                 const TileRack& oppRack, 
                                 int myScore, 
                                 int oppScore) {
    GameState initialState;
    initialState.letters = letters;
    initialState.blanks = blanks;
    initialState.myRack = myRack;
    initialState.oppRack = oppRack;
    initialState.myScore = myScore;
    initialState.oppScore = oppScore;
    initialState.isMyTurn = true;

    // Initial depth of 6 should be sufficient for endgame without taking too long
    // We can adjust this based on performance
    int bestScore = -100000;
    Move bestMove = Move::Pass();

    vector<MoveCandidate> moves = generateMoves(initialState);
    
    if (moves.empty()) {
        return Move::Pass();
    }

    // Sort moves to improve alpha-beta pruning
    for (auto& move : moves) {
        move.score = calculateScore(move, letters);
    }
    sort(moves.begin(), moves.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
        return a.score > b.score;
    });

    // Limit top moves for performance if needed, but for endgame we want accuracy
    // Increased from 20 to 40 for better endgame precision
    int movesToCheck = min((int)moves.size(), 40);

    for (int i = 0; i < movesToCheck; ++i) {
        GameState nextState = applyMove(initialState, moves[i]);
        // Increased depth from 4 to 6 for deeper endgame calculation
        int score = minimax(nextState, 6, -100000, 100000, false);
        
        if (score > bestScore) {
            bestScore = score;
            // Convert MoveCandidate to Move
            // We need to reconstruct the actual move string (only placed tiles)
            // AND find the correct starting position (first placed tile)
            string word = "";
            int r = moves[i].row;
            int c = moves[i].col;
            int dr = moves[i].isHorizontal ? 0 : 1;
            int dc = moves[i].isHorizontal ? 1 : 0;
            
            int startR = -1;
            int startC = -1;
            
            for (char letter : moves[i].word) {
                if (letters[r][c] == ' ') {
                    word += letter;
                    if (startR == -1) {
                        startR = r;
                        startC = c;
                    }
                }
                r += dr;
                c += dc;
            }
            
            if (startR != -1) {
                bestMove = Move::Play(startR, startC, moves[i].isHorizontal, word);
            } else {
                // Should not happen for valid moves that place tiles
                bestMove = Move::Pass();
            }
        }
    }

    return bestMove;
}

int EndgameSolver::minimax(GameState& state, int depth, int alpha, int beta, bool isMaximizing) {
    if (depth == 0 || isGameOver(state)) {
        return evaluateState(state);
    }

    vector<MoveCandidate> moves = generateMoves(state);
    
    if (moves.empty()) {
        // Pass
        GameState nextState = state;
        nextState.isMyTurn = !state.isMyTurn;
        // If both pass, game over
        if (state.lastMove.type == MoveType::PASS) {
             return evaluateState(state);
        }
        nextState.lastMove = Move::Pass();
        return minimax(nextState, depth - 1, alpha, beta, !isMaximizing);
    }

    // Sort moves
    for (auto& move : moves) {
        move.score = calculateScore(move, state.letters);
    }
    sort(moves.begin(), moves.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
        return a.score > b.score;
    });
    
    // Increased pruning limit from 10 to 15
    int movesToCheck = min((int)moves.size(), 15);

    if (isMaximizing) {
        int maxEval = -100000;
        for (int i = 0; i < movesToCheck; ++i) {
            GameState nextState = applyMove(state, moves[i]);
            int eval = minimax(nextState, depth - 1, alpha, beta, false);
            maxEval = max(maxEval, eval);
            alpha = max(alpha, eval);
            if (beta <= alpha) break;
        }
        return maxEval;
    } else {
        int minEval = 100000;
        for (int i = 0; i < movesToCheck; ++i) {
            GameState nextState = applyMove(state, moves[i]);
            int eval = minimax(nextState, depth - 1, alpha, beta, true);
            minEval = min(minEval, eval);
            beta = min(beta, eval);
            if (beta <= alpha) break;
        }
        return minEval;
    }
}

EndgameSolver::GameState EndgameSolver::applyMove(const GameState& currentState, const MoveCandidate& move) {
    GameState nextState = currentState;
    nextState.isMyTurn = !currentState.isMyTurn;
    nextState.lastMove = Move::Play(move.row, move.col, move.isHorizontal, move.word); // Store simplified move
    
    int score = calculateScore(move, currentState.letters);
    
    if (currentState.isMyTurn) {
        nextState.myScore += score;
        // Remove tiles from my rack
        int r = move.row;
        int c = move.col;
        int dr = move.isHorizontal ? 0 : 1;
        int dc = move.isHorizontal ? 1 : 0;
        
        for (char letter : move.word) {
            if (currentState.letters[r][c] == ' ') {
                // Remove from rack
                for (auto it = nextState.myRack.begin(); it != nextState.myRack.end(); ++it) {
                    if (it->letter == letter || (it->letter == '?' && islower(letter))) { 
                         nextState.myRack.erase(it);
                         break;
                    }
                }
                // Update board
                nextState.letters[r][c] = letter;
            }
            r += dr;
            c += dc;
        }
    } else {
        nextState.oppScore += score;
        // Remove tiles from opp rack
        int r = move.row;
        int c = move.col;
        int dr = move.isHorizontal ? 0 : 1;
        int dc = move.isHorizontal ? 1 : 0;
        
        for (char letter : move.word) {
            if (currentState.letters[r][c] == ' ') {
                 for (auto it = nextState.oppRack.begin(); it != nextState.oppRack.end(); ++it) {
                    if (it->letter == letter || (it->letter == '?' && islower(letter))) {
                         nextState.oppRack.erase(it);
                         break;
                    }
                }
                nextState.letters[r][c] = letter;
            }
            r += dr;
            c += dc;
        }
    }
    
    return nextState;
}

int EndgameSolver::evaluateState(const GameState& state) {
    // Simple evaluation: score difference
    // In endgame, we also want to penalize remaining tiles in rack
    int myPenalty = 0;
    for (const auto& tile : state.myRack) myPenalty += getTileValue(tile.letter);
    
    int oppPenalty = 0;
    for (const auto& tile : state.oppRack) oppPenalty += getTileValue(tile.letter);
    
    // If one rack is empty, they get the other's penalty added to their score
    int myFinalScore = state.myScore;
    int oppFinalScore = state.oppScore;
    
    if (state.myRack.empty()) {
        myFinalScore += oppPenalty * 2; // Bonus + penalty subtraction logic simplified
    } else {
        myFinalScore -= myPenalty;
    }
    
    if (state.oppRack.empty()) {
        oppFinalScore += myPenalty * 2;
    } else {
        oppFinalScore -= oppPenalty;
    }
    
    return myFinalScore - oppFinalScore;
}

bool EndgameSolver::isGameOver(const GameState& state) {
    return state.myRack.empty() || state.oppRack.empty(); 
}

vector<MoveCandidate> EndgameSolver::generateMoves(const GameState& state) {
    // Use a temporary AIPlayer to generate moves
    // Since EndgameSolver is a friend, we can access private members
    AIPlayer tempAI;
    
    // We need to set up the tempAI with the state's rack
    // But findAllMoves takes the rack as an argument, so that's easy.
    
    const TileRack& rack = state.isMyTurn ? state.myRack : state.oppRack;
    
    // findAllMoves populates tempAI.candidates
    tempAI.findAllMoves(state.letters, rack);
    
    return tempAI.candidates;
}