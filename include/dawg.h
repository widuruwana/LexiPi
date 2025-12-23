#pragma once

#include <vector>
#include <string>
#include <cstdint>

// 26 Letters + 1 Seperator ('^')
#define LETTER_COUNT 27

using namespace std;

// Orpheus Graph Node
struct DawgNode {
    bool isEndOfWord;
    // Children nodes.
    // use a fixed array of indices into the global vector
    // -1 indicates no child
    int children[LETTER_COUNT];

    // Bit 0 = 'A', Bit 1 = 'B' ...
    uint32_t edgeMask; // Bit mask of immediate children (for fast iteration)
    uint32_t subtreeMask; // Bit mask of all future children (for Oracular Lookahead)

    DawgNode() {
        for (int i = 0; i < LETTER_COUNT; i++) children[i] = -1;
        isEndOfWord = false;
        edgeMask = 0;
        subtreeMask = 0;
    }
};

class Dawg {
public:
    vector<DawgNode> nodes;
    int rootIndex = 0;

    Dawg();

    // Loads the dictionary file and builds the GADDAG
    void loadFromFile(const string &filename);

    // Checks if a standard word exists (mostly for debugging/constraints)
    bool isValidWord(const string &word) const;

private:
    
    //Insert a word into the graph in GADDAG format (rotations with separator)
    void insertGADDAG(const string &word);
};

// Global access to AI's brain
extern Dawg gDawg;
