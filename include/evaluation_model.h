#pragma once

#include "board.h"
#include "rack.h"
#include <string>
#include <map>
#include <array>

class EvaluationModel {
public:
    EvaluationModel();
    ~EvaluationModel() = default;

    // Load weights from a file (key=value format)
    bool loadWeights(const std::string& filepath);
    
    // Save weights to a file
    bool saveWeights(const std::string& filepath) const;

    // Calculate the evaluation score for a given state
    // scoreGained: Points obtained from the move just made
    // currentRack: The rack AFTER the move (the leave)
    // board: The board state AFTER the move
    float evaluate(int scoreGained, const TileRack& currentRack, const LetterBoard& board);
    
    // Get raw feature vector (for gradient calculation)
    std::map<std::string, float> getFeatures(int scoreGained, const TileRack& currentRack, const LetterBoard& board);
    
    // Update weights using a gradient map and learning rate
    void updateWeights(const std::map<std::string, float>& gradient, float learningRate);

private:
    std::map<std::string, float> weights;
    
    // Bingo probability lookup table (pre-computed from Scrabble tile distributions)
    // Probability that a letter appears in a random 7-tile bingo combination
    static constexpr std::array<float, 26> BINGO_PROBABILITIES = {{
        0.85f,  // A - very high (most common vowel in bingos)
        0.25f,  // B
        0.45f,  // C
        0.55f,  // D
        0.90f,  // E - highest bingo probability
        0.20f,  // F
        0.35f,  // G
        0.35f,  // H
        0.80f,  // I - very high bingo probability
        0.05f,  // J
        0.20f,  // K
        0.55f,  // L
        0.35f,  // M
        0.70f,  // N - high (common in endings)
        0.50f,  // O
        0.25f,  // P
        0.10f,  // Q
        0.85f,  // R - very high (most common consonant in bingos)
        0.95f,  // S - highest bingo probability!
        0.85f,  // T - very high
        0.40f,  // U
        0.15f,  // V
        0.20f,  // W
        0.10f,  // X
        0.35f,  // Y
        0.10f   // Z
    }};

    // Helper to check if a specific letter is accessible on the board
    // Accessible means it exists and has at least one empty neighbor
    bool isAccessible(const LetterBoard& board, char letter) const;

    // Feature extraction helpers
    float getLeaveValue(const TileRack& rack) const;
    float getBalancePenalty(const TileRack& rack) const;
    float getSynergyBonus(const TileRack& rack, const LetterBoard& board) const;
    float getBingoProbability(const TileRack& rack) const;
    float getBoardControlBonus(const LetterBoard& board) const;
};
