#pragma once

#include <vector>
#include <array>
#include <string>
#include <cstring> // Required for memcpy in high-performance structs

using namespace std;

// =========================================================
// BOARD CONSTANTS & TYPES
// =========================================================
constexpr int BOARD_SIZE = 15; // 15 x 15 Scrabble board
constexpr int CELL_INNER_WIDTH = 5; // Characters inside each cell for UI

enum class CellType {
    Normal,
    DLS, // Double Letter Score
    TLS, // Triple Letter Score
    DWS, // Double Word Score
    TWS  // Triple Word Score
};

enum class MoveType {
    PLAY,      // Placing a word
    PASS,      // Passing a turn
    EXCHANGE,  // Swap tiles with bag
    CHALLENGE, // Challenge previous move
    QUIT,      // Resign
    NONE       // (Internal Use)
};

// Represents a single Scrabble Tile
struct Tile {
    char letter; // 'A' to 'Z' for regular tiles, '?' for unplayed blank
    int points;  // Score value
};

// Standard game collections
using TileRack = vector<Tile>;
using TileBag = vector<Tile>;
using Board = array<array<CellType, BOARD_SIZE>, BOARD_SIZE>;
using LetterBoard = array<array<char, BOARD_SIZE>, BOARD_SIZE>;

// Tracks whether a letter on the board came from a blank tile.
// true = this square is a blank (score 0), false = normal tile
using BlankBoard = array<array<bool, BOARD_SIZE>, BOARD_SIZE>;

// Game can have 2-4 players
struct Player {
    TileRack rack;
    int score = 0;
    int passCount = 0;
};

// =========================================================
// CORE PRIMITIVE: The Move Contract (SBO / POD)
// =========================================================
/**
 * @brief The fundamental atomic unit of a player action.
 * @invariant memory: This struct is strictly POD (Plain Old Data). Zero heap allocations.
 * * @invariant THE FULL STRING DOCTRINE:
 * The `word` array MUST ALWAYS represent the FULL OVERLAPPING STRING placed on the board.
 * It MUST contain BOTH the newly placed tiles AND the existing board tiles.
 * * Example: If 'M' is on the board, and the player places tiles around it to spell "BIRDMAN"
 * using a blank for the 'A', the string MUST be exactly "BIRDMaN".
 * Differential strings (e.g., "BIRaN") are mathematically incompatible with this architecture and STRICTLY BANNED.
 */
struct Move {
    MoveType type = MoveType::NONE;

    // SBO: Max Scrabble word is 15 chars + null terminator.
    // Contains uppercase for real tiles, lowercase for blanks.
    char word[16];

    // The starting geometric coordinate of the FIRST letter in `word`
    int row = -1;
    int col = -1;
    bool horizontal = true;

    // SBO: Max exchange is 7 tiles + null terminator
    char exchangeLetters[8];

    // Default Constructor
    Move() {
        type = MoveType::NONE;
        word[0] = '\0';
        exchangeLetters[0] = '\0';
        row = -1; col = -1;
        horizontal = true;
    }

    Move(MoveType t) {
        type = t;
        word[0] = '\0';
        exchangeLetters[0] = '\0';
        row = -1; col = -1;
        horizontal = true;
    }

    // --- STATIC BUILDERS ---

    static Move Pass() {
        Move m(MoveType::PASS);
        return m;
    }

    static Move Quit() {
        Move m(MoveType::QUIT);
        return m;
    }

    static Move Challenge() {
        Move m(MoveType::CHALLENGE);
        return m;
    }

    /**
     * @pre `w` MUST be a full overlapping string, not a differential string.
     */
    static Move Play(int r, int c, bool h, const string& w) {
        Move m(MoveType::PLAY);
        m.row = r;
        m.col = c;
        m.horizontal = h;

        // Fast Copy (avoiding std::string heap allocation overhead in hot loops)
        size_t len = w.length();
        if (len > 15) len = 15;
        memcpy(m.word, w.c_str(), len);
        m.word[len] = '\0';

        return m;
    }

    static Move Exchange(const string& letters) {
        Move m(MoveType::EXCHANGE);
        m.horizontal = true;
        m.word[0] = ' '; m.word[1] = '\0'; // Space for renderer compatibility

        size_t len = letters.length();
        if (len > 7) len = 7;
        memcpy(m.exchangeLetters, letters.c_str(), len);
        m.exchangeLetters[len] = '\0';

        return m;
    }
};

// Result of trying to play a word
struct MoveResult {
    bool success;
    int score;
    const char* message; // Points to static string literal (Zero allocation)
};