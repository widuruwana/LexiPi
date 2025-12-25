#include "../include/ruleset.h"
#include "../include/logger.h"
#include <algorithm>

using namespace std;

// Letter -> points
static int letterPoints(char ch) {
    ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));

    switch (ch) {
        case 'A': case 'E': case 'I': case 'O': case 'U':
        case 'N': case 'R': case 'T': case 'L': case 'S':
            return 1;
        case 'D': case 'G':
            return 2;
        case 'B': case 'C': case 'M': case 'P':
            return 3;
        case 'F': case 'H': case 'V': case 'W': case 'Y':
            return 4;
        case 'K':
            return 5;
        case 'J': case 'X':
            return 8;
        case 'Q': case 'Z':
            return 10;
        default:
            return 0;
    }
}

// Score the main word that includes the square at (anchorRow,anchorCol)
static int scoreMainWord(const Board &bonusBoard, const LetterBoard &letters, const BlankBoard &blanks,
                        bool newTile[BOARD_SIZE][BOARD_SIZE], int anchorRow, int anchorCol, bool horizontal) {
    int dr = horizontal ? 0 : 1;
    int dc = horizontal ? 1 : 0;

    // Step backwards to find the true start of the word.
    int r = anchorRow;
    int c = anchorCol;

    // finding the start of the formed word.
    while (true) {
        int pr = r - dr;
        int pc = c - dc;
        if (pr < 0 || pr >= BOARD_SIZE || pc < 0 || pc >= BOARD_SIZE) {
            break;
        }
        if (letters[pr][pc] == ' ') break;

        //True start of the word (letters[r][c])
        r = pr;
        c = pc;
    }

    int totalLetterScore = 0;
    int wordMultiplier = 1;

    // Walking forward, until we hit an empty squre
    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && letters[r][c] != ' ') {

        char ch = letters[r][c];

        // No letter score if its a blank.
        int base = blanks[r][c] ? 0 : letterPoints(ch);
        int letterScore = base;

        if (newTile[r][c]) {
            CellType cell = bonusBoard[r][c];

            // Letter multiplier only affect the newly placed non-blank tiles.
            if (!blanks[r][c]) {
                if (cell == CellType::DLS) {
                    letterScore *= 2;
                }
                if (cell == CellType::TLS) {
                    letterScore *= 3;
                }
            }

            // Word multipliers only count if a new tile is on them
            if (cell == CellType::DWS) {
                wordMultiplier *= 2;
            }
            if (cell == CellType::TWS) {
                wordMultiplier *= 3;
            }
        }

        totalLetterScore += letterScore;

        r += dr;
        c += dc;
    }

    return (totalLetterScore * wordMultiplier);
}

