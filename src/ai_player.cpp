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

// Helper to calculate score for a specific move
int calculateTrueScore(const MoveCandidate &move, const LetterBoard& letters, const Board &bonusBoard) {

    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMultiplier = 1;
    int tilesPlacedCount = 0;

    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    // 1. Scoring the main word
    for (char letter : move.word) {
        // Safeguard
        if (r < 0 || r > 14 || c < 0 || c > 14) {
            return -1000;
        }

        int letterScore = getTileValue(letter);
        bool isNewlyPlaced = (letters[r][c] == ' ');

        if (isNewlyPlaced) {
            tilesPlacedCount++;

            // Apply Letter Multipliers (only for new tiles)
            CellType bonus = bonusBoard[r][c];

            if (bonus == CellType::DLS) letterScore *= 2;
            else if (bonus == CellType::TLS) letterScore *= 3;

            if (bonus == CellType::DWS) mainWordMultiplier *= 2;
            else if (bonus == CellType::TWS) mainWordMultiplier *= 3;
        }

        mainWordScore += letterScore;

        // 2. Score cross words
        if (isNewlyPlaced) {
            // Checks perpendicular direction
            int pdr = move.isHorizontal ? 1 : 0;
            int pdc = move.isHorizontal ? 0 : 1;

            bool hasNeighbour = false;

            // Checks bounds dynamically based on where we are looking
            int checkR1 = r - pdr;
            int checkC1 = c - pdc;
            if (checkR1 >= 0 && checkR1 < 15 && checkC1 >= 0 && checkC1 < 15 && letters[checkR1][checkC1] != ' ') {
                hasNeighbour = true;
            }

            int checkR2 = r + pdr;
            int checkC2 = c + pdc;
            if (checkR2 >= 0 && checkR2 < 15 && checkC2 >= 0 && checkC2 < 15 && letters[checkR2][checkC2] != ' ') {
                hasNeighbour = true;
            }

            if (hasNeighbour) {
                // Find the start of the cross word
                int currR = r;
                int currC = c;

                // Scan backwards
                while (currR - pdr >= 0 && currC - pdc >= 0 && letters[currR-pdr][currC-pdc] != ' ') {
                    currR -= pdr;
                    currC -= pdc;
                }

                int crossScore = 0;
                int crossMult = 1;

                // Scan forwards to score the entier cross word
                while (currR < 15 && currC < 15) {
                    char cellLetter = letters[currR][currC];

                    if (currR == r && currC == c) {
                        // In the tile we just placed, so bonuses apply
                        int crossLetterScore = getTileValue(letter);
                        CellType crossBonus = bonusBoard[currR][currC];

                        if (crossBonus == CellType::DLS) crossLetterScore *= 2;
                        else if (crossBonus == CellType::TLS) crossLetterScore *= 3;

                        if (crossBonus == CellType::DWS) crossMult *= 2;
                        else if (crossBonus == CellType::TWS) crossMult *= 3;

                        crossScore += crossLetterScore;
                    } else if (cellLetter != ' ') {
                        // Existing tile. Adding value (no bonuses)
                        crossScore += getTileValue(cellLetter);
                    } else {
                        break; // End of the word (empty square)
                    }
                    currR += pdr;
                    currC += pdc;
                }
                totalScore += (crossScore * crossMult);
            }
        }
        r += dr;
        c += dc;
    }
    totalScore += (mainWordScore * mainWordMultiplier);

    // 3. Bingo bonus (50+ for using all 7 tiles)
    if (tilesPlacedCount == 7) {
        totalScore += 50;
    }

    return totalScore;
}


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
        min(14, maxR + 1),
        max(0, minC - 1),
        min(14, maxC + 1),
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

    // True score for each move
    for (auto &cand : candidates ) {
        cand.score = calculateTrueScore(cand, letters, bonusBoard);
    }

    // Sort by score (descending)
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

    for (int c = range.minCol; c <= range.maxCol; c++) {
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
    // Standard word ( length > 1): Must be in the dictionary
    bool isStandardWord = (currentWord.length() > 1 && gDawg.nodes[nodeIdx].isEndOfWord);
    // Parallel Play/ Single tile move ( length == 1 ): Must have vertical neighbours
    bool isParallelPlay = (currentWord.length() == 1 && constraints.masks[col-1] != MASK_ANY);

    // Can stop if we formed a valid word AND (we are at the edge OR next square is empty)
    bool atEdge = (col >= 15);
    bool nextSquareEmpty = (col < 15 && letters[this->currentRow][col] != ' ');

    if ((isStandardWord || isParallelPlay) && anchorFilled && tilesPlaced) {
        int startCol = col - currentWord.length();
        // Coordinate fix
        int finalRow = this->currentIsHorizontal ? this->currentRow : startCol;
        int finalCol = this->currentIsHorizontal ? startCol : this->currentRow;
        if (finalRow >= 0 && finalRow < 15 && finalCol >= 0 && finalCol < 15) {
            candidates.push_back({finalRow, finalCol, currentWord, 0, this->currentIsHorizontal});
        }
    }

    // 2. Base case: Reached the edge, so we gotta stop
    if (col >= 15) return;

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
}



















