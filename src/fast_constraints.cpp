#include "../include/fast_constraints.h"
#include "../include/dict.h"
#include <iostream>
#include <string>

using namespace std;

// Helper to conver char to bit index (0-25)
static inline int toIdx(char c) {
    return toupper(c) - 'A';
}

CharMask ConstraintGenerator::computeCrossCheck(const LetterBoard &letters, int row, int col) {
    // If there are no verticle neighbors, ANY letter is valid vertically.
    bool hasUp = (row > 0 && letters[row-1][col] != ' ');
    bool hasDown = (row < 14 && letters[row+1][col] != ' ');

    if (!hasUp && !hasDown) {
        return MASK_ANY;
    }

    // Now there is a neighbor, so we have to find which letters form valid words.
    // We construct the verticle word with a "placeholder" for the current cell.

    // Finding the top of the verticle word.
    int startRow = row;
    while (startRow > 0 && letters[startRow - 1][col] != ' ') {
        startRow--;
    }

    // Finding the bottom
    int endRow = row;
    while (endRow < 14 && letters[endRow + 1][col] != ' ') {
        endRow++;
    }

    // Brute force trying all 26 letters in the gap
    CharMask allowed = MASK_NONE;
    string tempWord;

    // Pre-allocating space for optimization
    tempWord.reserve(endRow - startRow + 1);

    for (char tryLet = 'A'; tryLet <= 'Z'; tryLet++) {
        tempWord.clear();
        for (int i = startRow; i < endRow; i++) {
            if (i == row) {
                tempWord += tryLet;
            } else {
                tempWord += letters[i][col];
            }
        }

        if (isValidWord(tempWord)) {
            allowed |= (1 << (tryLet - 'A'));
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
























