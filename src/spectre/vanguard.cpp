#include "../../include/spectre/vanguard.h"
#include "../../include/spectre/spy.h"
#include "../../include/heuristics.h"
#include "../../include/spectre/move_generator.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <cstring>
#include <iostream>
#include <array>
#include <future>
#include <thread>
#include <cctype>

using namespace std;

namespace spectre {

thread_local mt19937 rng(random_device{}());
static thread_local Spy spy;

struct FastBag {
    char tiles[128]; int count;
    FastBag() : count(0) {}
    FastBag(const vector<char>& source) {
        count = 0;
        for (char c : source) { if (count < 128) tiles[count++] = c; }
    }
    void shuffleBag() {
        for (int i = count - 1; i > 0; i--) {
            uniform_int_distribution<int> dist(0, i);
            int j = dist(rng);
            char temp = tiles[i]; tiles[i] = tiles[j]; tiles[j] = temp;
        }
    }
    bool pop(char& out) {
        if (count == 0) return false;
        out = tiles[--count];
        return true;
    }
};

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

static int computeFullScore(const LetterBoard& board, const Board& bonusBoard, const MoveCandidate& move) {
    int totalScore = 0; int mainWordScore = 0; int mainWordMult = 1; int tilesPlaced = 0;
    int r = move.row; int c = move.col; int dr = move.isHorizontal ? 0 : 1; int dc = move.isHorizontal ? 1 : 0;

    for (int i = 0; move.word[i] != '\0'; i++) {
        char letter = move.word[i];
        int val = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);
        bool isNew = (board[r][c] == ' ');
        if (isNew) {
            tilesPlaced++;
            CellType bonus = bonusBoard[r][c];
            if (bonus == CellType::DLS) val *= 2; else if (bonus == CellType::TLS) val *= 3;
            if (bonus == CellType::DWS) mainWordMult *= 2; else if (bonus == CellType::TWS) mainWordMult *= 3;
        }
        mainWordScore += val;
        if (isNew) {
            int pdr = move.isHorizontal ? 1 : 0; int pdc = move.isHorizontal ? 0 : 1;
            bool hasNeighbor = false;
            if (r - pdr >= 0 && c - pdc >= 0 && board[r - pdr][c - pdc] != ' ') hasNeighbor = true;
            if (r + pdr < 15 && c + pdc < 15 && board[r + pdr][c + pdc] != ' ') hasNeighbor = true;
            if (hasNeighbor) {
                int crossScore = 0; int crossMult = 1;
                int cr = r, cc = c;
                while(cr - pdr >= 0 && cc - pdc >= 0 && board[cr - pdr][cc - pdc] != ' ') { cr -= pdr; cc -= pdc; }
                while(cr < 15 && cc < 15) {
                    char l;
                    if (cr == r && cc == c) {
                        l = letter; int cv = (islower(l)) ? 0 : Heuristics::getTileValue(l);
                        CellType cb = bonusBoard[cr][cc];
                        if (cb == CellType::DLS) cv *= 2; else if (cb == CellType::TLS) cv *= 3;
                        if (cb == CellType::DWS) crossMult *= 2; else if (cb == CellType::TWS) crossMult *= 3;
                        crossScore += cv;
                    } else {
                        l = board[cr][cc]; if (l == ' ') break; crossScore += Heuristics::getTileValue(l);
                    }
                    cr += pdr; cc += pdc;
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

static float calculateRackLeave(const LetterBoard& board, const int* rackCountsInput, const MoveCandidate& move) {
    int rackCounts[27]; memcpy(rackCounts, rackCountsInput, 27 * sizeof(int));
    int r = move.row; int c = move.col; int dr = move.isHorizontal ? 0 : 1; int dc = move.isHorizontal ? 1 : 0;
    for(int i = 0; move.word[i] != '\0'; ++i) {
        if (board[r][c] == ' ') {
            char l = move.word[i];
            if (l >= 'a' && l <= 'z') { if(rackCounts[26] > 0) rackCounts[26]--; }
            else { int idx = toupper(l) - 'A'; if(rackCounts[idx] > 0) rackCounts[idx]--; else if(rackCounts[26] > 0) rackCounts[26]--; }
        }
        r += dr; c += dc;
    }
    float leaveVal = 0.0f;
    for(int i = 0; i < 26; i++) { if (rackCounts[i] > 0) leaveVal += Heuristics::getLeaveValue((char)('A' + i)); }
    if (rackCounts[26] > 0) leaveVal += (Heuristics::getLeaveValue('?') * rackCounts[26]);
    return leaveVal;
}

static float calculateRackLeave(const LetterBoard& board, const TileRack& originalRack, const MoveCandidate& move) {
    int rackCounts[27] = {0};
    for(const auto& t : originalRack) {
        if(t.letter == '?') rackCounts[26]++; else if(isalpha(t.letter)) rackCounts[toupper(t.letter) - 'A']++;
    }
    return calculateRackLeave(board, rackCounts, move);
}

static int fastPlayout(LetterBoard currentBoard, const Board& bonusBoard, int* myRackCounts, int* oppRackCounts, FastBag bag, bool myTurn, Dawg& dict) {
    int myScore = 0; int oppScore = 0;
    int mySimRack[27]; int oppSimRack[27];
    memcpy(mySimRack, myRackCounts, 27 * sizeof(int));
    memcpy(oppSimRack, oppRackCounts, 27 * sizeof(int));

    if (myTurn) fastFillRack(mySimRack, bag); else fastFillRack(oppSimRack, bag);

    int maxTurns = (bag.count <= 9) ? 12 : 2;

    for (int turn = 0; turn < maxTurns; turn++) {
        int* currentRack = myTurn ? mySimRack : oppSimRack;
        int* currentScorePtr = myTurn ? &myScore : &oppScore;
        fastFillRack(currentRack, bag);

        bool emptyRack = true;
        for(int i=0; i<27; i++) if(currentRack[i] > 0) { emptyRack = false; break; }
        if (emptyRack) {
            int* otherRack = myTurn ? oppSimRack : mySimRack;
            int penalty = 0;
            for(int i=0; i<26; i++) penalty += otherRack[i] * Heuristics::getTileValue('A'+i);
            *currentScorePtr += (penalty * 2);
            break;
        }

        MoveCandidate bestMove; bestMove.score = -1;
        float bestMetric = -99999.0f;
        bool found = false;

        auto consumer = [&](MoveCandidate& cand, int* rack) -> bool {
            int realScore = computeFullScore(currentBoard, bonusBoard, cand);
            float metric = (float)realScore;
            if (myTurn) metric += calculateRackLeave(currentBoard, rack, cand); // Smart policy for me

            if (metric > bestMetric) {
                bestMove = cand; bestMove.score = (short)realScore;
                bestMetric = metric; found = true;
            }
            return true;
        };

        MoveGenerator::generate_raw(currentBoard, currentRack, dict, consumer);

        if (found) {
            *currentScorePtr += bestMove.score;
            int r = bestMove.row; int c = bestMove.col;
            int dr = bestMove.isHorizontal ? 0 : 1; int dc = bestMove.isHorizontal ? 1 : 0;
            for(int i=0; bestMove.word[i]!='\0'; ++i) {
                if (currentBoard[r][c] == ' ') {
                    currentBoard[r][c] = toupper(bestMove.word[i]);
                    char l = bestMove.word[i];
                    if (l >= 'a' && l <= 'z') { if(currentRack[26] > 0) currentRack[26]--; }
                    else { int idx = l - 'A'; if(currentRack[idx] > 0) currentRack[idx]--; else if(currentRack[26] > 0) currentRack[26]--; }
                }
                r += dr; c += dc;
            }
        }
        myTurn = !myTurn;
    }
    return myScore - oppScore;
}

MoveCandidate Vanguard::search(const LetterBoard& board, const Board& bonusBoard,
                               const TileRack& rack, const vector<char>& unseenBag,
                               Dawg& dict, int timeLimitMs) {

    vector<char> myRackVec;
    for (const auto& t : rack) myRackVec.push_back(t.letter);
    spy.updateGroundTruth(board, myRackVec);

    vector<MoveCandidate> candidates = MoveGenerator::generate(board, rack, dict, false);
    if (candidates.empty()) return {};

    for (auto& cand : candidates) {
        int boardPoints = computeFullScore(board, bonusBoard, cand);
        float leavePoints = calculateRackLeave(board, rack, cand);
        cand.score = boardPoints + (short)(leavePoints * 0.2f);
    }

    sort(candidates.begin(), candidates.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
        return a.score > b.score;
    });

    int candidateCount = min((int)candidates.size(), 8);
    unsigned int nThreads = std::thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 4;

    vector<future<void>> workers;
    struct ThreadResult { vector<long long> scores; vector<int> counts; };
    vector<ThreadResult> threadResults(nThreads, {vector<long long>(candidateCount, 0), vector<int>(candidateCount, 0)});

    FastBag templateBag(unseenBag);
    auto startTime = chrono::high_resolution_clock::now();
    int adjustedTimeLimit = timeLimitMs;
    if (unseenBag.size() <= 9) adjustedTimeLimit = max(2000, timeLimitMs * 2);

    for (unsigned int t = 0; t < nThreads; t++) {
        workers.push_back(async(launch::async,
            [&, t]() {
                // [CRITICAL FIX] Allocate scratch buffers ONCE per thread on the stack/TLS.
                // Reuse these buffers for every simulation iteration to prevent heap thrashing.
                std::vector<char> bagBuf; bagBuf.reserve(100);
                std::vector<double> weightBuf; weightBuf.reserve(100);

                FastBag localBag;
                while (true) {
                    auto now = chrono::high_resolution_clock::now();
                    if (chrono::duration_cast<chrono::milliseconds>(now - startTime).count() >= adjustedTimeLimit) break;

                    int k = rng() % candidateCount;
                    localBag = templateBag;
                    localBag.shuffleBag();

                    int oppRackCounts[27] = {0};
                    // Pass scratch buffers to zero-alloc generator
                    spy.generateWeightedRack(oppRackCounts, bagBuf, weightBuf);

                    LetterBoard nextBoard = board;
                    const auto& move = candidates[k];
                    int realPoints = computeFullScore(board, bonusBoard, move);

                    int myResidualRack[27] = {0};
                    for(const auto& t : rack) {
                        if(t.letter == '?') myResidualRack[26]++;
                        else if(isalpha(t.letter)) myResidualRack[toupper(t.letter) - 'A']++;
                    }
                    int r = move.row; int c = move.col;
                    int dr = move.isHorizontal ? 0 : 1; int dc = move.isHorizontal ? 1 : 0;
                    for(int i=0; move.word[i]!='\0'; ++i) {
                        if(nextBoard[r][c] == ' ') {
                            nextBoard[r][c] = toupper(move.word[i]);
                            char l = move.word[i];
                            if(l >= 'a' && l <= 'z') { if(myResidualRack[26]>0) myResidualRack[26]--; }
                            else { int idx = toupper(l) - 'A'; if(myResidualRack[idx]>0) myResidualRack[idx]--; else if(myResidualRack[26]>0) myResidualRack[26]--; }
                        }
                        r += dr; c += dc;
                    }

                    int outcome = fastPlayout(nextBoard, bonusBoard, myResidualRack, oppRackCounts, localBag, false, dict);
                    threadResults[t].scores[k] += (realPoints + outcome);
                    threadResults[t].counts[k]++;
                }
            }
        ));
    }

    for (auto& f : workers) f.wait();

    int bestIdx = 0;
    double maxVal = -999999.0;
    for (int i = 0; i < candidateCount; i++) {
        long long totalScore = 0; int totalSims = 0;
        for (unsigned int t = 0; t < nThreads; t++) {
            totalScore += threadResults[t].scores[i];
            totalSims += threadResults[t].counts[i];
        }
        if (totalSims > 0) {
            double avg = (double)totalScore / totalSims;
            if (avg > maxVal) { maxVal = avg; bestIdx = i; }
        }
    }
    return candidates[bestIdx];
}

int Vanguard::calculateScore(const LetterBoard& board, const Board& bonusBoard, const MoveCandidate& move) {
    return computeFullScore(board, bonusBoard, move);
}

int Vanguard::applyMove(LetterBoard& board, const MoveCandidate& move, int* rackCounts) { return 0; }
void Vanguard::fillRack(int* rackCounts, vector<char>& bag) {}

}