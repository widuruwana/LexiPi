#include "../include/ai_player.h"
#include "../include/heuristics.h" // For scoring
#include "../include/tile_tracker.h"
#include "../include/spectre/move_generator.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <future> // for async
#include <thread> // for hardware_concurrency

#include "../include/spectre/vanguard.h"

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

// Helper to calculate score for a specific move
int calculateTrueScore(const spectre::MoveCandidate &move, const LetterBoard& letters, const Board &bonusBoard) {

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
        if (letter == '\0') break; // STOP at null terminator (C-String fix)

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

    // FIX: use strlen instead of .length() for char array
    if (std::strlen(move.word) > 1) {
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

AIPlayer::DifferentialMove AIPlayer::calculateEngineMove(const LetterBoard &letters, const spectre::MoveCandidate &bestMove) {

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
        if (letter == '\0') break; // FIX: Stop loop at null terminator

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

    candidates.clear();
    const TileRack &myRack = me.rack;
    spectre::MoveCandidate bestMove;
    bestMove.word[0] = '\0';
    bestMove.score = -1;

    // SHARED INTELLIGENCE: Track Tiles Correctly
    // We do this for BOTH bots now, to ensure Heuristics are accurate.
    TileTracker tracker;

    // 1. Mark Board (checking for blanks correctly)
    for(int r=0; r<15; r++) {
        for(int c=0; c<15; c++) {
            if (letters[r][c] != ' ') {
                // If it's a blank tile on board, we must mark '?' as seen
                if (blankBoard[r][c]) tracker.markSeen('?');
                else tracker.markSeen(letters[r][c]);
            }
        }
    }
    // 2. Mark My Rack
    for(const auto& t : myRack) tracker.markSeen(t.letter);

    // 3. Update Heuristics based on what's left
    Heuristics::updateWeights(tracker);

    // ---------------------------------------------------------
    // BRANCH 1: SPEEDI_PI (The Speedster)
    // ---------------------------------------------------------
    if (style == AIStyle::SPEEDI_PI) {
        // 1. Raw Generation (Fastest)
        findAllMoves(letters, myRack);

        if (!candidates.empty()) {
            // Static Scoring
            for (auto &cand : candidates) {
                int boardPoints = calculateTrueScore(cand, letters, bonusBoard);

                // Leave Heuristics
                float leaveVal = 0.0f;
                for (int i=0; cand.leave[i] != '\0'; i++) {
                    leaveVal += Heuristics::getLeaveValue(cand.leave[i]);
                }

                cand.score = boardPoints + (int)leaveVal;
            }
            // Sort
            std::sort(candidates.begin(), candidates.end(),
                [](const spectre::MoveCandidate &a, const spectre::MoveCandidate &b) {
                    return a.score > b.score;
            });
            // Pick Top
            bestMove = candidates[0];
        }
    }
    // ---------------------------------------------------------
    // BRANCH 2: CUTIE_PI (The Champion)
    // ---------------------------------------------------------
    else {
        // Perfect Information Reconstruction
        // Instead of passing the "Bag", we pass "Everything We Don't See".
        // In the endgame (Bag empty), this contains EXACTLY the opponent's rack.
        vector<char> unseenPool;
        // TileTracker already computed Total Unseen = Bag + Opponent Rack
        // We just need to reconstruct the list from the tracker's counts.
        string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?";
        for (char c : alphabet) {
            int count = tracker.getUnseenCount(c);
            for (int k=0; k<count; k++) {
                unseenPool.push_back(c);
            }
        }

        // Run Vanguard MCTS
        // 50ms for "Fast" Sim in this context (or increase for Tournament Mode)
        bestMove = spectre::Vanguard::search(
            letters,
            bonusBoard,
            myRack,
            unseenPool, // Bag + opponents rack
            gDawg,
            500
        );
    }

    // ---------------------------------------------------------
    // COMMON EXECUTION
    // ---------------------------------------------------------

    bool shouldExchange = false;
    if (bestMove.word[0] == '\0') {
        shouldExchange = true;
    }
    else if (bestMove.score < 14 && isRackBad(myRack) && bag.size() >= 7) {
        shouldExchange = true;
    }

    if (shouldExchange) {
        if (bag.size() < 7) {
            if (bestMove.word[0] == '\0') {
                Move p; p.type=MoveType::PASS; return p;
            }
        } else {
            Move exMove;
            exMove.type = MoveType::EXCHANGE;
            exMove.exchangeLetters = getTilesToExchange(myRack);
            exMove.word = "";
            return exMove;
        }
    }

    DifferentialMove engineMove = calculateEngineMove(letters, bestMove);
    if (engineMove.row == -1 || engineMove.word.empty()) {
        Move passMove; passMove.type = MoveType::PASS; return passMove;
    }

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

// Bridge to the S.P.E.C.T.R.E. Engine
void AIPlayer::findAllMoves(const LetterBoard &letters, const TileRack &rack) {
    this->candidates = spectre::MoveGenerator::generate(letters, rack, gDawg);
}