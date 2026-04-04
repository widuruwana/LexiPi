#include "../../include/spectre/leave_evaluator.h"
#include <fstream>
#include <iostream>
#include <cstring>

namespace spectre {

    // =========================================================
    // HandTunedLeaves
    // =========================================================

    float HandTunedLeaves::evaluate(const int leaveCounts[27]) const {
        float val = 0.0f;
        for (int i = 0; i < 27; i++) {
            val += leaveCounts[i] * STATIC_LEAVES[i];
        }

        // Vowel/Consonant balance penalty
        int vowels = leaveCounts['A'-'A'] + leaveCounts['E'-'A'] +
                     leaveCounts['I'-'A'] + leaveCounts['O'-'A'] +
                     leaveCounts['U'-'A'];
        int consonants = 0;
        for (int i = 0; i < 26; i++) consonants += leaveCounts[i];
        consonants -= vowels;

        if (vowels == 0 && consonants > 0) val -= 5.0f;
        if (consonants == 0 && vowels > 0) val -= 5.0f;
        if (vowels > 4) val -= 4.0f;
        if (consonants > 5) val -= 4.0f;

        return val;
    }

    // =========================================================
    // TrainedLinearLeaves (Stub - to be filled by training pipeline)
    // =========================================================

    bool TrainedLinearLeaves::loadFromFile(const std::string& filename) {
        std::ifstream in(filename, std::ios::binary);
        if (!in) {
            std::cout << "[LeaveEval] Failed to load trained weights: " << filename << std::endl;
            return false;
        }

        in.read(reinterpret_cast<char*>(weights), 27 * sizeof(float));
        loaded = in.good();

        if (loaded) {
            std::cout << "[LeaveEval] Loaded trained weights from: " << filename << std::endl;
        }
        return loaded;
    }

    float TrainedLinearLeaves::evaluate(const int leaveCounts[27]) const {
        if (!loaded) return 0.0f; // Graceful degradation

        float val = 0.0f;
        for (int i = 0; i < 27; i++) {
            val += leaveCounts[i] * weights[i];
        }
        return val;
    }

} // namespace spectre
