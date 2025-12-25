#include "../include/move.h"
#include "../include/ruleset.h"
#include "../include/move_validator.h"

#include <algorithm>
#include <cctype>
#include <vector>

using namespace std;

// Uppercase helper
static string toUpper(const string &s) {
    string r = s;
    transform(r.begin(), r.end(), r.begin(),
        [](unsigned char ch){ return static_cast<char>(toupper(ch)); });
    return r;
}

// Finding a tile in the rack that can represent a letter.
static int findTilesForLetter(const TileRack &rack, const vector<bool> &used, char letter) {
    char upper = static_cast<char>(toupper(static_cast<unsigned char>(letter)));

    // Trying exact match first
    for (int i=0; i < static_cast<int>(rack.size()); i++ ) {
        if (used[i]) continue;
        if (!rack[i].is_blank && toupper(static_cast<unsigned char>(rack[i].letter)) == upper) {
            return i;
        }
    }

    // for blanks
    for (int i=0; i < static_cast<int>(rack.size()); i++ ) {
        if (used[i]) continue;
        if (rack[i].is_blank) {
            return i;
        }
    }

    return -1;
}

MoveResult playWord(const Board &bonusBoard, LetterBoard &letters, BlankBoard &blanks, TileBag &bag, TileRack &rack,
                    int startRow, int startCol, bool horizontal, const string &rackWordLower) {
    MoveResult res{};
    res.success = false;
    res.score = 0;

    if (rackWordLower.empty()) {
        res.errorMessage = "Empty rack word";
        return res;
    }

    string rackWord = toUpper(rackWordLower);
    int len = static_cast<int>(rackWord.size());
    int dr = horizontal ? 0 : 1;
    int dc = horizontal ? 1 : 0;

    struct NewTile {
        int row;
        int col;
        char letter;
        bool isBlank;
        int rackIndex;
    };

    vector < NewTile > newTiles;
    vector<bool> usedRack(rack.size(), false);

    int r = startRow;
    int c = startCol;
    int wi = 0; 

    // Check starting cell
    if (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && letters[r][c] != ' ') {
        res.errorMessage = "Starting cell should be empty";
        return res;
    }

    while (true) {
        if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) {
            if (wi == len) break;
            res.errorMessage = "Word goes off the board.";
            return res;
        }

        char existing = letters[r][c];

        if (existing == ' ') {
            if (wi >= len) break;

            NewTile nt;
            nt.row = r;
            nt.col = c;
            nt.letter = rackWord[wi];
            nt.isBlank = false;
            nt.rackIndex = -1;
            newTiles.push_back(nt);

            ++wi;
        }

        r += dr;
        c += dc;
    }

    if (wi != len) {
        res.errorMessage = "Not enough space in that direction for your tiles.";
        return res;
    }

    if (newTiles.empty()) {
        res.errorMessage = "Must place at least 1 tile";
        return res;
    }

    // Match tiles from rack
    for (auto &nt : newTiles) {
        int idx = findTilesForLetter(rack, usedRack, nt.letter);
        if (idx == -1) {
            res.errorMessage = "You dont have required tiles in your rack";
            return res;
        }
        usedRack[idx] = true;
        nt.isBlank = rack[idx].is_blank;
        nt.rackIndex = idx;
    }

    // Construct Move object for validation
    Move tempMove;
    tempMove.type = MoveType::PLAY;
    tempMove.horizontal = horizontal;
    tempMove.row = startRow;
    tempMove.col = startCol;
    
    for (const auto &nt : newTiles) {
        TilePlacement tp;
        tp.row = nt.row;
        tp.col = nt.col;
        tp.letter = nt.letter;
        tp.is_blank = nt.isBlank;
        tempMove.placements.push_back(tp);
        res.placements.push_back(tp);
    }

    // Validate using MoveValidator (checks dictionary, connectivity, rack usage)
    // Note: We pass the ORIGINAL letters/blanks because validateMove simulates the move
    ValidationResult valResult = MoveValidator::validateMove(bonusBoard, letters, blanks, rack, tempMove);
    
    if (!valResult.isValid) {
        res.success = false;
        res.errorMessage = valResult.errorMessage;
        return res;
    }

    // If valid, apply changes to board
    for (const auto &nt : newTiles) {
        letters[nt.row][nt.col] = nt.letter;
        blanks[nt.row][nt.col] = nt.isBlank;
    }

    res.success = true;
    res.score = valResult.score;

    // Remove used tiles from rack
    vector<int> usedIndices;
    for (int i = 0; i < static_cast<int>(usedRack.size()); i++) {
        if (usedRack[i]) {
            usedIndices.push_back(i);
        }
    }

    sort(usedIndices.begin(), usedIndices.end(), greater<int>());
    for (int idx: usedIndices) {
        rack.erase(rack.begin() + idx);
    }

    // Refill rack
    if (rack.size() < 7) {
        drawTiles(bag, rack, static_cast<int>(7 - rack.size()));
    }

    return res;
}
