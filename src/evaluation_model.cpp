#include "../include/evaluation_model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

using namespace std;

EvaluationModel::EvaluationModel() {
    // Default weights (baseline heuristics)
    weights["SCORE_WEIGHT"] = 1.0f;
    weights["LEAVE_WEIGHT"] = 4.0f;
    weights["BALANCE_WEIGHT"] = 2.0f;
    weights["SYNERGY_WEIGHT"] = 2.0f;

    // Default Leave Values (Approximation of static heuristics)
    weights["LEAVE_A"] = 3.0f; weights["LEAVE_B"] = -2.0f; weights["LEAVE_C"] = -1.0f; weights["LEAVE_D"] = 1.0f; weights["LEAVE_E"] = 5.0f;
    weights["LEAVE_F"] = -2.0f; weights["LEAVE_G"] = -1.5f; weights["LEAVE_H"] = 1.0f; weights["LEAVE_I"] = 2.0f; weights["LEAVE_J"] = -3.0f;
    weights["LEAVE_K"] = -2.5f; weights["LEAVE_L"] = 1.5f; weights["LEAVE_M"] = -1.0f; weights["LEAVE_N"] = 2.0f; weights["LEAVE_O"] = -1.0f;
    weights["LEAVE_P"] = -1.5f; weights["LEAVE_Q"] = -8.0f; weights["LEAVE_R"] = 6.0f; weights["LEAVE_S"] = 10.0f; weights["LEAVE_T"] = 3.0f;
    weights["LEAVE_U"] = -1.5f; weights["LEAVE_V"] = -3.5f; weights["LEAVE_W"] = -2.5f; weights["LEAVE_X"] = -3.0f; weights["LEAVE_Y"] = -2.0f;
    weights["LEAVE_Z"] = -2.0f;
    weights["LEAVE_BLANK"] = 40.0f;

    // Penalties
    weights["PENALTY_NO_VOWELS"] = -10.0f;
    weights["PENALTY_NO_CONSONANTS"] = -10.0f;
    weights["PENALTY_VOWEL_HEAVY"] = -5.0f;
    weights["PENALTY_CONSONANT_HEAVY"] = -5.0f;
    weights["PENALTY_Q_NO_U"] = -10.0f;

    // Synergies
    weights["BONUS_ING"] = 5.0f;
    weights["BONUS_Q_WITH_U_ON_BOARD"] = 6.0f; // Mitigates the Q penalty if U is accessible
}

bool EvaluationModel::loadWeights(const string& filepath) {
    ifstream file(filepath);
    if (!file.is_open()) {
        // Try looking in parent directories if not found
        string parentPath = "../" + filepath;
        file.open(parentPath);
        if (!file.is_open()) {
            parentPath = "../../" + filepath;
            file.open(parentPath);
        }
        
        if (!file.is_open()) {
            cerr << "[EvaluationModel] Warning: Could not open weights file: " << filepath << ". Using defaults." << endl;
            return false;
        }
    }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue; // Skip comments and empty lines

        stringstream ss(line);
        string key;
        float value;
        
        if (getline(ss, key, '=') && ss >> value) {
            // Trim key
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            
            weights[key] = value;
        }
    }
    
    cout << "[EvaluationModel] Loaded weights from " << filepath << endl;
    return true;
}

bool EvaluationModel::saveWeights(const string& filepath) const {
    ofstream file(filepath);
    if (!file.is_open()) {
        cerr << "[EvaluationModel] Error: Could not open file for writing: " << filepath << endl;
        return false;
    }
    
    file << "# Linear Regression Weights for Scrabble AI Evaluation" << endl;
    file << "# Format: KEY=VALUE" << endl << endl;
    
    for (const auto& pair : weights) {
        file << pair.first << "=" << pair.second << endl;
    }
    
    return true;
}

bool EvaluationModel::isAccessible(const LetterBoard& board, char letter) const {
    char target = toupper(letter);
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (toupper(board[r][c]) == target) {
                // Check neighbors for empty space
                if (r > 0 && board[r-1][c] == ' ') return true;
                if (r < 14 && board[r+1][c] == ' ') return true;
                if (c > 0 && board[r][c-1] == ' ') return true;
                if (c < 14 && board[r][c+1] == ' ') return true;
            }
        }
    }
    return false;
}

float EvaluationModel::getLeaveValue(const TileRack& rack) const {
    float total = 0.0f;
    for (const Tile& t : rack) {
        string key = "LEAVE_";
        if (t.letter == '?') {
            key += "BLANK";
        } else {
            key += (char)toupper(t.letter);
        }
        
        if (weights.count(key)) {
            total += weights.at(key);
        }
    }
    return total;
}

float EvaluationModel::getBalancePenalty(const TileRack& rack) const {
    int vowels = 0;
    int consonants = 0;
    
    for (const Tile& t : rack) {
        char c = toupper(t.letter);
        if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') {
            vowels++;
        } else if (c != '?') {
            consonants++;
        }
    }
    
    float penalty = 0.0f;
    if (vowels == 0 && consonants > 0) penalty += weights.at("PENALTY_NO_VOWELS");
    if (consonants == 0 && vowels > 0) penalty += weights.at("PENALTY_NO_CONSONANTS");
    if (vowels > 4) penalty += weights.at("PENALTY_VOWEL_HEAVY");
    if (consonants > 5) penalty += weights.at("PENALTY_CONSONANT_HEAVY");
    
    return penalty;
}

