#include "../include/dawg.h"
#include <iostream>
#include <algorithm>

using namespace std;

Dawg gDawg;

Dawg::Dawg() {
    // Creating the root node immediately so that the graph is never empty
    nodes.emplace_back();
}

void Dawg::clear() {
    nodes.clear();
    nodes.emplace_back();
    rootIndex = 0;
}

void Dawg::buildFromWordList(const vector < string > & wordList) {
    cout << "Compiling Dictionary into Orpheus Graph..." << endl;

    clear();

    // Assumes the input 'wordList' is sorted alphabetically for a standard Trie construction.
    // (true DAWG minimization would happen here, but for V1 a trie is sufficient.
    // and safer to implement quickly. Will add node merging later to save RAM

    for (const string & word : wordList) {
        int currentNodeIndex = rootIndex;

        for (char c: word) {
            int letterIdx = toupper(c) - 'A';
            if (letterIdx < 0 || letterIdx > 25) {
                continue; //skip invalid letters
            }

            // Update the mask for the "Constraint Tunnel"
            // |= means adds the new bit to the existing mask without destroying existing bits
            // ex:- edgeMask = 00000101 (A and C exist)
            // adding 'D' -> 1 << 3 = 00001000
            // edgeMask |= 00001000
            // edgeMask = 00001101 ( A, C, D exist)
            nodes[currentNodeIndex].edgeMask |= (1 << letterIdx);

            // Traverse or Create
            if (nodes[currentNodeIndex].children[letterIdx] == -1) {
                // Create a new node
                nodes.emplace_back();
                int newNodeIndex = static_cast<int>(nodes.size()- 1);
                nodes[currentNodeIndex].children[letterIdx] = newNodeIndex;
                currentNodeIndex = newNodeIndex;
            } else {
                currentNodeIndex = nodes[currentNodeIndex].children[letterIdx];
            }
        }
        nodes[currentNodeIndex].isEndOfWord = true;
    }

    size_t ramUsage = nodes.size() * sizeof(DawgNode);
    cout << "Graph Compiled. Nodes " << nodes.size()
         << " (Memory: " << (ramUsage / 1024 / 1024) << " MB)" << endl;
}

bool Dawg::contains(const string & word) const {
    int current = rootIndex;
    for (char c: word) {
        int idx = toupper(c) - 'A';
        if (idx < 0 || idx > 25) {
            return false;
        }

        if (nodes[current].children[idx] == -1) {
            return false;
        }
        current = nodes[current].children[idx];
    }
    return nodes[current].isEndOfWord;
}



























