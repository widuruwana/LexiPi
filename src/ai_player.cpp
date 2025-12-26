#include "../include/ai_player.h"
#include "../include/heuristics.h" // For scoring
#include "../include/tile_tracker.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <future> // for async
#include <thread> // for hardware_concurrency

using namespace std;
using namespace std::chrono;

const int SEPERATOR = 26;

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

        int letterScore = Heuristics::getTileValue(letter);
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
                        int crossLetterScore = Heuristics::getTileValue(letter);
                        CellType crossBonus = bonusBoard[currR][currC];

                        if (crossBonus == CellType::DLS) crossLetterScore *= 2;
                        else if (crossBonus == CellType::TLS) crossLetterScore *= 3;

                        if (crossBonus == CellType::DWS) crossMult *= 2;
                        else if (crossBonus == CellType::TWS) crossMult *= 3;

                        crossScore += crossLetterScore;
                    } else if (cellLetter != ' ') {
                        // Existing tile. Adding value (no bonuses)
                        crossScore += Heuristics::getTileValue(cellLetter);
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

    if (move.word.length() > 1) {
        totalScore += (mainWordScore * mainWordMultiplier);
    }

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

bool AIPlayer::isRackBad(const TileRack &rack) {
    int vowels = 0, consonants = 0;
    bool hasQ = false, hasU = false;
    int counts[26] = {0};

    for (const Tile &t : rack) {
        if (t.letter == '?') {
            vowels++;
            consonants++;
            continue;
        }
        if (strchr("AEIOU", t.letter)) {
            vowels++;
        } else {
            consonants++;
        }
        if (t.letter == 'Q') hasQ = true;
        if (t.letter == 'U') hasU = true;
        if (isalpha(t.letter)) {
            counts[toupper(t.letter) - 'A']++;
        }
    }

    // 1. Q without U
    if (hasQ && !hasU) return true;
    // 2. Imbalance (only if rack is full-ish)
    if (rack.size() >= 5 && (vowels == 0 || consonants == 0)) return true;
    // 3. Duplicates
    for (int i=0; i<26; i++) if (counts[i] >= 3) return true;

    return false;
}

string AIPlayer::getTilesToExchange(const TileRack &rack) {
    string toExchange = "";
    bool hasQ = false;
    for(auto t : rack) if(t.letter=='Q') hasQ=true;

    for (const Tile &t : rack) {
        bool keep = false;
        // Always keep Blank, S, and U (if we have Q)
        if (t.letter == '?' || t.letter == 'S') keep = true;
        if (t.letter == 'U' && hasQ) keep = true;

        // Keep high-value retainers "RETINA" if we aren't dumping
        string retain = "RETINA";
        if (retain.find(t.letter) != string::npos && !keep) keep = true;

        if (!keep) toExchange += t.letter;
    }

    // SAFETY: If heuristics kept everything but rack was bad, dump everything.
    if (toExchange.empty() && !rack.empty()) {
        for(auto t : rack) toExchange += t.letter;
    }
    return toExchange;
}

// Challenge Scanner
bool AIPlayer::shouldChallenge(const Move &opponentMove, const LetterBoard &board) const {
    if (opponentMove.type != MoveType::PLAY) return false;

    // We scan the board to find the ACTUAL words formed.

    int startR = opponentMove.row;
    int startC = opponentMove.col;
    int dr = opponentMove.horizontal ? 0 : 1;
    int dc = opponentMove.horizontal ? 1 : 0;

    // 1. Reconstruct and Validate MAIN WORD
    // Scan backwards from the start position to find the real beginning (in case of hooks/prefixes)
    int r = startR;
    int c = startC;
    while (r - dr >= 0 && c - dc >= 0 && board[r - dr][c - dc] != ' ') {
        r -= dr;
        c -= dc;
    }

    // Now scan forward to build the full string
    string mainWord = "";
    int scanR = r;
    int scanC = c;
    while (scanR < 15 && scanC < 15 && board[scanR][scanC] != ' ') {
        mainWord += board[scanR][scanC];
        scanR += dr;
        scanC += dc;
    }

    // Check Main Word
    if (mainWord.length() > 1 && !gDawg.isValidWord(mainWord)) {
        cout << "\n[AI] Challenging! Main word '" << mainWord << "' is invalid." << endl;
        return true;
    }

    // 2. Reconstruct and Validate CROSS WORDS
    // We iterate through the main word's path. Any tile that has perpendicular neighbors
    // forms a cross word that must be checked.

    // Reset scan pointers to start of Main Word
    scanR = r;
    scanC = c;

    int pdr = opponentMove.horizontal ? 1 : 0; // Perpendicular DR
    int pdc = opponentMove.horizontal ? 0 : 1; // Perpendicular DC

    for (size_t i = 0; i < mainWord.length(); ++i) {
        // Check for perpendicular neighbors at current square (scanR, scanC)
        bool hasNeighbor = false;
        if (scanR - pdr >= 0 && scanC - pdc >= 0 && board[scanR - pdr][scanC - pdc] != ' ') hasNeighbor = true;
        if (scanR + pdr < 15 && scanC + pdc < 15 && board[scanR + pdr][scanC + pdc] != ' ') hasNeighbor = true;

        if (hasNeighbor) {
            // Found a cross word! Scan backwards to find its start.
            int crossR = scanR;
            int crossC = scanC;
            while (crossR - pdr >= 0 && crossC - pdc >= 0 && board[crossR - pdr][crossC - pdc] != ' ') {
                crossR -= pdr;
                crossC -= pdc;
            }

            // Scan forward to build it
            string crossWord = "";
            while (crossR < 15 && crossC < 15 && board[crossR][crossC] != ' ') {
                crossWord += board[crossR][crossC];
                crossR += pdr;
                crossC += pdc;
            }

            if (crossWord.length() > 1 && !gDawg.isValidWord(crossWord)) {
                cout << "[AI] Challenging! Cross-word '" << crossWord << "' is invalid." << endl;
                return true;
            }
        }

        // Step to next tile in main word
        scanR += dr;
        scanC += dc;
    }

    return false;
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

    // Tracking unseen tiles
    TileTracker tracker;

    // 1. Mark board tiles
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (letters[r][c] != ' '){
                tracker.markSeen(letters[r][c]);
            }
        }
    }

    // 2. Mark the rack
    tracker.markSeen(rackStr);
    // 3. Update Heuristics
    Heuristics::updateWeights(tracker);

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

        // 1. Row points (on board)
        int points = calculateTrueScore(cand, letters, bonusBoard);

        // 2. Leave value
        float leaveVal = 0.0f;
        for (char t: cand.leave) {
            leaveVal += Heuristics::getLeaveValue(t);
        }

        // 3. Final score
        cand.score = points + (int)leaveVal;
    }

    // Sort by score (descending)
    std::sort(candidates.begin(), candidates.end(),
              [](const MoveCandidate &a, const MoveCandidate &b) {
                  return a.score > b.score;
    });

    // Exchange decision
    bool shouldExchange = false;
    int bestScore = candidates.empty() ? 0 : candidates[0].score;

    if (candidates.empty() && bag.size() >= 7) {
        shouldExchange = true;
    } else if (bestScore < 14 && isRackBad(myRack) && bag.size() >= 7) {
        shouldExchange = true;
        cout << "[AI] Rack is garbage. Attempting exchange." << endl;
    }

    if (shouldExchange) {
        // Double check we actually HAVE tiles to exchange (prevent empty-bag loop)
        if (bag.size() < 7) {
            cout << "[AI] Wanted to exchange, but bag has < 7 tiles. Must play or pass." << endl;
            if (candidates.empty()) { Move p; p.type=MoveType::PASS; return p; }
            // If we have moves, fall through to play the best one instead of passing
        }
        else {
            Move exMove;
            exMove.type = MoveType::EXCHANGE;
            exMove.exchangeLetters = getTilesToExchange(myRack);
            exMove.word = ""; // Clear

            cout << "[AI] Exchanging tiles: " << exMove.exchangeLetters << endl;
            return exMove;
        }
    }
    else if (candidates.empty()) {
        Move p; p.type=MoveType::PASS; return p;
    }

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

    int displayPoints = calculateTrueScore(bestMove, letters, bonusBoard);

    // Print status
    cout << "[AI] Found " << candidates.size() << " moves in " << duration.count() << " micro seconds." << endl;
    cout << "[AI] Play: " << bestMove.word
         << " (Send: " << engineMove.word << " @" << engineMove.row << "," << engineMove.col << ")"
         << " (Score: " << displayPoints << " + leave)" << endl;

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

    // Verticle columns (transposed)
    // DRY optimization. We rotate the board.
    LetterBoard transposed;
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            transposed[c][r] = letters[r][c];
        }
    }

    // Pre-calculate Constraints (Fast enough to do sequentially)
    RowConstraint constraintsH[15];
    RowConstraint constraintsV[15];

    for(int i=0; i<15; i++) {
        constraintsH[i] = ConstraintGenerator::generateRowConstraint(letters, i);
        constraintsV[i] = ConstraintGenerator::generateRowConstraint(transposed, i);
    }

    // 3. Multi-Threading Setup
    unsigned int nThreads = thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 2; // Safety fallback

    // We create tasks. Each task processes a chunk of rows.
    vector<future<vector<MoveCandidate>>> futures;
    int rowsPerThread = 15 / nThreads;
    if (rowsPerThread == 0) rowsPerThread = 1;

    //futures.clear();

    for (int t = 0; t < nThreads; t++) {
        int start = t * rowsPerThread;
        int end = (t == nThreads - 1) ? 15 : start + rowsPerThread;

        futures.push_back(async(launch::async, [this, start, end, &letters, &transposed, &constraintsH, &constraintsV, &rack]() {
            vector<MoveCandidate> localRes;
            localRes.reserve(500);

            // Horizontal
            for (int r = start; r < end; r++) {
                this->genMovesGADDAG(r, letters, rack, constraintsH[r], true, localRes);
            }
            // Vertical (Scanning transposed board)
            for (int r = start; r < end; r++) {
                this->genMovesGADDAG(r, transposed, rack, constraintsV[r], false, localRes);
            }
            return localRes;
        }));
    }

    // Collect Results
    for (auto &f : futures) {
        vector<MoveCandidate> res = f.get(); // Blocks until thread finishes
        candidates.insert(candidates.end(), res.begin(), res.end());
    }

    /*
    // Calculate the bounding box
    SearchRange range = getActiveBoardArea(letters);

    // Horizontal Rows
    for (int r = range.minRow; r <= range.maxRow; r++) {
        // Generate hybrid constrains (edgeMasks)
        RowConstraint constraints = ConstraintGenerator::generateRowConstraint(letters, r);
        genMovesGADDAG(r, letters, rack, constraints, true);

        //solveRow(r, letters, rack, true, range.isEmpty);
    }


    for (int c = range.minCol; c <= range.maxCol; c++) {
        // Generate hybrid constrains for transposed row (originally a column)
        RowConstraint constraints = ConstraintGenerator::generateRowConstraint(transposed, c);
        genMovesGADDAG(c, transposed, rack, constraints, false);
    }
    */
}

