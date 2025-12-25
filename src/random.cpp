#include "../include/random.h"
#include "../include/logger.h"
#include <chrono>

Random& Random::getInstance() {
    static Random instance;
    return instance;
}

void Random::seed(uint64_t seed) {
    currentSeed = seed;
    rng.seed(seed);
    LOG_INFO("RNG seeded with: ", seed);
}

int Random::nextInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng);
}

double Random::nextDouble() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng);
}

bool Random::nextBool() {
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) == 1;
}
