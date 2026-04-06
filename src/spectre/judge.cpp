#include "../../include/spectre/judge.h"
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

// ==============================================================================
// GLOBALS & TELEMETRY
// ==============================================================================
static std::atomic<int> g_nodesVisited(0);
static std::atomic<int> g_ttHits(0);

// THE KILL SWITCH: Thread-local prevents data races if multiple games run concurrently
static thread_local bool g_timeOut = false;

// ==============================================================================
// ZOBRIST HASHING (Transposition Table Keys)
// ==============================================================================
static uint64_t ZOBRIST_BOARD[15][15][27]; // 26 letters + 1 blank
static uint64_t ZOBRIST_RACK_MY[27][10];   // Letter x Count
static uint64_t ZOBRIST_RACK_OPP[27][10];
static uint64_t ZOBRIST_TURN;
static bool g_hashInitialized = false;

static void initZobrist() {
    if (g_hashInitialized) return;
    std::mt19937_64 rng(12345); // Fixed deterministic seed

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

    for(int r=0; r<15; r++) {
        for(int c=0; c<15; c++) {
            char ch = board[r][c];
            if (ch != ' ') {
                int idx = (ch == '?' || (ch>='a' && ch<='z')) ? 26 : (ch - 'A');
                if (idx >= 0 && idx <= 26) h ^= ZOBRIST_BOARD[r][c][idx];
            }
        }
    }

    for(int i=0; i<27; i++) {
        if (myRack[i] > 0) h ^= ZOBRIST_RACK_MY[i][myRack[i]];
        if (oppRack[i] > 0) h ^= ZOBRIST_RACK_OPP[i][oppRack[i]];
    }
    return h;
}

// ==============================================================================
// CORE SOLVER
// ==============================================================================
kernel::MoveCandidate Judge::solveEndgame(const LetterBoard& board, const Board& bonusBoard,
                         const TileRack& myRack, const TileRack& oppRack, const Dictionary& dict) {

    initZobrist();

    {
        ScopedLogger log;
        std::cout << "\n[JUDGE] =========================================" << std::endl;
        std::cout << "[JUDGE] ENDGAME SOLVER v2 (Transposition Table)" << std::endl;
    }

    // Reset state for new search
    g_nodesVisited = 0;
    g_ttHits = 0;
    g_timeOut = false;

    // 1. Array-based Rack Conversion (O(1) lookups during search)
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

    // Pre-allocate TT to prevent expensive mid-search heap reallocations
    std::unordered_map<uint64_t, TTEntry> transTable;
    transTable.reserve(100000);

    // Stopwatch Initialization
    auto startTime = steady_clock::now();
    int timeBudgetMs = 59000; // 59 Second Hard Limit

    // 2. Generate Root Moves
    vector<MoveCandidate> rootMoves;
    rootMoves.reserve(100);
    auto rootConsumer = [&](MoveCandidate& cand, int* leave) -> bool {
        rootMoves.push_back(cand);
        return true;
    };
    MoveGenerator::generate_raw(board, myRackCounts, dict, rootConsumer);

    // Always inject a voluntary pass
    MoveCandidate passMove;
    passMove.word[0] = '\0';
    passMove.score = 0;
    rootMoves.push_back(passMove);

    // Heuristic Sorting: Try mathematically heavy moves first to maximize alpha cutoffs
    for(auto& m : rootMoves) m.score = Mechanics::calculateTrueScore(m, board, bonusBoard);
    sort(rootMoves.begin(), rootMoves.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
        return a.score > b.score;
    });

    MoveCandidate bestMove = rootMoves[0]; // Fallback if everything times out instantly
    int bestVal = -999999;
    int alpha = -999999;
    int beta = 999999;

    // 3. Root Search Loop
    for (const auto& move : rootMoves) {

        // ROOT KILL SWITCH: Prevent launching new deep searches if time is up
        if (g_timeOut) {
            std::cout << "[JUDGE] 60-Second Hard Limit Reached. Aborting search." << std::endl;
            break;
        }

        LetterBoard nextBoard = board;
        int nextMyRack[27];
        memcpy(nextMyRack, myRackCounts, sizeof(nextMyRack));

        if (move.word[0] != '\0') {
            if (!applyMove(nextBoard, move, nextMyRack)) continue;
        }

        // Check for immediate "Going Out" victory at root
        bool rackEmpty = true;
        for(int i=0; i<27; i++) if(nextMyRack[i]>0) { rackEmpty=false; break; }

        int currentScore = move.score;

        if (rackEmpty) {
            int bonus = 0;
            for(int i=0; i<26; i++) bonus += oppRackCounts[i] * Heuristics::getTileValue((char)('A'+i));
            currentScore += (2 * bonus);
            if (currentScore > bestVal) { bestVal = currentScore; bestMove = move; }
            continue; // End of this branch
        }

        // Launch recursive deep dive for opponent's response
        int val = currentScore - minimax(nextBoard, bonusBoard, oppRackCounts, nextMyRack, dict,
                                        -beta, -alpha, false, 0, 1,
                                        startTime, timeBudgetMs, transTable);

        // POST-RECURSION KILL SWITCH: Do not update best move if the data is corrupted by timeout
        if (g_timeOut) {
            std::cout << "[JUDGE] 60-Second Hard Limit Reached mid-branch. Reverting to best safe move." << std::endl;
            break;
        }

        if (val > bestVal) {
            bestVal = val;
            bestMove = move;
        }
        alpha = std::max(alpha, val);
        if (alpha >= beta) break; // Alpha-Beta Cutoff
    }

    std::cout << "[JUDGE] Complete. Nodes: " << g_nodesVisited << " | TT Hits: " << g_ttHits
              << " | Best Val: " << bestVal << std::endl;

    return bestMove;
}

