#include "../../include/spectre/vanguard.h"
#include "../../include/spectre/spy.h"
#include "../../include/heuristics.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <cstring>
#include <iostream>

using namespace std;

namespace spectre {

// Thread-local Random Number Generator (Seeded once per thread)
thread_local mt19937 rng(random_device{}());

// =============================================================================
// OPTIMIZATION: FastBag
// A stack-based container for tiles to avoid heap allocations during playouts.
// =============================================================================
struct FastBag {
    char tiles[128]; // Max tiles in standard Scrabble is 100, buffer for safety
    int count;

    // Default constructor (Empty bag)
    FastBag() : count(0) {}

    // Construct from vector (Once per search root)
    FastBag(const vector<char>& source) {
        count = 0;
        for (char c : source) {
            if (count < 128) tiles[count++] = c;
        }
    }

    // Fisher-Yates shuffle
    void shuffleBag() {
        for (int i = count - 1; i > 0; i--) {
            uniform_int_distribution<int> dist(0, i);
            int j = dist(rng);
            char temp = tiles[i];
            tiles[i] = tiles[j];
            tiles[j] = temp;
        }
    }

    // Pop a tile (Returns false if empty)
    bool pop(char& out) {
        if (count == 0) return false;
        out = tiles[--count];
        return true;
    }
};

// =============================================================================
// HELPER: Fast Rack Filling
// Fills a rack histogram from the FastBag until it has 7 tiles.
// =============================================================================
static void fastFillRack(int* rackCounts, FastBag& bag) {
    int currentCount = 0;
    for (int i = 0; i < 27; i++) currentCount += rackCounts[i];

    while (currentCount < 7) {
        char t;
        if (!bag.pop(t)) break;

        if (t == '?') rackCounts[26]++;
        else if (t >= 'A' && t <= 'Z') rackCounts[t - 'A']++;

        currentCount++;
    }
}

// =============================================================================
// HELPER: Fast Playout Simulation
// Simulates 2 full turns (Opponent -> Me -> Opponent) using greedy logic.
// Returns the net score difference.
// =============================================================================
static int fastPlayout(LetterBoard currentBoard, const Board& bonusBoard,
                       int* oppRackCounts, FastBag bag, bool myTurn, Dawg& dict) {

    int myScore = 0;
    int oppScore = 0;
    int passes = 0;

    int mySimRack[27] = {0};
    int oppSimRack[27];
    memcpy(oppSimRack, oppRackCounts, 27 * sizeof(int));

    if (myTurn) fastFillRack(mySimRack, bag);
    else fastFillRack(oppSimRack, bag);

    // Run simulation for 2 rounds
    for (int turn = 0; turn < 2; turn++) {
        int* currentRack = myTurn ? mySimRack : oppSimRack;
        int* currentScorePtr = myTurn ? &myScore : &oppScore;

        fastFillRack(currentRack, bag);

        MoveCandidate bestMove;
        bestMove.score = -1;
        bool found = false;

        // CONSUMER LAMBDA
        // Uses 'currentBoard' which is updated every ply.
        auto consumer = [&](MoveCandidate& cand, int* rack) -> bool {
            int totalScore = 0;
            int mainWordScore = 0;
            int mainMult = 1;
            int tilesPlaced = 0;

            int r = cand.row;
            int c = cand.col;
            int dr = cand.isHorizontal ? 0 : 1;
            int dc = cand.isHorizontal ? 1 : 0;

            for(int i=0; cand.word[i]!='\0'; ++i) {
                char letter = cand.word[i];

                // Blank Scoring Fix
                int val = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);

                // Check against MUTABLE currentBoard
                if(currentBoard[r][c] == ' ') {
                    tilesPlaced++;
                    CellType bonus = bonusBoard[r][c];
                    if(bonus == CellType::DLS) val *= 2; else if(bonus == CellType::TLS) val *= 3;
                    if(bonus == CellType::DWS) mainMult *= 2; else if(bonus == CellType::TWS) mainMult *= 3;
                }
                mainWordScore += val;
                r += dr; c += dc;
            }
            totalScore = mainWordScore * mainMult;
            if(tilesPlaced == 7) totalScore += 50;

            if (totalScore > bestMove.score) {
                bestMove = cand;
                bestMove.score = (short)totalScore;
                found = true;
                if (totalScore > 35) return false; // Heuristic cut-off
            }
            return true;
        };

        MoveGenerator::generate_raw(currentBoard, currentRack, dict, consumer);

        if (found) {
            *currentScorePtr += bestMove.score;
            passes = 0;

            // APPLY MOVE TO BOARD (Crucial for simulation integrity)
            int r = bestMove.row; int c = bestMove.col;
            int dr = bestMove.isHorizontal ? 0 : 1; int dc = bestMove.isHorizontal ? 1 : 0;

            for(int i=0; bestMove.word[i]!='\0'; ++i) {
                if (currentBoard[r][c] == ' ') {
                    // 1. Update Board (Force Uppercase for GADDAG safety)
                    currentBoard[r][c] = toupper(bestMove.word[i]);

                    // 2. Remove from Rack
                    char l = bestMove.word[i];
                    if (l >= 'a' && l <= 'z') { // Blank
                        if(currentRack[26] > 0) currentRack[26]--;
                    } else { // Letter
                        int idx = l - 'A';
                        if(currentRack[idx] > 0) currentRack[idx]--;
                        else if(currentRack[26] > 0) currentRack[26]--;
                    }
                }
                r += dr; c += dc;
            }
        } else {
            passes++;
        }

        if (passes >= 2) break;
        myTurn = !myTurn;
    }

