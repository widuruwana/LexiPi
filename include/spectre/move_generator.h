#pragma once

#include <vector>
#include <string>
#include "../../include/board.h"
#include "../../include/rack.h"
#include "../../include/fast_constraints.h"
#include "../../include/dawg.h"

using namespace std;

namespace spectre {

struct MoveCandidate {
    short row;
    short col;
    short score;
    bool isHorizontal;
    char word[16];
    char leave[8]; // Remaining tiles
};

class MoveGenerator {
public:
    // The static engine - callable by any agent
    static vector<MoveCandidate> generate(
        const LetterBoard &board,
        const TileRack &rack,
        Dawg &dict
    );

private:

    // The GADDAG Entry Point: Starts searches at "Anchor" squares
    static void genMovesGADDAG(int row, const LetterBoard &board, int* rackCounts,
                        const RowConstraint &constraints, bool isHorizontal,
                        vector<MoveCandidate> &results, Dawg& dict);

    // Using raw char buffers (wordBuf) and length counters (wordLen) instead of string
    //-to avoid memory allocation during recursion

    // GADDAG Traversal: Going "Backwards" (Left/Up) through the graph
    static void goLeft(int row, int col, int node, const RowConstraint &constraints,
                uint32_t rackMask, uint32_t pruningMask, int* rackCounts, char* wordBuf, int wordLen,
                const LetterBoard &board, bool isHoriz, int anchorCol,
                vector<MoveCandidate> &results, Dawg& dict);

    // GADDAG Traversal: Going "Forwards" (Right/Down) after hitting a separator
    static void goRight(int row, int col, int node, const RowConstraint &constraints,
                 uint32_t rackMask, uint32_t pruningMask, int* rackCounts, char* wordBuf, int wordLen,
                 const LetterBoard &board, bool isHoriz, int anchorCol,
                 vector<MoveCandidate> &results, Dawg& dict);

    static uint32_t getRackMask(int* rackCounts);

};

}