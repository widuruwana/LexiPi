#include "../include/ai_player.h"
#include "../include/heuristics.h" // For scoring
#include <algorithm>
#include <iostream>

using namespace std;

Move AIPlayer::getMove(const Board &bonusBoard,
                       const LetterBoard &letters,
                       const BlankBoard &blankBoard,
                       const TileBag &bag,
                       const Player &me,
                       const Player &opponent,
                       int PlayerNum) {

    candidates.clear();

    // Extract the rack from player object
    const TileRack &myRack = me.rack;

    // Debug: Print what is used by Cutie_Pi
    string rackStr ="";
    for (const Tile& t: myRack) {
        rackStr += t.letter;
    }
    cout << "[AI] Cutie_Pi is thinking... (Rack: " << rackStr << ")" << endl;

    // 1. Find all possible moves using the LETTER BOARD ( ignore bonuses for logic )
    findAllMoves(letters, myRack);

    // 2. Pick the best one
    if (candidates.empty()) {
        cout << "[AI] No moves found. Passing." << endl;
        Move passMove;
        passMove.type = MoveType::PASS;
        return passMove;
    }

    // Sort by score (decending)
    // Note to Self: in V1 we are using estimated scores (length * 10)
    std::sort(candidates.begin(), candidates.end(),
              [](const MoveCandidate &a, const MoveCandidate &b) {
                  return a.score > b.score;
    });

    const MoveCandidate &bestMove = candidates[0];
    cout << "[AI] Play: " << bestMove.word << " at " << bestMove.row << "," << bestMove.col
         << " (" << bestMove.score << " est. pts)" << endl;

    // Construct the Move object
    Move result;
    result.type = MoveType::PLAY;
    result.row = bestMove.row;
    result.col = bestMove.col;
    result.word = bestMove.word;
    result.horizontal = bestMove.isHorizontal;
    return result;
}

Move AIPlayer::getEndGameDecision() {
    // Placeholder, just pass for now
    Move m;
    m.type = MoveType::PASS;
    return m;
}

void AIPlayer::findAllMoves(const LetterBoard &letters, const TileRack &rack) {
    // Pass 1: Horizontal Rows
    for (int r = 0; r < 15; r++) {
        solveRow(r, letters, rack, true);
    }

    // Pass 2: Verticle columns (transposed)
    // we create a temporary transposed board to resure the same row-solving logic
    LetterBoard transposed;
    for (int r=0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            transposed[c][r] = letters[r][c];
        }
    }

    for (int c = 0; c < 15; c++) {
        // When solving vertically, 'rowIdx' passed to solveRow is actually the COLUMN index
        solveRow(c, transposed, rack, false);
    }
}

void AIPlayer::solveRow(int rowIdx, const LetterBoard &letters, const TileRack &rack, bool isHorizontal) {

    // STORE STATE for the recursion
    this->currentRow = rowIdx;
    this->currentIsHorizontal = isHorizontal;

    // 1. Generate the constraint mask for this row
    RowConstraint constraints = ConstraintGenerator::generateRowConstraint(letters, rowIdx);

    // 2. Prepare rack string ( simple char string for the recursion )
    string rackStr = "";
    for (const Tile &t: rack) {
        rackStr += t.letter;
    }

    // 3. Scan the row for start points (0..14)
    for (int startCol = 0; startCol < 15; startCol++) {
        // Optimization: Dont start strictly inside an existing word (unless extending)
        if (startCol > 0 && letters[rowIdx][startCol - 1] != ' ') {
            continue;
        }

        recursiveSearch(gDawg.rootIndex, startCol, constraints,
           "", rackStr, false, letters);
    }
}

void AIPlayer::recursiveSearch(int nodeIdx,
                               int col,
                               const RowConstraint &constraints,
                               string currentWord,
                               string currentRack,
                               bool anchorFilled,
                               const LetterBoard &letters) {
    // Base case: out of bounds
    if (col >= 15) {
        if (gDawg.nodes[nodeIdx].isEndOfWord && anchorFilled & currentWord.length() > 1) {
            int startCol = col - currentWord.length();

            // Coordinate fix
            int finalRow = this->currentIsHorizontal ? this->currentRow : startCol;
            int finalCol = this->currentIsHorizontal ? startCol : this->currentRow;

            int estScore = currentWord.length() * 10; // Simple heuristic

            candidates.push_back({finalRow, finalCol, currentWord, estScore, this->currentIsHorizontal});
        }
        return;
    }

    // ORPHEUS Intersection
    uint32_t dictMask = gDawg.nodes[nodeIdx].edgeMask;
    uint32_t boardMask = constraints.masks[col];

    uint32_t validLetters = dictMask & boardMask;

    if (validLetters == 0) {
        return; // PRUNE
    }

    // Iterate bits
    for (int i = 0; i < 26; i++) {
        if (!((validLetters >> i) & 1)) {
            continue;
        }

        // Recovering the letter
        char letter = (char)('A' + i);
        int childNode = gDawg.nodes[nodeIdx].children[i];

        // Is this tile already on board
        if (letters[this->currentRow][col] == letter) {
            // Yes: must use it. Anchor satisfied
            recursiveSearch(childNode, col + 1, constraints, currentWord + letter, currentRack,
                            true, letters);
        }
        else if (letters[this->currentRow][col] == ' ') {
            // No: Its empty. Play from the rack
            size_t found = currentRack.find(letter);
            bool isBlank = (found == string::npos) && (currentRack.find('?') != string::npos);

            if (found != string::npos || isBlank) {
                // Remove time from the rack
                string nextRack = currentRack;
                if (found != string::npos) {
                    nextRack.erase(found, 1);
                } else {
                    size_t b = nextRack.find('?');
                    nextRack.erase(b, 1);
                }

                // Did we connect something?
                // if mask is not MASK_ANY, we satisfied a cross check (connected vertically)
                bool newAnchor = anchorFilled || (constraints.masks[col] != MASK_ANY);

                recursiveSearch(childNode, col + 1, constraints, currentWord + letter,
                                nextRack, newAnchor, letters);
            }
        }
    }

    // Stopping condition
    // If we form a valid word, try to save it.
    if (gDawg.nodes[nodeIdx].isEndOfWord && anchorFilled && currentWord.length() > 1) {
        // We can only stop if next square is empty or out of bounds.
        // If the next square has a letters, we are forced to continue extending
        if (col == 15 || (col < 15 && letters[this->currentRow][col] == ' ')) {
            int startCol = col - currentWord.length();
            int finalRow = this->currentIsHorizontal ? this->currentRow : startCol;
            int finalCol = this->currentIsHorizontal ? startCol : this->currentRow;
            int estScore = currentWord.length() * 10;
            candidates.push_back({finalRow, finalCol, currentWord, estScore, this->currentIsHorizontal});
        }
    }
}



















