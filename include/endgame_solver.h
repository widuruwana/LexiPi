#pragma once

#include "board.h"
#include "rack.h"
#include "move.h"
#include "ai_player.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

using namespace std;

// Simple hash function for game states
struct GameStateHash {
    size_t operator()(const LetterBoard& board) const noexcept {
        size_t hash = 0;
        for (int r = 0; r < 15; r++) {
            for (int c = 0; c < 15; c++) {
                hash = hash * 31 + (board[r][c] ? board[r][c] : 0);
            }
        }
        return hash;
    }
};

// Transposition table entry
struct TTEntry {
    int score;
    int depth;
    bool isExact;  // True if this is an exact value, false if bounds
};

class EndgameSolver {
public:
    EndgameSolver(const Board& bonusBoard) : bonusBoard(bonusBoard) {
        // Initialize transposition table with moderate size
        transpositionTable.reserve(10000);
    }

    // Main entry point for endgame solving
    Move solveEndgame(const LetterBoard& letters, 
                      const BlankBoard& blanks,
                      const TileRack& myRack, 
                      const TileRack& oppRack, 
                      int myScore, 
                      int oppScore) {
        // Clear transposition table for each new endgame position
        transpositionTable.clear();
        
        GameState initialState;
        initialState.letters = letters;
        initialState.blanks = blanks;
        initialState.myRack = myRack;
        initialState.oppRack = oppRack;
        initialState.myScore = myScore;
        initialState.oppScore = oppScore;
        initialState.isMyTurn = true;

        // Initial depth of 6 should be sufficient for endgame without taking too long
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
        int movesToCheck = min((int)moves.size(), 40);

        for (int i = 0; i < movesToCheck; ++i) {
            GameState nextState = applyMove(initialState, moves[i]);
            int score = minimax(nextState, 6, -100000, 100000, false);
            
            if (score > bestScore) {
                bestScore = score;
                // Convert MoveCandidate to Move
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
                    bestMove = Move::Pass();
                }
            }
        }

        return bestMove;
    }

