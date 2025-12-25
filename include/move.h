#pragma once

#include <string>
#include <vector>
#include "board.h"
#include "tiles.h"
#include "rack.h"

using namespace std;

// Game can have 2-4 players
struct Player {
    TileRack rack;
    int score = 0;
    int passCount = 0;
};

struct TilePlacement {
    int row;
    int col;
    char letter; // The letter placed (A-Z)
    bool is_blank; // Was this a blank tile?
};

// Result of trying to play a word
struct MoveResult {
    bool success;
    int score;
    string errorMessage;
    vector<TilePlacement> placements; // The explicit placements made
};

enum class MoveType {
    PLAY, // Placing a word
    PASS, // Passing a turn
    EXCHANGE, // Swap tiles with bag
    CHALLENGE, // Challenge previous move
    QUIT, // Resign
    NONE // (Internal Use)
};

// To hold the move data
struct Move {
    MoveType type = MoveType::NONE;

    // Canonical representation
    vector<TilePlacement> placements; // For PLAY
    vector<Tile> exchangeTiles; // For EXCHANGE

    // Legacy/Convenience fields (kept for compatibility but placements is source of truth)
    string word; // Playing word/exchanging tiles
    int row = -1;
    int col = -1;
    bool horizontal = true;

    // For exchange
    string exchangeLetters;

    static Move Pass() { return {MoveType::PASS}; }
    static Move Quit() { return {MoveType::QUIT}; }
    static Move Challenge() { return {MoveType::CHALLENGE}; }
    
    static Move Play(const vector<TilePlacement>& placements) {
        Move m;
        m.type = MoveType::PLAY;
        m.placements = placements;
        // Infer legacy fields if needed, or leave them empty/default
        if (!placements.empty()) {
            m.row = placements[0].row;
            m.col = placements[0].col;
            // Infer horizontal/word? 
            // It's better if the caller provides them if they want them, 
            // but for now let's just set the type and placements.
        }
        return m;
    }

    // Overload for legacy support during refactor, but we should move away from this
    static Move Play(int r, int c, bool h, string w, const vector<TilePlacement>& placements) {
        Move m;
        m.type = MoveType::PLAY;
        m.row = r;
        m.col = c;
        m.horizontal = h;
        m.word = w;
        m.placements = placements;
        return m;
    }
    
    // Legacy Play overload (without placements) - constructs placements from word
    static Move Play(int r, int c, bool h, string w) {
        Move m;
        m.type = MoveType::PLAY;
        m.row = r;
        m.col = c;
        m.horizontal = h;
        m.word = w;
        
        // Construct placements
        int dr = h ? 0 : 1;
        int dc = h ? 1 : 0;
        int cr = r;
        int cc = c;
        
        for (char ch : w) {
            TilePlacement tp;
            tp.row = cr;
            tp.col = cc;
            if (islower(ch)) {
                tp.letter = static_cast<char>(toupper(ch));
                tp.is_blank = true;
            } else {
                tp.letter = ch;
                tp.is_blank = false;
            }
            m.placements.push_back(tp);
            cr += dr;
            cc += dc;
        }
        
        return m;
    }

    static Move Exchange(const vector<Tile>& tiles) {
        Move m;
        m.type = MoveType::EXCHANGE;
        m.exchangeTiles = tiles;
        // Update legacy string
        string s = "";
        for(const auto& t : tiles) s += t.letter;
        m.exchangeLetters = s;
        return m;
    }
    
    // Legacy Exchange
    static Move Exchange(string tilesStr) {
        Move m;
        m.type = MoveType::EXCHANGE;
        m.exchangeLetters = tilesStr;
        // We can't populate exchangeTiles here without the rack/bag context to know if they are blanks
        // This should be avoided.
        return m;
    }
};

// Play a word using the tiles in the rack.
//
// - bonusBoard: fixed DL/TL/DW/TW layout
// - letters:    board letters (A–Z or ' ')
// - blanks:     which board cells are blanks
// - bag:        tile bag (to refill rack)
// - rack:       player's rack (tiles will be consumed/refilled)
// - startRow:   0-based (A=0, B=1, ...)
// - startCol:   0-based (1=>0, 2=>1, ...)
// - horizontal: true = left→right, false = top→bottom
// - rackWord:   ONLY the letters the player is placing from their rack
//               (do NOT include letters already on the board)

MoveResult playWord(
    const Board &bonusBoard,
    LetterBoard &letters,
    BlankBoard &blanks,
    TileBag &bag,
    TileRack &rack,
    int startRow,
    int startCol,
    bool horizontal,
    const string &rackWord
);