    return myScore - oppScore;
}

// =============================================================================
// MAIN SEARCH
// =============================================================================
MoveCandidate Vanguard::search(const LetterBoard& board, const Board& bonusBoard,
                               const TileRack& rack, const vector<char>& unseenBag,
                               Dawg& dict, int timeLimitMs) {

    vector<MoveCandidate> candidates = MoveGenerator::generate(board, rack, dict, false);
    if (candidates.empty()) return {};

    // 1. Static Scoring
    for (auto& cand : candidates) cand.score = calculateScore(board, bonusBoard, cand);

    // 2. Sort & Prune
    sort(candidates.begin(), candidates.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
        return a.score > b.score;
    });

    int candidateCount = min((int)candidates.size(), 12);

    // 3. Threading
    unsigned int nThreads = std::thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 4;

    vector<future<void>> workers;

    struct ThreadResult {
        vector<long long> scores;
        vector<int> counts;
    };
    vector<ThreadResult> threadResults(nThreads, {vector<long long>(candidateCount, 0), vector<int>(candidateCount, 0)});

    FastBag templateBag(unseenBag);
    auto startTime = chrono::high_resolution_clock::now();

    for (unsigned int t = 0; t < nThreads; t++) {
        workers.push_back(async(launch::async,
            [&, t]() {
                FastBag localBag;
                while (true) {
                    auto now = chrono::high_resolution_clock::now();
                    if (chrono::duration_cast<chrono::milliseconds>(now - startTime).count() >= timeLimitMs) {
                        break;
                    }

                    int k = rng() % candidateCount;

                    localBag = templateBag;
                    localBag.shuffleBag();

                    int oppRackCounts[27] = {0};
                    fastFillRack(oppRackCounts, localBag);

                    // PREPARE NEXT BOARD
                    // We must apply the candidate move to a temporary board
                    // before starting the simulation loop.
                    LetterBoard nextBoard = board; // Copy Root

                    // Apply Candidate K to nextBoard
                    // Note: We duplicate apply logic here to avoid overhead of function calls
                    const auto& move = candidates[k];
                    int r = move.row; int c = move.col;
                    int dr = move.isHorizontal ? 0 : 1; int dc = move.isHorizontal ? 1 : 0;
                    for(int i=0; move.word[i]!='\0'; ++i) {
                        if (nextBoard[r][c] == ' ') {
                            nextBoard[r][c] = toupper(move.word[i]);
                        }
                        r+=dr; c+=dc;
                    }

                    // Start Simulation with:
                    // - The board AFTER my move
                    // - Opponent's turn next
                    int outcome = fastPlayout(nextBoard, bonusBoard, oppRackCounts, localBag, false, dict);

                    threadResults[t].scores[k] += (candidates[k].score + outcome);
                    threadResults[t].counts[k]++;
                }
            }
        ));
    }

    for (auto& f : workers) f.wait();

    // 4. Aggregate
    int bestIdx = 0;
    double maxVal = -999999.0;

    for (int i = 0; i < candidateCount; i++) {
        long long totalScore = 0;
        int totalSims = 0;
        for (unsigned int t = 0; t < nThreads; t++) {
            totalScore += threadResults[t].scores[i];
            totalSims += threadResults[t].counts[i];
        }

        if (totalSims > 0) {
            double avg = (double)totalScore / totalSims;
            if (avg > maxVal) {
                maxVal = avg;
                bestIdx = i;
            }
        }
    }

    return candidates[bestIdx];
}

