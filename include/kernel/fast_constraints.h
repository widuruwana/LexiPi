#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <bit> // for std::countr_zero
#include "../engine/board.h"
#include "../engine/dictionary.h"

using namespace std;

// A bitmask representing allowed letters for a specific square.
using CharMask = uint32_t;

constexpr CharMask MASK_ANY = 0x03ffffff;
constexpr CharMask MASK_NONE = 0x00000000;
const int SEPERATOR = 26;

// 1. ROBUST INDEXER (Unifies A/a and blocks garbage)
static inline int fastToIdx(char c) {
    // Handle Lowercase (Blanks) -> Map to 0-25 Geometry
    if (c >= 'a' && c <= 'z') return c - 'a';
    // Handle Uppercase -> Map to 0-25 Geometry
    if (c >= 'A' && c <= 'Z') return c - 'A';
    // Garbage -> Return 30 (Safely handled by caller)
    return 30;
}

namespace kernel {
    struct RowConstraint {
        array < CharMask, BOARD_SIZE > masks;
        uint16_t anchorMask = 0;

        RowConstraint() {
            masks.fill(MASK_ANY);
            anchorMask = 0;
        }

        inline bool isAllowed(int col, char letter) const {
            if (col < 0 || col >= BOARD_SIZE) return false;
            int idx = letter - 'A';
            if (idx < 0 || idx > 25) return false;
            return (masks[col] >> idx) & 1;
        }
    };

    class ConstraintGenerator {
    private:
        // Helper: Check suffix (Downwards)
        static inline bool canTraverseSuffix(int nodeIdx, const LetterBoard &letters, int startRow, int col) {
            int curr = nodeIdx;
            int r = startRow;
            while (r < 15 && letters[r][col] != ' ') {
                curr = gDictionary.getChild(curr, fastToIdx(letters[r][col]));
                if (curr == -1) return false;
                r++;
            }
            return gDictionary.nodePtr[curr].isEndOfWord;
        }

        // INLINED: Compute Vertical Constraints (Correct GADDAG Traversal)
        // Traversal Order: Candidate -> Rev(WordAbove) -> Separator -> WordBelow
        static inline CharMask computeCrossCheck(const LetterBoard &letters, int row, int col) {
            bool hasUp = (row > 0 && letters[row-1][col] != ' ');
            bool hasDown = (row < 14 && letters[row+1][col] != ' ');

            // Optimization: If no vertical neighbors, the cross-check allows anything.
            if (!hasUp && !hasDown) return MASK_ANY;

            CharMask allowed = MASK_NONE;

            // Try every possible letter (A-Z) as the Candidate Tile
            for (int l = 0; l < 26; l++) {
                int curr = gDictionary.rootIndex;

                // 1. ANCHOR: Start with the Candidate Letter
                // In GADDAG, the placed tile is always the anchor of the path we verify.
                curr = gDictionary.getChild(curr, l);
                if (curr == -1) continue;

                // 2. UP: Traverse neighbors above (Reverse Order)
                // Path: Candidate -> neighbor(row-1) -> neighbor(row-2)...
                if (hasUp) {
                    int r = row - 1;
                    while (r >= 0 && letters[r][col] != ' ') {
                        curr = gDictionary.getChild(curr, fastToIdx(letters[r][col]));
                        if (curr == -1) goto next_candidate;
                        r--;
                    }
                }

                // 3. SEPARATOR: Cross from "Left/Up" side to "Right/Down" side
                curr = gDictionary.getChild(curr, SEPERATOR);
                if (curr == -1) continue;

                // 4. DOWN: Traverse neighbors below (Forward Order)
                // Path: ... -> Separator -> neighbor(row+1) -> neighbor(row+2)...
                if (hasDown) {
                    int r = row + 1;
                    while (r < 15 && letters[r][col] != ' ') {
                        curr = gDictionary.getChild(curr, fastToIdx(letters[r][col]));
                        if (curr == -1) goto next_candidate;
                        r++;
                    }
                    // We reached the end of the vertical tiles. Is this a valid word end?
                    if (gDictionary.nodePtr[curr].isEndOfWord) {
                        allowed |= (1 << l);
                    }
                } else {
                    // No tiles below. Is the prefix (Up+Cand) a valid word end itself?
                    if (gDictionary.nodePtr[curr].isEndOfWord) {
                        allowed |= (1 << l);
                    }
                }

                next_candidate:;
            }

            return allowed;
        }

    public:
        // INLINED: Generate Constraints for a Row
        static inline RowConstraint generateRowConstraint(const LetterBoard &letters, int rowIdx) {
            RowConstraint rowData;

            // 1. Compute Occupancy Masks
            uint16_t rowOcc = 0;
            uint16_t upOcc = 0;
            uint16_t downOcc = 0;

            for(int c=0; c<15; c++) {
                if (letters[rowIdx][c] != ' ') rowOcc |= (1 << c);
                if (rowIdx > 0 && letters[rowIdx-1][c] != ' ') upOcc |= (1 << c);
                if (rowIdx < 14 && letters[rowIdx+1][c] != ' ') downOcc |= (1 << c);
            }

            // 2. Identify "Dangerous" Columns (Empty squares with vertical neighbors)
            // These are implicitly anchors because they have cross-checks.
            uint16_t dangerousCols = (upOcc | downOcc) & (~rowOcc);

            // 3. Compute Neighbor Anchors (Empty squares next to tiles)
            uint16_t neighborCols = ((rowOcc << 1) | (rowOcc >> 1)) & (~rowOcc) & 0x7FFF;

            // 4. Combine to form the Master Anchor Mask
            // An anchor is: Dangerous (CrossCheck) OR Neighbor (Hook)
            rowData.anchorMask = dangerousCols | neighborCols;

            // Special Case: Start of Game (Center Star)
            // If row is empty and no vertical neighbors, but it's row 7, then (7,7) is valid.
            // Note: A truly empty board has rowOcc=0 and dangerousCols=0.
            if (rowIdx == 7 && rowOcc == 0 && dangerousCols == 0) {
                rowData.anchorMask |= (1 << 7);
            }

            // 5. Fill Constraints (Optimized Loops)
            uint16_t existing = rowOcc;
            while(existing) {
                int col = std::countr_zero(existing);
                int idx = fastToIdx(letters[rowIdx][col]);

                // CORE SAFETY FIX: Only set mask if index is valid (0-25)
                // This prevents '1 << 30' from ever entering the constraint system.
                if (idx < 26) {
                    rowData.masks[col] = (1 << idx);
                } else {
                    // Treat invalid chars (like '?') as blocking everything.
                    // This creates a "dead square" instead of crashing the engine.
                    rowData.masks[col] = 0;
                }

                existing &= ~(1 << col);
            }

            while(dangerousCols) {
                int col = std::countr_zero(dangerousCols);
                rowData.masks[col] = computeCrossCheck(letters, rowIdx, col);
                dangerousCols &= ~(1 << col);
            }

            return rowData;
        }
    };
}