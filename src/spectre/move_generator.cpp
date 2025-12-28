#include "../../include/spectre/move_generator.h"
#include <algorithm>
#include <cstring>
#include <thread>
#include <future>

using namespace std;
using namespace std::chrono;

namespace spectre {

const int SEPERATOR = 26;

// Helper to calculate the Rack Mask (which letters do we physically have?)
uint32_t MoveGenerator::getRackMask(int* rackCounts) {
    uint32_t mask = 0;
    if (rackCounts[26] > 0) return 0xFFFFFFFF; // Blank exists = Any letter possible

    for (int i = 0; i < 26; i++) {
        if (rackCounts[i] > 0) mask |= (1 << i);
    }
    return mask;
}

vector<MoveCandidate> MoveGenerator::generate(const LetterBoard &board, const TileRack &rack, Dawg &dict) {
    vector<MoveCandidate> candidates;
    candidates.reserve(2000);

    // Covert Rack to Histogram (int array) for fast access.
    int rackCounts[27] = {0}; // 0-25: A-Z, 26: Blank
    for (const Tile& t : rack) {
        if (t.letter == '?') rackCounts[26]++;
        else if (isalpha(t.letter)) rackCounts[toupper(t.letter) - 'A']++;
    }

    // Verticle columns (transposed)
    // DRY optimization. We rotate the board.
    LetterBoard transposed;
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            transposed[c][r] = board[r][c];
        }
    }

    // Pre-calculate Constraints (Fast enough to do sequentially)
    RowConstraint constraintsH[15];
    RowConstraint constraintsV[15];

    for(int i=0; i<15; i++) {
        constraintsH[i] = ConstraintGenerator::generateRowConstraint(board, i);
        constraintsV[i] = ConstraintGenerator::generateRowConstraint(transposed, i);
    }

    // 3. Multi-Threading Setup
    unsigned int nThreads = thread::hardware_concurrency();
    if (nThreads == 0) nThreads = 2; // Safety fallback

    // We create tasks. Each task processes a chunk of rows.
    vector<future<vector<MoveCandidate>>> futures;
    int rowsPerThread = 15 / nThreads;
    if (rowsPerThread == 0) rowsPerThread = 1;

    for (int t = 0; t < nThreads; t++) {
        int start = t * rowsPerThread;
        int end = (t == nThreads - 1) ? 15 : start + rowsPerThread;

        futures.push_back(async(launch::async,
            [start, end, &board, &transposed, &constraintsH, &constraintsV, rackCounts, &dict]() {
            vector<MoveCandidate> localRes;
            localRes.reserve(200);

            // Local Rack copy for this thread
            int localRack[27];

            // Horizontal
            for (int r = start; r < end; r++) {
                memcpy(localRack, rackCounts, sizeof(localRack)); // Reset rack
                genMovesGADDAG(r, board, localRack, constraintsH[r], true, localRes, dict);
            }
            // Vertical (Scanning transposed board)
            for (int r = start; r < end; r++) {
                memcpy(localRack, rackCounts, sizeof(localRack)); // Reset rack
                genMovesGADDAG(r, transposed, localRack, constraintsV[r], false, localRes, dict);
            }
            return localRes;
        }));
    }

    // Collect Results
    for (auto &f : futures) {
        vector<MoveCandidate> res = f.get(); // Blocks until thread finishes
        candidates.insert(candidates.end(), res.begin(), res.end());
    }
    // DEDUPLICATION
    // GADDAG generates the same word from multiple anchors. We must filter them.
    if (!candidates.empty()) {
        // Sort first (required for std::unique)
        sort(candidates.begin(), candidates.end(),
            [](const MoveCandidate& a, const MoveCandidate& b) {
                if (a.row != b.row) return a.row < b.row;
                if (a.col != b.col) return a.col < b.col;
                if (a.isHorizontal != b.isHorizontal) return a.isHorizontal; // False < True
                return strcmp(a.word, b.word) < 0;
        });

        // Remove duplicates
        auto last = unique(candidates.begin(), candidates.end(),
            [](const MoveCandidate& a, const MoveCandidate& b) {
                return a.row == b.row &&
                       a.col == b.col &&
                       a.isHorizontal == b.isHorizontal &&
                       strcmp(a.word, b.word) == 0;
        });

        candidates.erase(last, candidates.end());
    }

    return candidates;
}


