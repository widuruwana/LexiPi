#include "../../include/spectre/judge.h"
#include "../../include/spectre/spy.h"
#include "../../include/kernel/move_generator.h"
#include "../../include/kernel/heuristics.h"
#include "../../include/engine/mechanics.h"
#include "../../include/spectre/logger.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <atomic>
#include <iomanip>
#include <random>

using namespace std;
using namespace std::chrono;
using namespace kernel;

namespace spectre {

// --- TELEMETRY ---
static std::atomic<int> g_nodesVisited(0);
static std::atomic<int> g_ttHits(0);

// --- ZOBRIST HASHING (Static Initialization) ---
static uint64_t ZOBRIST_BOARD[15][15][27]; // 26 letters + 1 blank
static uint64_t ZOBRIST_RACK_MY[27][10];   // Letter x Count
static uint64_t ZOBRIST_RACK_OPP[27][10];
static uint64_t ZOBRIST_TURN;
static bool g_hashInitialized = false;

// Computes the leave equity for a move by subtracting placed tiles from rackCounts.
// Used for equity-augmented move ordering in alpha-beta — better ordering → more cutoffs.
// stack-only, zero allocation, O(word_length + 27).
static float leaveEquityForMove(const MoveCandidate& m, const LetterBoard& board, const int* rackCounts) {
    int leaveCounts[27];
    memcpy(leaveCounts, rackCounts, 27 * sizeof(int));
    if (m.word[0] != '\0') {
        int r = m.row, c = m.col;
        int dr = m.isHorizontal ? 0 : 1, dc = m.isHorizontal ? 1 : 0;
        for (int i = 0; m.word[i] != '\0'; i++, r += dr, c += dc) {
            if (board[r][c] == ' ') {
                char letter = m.word[i];
                if (letter >= 'a' && letter <= 'z') {
                    if (leaveCounts[26] > 0) leaveCounts[26]--;
                } else {
                    int idx = letter - 'A';
                    if (leaveCounts[idx] > 0) leaveCounts[idx]--;
                    else if (leaveCounts[26] > 0) leaveCounts[26]--;
                }
            }
        }
    }
    return Heuristics::computeLeaveEquity(leaveCounts);
}

static void initZobrist() {
    if (g_hashInitialized) return;
    std::mt19937_64 rng(12345); // Fixed seed for determinism

    for(int r=0; r<15; r++)
        for(int c=0; c<15; c++)
            for(int k=0; k<27; k++)
                ZOBRIST_BOARD[r][c][k] = rng();

    for(int i=0; i<27; i++)
        for(int c=0; c<10; c++) {
            ZOBRIST_RACK_MY[i][c] = rng();
            ZOBRIST_RACK_OPP[i][c] = rng();
        }

    ZOBRIST_TURN = rng();
    g_hashInitialized = true;
}

uint64_t Judge::computeHash(const LetterBoard& board, int* myRack, int* oppRack, bool myTurn) {
    uint64_t h = 0;
    if (myTurn) h ^= ZOBRIST_TURN;

    // Hash Board
    for(int r=0; r<15; r++) {
        for(int c=0; c<15; c++) {
            char ch = board[r][c];
            if (ch != ' ') {
                int idx = (ch == '?' || (ch>='a' && ch<='z')) ? 26 : (ch - 'A');
                if (idx >= 0 && idx <= 26) h ^= ZOBRIST_BOARD[r][c][idx];
            }
        }
    }

    // Hash Racks
    for(int i=0; i<27; i++) {
        if (myRack[i] > 0) h ^= ZOBRIST_RACK_MY[i][myRack[i]];
        if (oppRack[i] > 0) h ^= ZOBRIST_RACK_OPP[i][oppRack[i]];
    }
    return h;
}

kernel::MoveCandidate Judge::solveEndgame(const LetterBoard& board, const Board& bonusBoard,
                         const TileRack& myRack, const TileRack& oppRack, Dictionary& dict) {

    initZobrist();

    {
        ScopedLogger log;
        std::cout << "\n[JUDGE] =========================================" << std::endl;
        std::cout << "[JUDGE] ENDGAME SOLVER v2 (Transposition Table)" << std::endl;
    }

    g_nodesVisited = 0;
    g_ttHits = 0;

    // 1. Setup Racks
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

    // 2. Setup Transposition Table
    std::unordered_map<uint64_t, TTEntry> transTable;
    // Pre-allocate bucket count to prevent resize during recursion
    transTable.reserve(100000);

    // 3. Iterative Deepening (Optional, but good for ordering)
    // For endgame, we usually just go for max depth allowed by time.
    int maxDepth = 60; // Effectively infinite for Scrabble
    auto startTime = steady_clock::now();
    int timeBudgetMs = 5000; // 5 seconds for endgame calculation

    // 4. Initial Call
    // We pass a copy of the board to start the recursion
    LetterBoard mutableBoard = board;

    // Root Level Move Generation (to return the actual Move object)
    // We can't just call minimax because we need to know WHICH move gave the value.

    vector<MoveCandidate> rootMoves;
    rootMoves.reserve(100);
    auto rootConsumer = [&](MoveCandidate& cand, int* leave) -> bool {
        rootMoves.push_back(cand);
        return true;
    };
    MoveGenerator::generate_raw(board, myRackCounts, dict, rootConsumer);

    // Add Voluntary Pass
    MoveCandidate passMove;
    passMove.word[0] = '\0';
    passMove.score = 0;
    rootMoves.push_back(passMove);

    // Sort by score + leave equity for better alpha-beta move ordering
    for(auto& m : rootMoves) m.score = Mechanics::calculateTrueScore(m, board, bonusBoard);
    sort(rootMoves.begin(), rootMoves.end(), [&](const MoveCandidate& a, const MoveCandidate& b) {
        return ((float)a.score + leaveEquityForMove(a, board, myRackCounts))
             > ((float)b.score + leaveEquityForMove(b, board, myRackCounts));
    });

    MoveCandidate bestMove = rootMoves[0];
    int bestVal = -999999;
    int alpha = -999999;
    int beta = 999999;

    for (const auto& move : rootMoves) {
        // Clone State
        LetterBoard nextBoard = board;
        int nextMyRack[27];
        memcpy(nextMyRack, myRackCounts, sizeof(nextMyRack));

        // Apply
        if (move.word[0] != '\0') {
            if (!applyMove(nextBoard, move, nextMyRack)) continue;
        }

        // Check for "Going Out" immediately at root
        bool rackEmpty = true;
        for(int i=0; i<27; i++) if(nextMyRack[i]>0) { rackEmpty=false; break; }

        int currentScore = move.score;

        if (rackEmpty) {
            // Calculate Bonus
            int bonus = 0;
            for(int i=0; i<26; i++) bonus += oppRackCounts[i] * Heuristics::getTileValue((char)('A'+i));
            currentScore += (2 * bonus);
            // This is likely the best possible move, but we check logic anyway
            if (currentScore > bestVal) { bestVal = currentScore; bestMove = move; }
            continue;
        }

        // Recursion (Opponent's Turn)
        int val = currentScore - minimax(nextBoard, bonusBoard, oppRackCounts, nextMyRack, dict,
                                        -beta, -alpha, false, 0, 1,
                                        startTime, timeBudgetMs, transTable);

        if (val > bestVal) {
            bestVal = val;
            bestMove = move;
        }
        alpha = std::max(alpha, val);
        if (alpha >= beta) break;
    }

    std::cout << "[JUDGE] Complete. Nodes: " << g_nodesVisited << " | TT Hits: " << g_ttHits
              << " | Best Val: " << bestVal << std::endl;

    return bestMove;
}

kernel::MoveCandidate Judge::solvePreEndgame(const LetterBoard& board, const Board& bonusBoard,
                                              const TileRack& myRack, spectre::Spy& spy,
                                              Dictionary& dict, int numSamples) {
    initZobrist();
    g_nodesVisited = 0;
    g_ttHits = 0;

    // 1. My rack counts (stack array)
    int myRackCounts[27] = {0};
    for (const auto& t : myRack) {
        if (t.letter == '?') myRackCounts[26]++;
        else if (isalpha(t.letter)) myRackCounts[toupper(t.letter) - 'A']++;
    }

    // 2. Root move generation — build once, reused across all samples
    vector<MoveCandidate> rootMoves;
    rootMoves.reserve(200);
    auto rootConsumer = [&](MoveCandidate& cand, int* leave) -> bool {
        rootMoves.push_back(cand);
        return true;
    };
    MoveGenerator::generate_raw(board, myRackCounts, dict, rootConsumer);

    MoveCandidate passMove;
    passMove.word[0] = '\0'; passMove.score = 0;
    rootMoves.push_back(passMove);

    // Score and sort by equity-augmented value for consistent move ordering
    for (auto& m : rootMoves) m.score = Mechanics::calculateTrueScore(m, board, bonusBoard);
    sort(rootMoves.begin(), rootMoves.end(), [&](const MoveCandidate& a, const MoveCandidate& b) {
        return ((float)a.score + leaveEquityForMove(a, board, myRackCounts))
             > ((float)b.score + leaveEquityForMove(b, board, myRackCounts));
    });

    // 3. Accumulated scores across all samples
    vector<float> accScores(rootMoves.size(), 0.0f);

    // 4. Transposition table — cleared between samples, memory reused
    unordered_map<uint64_t, TTEntry> transTable;
    transTable.reserve(100000);

    const int timeBudgetPerSample = 5000 / numSamples; // ~625ms each for numSamples=8

    // 5. Sample loop: each iteration uses a different sampled opponent rack
    for (int s = 0; s < numSamples; s++) {
        transTable.clear();
        auto sampleStart = steady_clock::now();

        vector<char> oppChars = spy.sampleParticleRack();
        int oppRackCounts[27] = {0};
        for (char c : oppChars) {
            if (c == '?') oppRackCounts[26]++;
            else if (isalpha(c)) oppRackCounts[toupper(c) - 'A']++;
        }

        int alpha = -999999, beta = 999999; // reset per sample

        for (size_t i = 0; i < rootMoves.size(); i++) {
            const auto& move = rootMoves[i];

            LetterBoard nextBoard = board;
            int nextMyRack[27];
            memcpy(nextMyRack, myRackCounts, sizeof(nextMyRack));

            if (move.word[0] != '\0') {
                if (!applyMove(nextBoard, move, nextMyRack)) continue;
            }

            bool rackEmpty = true;
            for (int j = 0; j < 27; j++) if (nextMyRack[j] > 0) { rackEmpty = false; break; }

            int currentScore = move.score;

            if (rackEmpty && move.word[0] != '\0') {
                // Going out immediately — collect bonus
                int bonus = 0;
                for (int j = 0; j < 26; j++) bonus += oppRackCounts[j] * Heuristics::getTileValue((char)('A'+j));
                currentScore += (2 * bonus);
                accScores[i] += (float)currentScore;
                alpha = std::max(alpha, currentScore);
                continue;
            }

            int val = currentScore - minimax(nextBoard, bonusBoard, oppRackCounts, nextMyRack, dict,
                                             -beta, -alpha, false, 0, 1,
                                             sampleStart, timeBudgetPerSample, transTable);
            accScores[i] += (float)val;
            alpha = std::max(alpha, val);
            if (alpha >= beta) break; // alpha-beta cut across root moves for this sample
        }
    }

    // 6. Return root move with highest average score across all samples
    size_t bestIdx = 0;
    float bestAvg = accScores[0];
    for (size_t i = 1; i < accScores.size(); i++) {
        if (accScores[i] > bestAvg) { bestAvg = accScores[i]; bestIdx = i; }
    }

    return rootMoves[bestIdx];
}

int Judge::minimax(LetterBoard& board, const Board& bonusBoard,
                   int* currentRackCounts, int* otherRackCounts,
                   Dictionary& dict,
                   int alpha, int beta,
                   bool maximizingPlayer,
                   int passesInARow,
                   int depth,
                   const steady_clock::time_point& startTime,
                   int timeBudgetMs,
                   std::unordered_map<uint64_t, TTEntry>& transTable) {

    g_nodesVisited++;

    // 1. Time Check (Every 4096 nodes)
    if ((g_nodesVisited & 0xFFF) == 0) {
        if (duration_cast<milliseconds>(steady_clock::now() - startTime).count() > timeBudgetMs)
            return alpha; // Timeout fallback
    }

    // 2. Transposition Table Lookup
    int alphaOrig = alpha;
    uint64_t hash = computeHash(board, currentRackCounts, otherRackCounts, maximizingPlayer);

    if (transTable.count(hash)) {
        const TTEntry& tt = transTable[hash];
        if (tt.depth >= (60 - depth)) { // Simple depth check
            g_ttHits++;
            if (tt.flag == 0) return tt.value; // Exact
            if (tt.flag == 1) alpha = std::max(alpha, tt.value); // LowerBound
            else if (tt.flag == 2) beta = std::min(beta, tt.value); // UpperBound
            if (alpha >= beta) return tt.value;
        }
    }

    // 3. Move Generation
    vector<MoveCandidate> moves;
    moves.reserve(64);
    auto recursiveConsumer = [&](MoveCandidate& cand, int* leave) -> bool {
        moves.push_back(cand);
        return true;
    };
    MoveGenerator::generate_raw(board, currentRackCounts, dict, recursiveConsumer);

    // 4. Terminal State: Double Pass
    if (moves.empty()) {
        passesInARow++;
        // If I must pass, I add a "dummy" pass move implicitly by doing nothing here
        // But if moves IS empty, we fall through to the pass logic.
    } else {
        passesInARow = 0; // Reset if we have moves
    }

    if (passesInARow >= 2) {
        // GAME OVER by Passes
        int myRem = 0, oppRem = 0;
        for(int i=0; i<26; i++) {
            int val = Heuristics::getTileValue((char)('A'+i));
            myRem += currentRackCounts[i] * val;
            oppRem += otherRackCounts[i] * val;
        }
        // Net spread adjustment: I lose myRem, Opponent loses oppRem.
        // Relative value: (OppScore - oppRem) - (MyScore - myRem) ???
        // Standard rule: Scores are reduced by remaining tiles.
        // Marginal Gain = -myRem - (-oppRem) = oppRem - myRem.
        return (oppRem - myRem);
    }

    // Add Voluntary Pass if not forced (tactical pass)
    if (!moves.empty()) {
        MoveCandidate pass;
        pass.word[0] = '\0'; pass.score = 0;
        moves.push_back(pass);
    } else {
        // If moves is empty, we effectively executed a pass above.
        // We recurse with pass.
        return -minimax(board, bonusBoard, otherRackCounts, currentRackCounts, dict,
                        -beta, -alpha, !maximizingPlayer, passesInARow, depth+1,
                        startTime, timeBudgetMs, transTable);
    }

    // 5. Sorting & Search — equity-augmented ordering for better alpha-beta cutoffs
    for(auto& m : moves) m.score = Mechanics::calculateTrueScore(m, board, bonusBoard);
    sort(moves.begin(), moves.end(), [&](const MoveCandidate& a, const MoveCandidate& b) {
        return ((float)a.score + leaveEquityForMove(a, board, currentRackCounts))
             > ((float)b.score + leaveEquityForMove(b, board, currentRackCounts));
    });

    int bestVal = -999999;

    for (const auto& move : moves) {
        LetterBoard nextBoard = board; // Copy board (efficient enough for 15x15)
        int nextRack[27];
        memcpy(nextRack, currentRackCounts, sizeof(nextRack));

        // Apply Move
        if (move.word[0] != '\0') {
            if (!applyMove(nextBoard, move, nextRack)) continue;
        }

        int moveScore = move.score;
        bool rackEmpty = true;
        for(int i=0; i<27; i++) if(nextRack[i]>0) { rackEmpty=false; break; }

        // CHECK MATE (Going Out)
        if (rackEmpty && move.word[0] != '\0') {
            int bonus = 0;
            for(int i=0; i<26; i++) bonus += otherRackCounts[i] * Heuristics::getTileValue((char)('A'+i));
            int total = moveScore + (2 * bonus);

            if (total > bestVal) bestVal = total;
            alpha = std::max(alpha, bestVal);
            if (alpha >= beta) break;
            continue;
        }

        // RECURSE
        int val = moveScore - minimax(nextBoard, bonusBoard, otherRackCounts, nextRack, dict,
                                      -beta, -alpha, !maximizingPlayer,
                                      (move.word[0]=='\0' ? passesInARow+1 : 0),
                                      depth+1, startTime, timeBudgetMs, transTable);

        if (val > bestVal) bestVal = val;
        alpha = std::max(alpha, bestVal);
        if (alpha >= beta) break;
    }

    // 6. Store in TT
    TTEntry entry;
    entry.value = bestVal;
    entry.depth = (60 - depth); // Storing remaining depth concept, though less relevant in endgame
    if (bestVal <= alphaOrig) entry.flag = 2; // UpperBound
    else if (bestVal >= beta) entry.flag = 1; // LowerBound
    else entry.flag = 0; // Exact

    transTable[hash] = entry;

    return bestVal;
}

bool Judge::applyMove(LetterBoard& board, const MoveCandidate& move, int* rackCounts) {
    int r = move.row; int c = move.col;
    int dr = move.isHorizontal ? 0 : 1; int dc = move.isHorizontal ? 1 : 0;

    for (int i=0; move.word[i] != '\0'; i++) {
        char letter = move.word[i];
        if (board[r][c] == ' ') {
            bool isBlank = (letter >= 'a' && letter <= 'z');
            board[r][c] = isBlank ? (letter - 32) : letter;

            if (isBlank) {
                if (rackCounts[26] > 0) rackCounts[26]--;
                else return false;
            } else {
                int idx = letter - 'A';
                if (rackCounts[idx] > 0) rackCounts[idx]--;
                else if (rackCounts[26] > 0) rackCounts[26]--; // Use blank as fallback
                else return false;
            }
        }
        r += dr; c += dc;
    }
    return true;
}

}