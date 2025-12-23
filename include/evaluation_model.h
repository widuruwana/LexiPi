#pragma once

#include "board.h"
#include "rack.h"
#include <string>
#include <map>
#include <vector>

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

    // Helper to check if a specific letter is accessible on the board
    // Accessible means it exists and has at least one empty neighbor
    bool isAccessible(const LetterBoard& board, char letter) const;

    // Feature extraction helpers
    float getLeaveValue(const TileRack& rack) const;
    float getBalancePenalty(const TileRack& rack) const;
    float getSynergyBonus(const TileRack& rack, const LetterBoard& board) const;
};