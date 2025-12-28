#include "../../include/spectre/vanguard.h"
#include "../../include/heuristics.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <cstring>
#include <iostream>

using namespace std;

namespace spectre {

// Random Number Generator for simulations
thread_local mt19937 rng(random_device{}());

MoveCandidate Vanguard::search(const LetterBoard& board, const Board& bonusBoard,
                                   const TileRack& rack, const vector<char>& unseenBag,
                                   Dawg& dict, int timeLimitMs) {

    // Generate Candidate Moves (The "Broad" Search)
    vector<MoveCandidate> candidates = MoveGenerator::generate(board, rack, dict);

    // If few moves, just return the best static one (save time)
    if (candidates.empty()) return {};
    for (auto& cand : candidates) {
        cand.score = calculateScore(board, bonusBoard, cand);
    }

    // Sort by static score to prioritize "promising" moves
    sort(candidates.begin(), candidates.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
        return a.score > b.score;
    });

    if (candidates.size() == 1) return candidates[0];

    // Prune: Only simulate the top 10 moves
    int candidateCount = min((int)candidates.size(), 10);

    // Prepare Simulation Data
    // Convert Rack to Fast Histogram
    int myRackCounts[27] = {0};
    for (const Tile& t : rack) {
        if (t.letter == '?') myRackCounts[26]++;
        else if (isalpha(t.letter)) myRackCounts[toupper(t.letter) - 'A']++;
    }

    // Monte Carlo Simulation Loop
    vector<double> totalSpread(candidateCount, 0.0);
    vector<int> simCounts(candidateCount, 0);

    auto startTime = chrono::high_resolution_clock::now();
    int simulationsRun = 0;

    // We run simulations in parallel batches
    //unsigned int nThreads = thread::hardware_concurrency();
    unsigned int nThreads = 2;
    //if (nThreads == 0) nThreads = 2;

    // Time-bound loop
    while (true) {
        auto now = chrono::high_resolution_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - startTime).count();
        if (elapsed >= timeLimitMs) break;

        // Launch a batch of simulations (one for each candidate)
        vector<future<int>> futures;
        for (int i = 0; i < candidateCount; i++) {
            futures.push_back(async(launch::async, [i, &candidates, board, bonusBoard, &unseenBag, myRackCounts, &dict]() {

                // Copy Game State
                LetterBoard simBoard = board;
                vector<char> simBag = unseenBag; // Copy bag (slow-ish, but necessary)
                shuffle(simBag.begin(), simBag.end(), rng); // Randomize bag

                // Generate Opponent Rack from Bag
                int oppRackCounts[27] = {0};
                fillRack(oppRackCounts, simBag);

                // Apply MY candidate move
                int myRackClone[27];
                memcpy(myRackClone, myRackCounts, sizeof(myRackClone));

                // Apply MY Move
                // Note: candidates[i].score is already calculated!
                int currentScore = candidates[i].score;
                applyMove(simBoard, candidates[i], myRackClone);
                fillRack(myRackClone, simBag);

                // Refill my rack
                fillRack(myRackClone, simBag);

                // Play out the rest of the game
                // Start with OPPONENT turn (since I just moved)
                int futureSpread = playout(simBoard, bonusBoard, myRackClone, oppRackCounts, simBag, false, dict);

                return currentScore + futureSpread; // Total Spread
            }));
        }

        // Collect results
        for (int i = 0; i < candidateCount; i++) {
            int spread = futures[i].get();
            totalSpread[i] += spread;
            simCounts[i]++;
            simulationsRun++;
        }
    }

    // 4. Select Best Move based on Avg Spread
    int bestIdx = 0;
    double maxAvg = -99999.0;

    cout << "\n[Vanguard] Sims: " << simulationsRun << " | Time: " << timeLimitMs << "ms" << endl;

    for (int i = 0; i < candidateCount; i++) {
        double avg = totalSpread[i] / max(1, simCounts[i]);
        // Weighted Score: 70% Simulation, 30% Static Score (Stabilizer)
        double weighted = (avg * 0.7) + (candidates[i].score * 0.3);

        if (weighted > maxAvg) {
            maxAvg = weighted;
            bestIdx = i;
        }

        // Debug output for top moves
        if (i < 3) {
            cout << "  Move: " << candidates[i].word << " (" << candidates[i].score << "pts) -> Sim Spread: " << avg << endl;
        }
    }

    return candidates[bestIdx];
}