void MoveGenerator::genMovesGADDAG(int row,
                              const LetterBoard &board,
                              int *rackCounts,
                              const RowConstraint &constraints,
                              bool isHorizontal,
                              vector<MoveCandidate> &results,
                              Dawg& dict) {
    // Calculate Board Mask (Tiles that already exist in this row)
    uint32_t boardRowMask = 0;
    for(int c = 0; c < 15; c++) {
        if(board[row][c] != ' ') {
            boardRowMask |= (1 << (board[row][c] - 'A'));
        }
    }

    // Create the "Universe" Mask (My Rack + The Board Row)
    // If a letter isn't in this mask, it is IMPOSSIBLE to use it in this move.
    uint32_t myRackMask = getRackMask(rackCounts);
    uint32_t pruningMask = myRackMask | boardRowMask | (1 << SEPERATOR);

    // Buffer for building words (Max length 15 + safety)
    char wordBuf[20];

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

        // Start recursion
        wordBuf[0] = '\0';

        // Start GADDAG search at this achor (going left)
        // Passing the rack mask for speed
        goLeft(row, c, dict.rootIndex, constraints, myRackMask, pruningMask,
            rackCounts, wordBuf, 0, board, isHorizontal, c, results, dict);
    }
}

void MoveGenerator::goLeft(int row,
                      int col,
                      int node,
                      const RowConstraint &constraints,
                      uint32_t rackMask,
                      uint32_t pruningMask,
                      int* rackCounts,
                      char *wordBuf,
                      int wordLen,
                      const LetterBoard &board,
                      bool isHoriz,
                      int anchorCol,
                      vector<MoveCandidate> &results,
                      Dawg& dict) {
    // Process the current node (Can we stop going left?)
    // If we have a seperate edge here, it means we can turn around and go right
    // in GADDAG the seperator is an edge to a new node.

    bool canStopGoingLeft = (col < 0) || (board[row][col] == ' ');

    if (canStopGoingLeft) {
        int sepIndex = SEPERATOR;
        if ((dict.nodes[node].edgeMask >> sepIndex) & 1) {
            int separatorNode = dict.getChild(node, sepIndex);

            // "Play" the word so far (it was built backwards)
            // We need to reverse it to get correct prefix
            // But since we built it in wordBuf, we just need to REVERSE wordBuf
            // However, goRight appends. So we need a new buffer for goRight.
            char rightBuf[20];
            for (int i=0; i<wordLen; i++) {
                rightBuf[i] = wordBuf[wordLen - 1 - i]; //Reverse Copy
            }
            int rightLen = wordLen;

            // Send it to goRight
            goRight(row, anchorCol + 1, separatorNode, constraints, rackMask, pruningMask,
                rackCounts, rightBuf, rightLen, board, isHoriz, anchorCol, results, dict);
        }
    }

    // 2. Continue Going Left?
    if (col < 0) return; // Hit edge

    // Get constraints for this square
    char boardChar = (col >= 0) ? board[row][col] : ' ';
    uint32_t boardMask = (col >= 0) ? constraints.masks[col] : 0;

    // Calculate effective mask
    uint32_t dictMask = dict.nodes[node].edgeMask;
    uint32_t effectiveMask = dictMask;

    if (boardChar != ' ') {
        // Square is occupied. We MUST match the board letter.
        int charIdx = boardChar - 'A';
        if (!((effectiveMask >> charIdx) & 1)) return; // Dict doesn't have this letter

        wordBuf[wordLen] = boardChar;
        goLeft(row, col - 1, dict.getChild(node, charIdx), constraints, rackMask, pruningMask,
            rackCounts, wordBuf, wordLen + 1, board, isHoriz, anchorCol, results, dict);
    }else {
        // Square is empty. We place a tile.
        // Hybrid Check: Dict & Rack & Vertical_Constraints
        effectiveMask &= (rackMask & boardMask);

        // Iterate set bits (optimization)
        while (effectiveMask) {
            int i = __builtin_ctz(effectiveMask); // Get index of first set bit
            char letter = (char)('A' + i);
            int nextNode = dict.getChild(node, i);

            // Oracle Lookahead: Does the subtree contain any letters we actually have?
            if (!dict.canPrune(nextNode, pruningMask)) {

                bool usedBlank = false;
                if (rackCounts[i] > 0) {
                    rackCounts[i]--;
                } else if (rackCounts[26] > 0) {
                    rackCounts[26]--;
                    usedBlank = true;
                    letter = tolower(letter); // Mark blank as lowercase
                } else {
                    effectiveMask &= ~(1 << i); continue;
                }

                wordBuf[wordLen] = letter;

                // Recalculate Rack Mask if we run out of a letter
                uint32_t nextRackMask = rackMask;
                if (usedBlank && rackCounts[26] == 0) {
                    // Blank exhausted: Re-scan rack
                    nextRackMask = getRackMask(rackCounts);
                } else if (!usedBlank && rackCounts[i] == 0) {
                    // Specific letter exhausted
                    nextRackMask &= ~(1 << i);
                }

                // NOTE: We do NOT aggressively reduce 'pruningMask' here.
                // Reducing it without checking the board bits would over-prune valid moves.
                // Keeping it 'permissive' is safer for correctness.

                goLeft(row, col - 1, nextNode, constraints, nextRackMask, pruningMask,
                    rackCounts, wordBuf, wordLen + 1, board, isHoriz, anchorCol, results, dict);

                // Backtrack
                if (usedBlank) rackCounts[26]++;
                else rackCounts[i]++;
            }
            effectiveMask &= ~(1 << i); // Clear bit and continue
        }
    }
}


