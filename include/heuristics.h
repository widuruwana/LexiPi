#pragma once

#include <string>
#include <cctype>
#include "tile_tracker.h"

// Scrabble Tile Values (A=1, B=3, etc.)
// A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z
static constexpr int TILE_POINTS[26] = {
    1, 3, 3, 2, 1, 4, 2, 4, 1, 8, 5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10
};

// Rack leave values (Strategic value of keeping a tile)
static constexpr float BASE_LEAVE_VALUES[26] = {
    1.0f, -2.0f, -1.0f, 0.0f, 3.0f,  // A-E
    -2.0f, -2.0f, 1.0f, 0.0f, -3.0f, // F-J
    -2.5f, 0.5f, -1.0f, 1.0f, -1.0f, // K-O
    -1.5f, -7.0f, 4.0f, 8.0f, 1.5f,  // P-T
    -2.0f, -4.0f, -3.0f, -3.0f, -2.0f, // U-Y
    -2.0f                            // Z
};

class Heuristics {
public:
    // Dynamic multipliers
    static float vowelPenalty;
    static float consonantPenalty;

    // Analyze the "Unseen" pool (Bag + Opponent Rack) to adjust strategy
    static void updateWeights(const TileTracker &tracker) {
        int vowels = 0;
        int consonants = 0;

        string vChars = "AEIOU";

        // Count unseen vowels
        for (char v : vChars) {
            vowels += tracker.getUnseenCount(v);
        }

        // Count unseen consonents
        // Total unseen - Vowels - Blanks
        int totalUnseen = tracker.getTotalUnseen();
        int blanks = tracker.getUnseenCount('?');
        consonants = totalUnseen - vowels - blanks;

        if (totalUnseen <= 0) {
            vowelPenalty = 1.0f;
            consonantPenalty = 1.0f;
            return;
        }

        // Standard is ~42% vowels
        double vowelRatio = (double)vowels / totalUnseen;

        // If unseen pool is flooded with vowels (>50%), punish keeping them
        if (vowelRatio > 0.55) vowelPenalty = 3.0f;
        else if (vowelRatio > 0.45) vowelPenalty = 1.5f;
        else vowelPenalty = 1.0f;

        // if unseen pool is starved of vowels (<25%), punish keeping consonants
        // because we are desperate for vowels
        if (vowelRatio < 0.25) consonantPenalty = 2.0f;
        else consonantPenalty = 1.0f;
    }

    static float getLeaveValue(char letter) {
        if (letter == '?') return 25.0f; // Blanks are gold

        int idx = toupper(letter) - 'A';
        if (idx < 0 || idx > 26) return 0;

        float val = BASE_LEAVE_VALUES[idx];

        // Apply dynamic context
        bool isVowel = (std::string("AEIOU").find(toupper(letter)) != std::string::npos);

        if (isVowel) {
            // If keeping a vowel and bag is full of them -> Bad.
            val -= (vowelPenalty - 1.0f) * 2.0f;
        } else {
            // If keeping a consonant and bag has no vowels -> Bad.
            val -= (consonantPenalty - 1.0f) * 2.0f;
        }

        return val;
    }

    static int getTileValue(char letter) {
        if (letter == ' ' || letter == '?') return 0;
        int idx = toupper(letter) - 'A';
        if (idx >= 0 && idx < 26) return TILE_POINTS[idx];
        return 0;
    }
};

// Initialized statistics
inline float Heuristics::vowelPenalty = 1.0f;
inline float Heuristics::consonantPenalty = 1.0f;

/*
static constexpr float BLANK_LEAVE_VALUE = 25.0f;

inline int getTileValue(char letter) {
    if (letter == ' ' || letter == '?') return 0;
    int idx = toupper(letter) - 'A';
    if (idx >= 0 && idx < 26) return TILE_POINTS[idx];
    return 0;
}

inline float getLeaveValue(char letter) {
    if (letter == '?') return BLANK_LEAVE_VALUE;
    int idx = toupper(letter) - 'A';
    if (idx >= 0 && idx < 26) return BASE_LEAVE_VALUES[idx];
    return 0.0f;
}
*/