#include "../include/ai_player.h"
#include "../include/heuristics.h" // For scoring
#include <algorithm>
#include <iostream>
#include <chrono>

using namespace std;
using namespace std::chrono;

// Helper to calculate the Rack Mask (which letters do we physically have?)
uint32_t getRackMask(const string &rackStr) {
    uint32_t mask = 0;
    bool hasBlank = false;
    for (char c : rackStr) {
        if (c == '?') {
            hasBlank = true;
        } else if (isalpha(c)) {
            mask |= (1 << (toupper(c) - 'A'));
        }
    }
    // If we have a blank, we can form any letter
    if (hasBlank) return 0xFFFFFFFF; // Blank can be anything

    return mask;
}

// Optimization: Prune the search space to boundingBox + 1
struct SearchRange {
    int minRow, maxRow, minCol, maxCol;
    bool isEmpty; // track empty board state
};

SearchRange getActiveBoardArea(const LetterBoard &letters) {
    int minR = 14;
    int maxR = 0;
    int minC = 14;
    int maxC = 0;

    bool empty = true;

    // 225 iterations that fits in L1 cache
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (letters[r][c] != ' ') {
                if (r < minR) minR = r;
                if (c < minC) minC = c;
                if (r > maxR) maxR = r;
                if (c > maxC) maxC = c;
                empty = false;
            }
        }
    }

    if (empty) {
        return {7, 7, 7, 7, true};
    }

    // Expand by one to include the "Inflence Zone" (parallel plays)
    // Using min/max to ensure no going out of bounds.
    return{
        max(0, minR - 1),
        max(14, maxR + 1),
        max(0, maxC - 1),
        max(14, maxC + 1),
        false
    };
}

AIPlayer::DifferentialMove AIPlayer::calculateEngineMove(const LetterBoard &letters, const MoveCandidate &bestMove) {

    DifferentialMove diff;
    diff.row = -1;
    diff.col = -1;
    diff.word = "";

    int r = bestMove.row;
    int c = bestMove.col;
    int dr = bestMove.isHorizontal ? 0 : 1;
    int dc = bestMove.isHorizontal ? 1 : 0;

    // Scan for full word against the board.
    for (char letter : bestMove.word) {
        // Is the current square empty?
        if (letters[r][c] == ' ') {
            // Yes, we must place a tile here.

            // If this is the first empty square. then this is "Engine start"
            if (diff.row == -1 && diff.col == -1) {
                diff.row = r;
                diff.col = c;
            }

            // Add the letter to the string we send back to engine
            diff.word += letter;
        }

        //advance to next board cell
        r += dr;
        c += dc;
    }

    return diff;
}

Move AIPlayer::getMove(const Board &bonusBoard,
                       const LetterBoard &letters,
                       const BlankBoard &blankBoard,
                       const TileBag &bag,
                       const Player &me,
                       const Player &opponent,
                       int PlayerNum) {

    // To measure the time spent processing each turn
    auto startTime = high_resolution_clock::now();

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

    // Stop the timer
    auto stopTime = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stopTime - startTime); // Calculates in microsec

    // 2. Pick the best one
    if (candidates.empty()) {
        cout << "[AI] No moves found. Passing." << endl;
        Move passMove;
        passMove.type = MoveType::PASS;
        return passMove;
    }

    // Sort by score (descending)
    // Note to Self: in V1 we are using estimated scores (length * 10)
    std::sort(candidates.begin(), candidates.end(),
              [](const MoveCandidate &a, const MoveCandidate &b) {
                  return a.score > b.score;
    });

    const MoveCandidate &bestMove = candidates[0];

    // Generate correct move string
    // We only send the tiles we are placing, not the full word.
    DifferentialMove engineMove = calculateEngineMove(letters, bestMove);

    if (engineMove.row == -1 || engineMove.word.empty()) {
        cout << "[AI] Error: Move exists, but its already on board and require no Tiles. Passing" << endl;
        Move passMove;
        passMove.type = MoveType::PASS;
        return passMove;
    }

    // Print status
    cout << "[AI] Found " << candidates.size() << " moves in " << duration.count() << " micro seconds." << endl;
    cout << "[AI] Play: " << bestMove.word
         << " (Send: " << engineMove.word << " @" << engineMove.row << "," << engineMove.col << ")"
         << " (" << bestMove.score << " est. pts)" << endl;

    // Construct the Move object
    Move result;
    result.type = MoveType::PLAY;
    result.row = engineMove.row;
    result.col = engineMove.col;
    result.word = engineMove.word;
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

    // Calculate the bounding box
    SearchRange range = getActiveBoardArea(letters);

    // Horizontal Rows
    for (int r = range.minRow; r <= range.maxRow; r++) {
        solveRow(r, letters, rack, true, range.isEmpty);
    }

    // Verticle columns (transposed)
    // DRY optimization. We rotate the board.
    LetterBoard transposed;
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            transposed[c][r] = letters[r][c];
        }
    }

    for (int c = range.minCol; c < range.maxCol; c++) {
        // When solving vertically, 'rowIdx' passed to solveRow is actually the COLUMN index
        solveRow(c, transposed, rack, false, range.isEmpty);
    }
}

