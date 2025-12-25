#pragma once

#include <map>
#include <vector>
#include <string>
#include <cctype>
#include <random>
#include <algorithm>
#include "tiles.h"
#include "heuristics.h"

using namespace std;

class TileTracker {
private:
    map<char, int> unseenTiles;
    map<char, int> totalTiles;
    int totalUnseen;
    
public:
    TileTracker() {
        initialize();
    }
    
    void initialize() {
        // Standard Scrabble tile distribution
        totalTiles = {
            {'A', 9}, {'B', 2}, {'C', 2}, {'D', 4}, {'E', 12},
            {'F', 2}, {'G', 3}, {'H', 2}, {'I', 9}, {'J', 1},
            {'K', 1}, {'L', 4}, {'M', 2}, {'N', 6}, {'O', 8},
            {'P', 2}, {'Q', 1}, {'R', 6}, {'S', 4}, {'T', 6},
            {'U', 4}, {'V', 2}, {'W', 2}, {'X', 1}, {'Y', 2},
            {'Z', 1}, {'?', 2}  // Blanks
        };
        
        unseenTiles = totalTiles;
        totalUnseen = 100;
    }
    
    void markPlayed(const string &word) {
        for (char c : word) {
            char upper = toupper(c);
            
            // Lowercase letters indicate blank usage
            if (islower(c)) {
                if (unseenTiles['?'] > 0) {
                    unseenTiles['?']--;
                    totalUnseen--;
                }
            } else if (upper >= 'A' && upper <= 'Z') {
                if (unseenTiles[upper] > 0) {
                    unseenTiles[upper]--;
                    totalUnseen--;
                }
            }
        }
    }
    
    void markDrawn(const string &tiles) {
        for (char c : tiles) {
            char upper = toupper(c);
            if (c == '?') {
                if (unseenTiles['?'] > 0) {
                    unseenTiles['?']--;
                    totalUnseen--;
                }
            } else if (upper >= 'A' && upper <= 'Z') {
                if (unseenTiles[upper] > 0) {
                    unseenTiles[upper]--;
                    totalUnseen--;
                }
            }
        }
    }
    
    int getUnseenCount(char tile) const {
        char upper = toupper(tile);
        auto it = unseenTiles.find(upper);
        return (it != unseenTiles.end()) ? it->second : 0;
    }
    
    int getTotalUnseen() const {
        return totalUnseen;
    }
    
    bool hasBlankUnseen() const {
        auto it = unseenTiles.find('?');
        return (it != unseenTiles.end() && it->second > 0);
    }
    
    // Estimate opponent's threat level based on unseen tiles
    float estimateOpponentThreat() const {
        auto getUnseen = [this](char c) -> int {
            auto it = unseenTiles.find(c);
            return (it != unseenTiles.end()) ? it->second : 0;
        };
        
        float threat = 0.0f;
        
        // High-value tiles increase threat
        threat += getUnseen('Q') * 5.0f;
        threat += getUnseen('Z') * 5.0f;
        threat += getUnseen('X') * 4.0f;
        threat += getUnseen('J') * 4.0f;
        
        // Blanks are very dangerous
        threat += getUnseen('?') * 15.0f;
        
        // S tiles enable bingos
        threat += getUnseen('S') * 8.0f;
        
        // Good bingo tiles
        threat += getUnseen('E') * 2.0f;
        threat += getUnseen('R') * 2.0f;
        threat += getUnseen('T') * 2.0f;
        
        return threat;
    }
    
    // Check if opponent likely has bingo potential
    bool opponentHasBingoPotential(int oppRackSize) const {
        auto getUnseen = [this](char c) -> int {
            auto it = unseenTiles.find(c);
            return (it != unseenTiles.end()) ? it->second : 0;
        };
        
        if (oppRackSize < 7) return false;
        
        // If blanks are unseen, bingo is possible
        if (getUnseen('?') > 0) return true;
        
        // If many good tiles unseen, bingo is likely
        int goodTiles = getUnseen('E') + getUnseen('R') +
                        getUnseen('S') + getUnseen('T') +
                        getUnseen('A') + getUnseen('I');
        
        return goodTiles >= 10;
    }

    // Generate a likely opponent rack based on unseen tiles
    // This is used for Monte Carlo simulations and opponent modeling
    TileRack generateOpponentRack(int rackSize) {
        TileRack rack;
        vector<char> pool;
        
        // Create a pool of unseen tiles
        for (auto const& [key, val] : unseenTiles) {
            for (int i = 0; i < val; i++) {
                pool.push_back(key);
            }
        }
        
        // If pool is smaller than rack size, take everything
        if (pool.size() <= static_cast<size_t>(rackSize)) {
            for (char c : pool) {
                Tile t;
                t.letter = c;
                t.points = getTileValue(c);
                rack.push_back(t);
            }
            return rack;
        }
        
        // Shuffle and draw
        // Use a static random device to avoid re-seeding every time
        static std::random_device rd;
        static std::mt19937 g(rd());
        
        std::shuffle(pool.begin(), pool.end(), g);
        
        for (int i = 0; i < rackSize; i++) {
            Tile t;
            t.letter = pool[i];
            t.points = getTileValue(pool[i]);
            rack.push_back(t);
        }
        
        return rack;
    }
};