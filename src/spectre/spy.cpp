#include "../../include/spectre/spy.h"
#include "../../include/engine/mechanics.h"
#include <random>
#include <algorithm>
#include <iostream>

namespace spectre {

    Spy::Spy() {
        particles.reserve(PARTICLE_COUNT);
        unseen_pool.reserve(100);
    }

    void Spy::updateGroundTruth(const LetterBoard& board, const TileRack& myRack, const std::vector<Tile>& bag) {
        int standard_counts[27] = {
            9, 2, 2, 4, 12, 2, 3, 2, 9, 1, 1, 4, 2, 6, 8, 2, 1, 6, 4, 6, 4, 2, 2, 1, 2, 1, 2
        };
        for (int r = 0; r < 15; r++)
            for (int c = 0; c < 15; c++) {
                char ch = board[r][c];
                if (ch != ' ') {
                    if (ch >= 'a' && ch <= 'z') standard_counts[26]--;
                    else standard_counts[ch - 'A']--;
                }
            }
        for (const auto& t : myRack) {
            if (t.letter == '?') standard_counts[26]--;
            else if (t.letter >= 'A' && t.letter <= 'Z') standard_counts[t.letter - 'A']--;
        }
        unseen_pool.clear();
        for (int i = 0; i < 26; i++)
            for (int k = 0; k < standard_counts[i]; k++)
                unseen_pool.push_back((char)('A' + i));
        for (int k = 0; k < standard_counts[26]; k++)
            unseen_pool.push_back('?');
        if (particles.empty()) initialize_particles();
    }

    void Spy::initialize_particles() {
        static thread_local std::mt19937 rng(std::random_device{}());
        particles.clear();
        int target_rack_size = std::min((int)unseen_pool.size(), 7);
        for (int i = 0; i < PARTICLE_COUNT; i++) {
            Particle p;
            p.rack_size = target_rack_size;
            p.weight = 1.0 / PARTICLE_COUNT;
            std::vector<char> pool_copy = unseen_pool;
            std::shuffle(pool_copy.begin(), pool_copy.end(), rng);
            for (int j = 0; j < target_rack_size; j++) p.rack[j] = pool_copy[j];
            particles.push_back(p);
        }
    }

    void Spy::observeOpponentMove(const Move& move, const LetterBoard& preMoveBoard) {
        if (move.word[0] == '\0' || particles.empty()) return;
        std::string played_tiles = "";
        int r = move.row, c = move.col;
        int dr = move.horizontal ? 0 : 1;
        int dc = move.horizontal ? 1 : 0;
        for (int i = 0; move.word[i] != '\0'; i++) {
            if (r < 15 && c < 15 && preMoveBoard[r][c] == ' ')
                played_tiles += move.word[i];
            r += dr; c += dc;
        }
        double total_weight = 0.0;
        for (auto& p : particles) {
            if (!particle_contains_tiles(p, played_tiles)) p.weight = 0.0;
            else total_weight += p.weight;
        }
        if (total_weight > 0.0) {
            for (auto& p : particles) p.weight /= total_weight;
            resample_particles();
        } else {
            particles.clear();
            initialize_particles();
        }
    }

    void Spy::resample_particles() {
        if (particles.empty()) return;
        std::vector<Particle> new_particles;
        new_particles.reserve(PARTICLE_COUNT);
        double step = 1.0 / PARTICLE_COUNT;
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<double> dist(0.0, step);
        double r = dist(rng);
        double c = particles[0].weight;
        int i = 0;
        for (int m = 0; m < PARTICLE_COUNT; m++) {
            double u = r + (m * step);
            while (u > c && i < PARTICLE_COUNT - 1) { i++; c += particles[i].weight; }
            Particle cloned = particles[i];
            cloned.weight = 1.0 / PARTICLE_COUNT;
            new_particles.push_back(cloned);
        }
        particles = std::move(new_particles);
    }

    // Interface method: sampleOpponentRack (was sampleParticleRack)
    std::vector<char> Spy::sampleOpponentRack() const {
        if (particles.empty()) return {};
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, particles.size() - 1);
        const Particle& p = particles[dist(rng)];
        std::vector<char> sampled;
        sampled.reserve(p.rack_size);
        for(int i = 0; i < p.rack_size; i++) sampled.push_back(p.rack[i]);
        return sampled;
    }

    const std::vector<char>& Spy::getUnseenPool() const { return unseen_pool; }

    bool Spy::particle_contains_tiles(const Particle& p, const std::string& required_tiles) const {
        int p_counts[27] = {0};
        for (int i = 0; i < p.rack_size; i++) {
            char ch = p.rack[i];
            if (ch == '?') p_counts[26]++;
            else if (ch >= 'A' && ch <= 'Z') p_counts[ch - 'A']++;
        }
        for (char ch : required_tiles) {
            bool isBlank = (ch >= 'a' && ch <= 'z');
            if (isBlank) {
                if (p_counts[26] > 0) p_counts[26]--;
                else return false;
            } else {
                int idx = ch - 'A';
                if (idx >= 0 && idx < 26 && p_counts[idx] > 0) p_counts[idx]--;
                else if (p_counts[26] > 0) p_counts[26]--;
                else return false;
            }
        }
        return true;
    }

}
