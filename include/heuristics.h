#pragma once

#include <string>
#include <cctype>

// Scrabble Tile Values (A=1, B=3, etc.)
// A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z
static constexpr int TILE_VALUES[26] = {
    1, 3, 3, 2, 1, 4, 2, 4, 1, 8, 5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10
};

// Rack leave values (Strategic value of keeping a tile)
static constexpr float LEAVE_VALUES[26] = {
    1.0f, -2.0f, -1.0f, 0.0f, 3.0f,  // A-E
    -2.0f, -2.0f, 1.0f, 0.0f, -3.0f, // F-J
    -2.5f, 0.5f, -1.0f, 1.0f, -1.0f, // K-O
    -1.5f, -7.0f, 4.0f, 8.0f, 1.5f,  // P-T
    -2.0f, -4.0f, -3.0f, -3.0f, -2.0f, // U-Y
    -2.0f                            // Z
};

static constexpr float BLANK_LEAVE_VALUE = 25.0f;

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