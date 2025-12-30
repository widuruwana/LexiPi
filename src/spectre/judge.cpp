#include "../../include/spectre/judge.h"
#include "../../include/spectre/move_generator.h"
#include "../../include/heuristics.h"
#include "../../include/spectre/logger.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <array>
#include <chrono>
#include <mutex> //For thread safety

using namespace std;
using namespace std::chrono;

namespace spectre {

// Helper: Convert internal candidate to public Move structure
    static Move candidateToMove(const MoveCandidate& cand, const LetterBoard& board) {
        Move m;
        m.type = MoveType::PLAY;
        m.horizontal = cand.isHorizontal;

        string tilesToPlace = "";
        int startR = -1;
        int startC = -1;

        int r = cand.row;
        int c = cand.col;
        int dr = cand.isHorizontal ? 0 : 1;
        int dc = cand.isHorizontal ? 1 : 0;

        for (int i = 0; cand.word[i] != '\0'; i++) {
            if (board[r][c] == ' ') {
                if (startR == -1) {
                    startR = r;
                    startC = c;
                }
                tilesToPlace += cand.word[i];
            }
            r += dr;
            c += dc;
        }

        // Safety: If for some reason we didn't place any tiles, this is a pass
        if (tilesToPlace.empty() || startR == -1) {
            return Move::Pass();
        }

        m.row = startR;
        m.col = startC;
        m.word = tilesToPlace;
        return m;
    }

Move Judge::solveEndgame(const LetterBoard& board, const Board& bonusBoard,
                         const TileRack& myRack, const TileRack& oppRack, Dawg& dict) {

    // --- DEBUG: RACK VERIFICATION ---
    // Print exactly what tiles the Judge thinks it has.
    string rackStr = "";
    for(auto t : myRack) rackStr += t.letter;
    {
        std::lock_guard<std::mutex> lock(spectre::console_mutex);
        cout << "[JUDGE] Solver Active." << endl;
    }
    // --------------------------------

    // --- STEP 1: RACK CONVERSION ---
    // Convert 'TileRack' (vector of structs) to 'int array' (histogram)
    // This allows O(1) access speed during the millions of recursion steps.
    int myRackCounts[27] = {0};
    int oppRackCounts[27] = {0};

    for (const auto& t : myRack) {
        if (t.letter == '?') myRackCounts[26]++;
        else if (isalpha(t.letter)) myRackCounts[toupper(t.letter) - 'A']++;
    }
    for (const auto& t : oppRack) {
        if (t.letter == '?') oppRackCounts[26]++;
        else if (isalpha(t.letter)) oppRackCounts[toupper(t.letter) - 'A']++;
    }

    // --- STEP 2: ROOT MOVE GENERATION ---
    // Generate all legal moves from the current position.
    // We use a stack-based array (1024 slots) to avoid Heap Allocation overhead.
    std::array<MoveCandidate, 1024> moves;
    int moveCount = 0;
    auto consumer = [&](MoveCandidate& cand, int* rack) -> bool {
        if (moveCount < 1024) moves[moveCount++] = cand;
        return true;
    };
    MoveGenerator::generate_raw(board, myRackCounts, dict, consumer);

    cout << "[JUDGE] Generated " << moveCount << " root moves." << endl;

    if (moveCount == 0) {
        cout << "[JUDGE] No legal moves found." << endl;
        return Move::Pass();
    }

    // --- STEP 3: MOVE ORDERING ---
    // Sort moves by score. Checking the best moves first allows Alpha-Beta pruning
    // to discard bad branches much faster.
    for (int i=0; i<moveCount; i++) {
        moves[i].score = calculateMoveScore(board, bonusBoard, moves[i]);
    }
    std::sort(moves.begin(), moves.begin() + moveCount, [](const MoveCandidate& a, const MoveCandidate& b) {
        return a.score > b.score;
    });

    // CRITICAL: Default to Greedy!
    // If the timer runs out instantly, we return 'moves[0]' instead of Passing.
    Move bestMoveGlobal = candidateToMove(moves[0], board);

    // --- STEP 4: ITERATIVE DEEPENING ---
    // We start searching Depth 1, then Depth 2, etc.
    // This ensures we always have a valid result if the timer (3000ms) kills us.
    auto startTime = steady_clock::now();
    int timeBudgetMs = 3000;

    for (int depth = 1; depth <= 16; depth++) {
        // Time Check
        if (duration_cast<milliseconds>(steady_clock::now() - startTime).count() > timeBudgetMs) {
            cout << "[JUDGE] Time limit reached before Depth " << depth << endl;
            break;
        }

        int alpha = -999999;
        int beta = 999999;
        Move bestMoveCurrentDepth = Move::Pass();
        bool timeOut = false;
        int maxEval = -999999;

        // BEAM SEARCH OPTIMIZATION
        // Only consider the top 20 moves at the root.
        int limit = std::min(moveCount, 20);

        for (int i = 0; i < limit; i++) {
            // Periodic Time Check (every 10 root moves)
            if (i % 10 == 0 && duration_cast<milliseconds>(steady_clock::now() - startTime).count() > timeBudgetMs) {
                timeOut = true;
                break;
            }

            // Simulate the move
            LetterBoard simBoard = board;
            int nextMyRack[27]; memcpy(nextMyRack, myRackCounts, 27*sizeof(int));
            applyMove(simBoard, moves[i], nextMyRack);

            // Recurse (Minimax)
            int eval = moves[i].score + minimax(simBoard, bonusBoard, nextMyRack, oppRackCounts, dict,
                                               alpha, beta, false, 0, depth - 1, startTime, timeBudgetMs);

            // Handle Timeout Signal
            if (eval == -7777777) { timeOut = true; break; }

            if (eval > maxEval) {
                maxEval = eval;
                // Save best move for this depth
                bestMoveCurrentDepth = candidateToMove(moves[i], board);
            }
            alpha = max(alpha, eval);
        }

        if (!timeOut) {
            // Depth Completed Successfully: Update Global Best
            bestMoveGlobal = bestMoveCurrentDepth;
            cout << "[JUDGE] Depth " << depth << " Best: " << bestMoveGlobal.word
                 << " (" << maxEval << " pts)" << endl;
            if (depth > 14) break; // Game fully solved
        } else {
            cout << "[JUDGE] Timeout inside Depth " << depth << endl;
            break;
        }
    }

    return bestMoveGlobal;
}

int Judge::minimax(LetterBoard board, const Board& bonusBoard,
                   int* myRackCounts, int* oppRackCounts,
                   Dawg& dict, int alpha, int beta,
                   bool maximizingPlayer, int passesInARow,
                   int depth,
                   const std::chrono::steady_clock::time_point& startTime,
                   int timeBudgetMs) {

    // Check clock sparserly (every 4 depth levels) to save CPU cycles
    if (depth % 4 == 0) {
        if (duration_cast<milliseconds>(steady_clock::now() - startTime).count() > timeBudgetMs) {
            return -7777777; // Timeout Signal
        }
    }

    // Leaf Node: Return 0 (Evaluation is relative to the path taken)
    if (depth == 0) return 0;

    // Game Over: 2 Passes
    if (passesInARow >= 2) {
        // Calculate penalty (Tile Sum)
        int myPenalty = 0, oppPenalty = 0;
        for(int i=0; i<26; i++) {
             myPenalty += myRackCounts[i] * Heuristics::getTileValue('A'+i);
             oppPenalty += oppRackCounts[i] * Heuristics::getTileValue('A'+i);
        }
        return oppPenalty - myPenalty;
    }

    // Game Over: Empty Rack
    int* lastMoverRack = maximizingPlayer ? oppRackCounts : myRackCounts;
    bool isRackEmpty = true;
    for(int i=0; i<27; i++) if(lastMoverRack[i] > 0) { isRackEmpty = false; break; }

    if (isRackEmpty) {
        int* loserRack = maximizingPlayer ? myRackCounts : oppRackCounts;
        int penalty = 0;
        for(int i=0; i<26; i++) penalty += loserRack[i] * Heuristics::getTileValue('A'+i);
        // Bonus Rule: Winner gets 2x Loser's remaining points
        return maximizingPlayer ? -(penalty * 2) : (penalty * 2);
    }

    // Move Generation
    int* currentRack = maximizingPlayer ? myRackCounts : oppRackCounts;
    std::array<MoveCandidate, 256> moves;
    int moveCount = 0;
    auto consumer = [&](MoveCandidate& cand, int* rack) -> bool {
        if (moveCount < 256) moves[moveCount++] = cand;
        return true;
    };
    MoveGenerator::generate_raw(board, currentRack, dict, consumer);

    if (moveCount == 0) {
        // Pass Turn
        return minimax(board, bonusBoard, myRackCounts, oppRackCounts, dict,
                       alpha, beta, !maximizingPlayer, passesInARow + 1, depth - 1, startTime, timeBudgetMs);
    }

    // -------------------------------------------------------------------------
    // OPTIMIZATION: BEAM SEARCH & PARTIAL SORT
    // -------------------------------------------------------------------------
    // Instead of sorting ALL moves, we only care about the top N (Beam Width).
    // A beam of 8-12 is standard for Scrabble to prune bad branches early.
    int beamWidth = 8;
    int limit = std::min(moveCount, beamWidth);

    // Score all moves
    for (int i=0; i<moveCount; i++) moves[i].score = calculateMoveScore(board, bonusBoard, moves[i]);

    // Partial Sort: Only sort the top 'limit' elements to the front. O(N) instead of O(N log N).
    std::partial_sort(moves.begin(), moves.begin() + limit, moves.begin() + moveCount,
        [](const MoveCandidate& a, const MoveCandidate& b) {
            return a.score > b.score;
    });

    if (maximizingPlayer) {
        int maxEval = -999999;
        // ONLY iterate up to 'limit' (The Beam)
        for (int i=0; i<limit; i++) {
            LetterBoard nextBoard = board;
            int nextRack[27]; memcpy(nextRack, myRackCounts, 27*sizeof(int));
            applyMove(nextBoard, moves[i], nextRack);

            int ret = minimax(nextBoard, bonusBoard, nextRack, oppRackCounts, dict,
                             alpha, beta, false, 0, depth - 1, startTime, timeBudgetMs);
            if (ret == -7777777) return -7777777;

            int eval = moves[i].score + ret;
            maxEval = max(maxEval, eval);
            alpha = max(alpha, eval);
            if (beta <= alpha) break;
        }
        return maxEval;
    } else {
        int minEval = 999999;
        // ONLY iterate up to 'limit' (The Beam)
        for (int i=0; i<limit; i++) {
            LetterBoard nextBoard = board;
            int nextRack[27]; memcpy(nextRack, oppRackCounts, 27*sizeof(int));
            applyMove(nextBoard, moves[i], nextRack);

            int ret = minimax(nextBoard, bonusBoard, myRackCounts, nextRack, dict,
                             alpha, beta, true, 0, depth - 1, startTime, timeBudgetMs);
            if (ret == -7777777) return -7777777;

            int eval = -moves[i].score + ret;
            minEval = min(minEval, eval);
            beta = min(beta, eval);
            if (beta <= alpha) break;
        }
        return minEval;
    }
}

// -----------------------------------------------------------------------------
// INTERNAL HELPERS
// -----------------------------------------------------------------------------

int Judge::calculateMoveScore(const LetterBoard& board, const Board& bonusBoard, const MoveCandidate& move) {
    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMult = 1;
    int tilesPlaced = 0;
    int r = move.row; int c = move.col;
    int dr = move.isHorizontal ? 0 : 1; int dc = move.isHorizontal ? 1 : 0;

    for (int i = 0; move.word[i] != '\0'; i++) {
        char letter = move.word[i];

        // BLANK HANDLING
        int val = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);

        if (board[r][c] == ' ') {
            tilesPlaced++;
            CellType bonus = bonusBoard[r][c];
            if (bonus == CellType::DLS) val *= 2; else if (bonus == CellType::TLS) val *= 3;
            if (bonus == CellType::DWS) mainWordMult *= 2; else if (bonus == CellType::TWS) mainWordMult *= 3;
        }
        mainWordScore += val;

        // Cross Word Checks
        if (board[r][c] == ' ') {
             int pdr = move.isHorizontal ? 1 : 0; int pdc = move.isHorizontal ? 0 : 1;
             // Check if placing this tile connects to existing tiles
             if ((r>0 && board[r-1][c]!=' ') || (r<14 && board[r+1][c]!=' ') || (c>0 && board[r][c-1]!=' ') || (c<14 && board[r][c+1]!=' ')) {
                 int cr = r, cc = c; while(cr-pdr>=0 && cc-pdc>=0 && board[cr-pdr][cc-pdc]!=' ') { cr-=pdr; cc-=pdc; }
                 int crossScore=0, crossMult=1; bool hasW=false;
                 while(cr<15 && cc<15) {
                     char l = board[cr][cc];
                     if(cr==r && cc==c) {
                         // [FIX] Cross Blank Handling
                         int cv = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);
                         CellType cb = bonusBoard[cr][cc];
                         if (cb==CellType::DLS) cv*=2; else if(cb==CellType::TLS) cv*=3;
                         if (cb==CellType::DWS) crossMult*=2; else if(cb==CellType::TWS) crossMult*=3;
                         crossScore+=cv;
                     } else if (l!=' ') { crossScore+=Heuristics::getTileValue(l); hasW=true; } else break;
                     cr+=pdr; cc+=pdc;
                 }
                 if(hasW) totalScore += (crossScore*crossMult);
             }
        }
        r += dr; c += dc;
    }
    if (strlen(move.word) > 1) totalScore += (mainWordScore * mainWordMult);
    if (tilesPlaced == 7) totalScore += 50;
    return totalScore;
}

    void Judge::applyMove(LetterBoard& board, const MoveCandidate& move, int* rackCounts) {
    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    for (int i=0; move.word[i] != '\0'; i++) {
        if (board[r][c] == ' ') {
            board[r][c] = move.word[i];

            char letter = move.word[i];
            // [FIX] Check Case!
            // Lowercase = Blank used as letter. Uppercase = Real Tile.
            if (islower(letter)) {
                // Must be a blank
                if (rackCounts[26] > 0) rackCounts[26]--;
                // Fallback (logic error protection): if no blank, try real letter?
                // No, MoveGenerator guarantees correctness.
            }
            else if (isupper(letter)) {
                // Must be real letter
                if (rackCounts[letter - 'A'] > 0) rackCounts[letter - 'A']--;
                else if (rackCounts[26] > 0) rackCounts[26]--; // Fallback for bad states
            }
        }
        r += dr;
        c += dc;
    }
}

}