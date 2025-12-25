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
    weights["BINGO_WEIGHT"] = 5.0f;      // Weight for bingo probability
    weights["CONTROL_WEIGHT"] = 3.0f;    // Weight for board control

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
    
    // Board control bonuses (denying premium squares)
    weights["BONUS_TWS_DENIED"] = 15.0f;
    weights["BONUS_DWS_DENIED"] = 8.0f;
    weights["BONUS_CENTER_CONTROL"] = 5.0f;
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

float EvaluationModel::getBingoProbability(const TileRack& rack) const {
    // Calculate the probability of being able to form a bingo with remaining tiles
    // Uses pre-computed bingo probabilities for each letter
    
    int tileCounts[26] = {0};
    int blankCount = 0;
    
    for (const Tile& t : rack) {
        if (t.letter == '?') {
            blankCount++;
        } else {
            int idx = toupper(t.letter) - 'A';
            if (idx >= 0 && idx < 26) tileCounts[idx]++;
        }
    }
    
    // Simplified bingo probability: sum of individual probabilities
    // A more sophisticated version would consider combinations
    float probability = 0.0f;
    
    for (int i = 0; i < 26; i++) {
        if (tileCounts[i] > 0) {
            // Each tile of a letter contributes its probability
            probability += BINGO_PROBABILITIES[i] * tileCounts[i];
        }
    }
    
    // Blanks greatly increase bingo probability
    probability += blankCount * 0.3f;  // Each blank adds ~30% bingo chance
    
    // Bonus for having many tiles (closer to 7 = higher bingo chance)
    int tilesInRack = rack.size();
    if (tilesInRack >= 6) {
        probability += (tilesInRack - 5) * 0.1f;  // Extra bonus for 6-7 tiles
    }
    
    // Cap probability at 1.0
    return min(probability, 1.0f);
}

float EvaluationModel::getBoardControlBonus(const LetterBoard& board) const {
    float bonus = 0.0f;
    
    // Center control (7,7 is the star - most valuable)
    // Count tiles in center 3x3 area (rows 6-8, cols 6-8)
    int centerTiles = 0;
    for (int r = 6; r <= 8; r++) {
        for (int c = 6; c <= 8; c++) {
            if (board[r][c] != ' ') {
                centerTiles++;
            }
        }
    }
    if (centerTiles > 0) {
        bonus += centerTiles * weights.at("BONUS_CENTER_CONTROL");
    }
    
    // Extended center (rows 5-9, cols 5-9)
    int extendedCenter = 0;
    for (int r = 5; r <= 9; r++) {
        for (int c = 5; c <= 9; c++) {
            if (board[r][c] != ' ') {
                extendedCenter++;
            }
        }
    }
    if (extendedCenter > centerTiles) {
        bonus += (extendedCenter - centerTiles) * (weights.at("BONUS_CENTER_CONTROL") * 0.5f);
    }
    
    // Premium square denial bonus
    // Count how many TWS/DWS/TLS/DLS squares are occupied
    int premiumSquares = 0;
    // TWS locations
    int twsSquares[8][2] = {{0,0},{0,7},{0,14},{7,0},{7,14},{14,0},{14,7},{14,14}};
    for (auto& sq : twsSquares) {
        if (board[sq[0]][sq[1]] != ' ') premiumSquares++;
    }
    // DWS locations (more of them)
    int dwsCount = 0;
    // Simplified DWS check - just count occupied premium squares (would need bonusBoard for accurate check)
    
    bonus += premiumSquares * weights.at("BONUS_TWS_DENIED") * 0.5f;
    
    // Edge control bonus - having tiles near edges can be strategic
    int edgeTiles = 0;
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (board[r][c] != ' ') {
                // Check if on edge or near edge
                if (r == 0 || r == 14 || c == 0 || c == 14) edgeTiles++;
            }
        }
    }
    // Small bonus for edge presence (can be used for hooks later)
    bonus += edgeTiles * 0.5f;
    
    return bonus;
}

float EvaluationModel::evaluate(int scoreGained, const TileRack& currentRack, const LetterBoard& board) {
    float scoreTerm = (float)scoreGained * weights.at("SCORE_WEIGHT");
    float leaveTerm = getLeaveValue(currentRack) * weights.at("LEAVE_WEIGHT");
    float balanceTerm = getBalancePenalty(currentRack) * weights.at("BALANCE_WEIGHT");
    float synergyTerm = getSynergyBonus(currentRack, board) * weights.at("SYNERGY_WEIGHT");
    float bingoTerm = getBingoProbability(currentRack) * weights.at("BINGO_WEIGHT");
    float controlTerm = getBoardControlBonus(board) * weights.at("CONTROL_WEIGHT");
    
    return scoreTerm + leaveTerm + balanceTerm + synergyTerm + bingoTerm + controlTerm;
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
    
    // 5. Bingo Features
    features["BINGO_WEIGHT"] = getBingoProbability(currentRack);
    
    // 6. Board Control Features
    features["CONTROL_WEIGHT"] = getBoardControlBonus(board);
    
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
