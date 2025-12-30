#pragma once

#include <map>
#include <string>
#include <vector>
#include <cctype>

using namespace std;

class TileTracker {
private:
    map <char, int> unseenTiles;
    int totalUnseenTiles = 0;

public:
    TileTracker() {
        reset();
    }

    // Initialize with standard distribution.
    void reset() {
        unseenTiles = {
            {'A', 9}, {'B', 2}, {'C', 2}, {'D', 4}, {'E', 12},
            {'F', 2}, {'G', 3}, {'H', 2}, {'I', 9}, {'J', 1},
            {'K', 1}, {'L', 4}, {'M', 2}, {'N', 6}, {'O', 8},
            {'P', 2}, {'Q', 1}, {'R', 6}, {'S', 4}, {'T', 6},
            {'U', 4}, {'V', 2}, {'W', 2}, {'X', 1}, {'Y', 2},
            {'Z', 1}, {'?', 2} // Blanks
        };
        totalUnseenTiles = 100;
    }

    // Mark the tile as "Seen" (played on board or in our rack)
    void markSeen(char c) {
        char upper = toupper(c);

        // If input is lowercase, it represents a used blank tile.
        if (islower(c)) {
            if (unseenTiles['?'] > 0) {
                unseenTiles['?']--;
                totalUnseenTiles--;
            }
        } else if (unseenTiles.count(upper) && unseenTiles[upper] > 0) {
            unseenTiles[upper]--;
            totalUnseenTiles--;
        }
    }

    void markSeen(const string& str) {
        for (char c : str) {
            if (isalpha(c) || c == '?') {
                markSeen(c);
            }
        }
    }

    // Alias for Spy integration
    void remove(char c) {
        markSeen(c);
    }

    int getUnseenCount(char c) const {
        char upper = toupper(c);
        if (unseenTiles.count(upper)) {
            return unseenTiles.at(upper);
        }
        return 0;
    }

    int getTotalUnseen() const {
        return totalUnseenTiles;
    }

    // [NEW] Required for Monte Carlo Simulation
    // Exports all unseen tiles into a flat vector for the Spy to sample from.
    vector<char> getRemainingTiles() const {
        vector<char> bag;
        bag.reserve(totalUnseenTiles);
        for (auto const& [key, val] : unseenTiles) {
            for (int k = 0; k < val; ++k) {
                bag.push_back(key);
            }
        }
        return bag;
    }
};