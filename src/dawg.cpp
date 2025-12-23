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
    // For now, we are switching to GADDAG by default as requested
    buildGaddag(wordList);
}

void Dawg::buildGaddag(const vector<string> &wordList) {
    cout << "Compiling Dictionary into GADDAG..." << endl;
    
    clear();
    
    // GADDAG Construction:
    // For each word w = c1 c2 ... cn
    // For each k from 1 to n:
    //   Insert path: ck ck-1 ... c1 < ck+1 ... cn
    // Where < is the delimiter (index 26)
    
    for (const string &word : wordList) {
        int n = word.length();
        
        for (int k = 0; k < n; k++) {
            int currentNodeIndex = rootIndex;
            
            // 1. Insert prefix reversed: ck ... c1
            for (int i = k; i >= 0; i--) {
                int letterIdx = toupper(word[i]) - 'A';
                if (letterIdx < 0 || letterIdx > 25) continue;
                
                nodes[currentNodeIndex].edgeMask |= (1 << letterIdx);
                
                if (nodes[currentNodeIndex].children[letterIdx] == -1) {
                    nodes.emplace_back();
                    int newNodeIndex = static_cast<int>(nodes.size() - 1);
                    nodes[currentNodeIndex].children[letterIdx] = newNodeIndex;
                    currentNodeIndex = newNodeIndex;
                } else {
                    currentNodeIndex = nodes[currentNodeIndex].children[letterIdx];
                }
            }
            
            // 2. Insert delimiter '<' (index 26)
            // Only if there is a suffix (k < n-1) OR to mark end of reversed prefix
            // Standard GADDAG: always insert delimiter unless we are at the very end?
            // Actually, GADDAG usually treats the first part as the "reversed prefix" and the second as "suffix".
            // The delimiter separates them.
            // Path: ck..c1 < ck+1..cn
            
            int delimIdx = 26;
            // We use bit 26 for mask if we had 32 bits, but edgeMask is uint32_t so it fits.
            nodes[currentNodeIndex].edgeMask |= (1 << delimIdx);
            
            if (nodes[currentNodeIndex].children[delimIdx] == -1) {
                nodes.emplace_back();
                int newNodeIndex = static_cast<int>(nodes.size() - 1);
                nodes[currentNodeIndex].children[delimIdx] = newNodeIndex;
                currentNodeIndex = newNodeIndex;
            } else {
                currentNodeIndex = nodes[currentNodeIndex].children[delimIdx];
            }
            
            // 3. Insert suffix: ck+1 ... cn
            for (int i = k + 1; i < n; i++) {
                int letterIdx = toupper(word[i]) - 'A';
                if (letterIdx < 0 || letterIdx > 25) continue;
                
                nodes[currentNodeIndex].edgeMask |= (1 << letterIdx);
                
                if (nodes[currentNodeIndex].children[letterIdx] == -1) {
                    nodes.emplace_back();
                    int newNodeIndex = static_cast<int>(nodes.size() - 1);
                    nodes[currentNodeIndex].children[letterIdx] = newNodeIndex;
                    currentNodeIndex = newNodeIndex;
                } else {
                    currentNodeIndex = nodes[currentNodeIndex].children[letterIdx];
                }
            }
            
            // Mark end of word
            nodes[currentNodeIndex].isEndOfWord = true;
        }
    }
    
    size_t ramUsage = nodes.size() * sizeof(DawgNode);
    cout << "GADDAG Compiled. Nodes " << nodes.size()
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



























