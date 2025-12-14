#pragma once

#include <string>
#include <unordered_map>
#include <cctype>

// Standard rack leave values
static std::unordered_map<char, float> leaveValues = {
    {'A', 1.0}, {'B', -2.0}, {'C', -1.0}, {'D', 0.0}, {'E', 3.0},
    {'F', -2.0}, {'G', -2.0}, {'H', 1.0}, {'I', 0.0}, {'J', -3.0},
    {'K', -2.5}, {'L', 0.5}, {'M', -1.0}, {'N', 1.0}, {'O', -1.0},
    {'P', -1.5}, {'Q', -7.0}, {'R', 4.0}, {'S', 8.0}, {'T', 1.5},
    {'U', -2.0}, {'V', -4.0}, {'W', -3.0}, {'X', -3.0}, {'Y', -2.0},
    {'Z', -2.0}, {'?', 25.0}
};

inline float getRackLeaveScore(const std::string & rack) {
    float score = 0;
    for (char c: rack) {
        char up = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        if (leaveValues.count(up)) {
            score += leaveValues[up];
        }
    }
}