int Vanguard::calculateScore(const LetterBoard& board, const Board& bonusBoard, const MoveCandidate& move) {
    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMult = 1;
    int tilesPlaced = 0;
    int r = move.row; int c = move.col;
    int dr = move.isHorizontal ? 0 : 1; int dc = move.isHorizontal ? 1 : 0;

    for (int i = 0; move.word[i] != '\0'; i++) {
        char letter = move.word[i];

        // [FIX] BLANK HANDLING
        int val = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);

        bool isNew = (board[r][c] == ' ');

        if (isNew) {
            tilesPlaced++;
            CellType bonus = bonusBoard[r][c];
            if (bonus == CellType::DLS) val *= 2; else if (bonus == CellType::TLS) val *= 3;
            if (bonus == CellType::DWS) mainWordMult *= 2; else if (bonus == CellType::TWS) mainWordMult *= 3;
        }
        mainWordScore += val;

        // Cross word logic
        if (isNew) {
            int pdr = move.isHorizontal ? 1 : 0; int pdc = move.isHorizontal ? 0 : 1;
            bool hasNeighbor = false;
            if (r-pdr>=0 && c-pdc>=0 && board[r-pdr][c-pdc]!=' ') hasNeighbor=true;
            if (r+pdr<15 && c+pdc<15 && board[r+pdr][c+pdc]!=' ') hasNeighbor=true;

            if (hasNeighbor) {
                int crossScore = 0; int crossMult = 1;
                int cr = r, cc = c;
                while(cr-pdr>=0 && cc-pdc>=0 && board[cr-pdr][cc-pdc]!=' ') { cr-=pdr; cc-=pdc; }
                while(cr<15 && cc<15) {
                    char l = board[cr][cc];
                    if (cr==r && cc==c) {
                        // [FIX] Cross Blank Handling
                        int cv = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);

                        CellType cb = bonusBoard[cr][cc];
                        if (cb==CellType::DLS) cv*=2; else if(cb==CellType::TLS) cv*=3;
                        if (cb==CellType::DWS) crossMult*=2; else if(cb==CellType::TWS) crossMult*=3;
                        crossScore += cv;
                    } else if (l != ' ') {
                        crossScore += Heuristics::getTileValue(l);
                    } else break;
                    cr+=pdr; cc+=pdc;
                }
                totalScore += (crossScore * crossMult);
            }
        }
        r += dr; c += dc;
    }
    if (strlen(move.word) > 1) totalScore += (mainWordScore * mainWordMult);
    if (tilesPlaced == 7) totalScore += 50;
    return totalScore;
}

int Vanguard::applyMove(LetterBoard& board, const MoveCandidate& move, int* rackCounts) {
    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    for (int i=0; move.word[i] != '\0'; i++) {
        if (board[r][c] == ' ') {
            board[r][c] = move.word[i];
            // Remove from rack if tracking provided
            if (rackCounts) {
                char letter = move.word[i];
                if (letter >= 'a' && letter <= 'z') {
                     if (rackCounts[26] > 0) rackCounts[26]--;
                } else {
                    int idx = letter - 'A';
                    if (rackCounts[idx] > 0) rackCounts[idx]--;
                    else if (rackCounts[26] > 0) rackCounts[26]--;
                }
            }
        }
        r += dr;
        c += dc;
    }
    return move.score;
}

void Vanguard::fillRack(int* rackCounts, vector<char>& bag) {
    int count = 0;
    for(int i=0; i<27; i++) count += rackCounts[i];

    while (count < 7 && !bag.empty()) {
        char t = bag.back();
        bag.pop_back();

        if (t == '?') rackCounts[26]++;
        else rackCounts[t - 'A']++;
        count++;
    }
}

}