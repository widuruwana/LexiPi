#include "../../include/spectre/spy.h"
#include "../../include/spectre/move_generator.h"
#include "../../include/heuristics.h"
#include <algorithm>
#include <random>
#include <iostream>
#include <cstring>
#include <cmath>

using namespace std;

namespace spectre {

static thread_local mt19937 rng_spy(random_device{}());

Spy::Spy() { reset(); }

void Spy::reset() {
    tracker.reset();
    for(int i=0; i<27; ++i) beliefMatrix[i] = 1.0f;
}

void Spy::updateGroundTruth(const LetterBoard& board, const std::vector<char>& myRack) {
    tracker.reset();
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (board[r][c] != ' ') tracker.markSeen(board[r][c]);
        }
    }
    for (char c : myRack) tracker.markSeen(c);
}

void Spy::observeOpponentMove(const Move& move, int score, const LetterBoard& preMoveBoard, Dawg& dict) {
    if (move.type == MoveType::PLAY) {
        for(char c : move.word) {
            int idx = (c == '?') ? 26 : toupper(c) - 'A';
            if (idx >= 0 && idx < 27) beliefMatrix[idx] = 1.0f;
        }
        applyNegativeInference(preMoveBoard, score, dict);
        applyRackBalanceInference(move.word);
    }
    else if (move.type == MoveType::EXCHANGE) {
        int kept = 7 - move.exchangeLetters.length();
        if (kept > 0) applyRetentionBias(kept);
    }
    else if (move.type == MoveType::PASS) {
        for(int i=0; i<27; ++i) beliefMatrix[i] *= 0.9f;
    }
}

void Spy::applyNegativeInference(const LetterBoard& board, int opponentScore, Dawg& dict) {
    const char powerTiles[] = {'S', 'Z', 'Q', 'J', '?', 'X'};
    for (char target : powerTiles) {
        int idx = (target == '?') ? 26 : toupper(target) - 'A';
        if (tracker.getUnseenCount(target) == 0) continue;

        int ghostRack[27] = {0};
        ghostRack[idx]++;
        ghostRack['E' - 'A'] += 2; ghostRack['I' - 'A'] += 1; ghostRack['R' - 'A'] += 1;
        ghostRack['N' - 'A'] += 1; ghostRack['T' - 'A'] += 1;

        int maxGhostScore = 0;
        auto consumer = [&](MoveCandidate& cand, int* rack) -> bool {
            int approxScore = 0;
            for(int i=0; cand.word[i]!='\0'; ++i) approxScore += Heuristics::getTileValue(cand.word[i]);
            if (approxScore * 2 > maxGhostScore) maxGhostScore = approxScore * 2;
            return true;
        };

        MoveGenerator::generate_raw(board, ghostRack, dict, consumer);

        if (opponentScore < 20) beliefMatrix[idx] *= 0.8f;
    }
}

void Spy::applyRetentionBias(int keptCount) {
    beliefMatrix['S' - 'A'] *= 2.5f;
    beliefMatrix[26]        *= 2.5f;
    beliefMatrix['Q' - 'A'] *= 1.2f;
    beliefMatrix['U' - 'A'] *= 1.2f;
    beliefMatrix['V' - 'A'] *= 0.3f;
    beliefMatrix['W' - 'A'] *= 0.4f;
    beliefMatrix['J' - 'A'] *= 0.5f;
}

void Spy::applyRackBalanceInference(const std::string& wordPlayed) {
    int vowels = 0, consonants = 0;
    for (char c : wordPlayed) {
        if (strchr("AEIOU", toupper(c))) vowels++; else consonants++;
    }
    if (vowels > 0 && consonants == 0) {
        for(char c : "AEIOU") beliefMatrix[c - 'A'] *= 0.6f;
    } else if (consonants > 0 && vowels == 0) {
        for(char c : "BCDFGHJKLMNPQRSTVWXYZ") beliefMatrix[c - 'A'] *= 0.7f;
        for(char c : "AEIOU") beliefMatrix[c - 'A'] *= 1.5f;
    }
}

// [CRITICAL FIX] Optimized Generator (Zero Allocation via Scratch Buffers)
void Spy::generateWeightedRack(int* rackCounts, std::vector<char>& bagBuf, std::vector<double>& weightBuf) {
    memset(rackCounts, 0, 27 * sizeof(int));

    // 1. Reuse passed bag buffer
    tracker.populateRemainingTiles(bagBuf);
    if (bagBuf.empty()) return;

    // 2. Reuse passed weight buffer
    weightBuf.clear();
    // Ensure capacity only if bag grew (unlikely in game flow, but safe)
    if (weightBuf.capacity() < bagBuf.size()) weightBuf.reserve(bagBuf.size());

    double totalWeight = 0.0;
    for (char c : bagBuf) {
        int idx = (c == '?') ? 26 : toupper(c) - 'A';
        double w = beliefMatrix[idx];
        if (w < 0.01) w = 0.01;
        if (w > 100.0) w = 100.0;
        weightBuf.push_back(w);
        totalWeight += w;
    }

    int tilesNeeded = (bagBuf.size() < 7) ? bagBuf.size() : 7;

    // 3. Manual Weighted Sampling (Linear Scan) to avoid std::discrete_distribution allocation
    for (int i = 0; i < tilesNeeded; ++i) {
        if (totalWeight <= 0.0001) break;

        std::uniform_real_distribution<double> dist(0.0, totalWeight);
        double r = dist(rng_spy);

        int pickedIndex = -1;
        double currentSum = 0.0;

        for (size_t k = 0; k < weightBuf.size(); ++k) {
            currentSum += weightBuf[k];
            if (currentSum >= r) {
                pickedIndex = k;
                break;
            }
        }

        if (pickedIndex == -1) pickedIndex = weightBuf.size() - 1;

        char c = bagBuf[pickedIndex];
        if (c == '?') rackCounts[26]++;
        else if (isalpha(c)) rackCounts[toupper(c) - 'A']++;

        // Remove from pool logically
        totalWeight -= weightBuf[pickedIndex];
        weightBuf[pickedIndex] = 0.0;
    }
}

// Legacy stub
void Spy::generateWeightedRack(int* rackCounts) {
    std::vector<char> b; b.reserve(100);
    std::vector<double> w; w.reserve(100);
    generateWeightedRack(rackCounts, b, w);
}

}