void AIPlayer::genMovesGADDAG(int row,
                              const LetterBoard &board,
                              const TileRack &rack,
                              const RowConstraint &constraints,
                              bool isHorizontal,
                              vector<MoveCandidate> &results) {
    string rackStr = "";
    for (const Tile &t : rack) rackStr += t.letter;

    // Calculate Board Mask (Tiles that already exist in this row)
    uint32_t boardRowMask = 0;
    for(int c = 0; c < 15; c++) {
        if(board[row][c] != ' ') {
            boardRowMask |= (1 << (board[row][c] - 'A'));
        }
    }

    // Create the "Universe" Mask (My Rack + The Board Row)
    // If a letter isn't in this mask, it is IMPOSSIBLE to use it in this move.
    uint32_t myRackMask = getRackMask(rackStr);
    uint32_t pruningMask = myRackMask | boardRowMask;

    // Identify Anchors: empty squares adjacent to existing tiles
    for (int c = 0; c < 15; c++) {

        // Optimization: Only starting generation at an "Anchor"
        bool isAnchor = false;

        // Not an anchor if its already occupied
        if (board[row][c] != ' ') continue;

        // Checking neighbors
        if (c > 0 && board[row][c-1] != ' ') isAnchor = true;
        if (c < 14 && board[row][c+1] != ' ') isAnchor = true;
        if (constraints.masks[c] != MASK_ANY) isAnchor = true; // Vertical neighbor exists

        // Empty board ( center star is the only anchor)
        if (row == 7 && c == 7 && !isAnchor) {
            // Check if board is truly empty
            isAnchor = true;
        }

        if (!isAnchor) continue;

        // Start GADDAG search at this achor (going left)
        // Passing the rack mask for speed
        goLeft(row, c, gDawg.rootIndex, constraints, myRackMask, pruningMask,
            rackStr, "", board, isHorizontal, c, results);
    }
}

