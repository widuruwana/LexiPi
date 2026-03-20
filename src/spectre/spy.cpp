#include "../../include/spectre/spy.h"
#include "../../include/tile_tracker.h"
#include "../../include/kernel/move_generator.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/dictionary.h"
#include "../../include/spectre/logger.h"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <map>
#include <random>
#include <cstring> // For memset

using namespace std;

namespace spectre {

Spy::Spy() {
    particles.resize(PARTICLE_COUNT);
    for(auto& p : particles) p.weight = 1.0;
}

// Helper to generate a cache key from a rack
std::string getRackKey(const std::vector<char>& rack) {
    std::string s(rack.begin(), rack.end());
    std::sort(s.begin(), s.end());
    return s;
}

const std::vector<char>& Spy::getUnseenPool() const {
    return unseenPool;
}

void Spy::observeOpponentMove(const Move& move, const LetterBoard& preMoveBoard) {
    if (unseenPool.empty()) return;

    {
        ScopedLogger log;
        std::cout << "[SPY] Observing Move: " << move.word << std::endl;
    }

    // 1. DEDUCE PLAYED TILES
    std::vector<char> tilesPlayed;
    int r = move.row; int c = move.col;
    int dr = move.horizontal ? 0 : 1;
    int dc = move.horizontal ? 1 : 0;

    for (char letter : move.word) {
        if (preMoveBoard[r][c] == ' ') {
            tilesPlayed.push_back(letter);
        }
        r += dr; c += dc;
    }

    // 2. SCORE THE MOVE
    Board bonusBoard = createBoard();
    kernel::MoveCandidate mc;
    mc.row = move.row;
    mc.col = move.col;
    mc.isHorizontal = move.horizontal;

    strncpy(mc.word, move.word, 16);
    mc.word[15] = '\0'; // Safety null-termination

    int actualScore = Mechanics::calculateTrueScore(mc, preMoveBoard, bonusBoard);

    std::cout << "[SPY] Observing Move: " << move.word
              << " | Score: " << actualScore
              << " | Tiles Required: ";
    for(char c : tilesPlayed) std::cout << c;
    std::cout << std::endl;

    // 3. UPDATE PARTICLES (With Caching)
    double totalWeight = 0.0;
    std::map<std::string, int> scoreCache; // <--- THE CACHE

    int hardFilterPassCount = 0;
    int perfectRationalityCount = 0;

    for (auto& p : particles) {
        // A. HARD FILTER
        std::vector<char> rackCopy = p.rack;
        bool possible = true;
        for (char t : tilesPlayed) {
            auto it = std::find(rackCopy.begin(), rackCopy.end(), t);
            if (it != rackCopy.end()) *it = ' ';
            else {
                auto bit = std::find(rackCopy.begin(), rackCopy.end(), '?');
                if (bit != rackCopy.end()) *bit = ' ';
                else { possible = false; break; }
            }
        }

        if (!possible) {
            p.weight = 0.0;
            continue;
        }

        hardFilterPassCount++;

        // B. SOFT FILTER (Optimized)
        // Check cache first
        std::string key = getRackKey(p.rack);
        int bestPossible = 0;

        if (scoreCache.count(key)) {
            bestPossible = scoreCache[key];
        } else {
            // Expensive calculation happens only once per unique rack
            // NOW OPTIMIZED with generate_raw
            bestPossible = findBestPossibleScore(p.rack, preMoveBoard);
            scoreCache[key] = bestPossible;
        }

        // [FIX] ROBUST RATIONALITY LOGIC
        double rationality = 0.0;

        // CASE 1: Opponent played BETTER than we thought possible (Genius or Generator mismatch)
        // This is STRONG evidence they have this rack.
        if (actualScore > bestPossible) {
            rationality = 1.0;
        }
        // CASE 2: Opponent played optimally or near-optimally
        else {
            int delta = bestPossible - actualScore;
            if (delta == 0) rationality = 1.0;
            else if (delta <= 5) rationality = 0.8;  // Relaxed from 0.9
            else if (delta <= 15) rationality = 0.4; // Relaxed from 0.5
            else if (delta <= 30) rationality = 0.1;
            else rationality = 0.01; // Blunder
        }

        if (rationality > 0.8) perfectRationalityCount++;

        p.weight *= rationality;
        totalWeight += p.weight;
    }

    {
        ScopedLogger log;
        std::cout << "[SPY] Particle Report:"
                  << "\n\t Total Particles: " << particles.size()
                  << "\n\t Passed Hard Filter: " << hardFilterPassCount
                  << "\n\t Perfect Rationality: " << perfectRationalityCount
                  << "\n\t Total Mass: " << totalWeight
                  << std::endl;
    }

    // 4. RESAMPLE OR REBOOT
    if (totalWeight < 0.0001) {
        ScopedLogger log;
        std::cout << "[SPY] Model Mismatch (TotalMass=" << totalWeight << "). Rebooting particles..." << std::endl;

        // Reboot Logic: Assume they definitely had the tiles they played
        // Fill the rest randomly.
        static thread_local std::mt19937 rng(std::random_device{}());
        for(auto& p : particles) {
            p.rack.clear();
            p.weight = 1.0;
            for(char c : tilesPlayed) p.rack.push_back(c);

            std::vector<char> pool = unseenPool;
            std::shuffle(pool.begin(), pool.end(), rng);
            while(p.rack.size() < 7 && !pool.empty()) {
                p.rack.push_back(pool.back());
                pool.pop_back();
            }
        }
    } else {
        resampleParticles(totalWeight);
    }

    // 5. TRANSITION
    for(auto& p : particles) {
        for(char t : tilesPlayed) {
            auto it = std::find(p.rack.begin(), p.rack.end(), t);
            if(it != p.rack.end()) p.rack.erase(it);
            else {
                auto bit = std::find(p.rack.begin(), p.rack.end(), '?');
                if(bit != p.rack.end()) p.rack.erase(bit);
            }
        }
    }
}

// OPTIMIZED: Uses generate_raw (Stack Only) - 100x Faster
int Spy::findBestPossibleScore(const std::vector<char>& rack, const LetterBoard& board) {
    // 1. Convert to Rack Counts (Stack Array)
    int rackCounts[27] = {0};
    for(char c : rack) {
        if (c == '?') rackCounts[26]++;
        else if (c >= 'A' && c <= 'Z') rackCounts[c - 'A']++;
        else if (c >= 'a' && c <= 'z') rackCounts[c - 'a']++;
    }

    int maxScore = 0;
    Board bonusBoard = createBoard();

    // 2. Lambda Consumer (No Vectors!)
    auto scoringConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
        int s = Mechanics::calculateTrueScore(cand, board, bonusBoard);
        if (s > maxScore) maxScore = s;
        return true;
    };