// ==============================================================================
// RECURSIVE MINIMAX (Negamax Formulation)
// ==============================================================================
int Judge::minimax(LetterBoard& board, const Board& bonusBoard,
                   int* currentRackCounts, int* otherRackCounts,
                   const Dictionary& dict,
                   int alpha, int beta,
                   bool maximizingPlayer,
                   int passesInARow,
                   int depth,
                   const steady_clock::time_point& startTime,
                   int timeBudgetMs,
                   std::unordered_map<uint64_t, TTEntry>& transTable) {

    // INSTANT UNWIND: If flag is set globally, collapse the stack
    if (g_timeOut) return 0;
    g_nodesVisited++;

    // HIGH FREQUENCY PULSE: Check clock every 64 nodes (Bitwise AND is virtually 0 CPU cycles)
    if ((g_nodesVisited & 0x3F) == 0) {
        if (duration_cast<milliseconds>(steady_clock::now() - startTime).count() > timeBudgetMs) {
            g_timeOut = true;
            return 0;
        }
    }

    // 1. TT Lookup
    int alphaOrig = alpha;
    uint64_t hash = computeHash(board, currentRackCounts, otherRackCounts, maximizingPlayer);

    if (transTable.count(hash)) {
        const TTEntry& tt = transTable[hash];
        if (tt.depth >= (60 - depth)) {
            g_ttHits++;
            if (tt.flag == 0) return tt.value;
            if (tt.flag == 1) alpha = std::max(alpha, tt.value);
            else if (tt.flag == 2) beta = std::min(beta, tt.value);
            if (alpha >= beta) return tt.value;
        }
    }

    // 2. Generate Moves
    vector<MoveCandidate> moves;
    moves.reserve(64);
    auto recursiveConsumer = [&](MoveCandidate& cand, int* leave) -> bool {
        moves.push_back(cand);
        return true;
    };
    MoveGenerator::generate_raw(board, currentRackCounts, dict, recursiveConsumer);

    // Redundant time check to catch generator hangs
    if (duration_cast<milliseconds>(steady_clock::now() - startTime).count() > timeBudgetMs) {
        g_timeOut = true;
        return 0;
    }

    // 3. Terminal State Check (Double Pass)
    if (moves.empty()) passesInARow++;
    else passesInARow = 0;

    if (passesInARow >= 2) {
        // Evaluate Unplayed Tile Penalties
        int myRem = 0, oppRem = 0;
        for(int i=0; i<26; i++) {
            int val = Heuristics::getTileValue((char)('A'+i));
            myRem += currentRackCounts[i] * val;
            oppRem += otherRackCounts[i] * val;
        }
        return (oppRem - myRem); // Net spread of penalty
    }

    if (!moves.empty()) {
        MoveCandidate pass;
        pass.word[0] = '\0'; pass.score = 0;
        moves.push_back(pass);
    } else {
        // Forced Pass branch
        int val = -minimax(board, bonusBoard, otherRackCounts, currentRackCounts, dict,
                        -beta, -alpha, !maximizingPlayer, passesInARow, depth+1,
                        startTime, timeBudgetMs, transTable);
        if (g_timeOut) return 0;
        return val;
    }

    // Sort generated moves for optimal Alpha-Beta pruning
    for(auto& m : moves) m.score = Mechanics::calculateTrueScore(m, board, bonusBoard);
    sort(moves.begin(), moves.end(), [](const MoveCandidate& a, const MoveCandidate& b){ return a.score > b.score; });

    int bestVal = -999999;

    // 4. Recursive Evaluation
    for (const auto& move : moves) {
        LetterBoard nextBoard = board;
        int nextRack[27];
        memcpy(nextRack, currentRackCounts, sizeof(nextRack));

        if (move.word[0] != '\0') {
            if (!applyMove(nextBoard, move, nextRack)) continue;
        }

        int moveScore = move.score;
        bool rackEmpty = true;
        for(int i=0; i<27; i++) if(nextRack[i]>0) { rackEmpty=false; break; }

        if (rackEmpty && move.word[0] != '\0') {
            int bonus = 0;
            for(int i=0; i<26; i++) bonus += otherRackCounts[i] * Heuristics::getTileValue((char)('A'+i));
            int total = moveScore + (2 * bonus);

            if (total > bestVal) bestVal = total;
            alpha = std::max(alpha, bestVal);
            if (alpha >= beta) break;
            continue;
        }

        int val = moveScore - minimax(nextBoard, bonusBoard, otherRackCounts, nextRack, dict,
                                      -beta, -alpha, !maximizingPlayer,
                                      (move.word[0]=='\0' ? passesInARow+1 : 0),
                                      depth+1, startTime, timeBudgetMs, transTable);

        if (g_timeOut) return 0; // ABORT SIGNAL

        if (val > bestVal) bestVal = val;
        alpha = std::max(alpha, bestVal);
        if (alpha >= beta) break; // Alpha-Beta Cutoff
    }

    // 5. CACHE PROTECTION: Do not save incomplete calculations
    if (g_timeOut) return 0;

    TTEntry entry;
    entry.value = bestVal;
    entry.depth = (60 - depth);
    if (bestVal <= alphaOrig) entry.flag = 2;
    else if (bestVal >= beta) entry.flag = 1;
    else entry.flag = 0;

    transTable[hash] = entry;

    return bestVal;
}

// Applies physics to local copies of the board and rack arrays
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
                else if (rackCounts[26] > 0) rackCounts[26]--; // Blanks as fallback
                else return false;
            }
        }
        r += dr; c += dc;
    }
    return true;
}

}