void AIPlayer::goLeft(int row,
                      int col,
                      int node,
                      const RowConstraint &constraints,
                      uint32_t rackMask,
                      uint32_t pruningMask,
                      string currentRack,
                      string wordSoFar,
                      const LetterBoard &board,
                      bool isHoriz,
                      int anchorCol,
                      vector<MoveCandidate> &results) {
    // Process the current node (Can we stop going left?)
    // If we have a seperate edge here, it means we can turn around and go right
    // in GADDAG the seperator is an edge to a new node.

    bool canStopGoingLeft = (col < 0) || (board[row][col] == ' ');

    if (canStopGoingLeft) {
        int sepIndex = SEPERATOR;
        if ((gDawg.nodes[node].edgeMask >> sepIndex) & 1) {
            int separatorNode = gDawg.getChild(node, sepIndex);

            // we have reversed prefix (ex: "VER")
            string correctedPrefix = wordSoFar;

            // Reversing it to get actual prefix (ex: "REV")
            reverse(correctedPrefix.begin(), correctedPrefix.end());

            // Send it to goRight
            goRight(row, anchorCol + 1, separatorNode, constraints, rackMask, pruningMask,
                currentRack, correctedPrefix, board, isHoriz, anchorCol, results);
        }
    }

    // 2. Continue Going Left?
    if (col < 0) return; // Hit edge

    // Get constraints for this square
    char boardChar = (col >= 0) ? board[row][col] : ' ';
    uint32_t boardMask = (col >= 0) ? constraints.masks[col] : 0;

    // Calculate effective mask
    uint32_t dictMask = gDawg.nodes[node].edgeMask;
    uint32_t effectiveMask = dictMask;

    if (boardChar != ' ') {
        // Square is occupied. We MUST match the board letter.
        int charIdx = boardChar - 'A';
        if (!((effectiveMask >> charIdx) & 1)) return; // Dict doesn't have this letter

        // Recurse with that letter
        int nextNode = gDawg.getChild(node, charIdx);
        goLeft(row, col - 1, nextNode, constraints, rackMask, pruningMask,
            currentRack, wordSoFar + boardChar, board, isHoriz, anchorCol, results);
    }
    else {
        // Square is empty. We place a tile.
        // Hybrid Check: Dict & Rack & Vertical_Constraints
        effectiveMask &= (rackMask & boardMask);

        // Iterate set bits (optimization)
        while (effectiveMask) {
            int i = __builtin_ctz(effectiveMask); // Get index of first set bit
            char letter = (char)('A' + i);

            uint32_t mask = (1 << i) - 1;
            int offset = __builtin_popcount(gDawg.nodes[node].edgeMask & mask);
            int nextNode = gDawg.childrenPool[gDawg.nodes[node].firstChildIndex + offset];

            if ( (gDawg.nodes[nextNode].subtreeMask &  pruningMask) == 0 && !gDawg.nodes[nextNode].isEndOfWord) {
                //  Oracle: You have no tiles for anything down this path. Prune it
                effectiveMask &= ~(1 << i);
                continue;
            }

            // Handle Rack (remove tile)
            size_t found = currentRack.find(letter);
            bool isBlank = (found == string::npos) && (currentRack.find('?') != string::npos);

            if (found != string::npos) {
                string nextRack = currentRack;
                nextRack.erase(found, 1);

                goLeft(row, col - 1, nextNode, constraints, getRackMask(nextRack), pruningMask,
                    nextRack, wordSoFar + letter, board, isHoriz, anchorCol, results);
            }else if (isBlank) {
                // Blank handling
                string nextRack = currentRack;
                size_t b = nextRack.find('?');
                nextRack.erase(b, 1);
                char blankChar = tolower(letter); // Mark as blank

                goLeft(row, col - 1, nextNode, constraints, getRackMask(nextRack), pruningMask,
                    nextRack, wordSoFar + blankChar, board, isHoriz, anchorCol, results);
            }

            effectiveMask &= ~(1 << i); // Clear bit and continue
        }
    }
}

