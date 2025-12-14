#pragma once

#include <vector>
#include <string>
#include <cstdint>

using namespace std;

// Orpheus Graph Node
struct DawgNode {
    bool isEndOfWord = false;

    // Constraint Mask
    // Bit 0 = 'A', Bit 1 = 'B' ...
    uint32_t edgeMask = 0;

    // Children nodes.
    // use a fixed array of indices into the global vector
    // -1 indicates no child
    int children[26];

    DawgNode() {
        for (int i = 0; i < 26; i++) {
            children[i] = -1;
        }
    }
};

class Dawg {
public:
    vector<DawgNode> nodes;
    int rootIndex = 0;

    Dawg();

    // Take the raw word list and compiles it into the graph
    void buildFromWordList(const vector < string > & wordList);

    // Verification
    bool contains(const string & word) const;

    // Clear memory
    void clear();

};

// Global access to AI's brain
extern Dawg gDawg;
