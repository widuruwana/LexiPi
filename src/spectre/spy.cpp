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

void Spy::observeOpponentMove(const Move& move, const LetterBoard& preMoveBoard, const Board& bonusBoard) {
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
    kernel::MoveCandidate mc;
    mc.row = move.row;
    mc.col = move.col;
    mc.isHorizontal = move.horizontal;
    strncpy(mc.word, move.word, 16);
    mc.word[15] = '\0';

    int actualScore = Mechanics::calculateTrueScore(mc, preMoveBoard, bonusBoard);

    // 3. REVERSE INFERENCE (Seeding the Knowns)
    // We force every particle to hold the tiles that were just played,
    // preserving as much of their historical "unplayed" rack as possible.
    for (auto& p : particles) {
        std::vector<char> required = tilesPlayed;
        std::vector<char> currentRack = p.rack;
        std::vector<char> missing;

        // Find what the particle is missing
        for (char t : required) {
            auto it = std::find(currentRack.begin(), currentRack.end(), t);
            if (it != currentRack.end()) {
                currentRack.erase(it); // Tile found
            } else {
                auto bit = std::find(currentRack.begin(), currentRack.end(), '?');
                if (bit != currentRack.end()) {
                    currentRack.erase(bit); // Blank found
                } else {
                    missing.push_back(t); // Tile missing
                }
            }
        }

        // currentRack now holds the unplayed "leave" tiles.
        // We must discard random leaves to make room for the forced missing tiles.
        static thread_local std::mt19937 rng(std::random_device{}());
        std::shuffle(currentRack.begin(), currentRack.end(), rng);

        int missingCount = missing.size();
        for(int i = 0; i < missingCount; i++) {
            if (!currentRack.empty()) currentRack.pop_back();
        }

        // Rebuild the particle: Required Tiles + Surviving Leaves
        p.rack.clear();
        for(char t : tilesPlayed) p.rack.push_back(t);
        for(char t : currentRack) p.rack.push_back(t);
    }

    // 4. SOFT FILTER (Rationality Check)
    double totalWeight = 0.0;
    std::map<std::string, int> scoreCache;
    int perfectRationalityCount = 0;

    for (auto& p : particles) {
        std::string key = getRackKey(p.rack);
        int bestPossible = 0;

        if (scoreCache.count(key)) {
            bestPossible = scoreCache[key];
        } else {
            bestPossible = findBestPossibleScore(p.rack, preMoveBoard);
            scoreCache[key] = bestPossible;
        }

        // Evaluate how rational the opponent's move was given this hypothetical rack
        double rationality = 0.0;
        if (actualScore > bestPossible) {
            rationality = 1.0;
        } else {
            int delta = bestPossible - actualScore;
            if (delta == 0) rationality = 1.0;
            else if (delta <= 5) rationality = 0.8;
            else if (delta <= 15) rationality = 0.4;
            else if (delta <= 30) rationality = 0.1;
            else rationality = 0.01;
        }

        if (rationality > 0.8) perfectRationalityCount++;

        p.weight *= rationality;
        totalWeight += p.weight;
    }

    {
        ScopedLogger log;
        std::cout << "[SPY] Particle Report (Reverse Inference):"
                  << "\n\t Total Particles: " << particles.size()
                  << "\n\t Perfect Rationality: " << perfectRationalityCount
                  << "\n\t Total Mass: " << totalWeight
                  << std::endl;
    }

    // 5. RESAMPLE & TRANSITION
    if (totalWeight > 0.0) {
        resample_particles();
    } else {
        // Extreme edge case fallback (should rarely happen now)
        for (auto& p : particles) p.weight = 1.0 / particles.size();
    }

    // Remove the played tiles from the newly validated racks
    // The remaining tiles are what the Spy believes the opponent "held back"
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

void Spy::resample_particles() {
    if (particles.empty()) return;

    int num_particles = particles.size();
    std::vector<Particle> new_particles;
    new_particles.reserve(num_particles);

    // 1. Calculate Total Weight
    double total_weight = 0.0;
    for (const auto& p : particles) {
        total_weight += p.weight;
    }

    // Failsafe: If all particles were crushed to 0 (e.g., impossible board state),
    // we re-initialize uniformly rather than crashing.
    if (total_weight <= 0.0) {
        for (auto& p : particles) {
            p.weight = 1.0 / num_particles;
        }
        return;
    }

    // 2. Systematic Resampling Setup
    double step = total_weight / num_particles;

    // Generate a single random starting point in the first interval
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, step);
    double r = dist(rng);

    // 3. The Stratified Sweep (O(N) Time Complexity)
    double c = particles[0].weight;
    int i = 0;

    for (int m = 0; m < num_particles; m++) {
        double u = r + (m * step);

        // Advance the pointer until the cumulative weight exceeds the current threshold
        while (u > c && i < num_particles - 1) {
            i++;
            c += particles[i].weight;
        }

        // Clone the selected particle and reset its weight
        Particle cloned_particle = particles[i];
        cloned_particle.weight = 1.0 / num_particles; // Normalize new weights
        new_particles.push_back(std::move(cloned_particle));
    }

    // 4. Commit the new generation
    particles = std::move(new_particles);
}

std::vector<char> Spy::generateWeightedRack() const {
    if (particles.empty()) return {};
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, particles.size() - 1);
    return particles[dist(rng)].rack;
}

TileRack Spy::inferOpponentRack() const {
    std::vector<char> inferredOppChars;
    size_t poolSize = unseenPool.size();

    // Bypass particle filter at endgame thresholds
    if (poolSize < 9) {
        if (poolSize <= 7) {
            inferredOppChars = unseenPool; // Perfect Info
        } else {
            inferredOppChars = unseenPool; // Near-Perfect Info
            static thread_local std::mt19937 rng(std::random_device{}());
            std::shuffle(inferredOppChars.begin(), inferredOppChars.end(), rng);
            if (inferredOppChars.size() > 7) inferredOppChars.resize(7);
        }
    } else {
        // Deep Midgame
        inferredOppChars = generateWeightedRack();
    }

    TileRack oppRack;
    for(char c : inferredOppChars) {
        Tile t;
        t.letter = c;
        t.points = Mechanics::getPointValue(c);
        oppRack.push_back(t);
    }

    return oppRack;
}

}