#include "../include/fast_constraints.h"
#include "../include/dict.h"
#include "../include/dawg.h"
#include <iostream>
#include <string>

using namespace std;

// Helper to conver char to bit index (0-25)
static inline int toIdx(char c) {
    return toupper(c) - 'A';
}

const int SEPERATOR = 26;

// Helper to verify if a suffix exists from a specific node
// Returns true if we can traverse suffix from nodeIdx and end at a valid word.
bool canTraverseSuffix(int nodeIdx, const string &suffix) {
    int curr = nodeIdx;
    for (char c: suffix) {
        int idx = toIdx(c);
        curr = gDawg.getChild(curr, idx);
        if (curr == -1) return false;
    }
    return gDawg.nodes[curr].isEndOfWord;
}

CharMask ConstraintGenerator::computeCrossCheck(const LetterBoard &letters, int row, int col) {
    // If there are no verticle neighbors, ANY letter is valid vertically.
    bool hasUp = (row > 0 && letters[row-1][col] != ' ');
    bool hasDown = (row < 14 && letters[row+1][col] != ' ');

    if (!hasUp && !hasDown) {
        return MASK_ANY;
    }

    string prefix = "";
    int r = row - 1;
    while (r >= 0 && letters[r][col] != ' ') {
        prefix += letters[r][col]; // Prepend to build correctly
        r--;
    }

    string suffix = "";
    r = row + 1;
    while (r < 15 && letters[r][col] != ' ') {
        suffix += letters[r][col];
        r++;
    }

    CharMask allowed = MASK_NONE;

    if (!prefix.empty()) {
        int curr = gDawg.rootIndex;

        // First letter of the prefix
        curr = gDawg.getChild(curr, toIdx(prefix[0]));
        if (curr == -1) return MASK_NONE;

        // Separator
        curr = gDawg.getChild(curr, SEPERATOR);
        if (curr == -1) return MASK_NONE;

        // Rest of the prefix
        for (size_t i = 1; i < prefix.size(); i++) {
            curr = gDawg.getChild(curr, toIdx(prefix[i]));
            if (curr == -1) return MASK_NONE;
        }

        uint32_t possible = gDawg.nodes[curr].edgeMask; // The children that exist

        while (possible) {
            int i = __builtin_ctz(possible); // Get next set bit
            char tryLet = (char)('A' + i);

            // We have the child, but does the Suffix fit?
            int nextNode = gDawg.getChild(curr, i);
            if (canTraverseSuffix(nextNode, suffix)) {
                allowed |= (1 << i);
            }

            possible &= ~(1 << i); // Clear bit
        }
    } else {
    // CASE B: No Prefix (e.g. _ "A T") -> Word "AT"
    // Path: CANDIDATE -> SEPARATOR -> SUFFIX
        int root = gDawg.rootIndex;

        // Try every starting letter
        for (int i = 0; i < 26; i++) {
            int curr = gDawg.getChild(root, i);
            if (curr == -1) continue;

            // Must have separator
            curr = gDawg.getChild(curr, SEPERATOR);
            if (curr == -1) continue;

            // Check if suffix fits
            if (canTraverseSuffix(curr, suffix)) {
                allowed |= (1 << i);
            }
        }
    }

    return allowed;
}

RowConstraint ConstraintGenerator::generateRowConstraint(const LetterBoard &letters, int rowIdx) {
    RowConstraint rowData;

    for (int col = 0; col < BOARD_SIZE; col++) {
        // CASE 1: Square is already occupied
        if (letters[rowIdx][col] != ' ') {
            // Already occupied
            int idx = toIdx(letters[rowIdx][col]);
            rowData.masks[col] = (idx >= 0 && idx < 26) ? (1 << idx) : 0;
        }

        // Empty Square. Check vertical constraints.
        rowData.masks[col] = computeCrossCheck(letters, rowIdx, col);
    }

    return rowData;

}
