float EvaluationModel::getSynergyBonus(const TileRack& rack, const LetterBoard& board) const {
    float bonus = 0.0f;
    bool hasQ = false;
    bool hasU = false;
    bool hasI = false;
    bool hasN = false;
    bool hasG = false;
    
    for (const Tile& t : rack) {
        char c = toupper(t.letter);
        if (c == 'Q') hasQ = true;
        if (c == 'U') hasU = true;
        if (c == 'I') hasI = true;
        if (c == 'N') hasN = true;
        if (c == 'G') hasG = true;
    }
    
    // Q Logic
    if (hasQ && !hasU) {
        // Check if U is on board and accessible
        if (isAccessible(board, 'U')) {
            bonus += weights.at("BONUS_Q_WITH_U_ON_BOARD");
        } else {
            bonus += weights.at("PENALTY_Q_NO_U");
        }
    }
    
    // ING Logic
    if (hasI && hasN && hasG) {
        bonus += weights.at("BONUS_ING");
    }
    
    return bonus;
}

float EvaluationModel::evaluate(int scoreGained, const TileRack& currentRack, const LetterBoard& board) {
    float scoreTerm = (float)scoreGained * weights.at("SCORE_WEIGHT");
    float leaveTerm = getLeaveValue(currentRack) * weights.at("LEAVE_WEIGHT");
    float balanceTerm = getBalancePenalty(currentRack) * weights.at("BALANCE_WEIGHT");
    float synergyTerm = getSynergyBonus(currentRack, board) * weights.at("SYNERGY_WEIGHT");
    
    return scoreTerm + leaveTerm + balanceTerm + synergyTerm;
}

std::map<std::string, float> EvaluationModel::getFeatures(int scoreGained, const TileRack& currentRack, const LetterBoard& board) {
    std::map<std::string, float> features;
    
    // 1. Score Feature
    features["SCORE_WEIGHT"] = (float)scoreGained;
    
    // 2. Leave Features
    // We need to decompose getLeaveValue into individual tile counts
    float totalLeaveVal = 0.0f;
    for (const Tile& t : currentRack) {
        string key = "LEAVE_";
        if (t.letter == '?') {
            key += "BLANK";
        } else {
            key += (char)toupper(t.letter);
        }
        
        // The feature value is the count of this tile (usually 1 per tile)
        // But wait, the linear model is:
        // Score = w_score * score + w_leave * (sum(w_tile * count_tile)) ...
        // This is a nested linear model.
        // To make it fully linear for gradient descent, we treat w_leave as a hyper-parameter
        // or we treat each tile as a direct feature.
        // Let's treat each tile as a direct feature scaled by LEAVE_WEIGHT.
        // Actually, let's simplify: The gradient for LEAVE_A is (LEAVE_WEIGHT * count_A).
        // The gradient for LEAVE_WEIGHT is (sum(w_tile * count_tile)).
        
        features[key] += weights.at("LEAVE_WEIGHT"); // Gradient w.r.t tile weight
        totalLeaveVal += weights.at(key);
    }
    features["LEAVE_WEIGHT"] = totalLeaveVal; // Gradient w.r.t LEAVE_WEIGHT
    
    // 3. Balance Features
    float balancePenalty = getBalancePenalty(currentRack);
    features["BALANCE_WEIGHT"] = balancePenalty;
    
    // We need to know WHICH penalty triggered to update its specific weight
    // Re-run logic to find triggers
    int vowels = 0;
    int consonants = 0;
    for (const Tile& t : currentRack) {
        char c = toupper(t.letter);
        if (c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U') vowels++;
        else if (c != '?') consonants++;
    }
    
    if (vowels == 0 && consonants > 0) features["PENALTY_NO_VOWELS"] += weights.at("BALANCE_WEIGHT");
    if (consonants == 0 && vowels > 0) features["PENALTY_NO_CONSONANTS"] += weights.at("BALANCE_WEIGHT");
    if (vowels > 4) features["PENALTY_VOWEL_HEAVY"] += weights.at("BALANCE_WEIGHT");
    if (consonants > 5) features["PENALTY_CONSONANT_HEAVY"] += weights.at("BALANCE_WEIGHT");
    
    // 4. Synergy Features
    float synergyBonus = getSynergyBonus(currentRack, board);
    features["SYNERGY_WEIGHT"] = synergyBonus;
    
    // Re-run logic for specific synergies
    bool hasQ = false, hasU = false, hasI = false, hasN = false, hasG = false;
    for (const Tile& t : currentRack) {
        char c = toupper(t.letter);
        if (c == 'Q') hasQ = true;
        if (c == 'U') hasU = true;
        if (c == 'I') hasI = true;
        if (c == 'N') hasN = true;
        if (c == 'G') hasG = true;
    }
    
    if (hasQ && !hasU) {
        if (isAccessible(board, 'U')) features["BONUS_Q_WITH_U_ON_BOARD"] += weights.at("SYNERGY_WEIGHT");
        else features["PENALTY_Q_NO_U"] += weights.at("SYNERGY_WEIGHT");
    }
    if (hasI && hasN && hasG) features["BONUS_ING"] += weights.at("SYNERGY_WEIGHT");
    
    return features;
}

void EvaluationModel::updateWeights(const std::map<std::string, float>& gradient, float learningRate) {
    for (const auto& pair : gradient) {
        const string& key = pair.first;
        float grad = pair.second;
        
        if (weights.count(key)) {
            weights[key] += (learningRate * grad);
        }
    }
}