#include "../include/ai_player.h"
#include "../include/heuristics.h"
#include "../include/tile_tracker.h"
#include "../include/spectre/move_generator.h"
#include "../include/spectre/judge.h"
#include "../include/spectre/logger.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <chrono>

#include "../include/spectre/vanguard.h"

using namespace spectre;
using namespace std;
using namespace std::chrono;

const int SEPERATOR = 26;

AIPlayer::AIPlayer(AIStyle style) : style(style) {}

string AIPlayer::getName() const {
    return (style == AIStyle::SPEEDI_PI) ? "Speedi_Pi" : "Cutie_Pi";
}

// Optimization: Prune the search space to boundingBox + 1
struct SearchRange {
    int minRow, maxRow, minCol, maxCol;
    bool isEmpty; // track empty board state
};

// Calculate score for a specific move
static int calculateTrueScoreInternal(const MoveCandidate &move, const LetterBoard& letters, const Board &bonusBoard) {
    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMultiplier = 1;
    int tilesPlacedCount = 0;
    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    for (int i = 0; move.word[i] != '\0'; i++) {
        char letter = move.word[i];

        // [FIX] BLANK TILE HANDLING
        // Lowercase char = Blank used as that letter. Score MUST be 0.
        // Uppercase char = Real tile. Score comes from Heuristics.
        int letterScore = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);

        bool isNewlyPlaced = (letters[r][c] == ' ');

        if (isNewlyPlaced) {
            tilesPlacedCount++;
            CellType bonus = bonusBoard[r][c];
            // Apply Premium Squares
            if (bonus == CellType::DLS) letterScore *= 2;
            else if (bonus == CellType::TLS) letterScore *= 3;
            if (bonus == CellType::DWS) mainWordMultiplier *= 2;
            else if (bonus == CellType::TWS) mainWordMultiplier *= 3;
        }
        mainWordScore += letterScore;

        if (isNewlyPlaced) {
            // Check for Cross-Words (Perpendicular)
            int pdr = move.isHorizontal ? 1 : 0;
            int pdc = move.isHorizontal ? 0 : 1;
            bool hasNeighbour = false;
            if (r-pdr>=0 && c-pdc>=0 && letters[r-pdr][c-pdc]!=' ') hasNeighbour=true;
            else if (r+pdr<15 && c+pdc<15 && letters[r+pdr][c+pdc]!=' ') hasNeighbour=true;

            if (hasNeighbour) {
                int currR = r, currC = c;
                // Rewind to start of cross-word
                while (currR-pdr>=0 && currC-pdc>=0 && letters[currR-pdr][currC-pdc]!=' ') { currR-=pdr; currC-=pdc; }

                int crossScore = 0;
                int crossMult = 1;

                // Scan forward
                while (currR<15 && currC<15) {
                    char cellLetter = letters[currR][currC];
                    if (currR==r && currC==c) {
                        // This is the tile we just placed.
                        // [FIX] Ensure Blank is 0 here too.
                        int crossLetterScore = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);

                        CellType crossBonus = bonusBoard[currR][currC];
                        if (crossBonus == CellType::DLS) crossLetterScore*=2;
                        else if (crossBonus == CellType::TLS) crossLetterScore*=3;
                        if (crossBonus == CellType::DWS) crossMult*=2;
                        else if (crossBonus == CellType::TWS) crossMult*=3;
                        crossScore += crossLetterScore;
                    } else if (cellLetter!=' ') {
                        // Existing tiles on board are always valid/uppercase
                        crossScore += Heuristics::getTileValue(cellLetter);
                    } else break;
                    currR+=pdr; currC+=pdc;
                }
                totalScore += (crossScore * crossMult);
            }
        }
        r += dr; c += dc;
    }

    // Add Main Word Score + Bingo Bonus
    if (strlen(move.word) > 1) totalScore += (mainWordScore * mainWordMultiplier);
    if (tilesPlacedCount == 7) totalScore += 50;

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

    // Expand by one to include the "Influence Zone" (parallel plays)
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
    for (int i = 0; bestMove.word[i] != '\0'; i++) {
        if (letters[r][c] == ' ') {
            if (diff.row == -1) {
                diff.row = r;
                diff.col = c;
            }
            diff.word += bestMove.word[i];
        }
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

    candidates.clear();
    const TileRack &myRack = me.rack;
    MoveCandidate bestMove;
    bestMove.word[0] = '\0';
    bestMove.score = -1;

    // --- DATA FLOW: 1. SPY NETWORK (Tile Tracking) ---
    // We need to know what tiles are left in the universe to infer the opponent's rack.
    TileTracker tracker;

    // Scan the Board: Mark every tile currently on the board as "Seen"
    for(int r=0; r<15; r++) for(int c=0; c<15; c++) if (letters[r][c] != ' ') {
        if (blankBoard[r][c]) tracker.markSeen('?'); else tracker.markSeen(letters[r][c]);
    }
    // Scan My Rack: Mark my own tiles as "Seen"
    for(const auto& t : myRack) tracker.markSeen(t.letter);

    // Dynamic Heuristics: Update letter values based on scarcity (for Midgame)
    Heuristics::updateWeights(tracker);

    // =========================================================
    // PHASE 2: THE JUDGE (ENDGAME SOLVER)
    // CONDITION: Bag is Empty AND we are the Smart Bot (Cutie_Pi)
    // =========================================================
    if (bag.empty() && style == AIStyle::CUTIE_PI) {
        {
            // Thread-safe logging
            std::lock_guard<std::mutex> lock(spectre::console_mutex);
            cout << "[AI] Phase 2: Activating The Judge." << endl;
        }

        // --- DATA FLOW: 2. OPPONENT INFERENCE ---
        // Since the bag is empty, the opponent MUST hold all tiles we haven't seen.
        // We reconstruct the opponent's rack perfectly here.
        TileRack oppRack;
        string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?";
        for (char c : alphabet) {
            int count = tracker.getUnseenCount(c);
            for (int k=0; k<count; k++) {
                Tile t; t.letter = c; t.points = Heuristics::getTileValue(c);
                oppRack.push_back(t);
            }
        }

        // --- DATA FLOW: 3. EXECUTION ---
        // Pass perfect information to the deterministic Minimax solver.
        Move endgameMove = Judge::solveEndgame(letters, bonusBoard, myRack, oppRack, gDawg);

        if (endgameMove.type == MoveType::PLAY) {
            std::lock_guard<std::mutex> lock(spectre::console_mutex);
            cout << "[AI] Judge SUCCESS. Playing: " << endgameMove.word << endl;
            return endgameMove;
        }
    }

    // =========================================================
    // STANDARD ENGINE (Midgame / Speedi_Pi)
    // =========================================================

    if (style == AIStyle::SPEEDI_PI) {
        // ... (Standard Speedi_Pi Logic - Unchanged) ...
        auto greedyConsumer = [&](MoveCandidate& cand, int* rackCounts) -> bool {
            int boardPoints = calculateTrueScoreInternal(cand, letters, bonusBoard);
            // ... heuristic leave calculation ...
            cand.score = boardPoints; // + leaveVal
            if (cand.score > bestMove.score) bestMove = cand;
            return true;
        };
        MoveGenerator::generate_custom(letters, myRack, gDawg, greedyConsumer);

        {
            // Thread-safe logging
            std::lock_guard<std::mutex> lock(spectre::console_mutex);
            cout << "[AI] Speedi_Pi Best Score: " << bestMove.score << " (" << bestMove.word << ")" << endl;
        }
    }
    else {
        // ... (Standard Cutie_Pi Logic - Unchanged) ...
        // Uses Monte Carlo Tree Search (Vanguard) to simulate futures with random opponent racks.
        vector<char> unseenPool;
        string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?";
        for (char c : alphabet) {
            int count = tracker.getUnseenCount(c);
            for (int k=0; k<count; k++) unseenPool.push_back(c);
        }
        bestMove = Vanguard::search(letters, bonusBoard, myRack, unseenPool, gDawg, 500);

        {
            // Thread-safe logging
            std::lock_guard<std::mutex> lock(spectre::console_mutex);
            cout << "[AI] Vanguard Best Score: " << bestMove.score << " (" << bestMove.word << ")" << endl;
        }
    }

    // --- DATA FLOW: 4. FORMATTING & RETURN ---
    // Convert internal 'MoveCandidate' to the Game Engine's 'Move' struct.

    // Check for Exchange/Pass conditions
    bool shouldExchange = false;
    if (bestMove.word[0] == '\0') shouldExchange = true;
    else if (bestMove.score < 14 && isRackBad(myRack) && bag.size() >= 7) shouldExchange = true;

    if (shouldExchange) {
        if (bag.size() < 7) {
            if (bestMove.word[0] == '\0') {
                return Move::Pass();
            }
        } else {
            Move exMove;
            exMove.type = MoveType::EXCHANGE;
            exMove.exchangeLetters = getTilesToExchange(myRack);
            return exMove;
        }
    }

    DifferentialMove engineMove = calculateEngineMove(letters, bestMove);
    if (engineMove.row == -1 || engineMove.word.empty()) return Move::Pass();

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

// Bridge to the S.P.E.C.T.R.E. Engine
void AIPlayer::findAllMoves(const LetterBoard &letters, const TileRack &rack) {
    this->candidates = MoveGenerator::generate(letters, rack, gDawg, false);
}