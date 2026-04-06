#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <bit>
#include <iostream>

#define LETTER_COUNT 27

using namespace std;

struct DawgNode {
    uint32_t edgeMask;
    uint32_t subtreeMask;
    int firstChildIndex;
    bool isEndOfWord;
};

class Dictionary {
public:
    vector<DawgNode> nodes;
    vector<int> childrenPool;

    // RAW ACCESS POINTERS
    const DawgNode* nodePtr = nullptr;
    const int* childPtr = nullptr;

    int rootIndex = 0;

    Dictionary() {}

    // --- SETUP / MUTATORS ---
    void prepareOptimizedPointers() {
        if (!nodes.empty()) nodePtr = nodes.data();
        if (!childrenPool.empty()) childPtr = childrenPool.data();
    }

    bool loadFromFile(const string &filename);

    // --- THREAD-SAFE CONST ACCESSORS ---
    static inline int fastIndex(char c) {
        if (c == '^') return 26;
        return (c & 31) - 1;
    }

    inline int getChild(int nodeIdx, int letterIdx) const {
        const DawgNode& node = nodePtr[nodeIdx];

        if (!((node.edgeMask >> letterIdx) & 1)) return -1;

        uint32_t mask = (1 << letterIdx) - 1;

        #if defined(_MSC_VER)
            int offset = __popcnt(node.edgeMask & mask);
        #else
            int offset = __builtin_popcount(node.edgeMask & mask);
        #endif

        return childPtr[node.firstChildIndex + offset];
    }

    inline bool canPrune(int nodeIdx, uint32_t rackMask) const {
        return (nodePtr[nodeIdx].subtreeMask != 0) &&
               ((nodePtr[nodeIdx].subtreeMask & rackMask) == 0);
    }

    inline bool isValidWord(const string &word) const {
        if (word.empty()) return false;
        int curr = rootIndex;

        int idx0 = fastIndex(word[0]);
        curr = getChild(curr, idx0);
        if (curr == -1) return false;

        curr = getChild(curr, 26);
        if (curr == -1) return false;

        for (size_t i = 1; i < word.length(); i++) {
            curr = getChild(curr, fastIndex(word[i]));
            if (curr == -1) return false;
        }

        return nodePtr[curr].isEndOfWord;
    }

private:
    void buildFromWordList(const vector<string> &wordList);
    void insertGADDAG(const string &word);
    bool loadBinary(const string& filename);
    bool saveBinary(const string& filename);
};

// ====================================================================
// THE COMPILE-TIME LOCK
// ====================================================================

// 1. The Setup Hook: Use this EXACTLY ONCE at startup to load the DAWG.
extern Dictionary gMutableDictionary;

// 2. The Engine Global: Strictly Const. Multithreading is mathematically safe.
extern const Dictionary& gDictionary;