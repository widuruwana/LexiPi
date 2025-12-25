#pragma once

#include <random>
#include <cstdint>

// Deterministic RNG with seeding
class Random {
public:
    static Random& getInstance();
    
    // Seed the RNG (call once at game start)
    void seed(uint64_t seed);
    
    // Get current seed
    uint64_t getSeed() const { return currentSeed; }
    
    // Random integer in range [min, max]
    int nextInt(int min, int max);
    
    // Random float in range [0.0, 1.0)
    double nextDouble();
    
    // Random boolean
    bool nextBool();
    
    // Shuffle a vector (deterministic)
    template<typename T>
    void shuffle(std::vector<T>& vec) {
        std::shuffle(vec.begin(), vec.end(), rng);
    }
    
    // Prevent copying
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;

private:
    Random() = default;
    ~Random() = default;
    
    std::mt19937_64 rng;
    uint64_t currentSeed = 0;
};