// Score a cross word created by placing a tile at (anchorRow, anchorCol)
static int scoreCrossWord(const Board &bonusBoard, const LetterBoard &letters, const BlankBoard &blanks,
                        bool newTile[BOARD_SIZE][BOARD_SIZE], int anchorRow, int anchorCol, bool mainHorizontal) {
    // perpendicular direction
    int dr = mainHorizontal ? 1 : 0;
    int dc = mainHorizontal ? 0 : 1;

    int r = anchorRow;
    int c = anchorCol;

    // First check if there is any neighbor in that perpendicular direction
    bool hasNeighbor = false;

    // checking either side
    int nr = r - dr;
    int nc = c - dc;
    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && letters[nr][nc] !=' ') {
        hasNeighbor = true;
    }

    nr = r + dr;
    nc = c + dc;
    if (!hasNeighbor && nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && letters[nr][nc] !=' ') {
        hasNeighbor = true;
    }

    // Single tile with no adjacent letter in cross direction
    if (!hasNeighbor) {
        return 0;
    }

    // Back up to the start of the cross word
    r = anchorRow;
    c = anchorCol;

    while (true) {
        int pr = r - dr;
        int pc = c - dc;
        if (pr < 0 || pr >= BOARD_SIZE || pc < 0 || pc >= BOARD_SIZE) {
            break;
        }
        if (letters[pr][pc] == ' ') {
            break;
        }

        // start of the cross word.
        r = pr;
        c = pc;
    }

    int totalLetterScore = 0;
    int wordMultiplier = 1;
    int length = 0;

    // Walk forward along the cross-word direction
    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && letters[r][c] != ' ') {

        ++length;

        char ch = letters[r][c];
        int base = blanks[r][c] ? 0 : letterPoints(ch);
        int letterScore = base;

        if (newTile[r][c]) {
            CellType cell = bonusBoard[r][c];

            if (!blanks[r][c]) {
                if (cell == CellType::DLS) {
                    letterScore *= 2;
                }else if (cell == CellType::TLS) {
                    letterScore *= 3;
                }
            }

            if (cell == CellType::DWS) {
                wordMultiplier *= 2;
            } else if (cell == CellType::TWS) {
                wordMultiplier *= 3;
            }
        }

        totalLetterScore += letterScore;
        r += dr;
        c += dc;
    }

    if (length <= 1) {
        // safety: no real cross-words formed
        return 0;
    }

    return totalLetterScore * wordMultiplier;
}

Ruleset& Ruleset::getInstance() {
    static Ruleset instance;
    return instance;
}

void Ruleset::initialize(
    const std::string& lexiconId,
    int bingoBonus,
    int rackSize,
    int boardSize) {
    
    if (initialized) {
        LOG_WARN("Ruleset already initialized. Skipping re-initialization.");
        return;
    }
    
    this->lexiconId = lexiconId;
    this->bingoBonus = bingoBonus;
    this->rackSize = rackSize;
    this->boardSize = boardSize;
    
    initialized = true;
    
    LOG_INFO("Ruleset initialized:");
    LOG_INFO("  Lexicon: ", lexiconId);
    LOG_INFO("  Bingo Bonus: ", bingoBonus);
    LOG_INFO("  Rack Size: ", rackSize);
    LOG_INFO("  Board Size: ", boardSize);
}

int Ruleset::scoreMove(const Board& bonusBoard, const LetterBoard& letters, const BlankBoard& blanks, const Move& move) {
    if (move.type != MoveType::PLAY) return 0;
    
    // Reconstruct newTile array
    bool newTile[BOARD_SIZE][BOARD_SIZE] = {false};
    // We also need a temporary view of the board WITH the new tiles
    LetterBoard tempLetters = letters;
    BlankBoard tempBlanks = blanks;
    
    for (const auto& p : move.placements) {
        if (p.row >= 0 && p.row < BOARD_SIZE && p.col >= 0 && p.col < BOARD_SIZE) {
            newTile[p.row][p.col] = true;
            tempLetters[p.row][p.col] = p.letter;
            tempBlanks[p.row][p.col] = p.is_blank;
        }
    }
    
    if (move.placements.empty()) return 0;
    
    // Find orientation
    bool horizontal = move.horizontal;
    if (move.placements.size() > 1) {
        if (move.placements[0].row != move.placements[1].row) {
            horizontal = false;
        }
    }
    
    // Use the first placement as anchor
    int startRow = move.placements[0].row;
    int startCol = move.placements[0].col;
    
    // Score main word
    int mainScore = scoreMainWord(bonusBoard, tempLetters, tempBlanks, newTile, startRow, startCol, horizontal);
    
    // Score cross words
    int crossScore = 0;
    for (const auto& p : move.placements) {
        crossScore += scoreCrossWord(bonusBoard, tempLetters, tempBlanks, newTile, p.row, p.col, horizontal);
    }
    
    int totalScore = mainScore + crossScore;
    
    // Bingo bonus
    if (move.placements.size() == 7) {
        totalScore += bingoBonus;
    }
    
    return totalScore;
}
