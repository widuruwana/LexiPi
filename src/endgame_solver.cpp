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

EndgameSolver::GameState EndgameSolver::applyMove(const GameState& currentState, const MoveCandidate& move) {
    GameState nextState = currentState;
    nextState.isMyTurn = !currentState.isMyTurn;
    nextState.lastMove = Move::Play(move.row, move.col, move.isHorizontal, move.word);
    
    int score = calculateScore(move, currentState.letters);
    
    if (currentState.isMyTurn) {
        nextState.myScore += score;
        int r = move.row;
        int c = move.col;
        int dr = move.isHorizontal ? 0 : 1;
        int dc = move.isHorizontal ? 1 : 0;
        
        for (char letter : move.word) {
            if (currentState.letters[r][c] == ' ') {
                for (auto it = nextState.myRack.begin(); it != nextState.myRack.end(); ++it) {
                    if (it->letter == letter || (it->letter == '?' && islower(letter))) { 
                        nextState.myRack.erase(it);
                        break;
                    }
                }
                nextState.letters[r][c] = letter;
            }
            r += dr;
            c += dc;
        }
    } else {
        nextState.oppScore += score;
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
    int myPenalty = 0;
    for (const auto& tile : state.myRack) myPenalty += getTileValue(tile.letter);
    
    int oppPenalty = 0;
    for (const auto& tile : state.oppRack) oppPenalty += getTileValue(tile.letter);
    
    int myFinalScore = state.myScore;
    int oppFinalScore = state.oppScore;
    
    if (state.myRack.empty()) {
        myFinalScore += oppPenalty * 2;
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
    AIPlayer tempAI;
    const TileRack& rack = state.isMyTurn ? state.myRack : state.oppRack;
    tempAI.findAllMoves(state.letters, rack);
    return tempAI.candidates;
}
