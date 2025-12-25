#pragma once

#include <string>
#include <cctype>

// Scrabble Tile Values (A=1, B=3, etc.)
// A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z
static constexpr int TILE_VALUES[26] = {
    1, 3, 3, 2, 1, 4, 2, 4, 1, 8, 5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10
};

// Rack leave values (Strategic value of keeping a tile)
// Aggressively tuned for Bingo Probability
static constexpr float LEAVE_VALUES[26] = {
    3.0f, -2.0f, -1.0f, 1.0f, 5.0f,   // A-E (vowels very valuable)
    -2.0f, -1.5f, 1.0f, 2.0f, -3.0f,  // F-J
    -2.5f, 1.5f, -1.0f, 2.0f, -1.0f,  // K-O
    -1.5f, -8.0f, 6.0f, 10.0f, 3.0f,  // P-T (S,R,T extremely valuable)
    -1.5f, -3.5f, -2.5f, -3.0f, -2.0f, // U-Y
    -2.0f                              // Z
};

static constexpr float BLANK_LEAVE_VALUE = 40.0f;  // Massive value for blank

inline int getTileValue(char letter) {
    if (letter == ' ' || letter == '?') return 0;
    int idx = toupper(letter) - 'A';
    if (idx >= 0 && idx < 26) return TILE_VALUES[idx];
    return 0;
}

inline float getLeaveValue(char letter) {
    if (letter == '?') return BLANK_LEAVE_VALUE;
    int idx = toupper(letter) - 'A';
    if (idx >= 0 && idx < 26) return LEAVE_VALUES[idx];
    return 0.0f;
}

// Calculate the total leave value for a rack
inline float calculateRackLeave(const string &rackStr) {
    float total = 0.0f;
    bool hasQ = false;
    bool hasU = false;
    bool hasI = false;
    bool hasN = false;
    bool hasG = false;
    
    for (char c : rackStr) {
        total += getLeaveValue(c);
        char upper = toupper(c);
        if (upper == 'Q') hasQ = true;
        if (upper == 'U') hasU = true;
        if (upper == 'I') hasI = true;
        if (upper == 'N') hasN = true;
        if (upper == 'G') hasG = true;
    }
    
    // Synergy penalties/bonuses
    if (hasQ && !hasU) {
        total -= 10.0f; // Q without U is terrible
    }
    
    // ING synergy
    if (hasI && hasN && hasG) {
        total += 5.0f;
    }
    
    // Vowel balance check
    int vowels = 0;
    int consonants = 0;
    for (char c : rackStr) {
        char upper = toupper(c);
        if (upper == 'A' || upper == 'E' || upper == 'I' || upper == 'O' || upper == 'U') {
            vowels++;
        } else if (upper != '?') {
            consonants++;
        }
    }
    
    // Ideal balance is roughly 40-50% vowels
    // Penalize extremes heavily
    if (vowels == 0 && consonants > 0) total -= 10.0f;
    if (consonants == 0 && vowels > 0) total -= 10.0f;
    if (vowels > 4) total -= 5.0f;
    if (consonants > 5) total -= 5.0f;
    
    return total;
}