    // 3. Raw Generation (Fastest Path)
    kernel::MoveGenerator::generate_raw(board, rackCounts, gDictionary, scoringConsumer);

    return maxScore;
}

void Spy::updateGroundTruth(const LetterBoard& board, const TileRack& myRack, const TileBag& bag) {
    // 1. Rebuild Unseen Pool
    unseenPool.clear();
    TileTracker tracker;
    for(int r=0; r<15; r++) for(int c=0; c<15; c++)
        if(board[r][c] != ' ') tracker.markSeen(board[r][c]);
    for(const auto& t : myRack) tracker.markSeen(t.letter);

    string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?";
    for (char c : alphabet) {
        int count = tracker.getUnseenCount(c);
        for (int k=0; k<count; k++) unseenPool.push_back(c);
    }

    {
        ScopedLogger log;
        std::cout << "[SPY] Ground Truth Updated. Unseen Pool Size: " << unseenPool.size() << std::endl;
    }

    // 2. REFILL PARTICLES (OPTIMIZED SCRATCH BUFFER)
    static thread_local std::mt19937 rng(std::random_device{}());

    if(particles.empty()) { initParticles(); return; }

    // Optimization: Create ONE scratch buffer for the pool
    std::vector<char> scratchPool;
    scratchPool.reserve(100);

    for (auto& p : particles) {
        if (p.rack.size() >= 7) continue;

        // Reset scratch pool from the master source
        scratchPool = unseenPool;

        // "Subtract" existing rack tiles from scratch pool
        for(char existing : p.rack) {
            for(size_t i = 0; i < scratchPool.size(); i++) {
                if (scratchPool[i] == existing) {
                    scratchPool[i] = scratchPool.back();
                    scratchPool.pop_back();
                    break;
                }
            }
        }

        // Fill remaining slots
        while(p.rack.size() < 7 && !scratchPool.empty()) {
            std::uniform_int_distribution<int> dist(0, scratchPool.size() - 1);
            int idx = dist(rng);

            p.rack.push_back(scratchPool[idx]);

            // Remove the picked tile (Swap & Pop)
            scratchPool[idx] = scratchPool.back();
            scratchPool.pop_back();
        }
    }
}

void Spy::initParticles() {
    static thread_local std::mt19937 rng(std::random_device{}());
    for(int i=0; i<PARTICLE_COUNT; i++) {
        particles[i].rack.clear();
        particles[i].weight = 1.0;

        if (unseenPool.empty()) continue;

        std::vector<char> pool = unseenPool;
        std::shuffle(pool.begin(), pool.end(), rng);

        int draw = std::min((int)pool.size(), 7);
        for(int k=0; k<draw; k++) particles[i].rack.push_back(pool[k]);
    }
}

void Spy::resampleParticles(double totalWeight) {
    std::vector<Particle> newParticles;
    newParticles.reserve(PARTICLE_COUNT);

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, totalWeight);

    for (int i = 0; i < PARTICLE_COUNT; i++) {
        double r = dist(rng);
        double currentSum = 0.0;

        for (const auto& p : particles) {
            currentSum += p.weight;
            if (currentSum >= r) {
                newParticles.push_back(p);
                break;
            }
        }
    }

    // Normalize weights after resampling
    for(auto& p : newParticles) p.weight = 1.0;
    particles = newParticles;
}

std::vector<char> Spy::generateWeightedRack() const {
    if (particles.empty()) return {};
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, particles.size() - 1);
    return particles[dist(rng)].rack;
}

}