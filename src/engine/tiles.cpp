#include "../../include/engine/tiles.h"
#include "../../include/engine/rack.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <ctime>
#include <thread>

using namespace std;

TileBag createStandardTileBag() {
    TileBag bag;
    bag.reserve(100); // Optimization: Reserve memory to avoid reallocations
    refillStandardTileBag(bag);
    return bag;
}

// NEW: Zero-Allocation Refill
void refillStandardTileBag(TileBag& bag) {
    bag.clear();

    /*C++ LEARNING
        [&bag] means the lambda wants to acces to the variable bag and it wants access by reference.
        so the lambda can modify the baf directly.
        letter -> The letter to be added
        points -> The number of points the tile is worth
        count -> How many of those tiles to be added
        ex: addTiles('A', 1, 9) means 9 tiles of letter A each worth 1 point are added to the bag.
        bag.push_back(Tile{'A', 1}); x 9 times
     */

    // Helper to add without reallocation
    auto addTiles = [&](char l, int p, int c) {
        for(int i=0; i<c; i++) bag.push_back({l, p});
    };

    // Letter distribution:
    // E×12; A,I×9; O×8; N,R,T×6; L,S,U,D×4; G×3;
    // B,C,M,P,F,H,V,W,Y×2; K,J,X,Q,Z×1; blanks×2

    //1-point letters
    addTiles('E', 1, 12);
    addTiles('A', 1, 9);
    addTiles('I', 1,  9);
    addTiles('O', 1,  8);
    addTiles('N', 1,  6);
    addTiles('R', 1,  6);
    addTiles('T', 1,  6);
    addTiles('L', 1,  4);
    addTiles('S', 1,  4);
    addTiles('U', 1,  4);

    // 2-point letters
    addTiles('D', 2, 4);
    addTiles('G', 2, 3);

    // 3-point letters
    addTiles('B', 3, 2);
    addTiles('C', 3, 2);
    addTiles('M', 3, 2);
    addTiles('P', 3, 2);

    // 4-point letters
    addTiles('F', 4, 2);
    addTiles('H', 4, 2);
    addTiles('V', 4, 2);
    addTiles('W', 4, 2);
    addTiles('Y', 4, 2);

    // 5-point letter
    addTiles('K', 5, 1);

    // 8-point letters
    addTiles('J', 8, 1);
    addTiles('X', 8, 1);

    // 10-point letters
    addTiles('Q',10, 1);
    addTiles('Z',10, 1);

    // Blanks: use '?' as the letter, 0 points
    addTiles('?', 0, 2);
}

// Lightweight Xorshift RNG (Much faster than mt19937)
struct FastRNG {
    uint32_t state;
    FastRNG() {
        // Seed with time + thread ID to ensure parallel randomness
        size_t seed = std::hash<std::thread::id>{}(std::this_thread::get_id()) +
                      std::chrono::high_resolution_clock::now().time_since_epoch().count();
        state = static_cast<uint32_t>(seed);
        if (state == 0) state = 0xDEADBEEF;
    }

    using result_type = uint32_t;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return UINT32_MAX; }

    uint32_t operator()() {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return state = x;
    }
};

//Suffels the tile bag (use once at game start)
void shuffleTileBag(TileBag &bag) {
    // Thread-local FastRNG. Initialized once per thread.
    // No locks, no heavyweight constructors.
    thread_local FastRNG rng;
    shuffle(bag.begin(), bag.end(), rng);
}