void MoveGenerator::goRight(int row,
                       int col,
                       int node,
                       const RowConstraint &constraints,
                       uint32_t rackMask,
                       uint32_t pruningMask,
                       int* rackCounts,
                       char *wordBuf,
                       int wordLen,
                       const LetterBoard &board,
                       bool isHoriz,
                       int anchorCol,
                       vector<MoveCandidate> &results,
                       Dawg& dict) {
    // 1. Check if we formed a valid word
    if (dict.nodes[node].isEndOfWord) {
        // Valid word!
        // We need to verify we aren't butting up against another tile
        // Standard check: is next square empty?
        if ((col > 14) || (board[row][col] == ' ')) {
            MoveCandidate cand;

            // 1. Coordinates
            cand.score = 0; // Calculated later
            cand.isHorizontal = isHoriz;
            int len = wordLen;
            int startC = col - len;
            cand.row = (short)(isHoriz ? row : startC);
            cand.col = (short)(isHoriz ? startC : row);

            // 2. Word Copy (Zero Allocation)
            memcpy(cand.word, wordBuf, len);
            cand.word[len] = '\0'; // Null terminate

            // 3. Leave Generation (Zero Allocation, No Strings)
            // We just fill the char array directly.
            int leaveIdx = 0;
            for(int i=0; i<26; i++) {
                int count = rackCounts[i];
                if (count > 0) {
                    // Manual unroll for small counts
                    cand.leave[leaveIdx++] = (char)('A' + i);
                    if (count > 1) cand.leave[leaveIdx++] = (char)('A' + i);
                    // Scrabble racks rarely have >2 of same letter left after a move
                }
            }
            for(int k=0; k<rackCounts[26]; k++) cand.leave[leaveIdx++] = '?';
            cand.leave[leaveIdx] = '\0';

            results.push_back(cand);
        }
    }

    if (col > 14) return;

    // 2. Continue Going Right
    char boardChar = board[row][col];
    uint32_t boardMask = constraints.masks[col];
    uint32_t dictMask = dict.nodes[node].edgeMask;
    uint32_t effectiveMask = dictMask;

    if (boardChar != ' ') {
        // Occupied
        int charIdx = boardChar - 'A';
        if (!((effectiveMask >> charIdx) & 1)) return;

        wordBuf[wordLen] = boardChar;
        goRight(row, col + 1, dict.getChild(node, charIdx), constraints, rackMask, pruningMask,
            rackCounts, wordBuf, wordLen + 1, board, isHoriz, anchorCol, results, dict);
    } else {
        // Empty
        effectiveMask &= (rackMask & boardMask);

        while (effectiveMask) {
            int i = __builtin_ctz(effectiveMask);
            char letter = (char)('A' + i);
            int nextNode = dict.getChild(node, i);

            if (!dict.canPrune(nextNode, pruningMask)) {
                bool usedBlank = false;
                if (rackCounts[i] > 0) {
                    rackCounts[i]--;
                } else if (rackCounts[26] > 0) {
                    rackCounts[26]--;
                    usedBlank = true;
                    letter = tolower(letter);
                } else {
                    effectiveMask &= ~(1 << i); continue;
                }

                wordBuf[wordLen] = letter;

                uint32_t nextRackMask = rackMask;
                if (usedBlank && rackCounts[26] == 0) {
                    nextRackMask = getRackMask(rackCounts);
                } else if (!usedBlank && rackCounts[i] == 0) {
                    nextRackMask &= ~(1 << i);
                }

                goRight(row, col + 1, nextNode, constraints, nextRackMask, pruningMask,
                    rackCounts, wordBuf, wordLen + 1, board, isHoriz, anchorCol, results, dict);

                if (usedBlank) rackCounts[26]++;
                else rackCounts[i]++;
            }
            effectiveMask &= ~(1 << i);
        }
    }
}

}