void AIPlayer::solveRow(int rowIdx, const LetterBoard &letters,
                        const TileRack &rack, bool isHorizontal, bool isEmptyBoard) {

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

    // Pre-calculate the rack mask
    uint32_t rackMask = getRackMask(rackStr);

    // 3. Scan the row for start points (0..14)
    for (int startCol = 0; startCol < 15; startCol++) {
        // Optimization: Dont start strictly inside an existing word (unless extending)
        if (startCol > 0 && letters[rowIdx][startCol - 1] != ' ') {
            continue;
        }

        recursiveSearch(gDawg.rootIndex, startCol, constraints, rackMask,
           "", rackStr, false, letters, isEmptyBoard, false);
    }
}

void AIPlayer::recursiveSearch(int nodeIdx,
                               int col,
                               const RowConstraint &constraints,
                               uint32_t rackMask,
                               string currentWord,
                               string currentRack,
                               bool anchorFilled,
                               const LetterBoard &letters,
                               bool isEmptyBoard,
                               bool tilesPlaced) {
    // Base case: out of bounds
    if (col >= 15) {
        if (gDawg.nodes[nodeIdx].isEndOfWord && anchorFilled && tilesPlaced && currentWord.length() > 1) {
            int startCol = col - currentWord.length();

            // Coordinate fix
            int finalRow = this->currentIsHorizontal ? this->currentRow : startCol;
            int finalCol = this->currentIsHorizontal ? startCol : this->currentRow;

            int estScore = currentWord.length() * 10; // Simple heuristic

            candidates.push_back({finalRow, finalCol, currentWord, estScore, this->currentIsHorizontal});
        }
        return;
    }

    // --- TRIPLE CONSTRAINT INTERSECTION ---
    // 1. Dictionary says: "Words exist with these letters"
    // 2. Board says: "Only these letters fit here"
    // 3. Rack says: "I only have these letters" (Only effective if no blank)
    uint32_t dictMask = gDawg.nodes[nodeIdx].edgeMask;
    uint32_t boardMask = constraints.masks[col];

    // THE OPTIMIZATION:
    // If the board square is empty, we MUST use a rack tile.
    // So we can intersect with rackMask.
    // If the board square is FULL, we ignore rackMask (we use the board tile).

    uint32_t effectiveMask = boardMask & dictMask;
    if (letters[this->currentRow][col] == ' ') {
        effectiveMask &= rackMask;
    }

    if (effectiveMask == 0) {
        return; // PRUNE
    }

    // Iterate bits
    for (int i = 0; i < 26; i++) {
        if (!((effectiveMask >> i) & 1)) {
            continue;
        }

        // Recovering the letter
        char letter = (char)('A' + i);
        int childNode = gDawg.nodes[nodeIdx].children[i];

        // Is this tile already on board
        if (letters[this->currentRow][col] == letter) {
            // Yes: must use it. Anchor satisfied
            recursiveSearch(childNode, col + 1, constraints, rackMask, currentWord + letter, currentRack,
                            true, letters, isEmptyBoard, tilesPlaced);
        }
        else if (letters[this->currentRow][col] == ' ') {
            // No: Its empty. Play from the rack
            // tilesPlaced become TRUE
            // If board is empty, it satisfies anchor filled ONLY if it touches the center square (7, 7)
            bool isCenter = (isEmptyBoard && this->currentRow == 7 && col ==7);

            // Standard anchor logic (crossing an existing word)
            bool isCross = (constraints.masks[col] != MASK_ANY);

            bool newAnchor = anchorFilled || isCross || isCenter;

            size_t found = currentRack.find(letter);
            bool isBlank = (found == string::npos) && (currentRack.find('?') != string::npos);

            if (found != string::npos) {
                //Normal tile
                string nextRack = currentRack;
                nextRack.erase(found, 1);

                recursiveSearch(childNode, col + 1, constraints, rackMask, currentWord + letter,
                                nextRack, newAnchor, letters, isEmptyBoard, true);
            } else if (isBlank) {
                //Blank tile
                string nextRack = currentRack;
                size_t b = nextRack.find('?');
                nextRack.erase(b, 1);

                // Fix: Appending lowercase letter to indicate blank usage
                char blankRep = tolower(letter);

                recursiveSearch(childNode, col + 1, constraints, rackMask, currentWord + blankRep,
                                nextRack, newAnchor, letters, isEmptyBoard, true);
            }
        }
    }

    // Stopping condition
    // If we form a valid word, try to save it.
    if (gDawg.nodes[nodeIdx].isEndOfWord && anchorFilled && tilesPlaced && currentWord.length() > 1) {
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



















