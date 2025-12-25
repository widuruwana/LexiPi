#include "../include/fast_constraints.h"
#include "../include/dict.h"
#include "../include/dawg.h"
#include <iostream>
#include <string>

using namespace std;

// Helper to convert char to bit index (0-25)
static inline int toIdx(char c) {
    return toupper(c) - 'A';
}

// Helper to build the complete cross-word and validate it
static bool isValidCrossWord(const string &upperPart, char letter, const string &lowerPart) {
    string crossWord;
    crossWord.reserve(upperPart.length() + 1 + lowerPart.length());
    crossWord += upperPart;
    crossWord += toupper(letter);
    crossWord += lowerPart;
    
    // Cross-word must be at least 2 letters
    if (crossWord.length() < 2) {
        return false;
    }
    
    return isValidWord(crossWord);
}

CharMask ConstraintGenerator::computeCrossCheck(const LetterBoard &letters, int row, int col) {
    // If there are no vertical neighbors, ANY letter is valid vertically.
    bool hasUp = (row > 0 && letters[row-1][col] != ' ');
    bool hasDown = (row < 14 && letters[row+1][col] != ' ');

    if (!hasUp && !hasDown) {
        return MASK_ANY;
    }

    // GADDAG Optimization:
    // Instead of constructing the string and querying the dictionary 26 times,
    // we traverse the GADDAG graph to find valid letters.
    //
    // CRITICAL FIX: We now validate the COMPLETE cross-word (upperPart + letter + lowerPart)
    // against the dictionary, not just whether the letter could lead to a valid word.

    // 1. Identify Upper and Lower parts
    string upperPart;
    int r = row - 1;
    while (r >= 0 && letters[r][col] != ' ') {
        upperPart.insert(0, 1, letters[r][col]);
        r--;
    }

    string lowerPart;
    r = row + 1;
    while (r < 15 && letters[r][col] != ' ') {
        lowerPart += letters[r][col];
        r++;
    }

    CharMask allowed = MASK_NONE;

    if (!upperPart.empty()) {
        // Case 1: Has Upper Neighbors
        // We use the last character of the upper part as the Anchor.
        // Path: Anchor -> (Rest of Upper Reversed) -> Delimiter -> Candidate -> LowerPart

        char anchorChar = upperPart.back();
        string upperPrefix = upperPart.substr(0, upperPart.length() - 1);

        // 1. Traverse Anchor
        int node = gDawg.rootIndex;
        int anchorIdx = toIdx(anchorChar);
        if (gDawg.nodes[node].children[anchorIdx] == -1) return MASK_NONE;
        node = gDawg.nodes[node].children[anchorIdx];

        // 2. Traverse Upper Prefix (Reversed)
        for (int i = (int)upperPrefix.length() - 1; i >= 0; i--) {
            int idx = toIdx(upperPrefix[i]);
            if (gDawg.nodes[node].children[idx] == -1) return MASK_NONE;
            node = gDawg.nodes[node].children[idx];
        }

        // 3. Traverse Delimiter
        int delimIdx = 26;
        if (gDawg.nodes[node].children[delimIdx] == -1) return MASK_NONE;
        node = gDawg.nodes[node].children[delimIdx];

        // 4. Check Candidates - VALIDATE COMPLETE CROSS-WORD
        for (int c = 0; c < 26; c++) {
            int childNode = gDawg.nodes[node].children[c];
            if (childNode != -1) {
                char letter = 'A' + c;
                // FIX: Validate the complete cross-word is in the dictionary
                if (isValidCrossWord(upperPart, letter, lowerPart)) {
                    allowed |= (1 << c);
                }
            }
        }

    } else {
        // Case 2: No Upper Neighbors (Must have Lower Neighbors)
        // We use the first character of the lower part as the Anchor.
        // Path: Anchor -> Candidate -> Delimiter -> Rest of Lower

        char anchorChar = lowerPart.front();
        string lowerSuffix = lowerPart.substr(1);

        // 1. Traverse Anchor
        int node = gDawg.rootIndex;
        int anchorIdx = toIdx(anchorChar);
        if (gDawg.nodes[node].children[anchorIdx] == -1) return MASK_NONE;
        node = gDawg.nodes[node].children[anchorIdx];

        // 2. Check Candidates - VALIDATE COMPLETE CROSS-WORD
        for (int c = 0; c < 26; c++) {
            int childNode = gDawg.nodes[node].children[c];
            if (childNode != -1) {
                // 3. Traverse Delimiter
                int delimIdx = 26;
                int delimNode = gDawg.nodes[childNode].children[delimIdx];

                if (delimNode != -1) {
                    // Need to verify the path leads to a valid word ending
                    // For cross-word validation, check if candidate + lowerPart forms valid word
                    char letter = 'A' + c;
                    string fullLower = letter + lowerPart;
                    
                    // Check if fullLower (with uppercase conversion) is a valid continuation
                    int curr = childNode;
                    bool match = true;
                    for (char l : lowerSuffix) {
                        int lIdx = toIdx(l);
                        if (gDawg.nodes[curr].children[lIdx] == -1) {
                            match = false;
                            break;
                        }
                        curr = gDawg.nodes[curr].children[lIdx];
                    }
                    if (match && gDawg.nodes[curr].isEndOfWord) {
                        // FIX: Now validate the complete cross-word is in the dictionary
                        if (isValidCrossWord(upperPart, letter, lowerPart)) {
                            allowed |= (1 << c);
                        }
                    }
                }
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
            char existing =  letters[rowIdx][col];
            int idx = toIdx(existing);

            // Safety check for invalid characters on board
            if (idx >= 0 && idx <= 25) {
                // Simple constraint: It MUST be this letter
                rowData.masks[col] = (1 << idx);
            } else {
                rowData.masks[col] = MASK_NONE;
            }
            continue;
        }

        // CASE 2: Empty Square. Check vertical constraints.
        CharMask crossCheck = computeCrossCheck(letters, rowIdx, col);
        rowData.masks[col] = crossCheck;
    }

    return rowData;

}

bool ConstraintGenerator::validateCrossWord(const LetterBoard &letters, int row, int col, char letter) {
    // Check if there are any vertical neighbors
    bool hasUp = (row > 0 && letters[row-1][col] != ' ');
    bool hasDown = (row < 14 && letters[row+1][col] != ' ');
    
    // If no vertical neighbors, no cross-word to validate
    if (!hasUp && !hasDown) {
        return true;
    }
    
    // Build the cross-word parts
    string upperPart;
    int r = row - 1;
    while (r >= 0 && letters[r][col] != ' ') {
        upperPart.insert(0, 1, letters[r][col]);
        r--;
    }
    
    string lowerPart;
    r = row + 1;
    while (r < 15 && letters[r][col] != ' ') {
        lowerPart += letters[r][col];
        r++;
    }
    
    // Validate the complete cross-word
    return isValidCrossWord(upperPart, letter, lowerPart);
}
























