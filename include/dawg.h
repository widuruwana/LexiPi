#pragma once

#include <vector>
#include <string>
#include <cstdint>

// 26 Letters + 1 Seperator ('^')
#define LETTER_COUNT 27

using namespace std;

// Orpheus Graph Node
struct DawgNode {
    uint32_t edgeMask; // Bit mask of immediate children (for fast iteration)
    uint32_t subtreeMask; // Bit mask of all future children (for Oracular Lookahead)
    int firstChildIndex; // Index into the global childrenPool
    bool isEndOfWord; // Flag

    DawgNode() : edgeMask(0), subtreeMask(0), firstChildIndex(-1), isEndOfWord(false) {}
};

class Dawg {
public:
    vector<DawgNode> nodes;
    vector<int> childrenPool; // Compact storage for edges
    int rootIndex = 0;

    Dawg();

    // Loads the dictionary file and builds the GADDAG
    // Build from text list -> Compress -> Save to Binary
    void buildFromWordList(const vector<string> &wordList);

    // Load dictionary from Binary
    bool loadBinary(const string& filename);
    bool saveBinary(const string& filename);

    // Checks if a standard word exists (mostly for debugging/constraints)
    bool isValidWord(const string &word) const;

    inline bool canPrune(int nodeIdx, uint32_t rackMask) const;

    // Fast Lookup: Replaced "nodes[i].children[c]"
    // Uses hardware bit-counting for O(1) access
    inline int getChild(int nodeIdx, int letterIdx) const;

private:
    //Insert a word into the graph in GADDAG format (rotations with separator)
    void insertGADDAG(const string &word);
};

// Global access to AI's brain
extern Dawg gDawg;