void AIPlayer::goRight(int row,
                       int col,
                       int node,
                       const RowConstraint &constraints,
                       uint32_t rackMask,
                       uint32_t pruningMask,
                       string currentRack,
                       string wordSoFar,
                       const LetterBoard &board,
                       bool isHoriz,
                       int anchorCol,
                       vector<MoveCandidate> &results) {
    // 1. Check if we formed a valid word
    if (gDawg.nodes[node].isEndOfWord) {
        // Valid word!
        // We need to verify we aren't butting up against another tile
        // Standard check: is next square empty?
        bool validEnd = ((col > 14) || (board[row][col] == ' '));

        if (validEnd) {
            // Reconstruct the full word
            string finalWord = wordSoFar;

            // Calculate start row/col based on length
            int len = finalWord.length();
            int startC = col - len;

            int rFinal = isHoriz ? row : startC;
            int cFinal = isHoriz ? startC : row;

            results.push_back({
                rFinal,
                cFinal,
                finalWord,
                0,
                isHoriz,
                currentRack
            });
        }
    }

    if (col > 14) return;

    // 2. Continue Going Right
    char boardChar = board[row][col];
    uint32_t boardMask = constraints.masks[col];
    uint32_t dictMask = gDawg.nodes[node].edgeMask;
    uint32_t effectiveMask = dictMask;

    if (boardChar != ' ') {
        // Occupied
        int charIdx = boardChar - 'A';
        if (!((effectiveMask >> charIdx) & 1)) return;

        int nextNode = gDawg.getChild(node, charIdx);;

        goRight(row, col + 1, nextNode, constraints, rackMask,  pruningMask,
            currentRack, wordSoFar + boardChar, board, isHoriz, anchorCol, results);
    } else {
        // Empty
        effectiveMask &= (rackMask & boardMask);

        while (effectiveMask) {
            int i = __builtin_ctz(effectiveMask);
            char letter = (char)('A' + i);

            uint32_t mask = (1 << i) - 1;
            int offset = __builtin_popcount(gDawg.nodes[node].edgeMask & mask);
            int nextNode = gDawg.childrenPool[gDawg.nodes[node].firstChildIndex + offset];

            if ( (gDawg.nodes[nextNode].subtreeMask &  pruningMask) == 0 && !gDawg.nodes[nextNode].isEndOfWord) {
                //  Oracle: You have no tiles for anything down this path. Prune it
                effectiveMask &= ~(1 << i);
                continue;
            }

            size_t found = currentRack.find(letter);
            bool isBlank = (found == string::npos) && (currentRack.find('?') != string::npos);

            if (found != string::npos) {
                string nextRack = currentRack;
                nextRack.erase(found, 1);
                goRight(row, col + 1, nextNode, constraints, getRackMask(nextRack), pruningMask,
                    nextRack, wordSoFar + letter, board, isHoriz, anchorCol, results);

            } else if (isBlank) {
                string nextRack = currentRack;
                size_t b = nextRack.find('?');
                nextRack.erase(b, 1);
                goRight(row, col + 1, nextNode, constraints, getRackMask(nextRack), pruningMask,
                    nextRack, wordSoFar + (char)tolower(letter), board, isHoriz, anchorCol, results);
            }
            effectiveMask &= ~(1 << i);
        }
    }
}

/*
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
    bool nextSquareEmpty = atEdge || (letters[this->currentRow][col] == ' ');

    if (nextSquareEmpty && (isStandardWord || isParallelPlay) && anchorFilled && tilesPlaced) {
        int startCol = col - currentWord.length();
        // Coordinate fix
        int finalRow = this->currentIsHorizontal ? this->currentRow : startCol;
        int finalCol = this->currentIsHorizontal ? startCol : this->currentRow;

        // Bounds check for safety
        if (finalRow >= 0 && finalRow < 15 && finalCol >= 0 && finalCol < 15) {
            candidates.push_back({
                finalRow,
                finalCol,
                currentWord,
                0,
                this->currentIsHorizontal,
                currentRack
            });
        }
    }

    // 2. Base case: Reached the edge, so we gotta stop
    if (col >= 15) return;

    // --- TRIPLE CONSTRAINT INTERSECTION ---
    // 1. Dictionary: "Words exist with these letters"
    // 2. Board: "Only these letters fit here"
    // 3. Rack: "I only have these letters" (Only effective if no blank)
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
*/



