int Vanguard::playout(LetterBoard board, const Board& bonusBoard, int* myRackCounts, int* oppRackCounts,
                          vector<char> bag, bool myTurn, Dawg& dict) {

    int myScore = 0;
    int oppScore = 0;
    int passes = 0;
    int turns = 0;

    // Sim Depth: 6 turns is usually enough to see board control effects
    while (passes < 2 && !bag.empty() && turns < 2) {
        turns++;

        int* currentRack = myTurn ? myRackCounts : oppRackCounts;
        int& currentScore = myTurn ? myScore : oppScore;

        // 1. Generate Moves
        TileRack tempRack;
        for(int i=0; i<26; i++) {
            for(int k=0; k<currentRack[i]; k++) {
                tempRack.push_back({(char)('A'+i)});
            }
        }
        for(int k=0; k<currentRack[26]; k++) {
            tempRack.push_back({'?'});
        }

        vector<MoveCandidate> moves = MoveGenerator::generate(board, tempRack, dict);

        if (moves.empty()) {
            passes++;
        } else {
            passes = 0;

            MoveCandidate* bestMove = nullptr;
            int maxScore = -1;

            for (auto& m : moves) {
                int s = calculateScore(board, bonusBoard, m);
                if (s > maxScore) {
                    maxScore = s;
                    bestMove = &m;
                }
            }

            if (bestMove) {
                currentScore += maxScore;
                applyMove(board, *bestMove, currentRack);
                fillRack(currentRack, bag);
            }
        }

        myTurn = !myTurn;
    }

    // Return spread from MY perspective
    // If I called playout(), 'myScore' is accumulated relative to the start of playout.
    // Note: The caller (search) handles the perspective swap.
    return myScore - oppScore;
}

int Vanguard::calculateScore(const LetterBoard& board, const Board& bonusBoard, const MoveCandidate& move) {
    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMult = 1;
    int tilesPlaced = 0;

    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    for (int i = 0; move.word[i] != '\0'; i++) {
        char letter = move.word[i];
        int val = Heuristics::getTileValue(letter);

        bool isNew = (board[r][c] == ' ');
        if (isNew) {
            tilesPlaced++;
            CellType bonus = bonusBoard[r][c];
            if (bonus == CellType::DLS) val *= 2;
            else if (bonus == CellType::TLS) val *= 3;
            if (bonus == CellType::DWS) mainWordMult *= 2;
            else if (bonus == CellType::TWS) mainWordMult *= 3;
        }
        mainWordScore += val;

        // Cross Words
        if (isNew) {
            int pdr = move.isHorizontal ? 1 : 0;
            int pdc = move.isHorizontal ? 0 : 1;
            bool hasNeighbor = false;
            if (r-pdr>=0 && c-pdc>=0 && board[r-pdr][c-pdc]!=' ') hasNeighbor=true;
            if (r+pdr<15 && c+pdc<15 && board[r+pdr][c+pdc]!=' ') hasNeighbor=true;

            if (hasNeighbor) {
                int crossScore = 0;
                int crossMult = 1;
                int cr = r, cc = c;
                while(cr-pdr>=0 && cc-pdc>=0 && board[cr-pdr][cc-pdc]!=' ') { cr-=pdr; cc-=pdc; }
                while(cr<15 && cc<15) {
                    char l = board[cr][cc];
                    if (cr==r && cc==c) {
                        int cv = Heuristics::getTileValue(letter);
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
        r += dr;
        c += dc;
    }

    if (strlen(move.word) > 1) totalScore += (mainWordScore * mainWordMult);
    if (tilesPlaced == 7) totalScore += 50;

    // --- CRITICAL FIX: ADD LEAVE HEURISTIC ---
    // This allows the engine to value rack health during simulation.
    float leaveVal = 0.0f;
    for (int i=0; move.leave[i] != '\0'; i++) {
        leaveVal += Heuristics::getLeaveValue(move.leave[i]);
    }
    totalScore += (int)leaveVal;

    return totalScore;
}

int Vanguard::applyMove(LetterBoard& board, const MoveCandidate& move, int* rackCounts) {
    // 1. Place tiles on board
    // 2. Remove from rack
    // 3. Return score (already calculated in move.score, assuming MoveGenerator is accurate)

    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    // Crude application (sim only needs geometry, not perfect bonuses for future turns)
    // We trust MoveGenerator's score.

    for (int i=0; move.word[i] != '\0'; i++) {
        if (board[r][c] == ' ') {
            board[r][c] = move.word[i];

            // Remove from rack
            char letter = move.word[i];
            if (letter >= 'a' && letter <= 'z') { // Blank
                 if (rackCounts[26] > 0) rackCounts[26]--;
            } else {
                int idx = letter - 'A';
                if (rackCounts[idx] > 0) rackCounts[idx]--;
                else if (rackCounts[26] > 0) rackCounts[26]--; // Use blank if letter missing
            }
        }
        r += dr;
        c += dc;
    }
    return move.score;
}

void Vanguard::fillRack(int* rackCounts, vector<char>& bag) {
    // Count tiles in rack
    int count = 0;
    for(int i=0; i<27; i++) {
        count += rackCounts[i];
    }

    while (count < 7 && !bag.empty()) {
        char t = bag.back();
        bag.pop_back();

        if (t == '?') rackCounts[26]++;
        else rackCounts[t - 'A']++;
        count++;
    }
}

}