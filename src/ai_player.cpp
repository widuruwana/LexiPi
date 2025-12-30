#include "../include/ai_player.h"
#include "../include/board.h"
#include "../include/move.h"
#include "../include/heuristics.h"
#include "../include/spectre/logger.h" // [NEW] Thread-safe logging
#include <iostream>
#include <algorithm>
#include <vector>
#include <cstring>
#include "../include/spectre/vanguard.h"
#include "../include/spectre/judge.h"

// [FIX] Namespace visibility
using namespace spectre;
using namespace std;

// Helper function to calculate the score of a move
int calculateTrueScoreInternal(const spectre::MoveCandidate& move, const LetterBoard& letters, const Board& bonusBoard) {
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
        int letterScore = (islower(letter)) ? 0 : Heuristics::getTileValue(letter);

        bool isNewlyPlaced = (letters[r][c] == ' ');

        if (isNewlyPlaced) {
            tilesPlacedCount++;
            CellType bonus = bonusBoard[r][c];
            if (bonus == CellType::DLS) letterScore *= 2;
            else if (bonus == CellType::TLS) letterScore *= 3;
            if (bonus == CellType::DWS) mainWordMultiplier *= 2;
            else if (bonus == CellType::TWS) mainWordMultiplier *= 3;
        }
        mainWordScore += letterScore;

        if (isNewlyPlaced) {
            int pdr = move.isHorizontal ? 1 : 0;
            int pdc = move.isHorizontal ? 0 : 1;

            bool hasNeighbour = false;
            if (r - pdr >= 0 && c - pdc >= 0 && letters[r - pdr][c - pdc] != ' ') hasNeighbour = true;
            else if (r + pdr < 15 && c + pdc < 15 && letters[r + pdr][c + pdc] != ' ') hasNeighbour = true;

            if (hasNeighbour) {
                int currR = r;
                int currC = c;
                while (currR - pdr >= 0 && currC - pdc >= 0 && letters[currR - pdr][currC - pdc] != ' ') {
                    currR -= pdr;
                    currC -= pdc;
                }

                int crossScore = 0;
                int crossMult = 1;

                while (currR < 15 && currC < 15) {
                    char cellLetter;
                    if (currR == r && currC == c) {
                        cellLetter = letter;
                        int crossLetterScore = (islower(cellLetter)) ? 0 : Heuristics::getTileValue(cellLetter);
                        CellType crossBonus = bonusBoard[currR][currC];
                        if (crossBonus == CellType::DLS) crossLetterScore *= 2;
                        else if (crossBonus == CellType::TLS) crossLetterScore *= 3;
                        if (crossBonus == CellType::DWS) crossMult *= 2;
                        else if (crossBonus == CellType::TWS) crossMult *= 3;
                        crossScore += crossLetterScore;
                    } else {
                        cellLetter = letters[currR][currC];
                        if (cellLetter == ' ') break;
                        crossScore += Heuristics::getTileValue(cellLetter);
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

    if (strlen(move.word) > 1) {
        totalScore += (mainWordScore * mainWordMultiplier);
    }

    if (tilesPlacedCount == 7) totalScore += 50;

    return totalScore;
}

AIPlayer::AIPlayer(AIStyle style) : style(style) {}

string AIPlayer::getName() const {
    return (style == AIStyle::SPEEDI_PI) ? "Speedi_Pi" : "Cutie_Pi";
}

AIPlayer::DifferentialMove AIPlayer::calculateEngineMove(const LetterBoard &letters, const spectre::MoveCandidate &bestMove) {
    DifferentialMove diff;
    diff.row = -1; diff.col = -1; diff.word = "";
    int r = bestMove.row; int c = bestMove.col;
    int dr = bestMove.isHorizontal ? 0 : 1; int dc = bestMove.isHorizontal ? 1 : 0;

    for (int i = 0; bestMove.word[i] != '\0'; i++) {
        if (letters[r][c] == ' ') {
            if (diff.row == -1) { diff.row = r; diff.col = c; }
            diff.word += bestMove.word[i];
        }
        r += dr; c += dc;
    }
    return diff;
}

bool AIPlayer::isRackBad(const TileRack &rack) {
    int vowels = 0, consonants = 0;
    bool hasQ = false, hasU = false;
    int counts[26] = {0};

    for (const Tile &t : rack) {
        if (t.letter == '?') { vowels++; consonants++; continue; }
        if (strchr("AEIOU", t.letter)) vowels++; else consonants++;
        if (t.letter == 'Q') hasQ = true;
        if (t.letter == 'U') hasU = true;
        if (isalpha(t.letter)) counts[toupper(t.letter) - 'A']++;
    }
    if (hasQ && !hasU) return true;
    if (rack.size() >= 5 && (vowels == 0 || consonants == 0)) return true;
    for (int i=0; i<26; i++) if (counts[i] >= 3) return true;
    return false;
}

string AIPlayer::getTilesToExchange(const TileRack &rack) {
    string toExchange = "";
    bool hasQ = false;
    for(auto t : rack) if(t.letter=='Q') hasQ=true;

    for (const Tile &t : rack) {
        bool keep = false;
        if (t.letter == '?' || t.letter == 'S') keep = true;
        if (t.letter == 'U' && hasQ) keep = true;
        string retain = "RETINA";
        if (retain.find(t.letter) != string::npos && !keep) keep = true;
        if (!keep) toExchange += t.letter;
    }
    if (toExchange.empty() && !rack.empty()) {
        for(auto t : rack) toExchange += t.letter;
    }
    return toExchange;
}

bool AIPlayer::shouldChallenge(const Move &opponentMove, const LetterBoard &board) const {
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

    TileTracker tracker;
    for(int r=0; r<15; r++) for(int c=0; c<15; c++) if (letters[r][c] != ' ') {
        if (blankBoard[r][c]) tracker.markSeen('?'); else tracker.markSeen(letters[r][c]);
    }
    for(const auto& t : myRack) tracker.markSeen(t.letter);
    spectre::Heuristics::updateWeights(tracker);

    // =========================================================
    // PHASE 2: THE JUDGE (ENDGAME SOLVER)
    // =========================================================
    if (bag.empty() && style == AIStyle::CUTIE_PI) {
        {
            // [FIX] Thread-safe logging
            std::lock_guard<std::mutex> lock(spectre::console_mutex);
            cout << "[AI] Phase 2: Activating The Judge." << endl;
        }

        TileRack oppRack;
        string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?";
        for (char c : alphabet) {
            int count = tracker.getUnseenCount(c);
            for (int k=0; k<count; k++) {
                Tile t; t.letter = c; t.points = spectre::Heuristics::getTileValue(c);
                oppRack.push_back(t);
            }
        }

        Move endgameMove = spectre::Judge::solveEndgame(letters, bonusBoard, myRack, oppRack, gDawg);

        if (endgameMove.type == MoveType::PLAY) {
             std::lock_guard<std::mutex> lock(spectre::console_mutex);
             cout << "[AI] Judge SUCCESS. Playing: " << endgameMove.word << endl;
             return endgameMove;
        }
    }

    // =========================================================
    // STANDARD ENGINE
    // =========================================================
    if (style == AIStyle::SPEEDI_PI) {
        auto greedyConsumer = [&](spectre::MoveCandidate& cand, int* rackCounts) -> bool {
            int boardPoints = calculateTrueScoreInternal(cand, letters, bonusBoard);
            cand.score = boardPoints;
            if (cand.score > bestMove.score) bestMove = cand;
            return true;
        };
        spectre::MoveGenerator::generate_custom(letters, myRack, gDawg, greedyConsumer);

        {
            // [FIX] Thread-safe logging
            std::lock_guard<std::mutex> lock(spectre::console_mutex);
            cout << "[AI] Speedi_Pi Best Score: " << bestMove.score << " (" << bestMove.word << ")" << endl;
        }
    }
    else {
        vector<char> unseenPool;
        string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ?";
        for (char c : alphabet) {
            int count = tracker.getUnseenCount(c);
            for (int k=0; k<count; k++) unseenPool.push_back(c);
        }
        bestMove = spectre::Vanguard::search(letters, bonusBoard, myRack, unseenPool, gDawg, 500);

        {
            // [FIX] Thread-safe logging
            std::lock_guard<std::mutex> lock(spectre::console_mutex);
            cout << "[AI] Vanguard Best Score: " << bestMove.score << " (" << bestMove.word << ")" << endl;
        }
    }

    bool shouldExchange = false;
    if (bestMove.word[0] == '\0') shouldExchange = true;
    else if (bestMove.score < 14 && isRackBad(myRack) && bag.size() >= 7) shouldExchange = true;

    if (shouldExchange) {
        if (bag.size() < 7) {
            if (bestMove.word[0] == '\0') return Move::Pass();
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

    // Add tiles to move structure for game engine compatibility
    for (char c : result.word) {
        Tile t; t.letter = c; t.points = Heuristics::getTileValue(c);
        result.tiles.push_back(t);
    }

    return result;
}

Move AIPlayer::getEndGameDecision() {
    Move m; m.type = MoveType::PASS; return m;
}

void AIPlayer::findAllMoves(const LetterBoard &letters, const TileRack &rack) {
    this->candidates = spectre::MoveGenerator::generate(letters, rack, gDawg, false);
}

vector<char> AIPlayer::exchangeTiles(const vector<Tile>& rack) {
    // Simple heuristic: Exchange if no vowels or no consonants
    int vowels = 0;
    string toExchange = "";
    for (const auto& t : rack) {
        if (strchr("AEIOU", t.letter)) vowels++;
    }

    if (vowels == 0 || vowels == rack.size()) {
        for (const auto& t : rack) toExchange += t.letter;
    }

    vector<char> res;
    for(char c : toExchange) res.push_back(c);
    return res;
}