#include "../../include/spectre/spy.h"
#include "../../include/heuristics.h"
#include <algorithm>
#include <cstring>
#include <iostream>

using namespace std;

namespace spectre {

// The "Killer Rack" - statistically most likely to Bingo or punish open spots
// We use: Blank, S, E, T, A, I, N
static const char* KILLER_RACK_STR = "?SETAIN";

int Spy::getRiskScore(const LetterBoard& board, const Board& bonusBoard, Dawg& dict) {
    // 1. Construct the Killer Rack
    TileRack spyRack;
    for (int i = 0; KILLER_RACK_STR[i] != '\0'; i++) {
        spyRack.push_back({KILLER_RACK_STR[i]});
    }

    // 2. Generate Opponent's Best Response
    // We trust MoveGenerator to be fast enough (<1ms)
    vector<MoveCandidate> responses = MoveGenerator::generate(board, spyRack, dict);

    if (responses.empty()) return 0;

    // 3. Find the Maximum Damage
    int maxDamage = 0;
    for (const auto& move : responses) {
        // We only care about raw points here (Damage)
        int damage = calculateSpyScore(board, bonusBoard, move);
        if (damage > maxDamage) {
            maxDamage = damage;
        }
    }

    return maxDamage;
}

int Spy::calculateSpyScore(const LetterBoard& board, const Board& bonusBoard, const MoveCandidate& move) {
    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMult = 1;
    int tilesPlaced = 0;

    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    for (int i = 0; move.word[i] != '\0'; i++) {
        char letter = move.word[i];
        int val = Heuristics::getTileValue(letter);

        bool isNew = (board[r][c] == ' ');
        if (isNew) {
            tilesPlaced++;
            CellType bonus = bonusBoard[r][c];
            if (bonus == CellType::DLS) val *= 2;
            else if (bonus == CellType::TLS) val *= 3;
            if (bonus == CellType::DWS) mainWordMult *= 2;
            else if (bonus == CellType::TWS) mainWordMult *= 3;
        }
        mainWordScore += val;

        // Simplified Cross-Word Score (For speed, Spy assumes validity)
        if (isNew) {
            int pdr = move.isHorizontal ? 1 : 0;
            int pdc = move.isHorizontal ? 0 : 1;
            bool hasNeighbor = false;
            if (r-pdr>=0 && c-pdc>=0 && board[r-pdr][c-pdc]!=' ') hasNeighbor=true;
            if (r+pdr<15 && c+pdc<15 && board[r+pdr][c+pdc]!=' ') hasNeighbor=true;

            if (hasNeighbor) {
                int crossScore = 0;
                int crossMult = 1;
                int cr = r, cc = c;
                while(cr-pdr>=0 && cc-pdc>=0 && board[cr-pdr][cc-pdc]!=' ') { cr-=pdr; cc-=pdc; }
                while(cr<15 && cc<15) {
                    char l = board[cr][cc];
                    if (cr==r && cc==c) {
                        int cv = Heuristics::getTileValue(letter);
                        CellType cb = bonusBoard[cr][cc];
                        if (cb==CellType::DLS) cv*=2; else if(cb==CellType::TLS) cv*=3;
                        if (cb==CellType::DWS) crossMult*=2; else if(cb==CellType::TWS) crossMult*=3;
                        crossScore += cv;
                    } else if (l != ' ') {
                        crossScore += Heuristics::getTileValue(l);
                    } else break;
                    cr+=pdr; cc+=pdc;
                }
                totalScore += (crossScore * crossMult);
            }
        }
        r += dr;
        c += dc;
    }

    if (strlen(move.word) > 1) totalScore += (mainWordScore * mainWordMult);
    if (tilesPlaced == 7) totalScore += 50;

    return totalScore;
}

}