private:
    const Board& bonusBoard;
    
    // Transposition table for caching evaluated positions
    // Key: board state hash + racks, Value: best score
    struct TranspositionKey {
        LetterBoard letters;
        TileRack myRack;
        TileRack oppRack;
        bool isMyTurn;
        
        bool operator==(const TranspositionKey& other) const {
            if (isMyTurn != other.isMyTurn) return false;
            if (myRack.size() != other.myRack.size() || oppRack.size() != other.oppRack.size()) return false;
            // Check board equality
            for (int r = 0; r < 15; r++) {
                for (int c = 0; c < 15; c++) {
                    if (letters[r][c] != other.letters[r][c]) return false;
                }
            }
            // Check rack equality (simplified - compare letters)
            string myStr, otherMyStr, oppStr, otherOppStr;
            for (const auto& t : myRack) myStr += t.letter;
            for (const auto& t : other.myRack) otherMyStr += t.letter;
            for (const auto& t : oppRack) oppStr += t.letter;
            for (const auto& t : other.oppRack) otherOppStr += t.letter;
            return myStr == otherMyStr && oppStr == otherOppStr;
        }
    };
    
    struct TranspositionKeyHash {
        size_t operator()(const TranspositionKey& key) const noexcept {
            size_t hash = 0;
            // Hash board
            for (int r = 0; r < 15; r++) {
                for (int c = 0; c < 15; c++) {
                    hash = hash * 31 + (key.letters[r][c] ? key.letters[r][c] : 0);
                }
            }
            // Hash turn
            hash = hash * 2 + (key.isMyTurn ? 1 : 0);
            // Hash racks
            for (const auto& t : key.myRack) {
                hash = hash * 31 + t.letter;
            }
            for (const auto& t : key.oppRack) {
                hash = hash * 31 + t.letter;
            }
            return hash;
        }
    };
    
    unordered_map<TranspositionKey, TTEntry, TranspositionKeyHash> transpositionTable;
    
    // Simplified cache using just board state and turn (faster)
    struct SimpleKey {
        LetterBoard letters;
        bool isMyTurn;
        
        bool operator==(const SimpleKey& other) const {
            if (isMyTurn != other.isMyTurn) return false;
            for (int r = 0; r < 15; r++) {
                for (int c = 0; c < 15; c++) {
                    if (letters[r][c] != other.letters[r][c]) return false;
                }
            }
            return true;
        }
    };
    
    struct SimpleKeyHash {
        size_t operator()(const SimpleKey& key) const noexcept {
            size_t hash = 0;
            for (int r = 0; r < 15; r++) {
                for (int c = 0; c < 15; c++) {
                    hash = hash * 31 + (key.letters[r][c] ? key.letters[r][c] : 0);
                }
            }
            hash = hash * 2 + (key.isMyTurn ? 1 : 0);
            return hash;
        }
    };
    
    unordered_map<SimpleKey, pair<int, int>, SimpleKeyHash> simpleCache; // (score, depth)
    
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

    // Create a simple cache key from game state
    SimpleKey makeCacheKey(const GameState& state) const {
        SimpleKey key;
        key.letters = state.letters;
        key.isMyTurn = state.isMyTurn;
        return key;
    }

    // Minimax with alpha-beta pruning AND transposition table
    int minimax(GameState& state, int depth, int alpha, int beta, bool isMaximizing) {
        // Check cache first
        SimpleKey cacheKey = makeCacheKey(state);
        auto cacheIt = simpleCache.find(cacheKey);
        if (cacheIt != simpleCache.end()) {
            // Found a cached result - use it if depth is sufficient
            if (cacheIt->second.second >= depth) {
                return cacheIt->second.first;
            }
        }
        
        // Check for terminal conditions
        if (depth == 0 || isGameOver(state)) {
            int eval = evaluateState(state);
            // Cache this result
            simpleCache[cacheKey] = {eval, depth};
            return eval;
        }

        vector<MoveCandidate> moves = generateMoves(state);
        
        if (moves.empty()) {
            // Pass
            GameState nextState = state;
            nextState.isMyTurn = !state.isMyTurn;
            if (state.lastMove.type == MoveType::PASS) {
                int eval = evaluateState(state);
                simpleCache[cacheKey] = {eval, depth};
                return eval;
            }
            nextState.lastMove = Move::Pass();
            int eval = minimax(nextState, depth - 1, alpha, beta, !isMaximizing);
            simpleCache[cacheKey] = {eval, depth};
            return eval;
        }

        // Sort moves for better pruning
        for (auto& move : moves) {
            move.score = calculateScore(move, state.letters);
        }
        sort(moves.begin(), moves.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
            return a.score > b.score;
        });
        
        int movesToCheck = min((int)moves.size(), 15);

        int result;
        if (isMaximizing) {
            int maxEval = -100000;
            for (int i = 0; i < movesToCheck; ++i) {
                GameState nextState = applyMove(state, moves[i]);
                int eval = minimax(nextState, depth - 1, alpha, beta, false);
                maxEval = max(maxEval, eval);
                alpha = max(alpha, eval);
                if (beta <= alpha) break;
            }
            result = maxEval;
        } else {
            int minEval = 100000;
            for (int i = 0; i < movesToCheck; ++i) {
                GameState nextState = applyMove(state, moves[i]);
                int eval = minimax(nextState, depth - 1, alpha, beta, true);
                minEval = min(minEval, eval);
                beta = min(beta, eval);
                if (beta <= alpha) break;
            }
            result = minEval;
        }
        
        // Cache the result
        simpleCache[cacheKey] = {result, depth};
        return result;
    }

    // Generate all legal moves for the current player
    vector<MoveCandidate> generateMoves(const GameState& state);

    // Evaluate the terminal state or leaf node
    int evaluateState(const GameState& state);

    // Check if the game is over
    bool isGameOver(const GameState& state);
    
    // Helper to apply a move to a state
    GameState applyMove(const GameState& currentState, const MoveCandidate& move);
    
    // Helper to calculate score
    int calculateScore(const MoveCandidate& move, const LetterBoard& letters);
};
