#include "../include/ai_player.h"  // For MoveCandidate - must be first
#include "../include/move_validator.h"
#include "../include/dict.h"
#include "../include/logger.h"
#include "../include/tiles.h"  // For getTileValue
#include "../include/ruleset.h"
#include <algorithm>
#include <cctype>

using namespace std;

Move MoveValidator::fromCandidate(const MoveCandidate& candidate) {
    Move move;
    move.type = MoveType::PLAY;
    move.horizontal = candidate.isHorizontal;
    move.row = candidate.row;
    move.col = candidate.col;
    move.word = candidate.word;
    
    // Parse word into placements with blank tracking
    // Note: This assumes candidate.word ONLY contains new tiles, or caller handles filtering.
    // If candidate.word contains existing tiles, this will create placements for them,
    // which might cause rack validation to fail.
    int r = candidate.row;
    int c = candidate.col;
    int dr = candidate.isHorizontal ? 0 : 1;
    int dc = candidate.isHorizontal ? 1 : 0;
    
    for (char ch : candidate.word) {
        TilePlacement placement;
        placement.row = r;
        placement.col = c;
        placement.is_blank = islower(ch);
        placement.letter = static_cast<char>(toupper(ch));
        
        move.placements.push_back(placement);
        
        r += dr;
        c += dc;
    }
    
    return move;
}

vector<string> MoveValidator::extractFormedWords(
    const LetterBoard& letters,
    const Move& move) {
    
    vector<string> words;
    
    if (move.placements.empty()) return words;

    // Determine orientation and start from placements or move fields
    bool isHorizontal = move.horizontal;
    int startRow = move.placements[0].row;
    int startCol = move.placements[0].col;
    
    // Extract main word
    int r = startRow;
    int c = startCol;
    int dr = isHorizontal ? 0 : 1;
    int dc = isHorizontal ? 1 : 0;
    
    // Find start of main word
    while (r - dr >= 0 && r - dr < BOARD_SIZE && 
           c - dc >= 0 && c - dc < BOARD_SIZE && 
           letters[r - dr][c - dc] != ' ') {
        r -= dr;
        c -= dc;
    }
    
    // Extract main word
    string mainWord;
    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && 
           letters[r][c] != ' ') {
        mainWord += letters[r][c];
        r += dr;
        c += dc;
    }
    
    if (!mainWord.empty()) {
        words.push_back(mainWord);
    }
    
    // Extract cross words for each placement
    int crossDr = isHorizontal ? 1 : 0;
    int crossDc = isHorizontal ? 0 : 1;
    
    for (const auto& placement : move.placements) {
        // Check if this placement forms a cross word
        bool hasNeighbor = false;
        
        int nr = placement.row - crossDr;
        int nc = placement.col - crossDc;
        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && 
            letters[nr][nc] != ' ') {
            hasNeighbor = true;
        }
        
        nr = placement.row + crossDr;
        nc = placement.col + crossDc;
        if (!hasNeighbor && nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && 
            letters[nr][nc] != ' ') {
            hasNeighbor = true;
        }
        
        if (hasNeighbor) {
            // Find start of cross word
            int cr = placement.row;
            int cc = placement.col;
            
            while (cr - crossDr >= 0 && cr - crossDr < BOARD_SIZE && 
                   cc - crossDc >= 0 && cc - crossDc < BOARD_SIZE && 
                   letters[cr - crossDr][cc - crossDc] != ' ') {
                cr -= crossDr;
                cc -= crossDc;
            }
            
            // Extract cross word
            string crossWord;
            while (cr >= 0 && cr < BOARD_SIZE && cc >= 0 && cc < BOARD_SIZE && 
                   letters[cr][cc] != ' ') {
                crossWord += letters[cr][cc];
                cr += crossDr;
                cc += crossDc;
            }
            
            if (crossWord.length() > 1) {
                words.push_back(crossWord);
            }
        }
    }
    
    return words;
}

bool MoveValidator::validateWords(const vector<string>& words) {
    for (const string& word : words) {
        // Normalize to uppercase for dictionary lookup
        string normalized;
        for (char ch : word) {
            if (isalpha(ch)) {
                normalized += static_cast<char>(toupper(ch));
            }
        }
        
        if (!isValidWord(normalized)) {
            LOG_DEBUG("Invalid word found: ", normalized);
            return false;
        }
    }
    return true;
}

bool MoveValidator::validateConnectivity(
    const LetterBoard& letters,
    const Move& move) {
    
    // Check if board is empty (first move)
    bool boardEmpty = true;
    for (int r = 0; r < BOARD_SIZE && boardEmpty; r++) {
        for (int c = 0; c < BOARD_SIZE && boardEmpty; c++) {
            if (letters[r][c] != ' ') {
                boardEmpty = false;
            }
        }
    }
    
    if (boardEmpty) {
        // First move must cover center
        int centerRow = BOARD_SIZE / 2;
        int centerCol = BOARD_SIZE / 2;
        
        for (const auto& placement : move.placements) {
            if (placement.row == centerRow && placement.col == centerCol) {
                return true;
            }
        }
        return false;
    }
    
    // Subsequent moves must connect to existing tiles
    for (const auto& placement : move.placements) {
        static const int drs[4] = {-1, 1, 0, 0};
        static const int dcs[4] = {0, 0, -1, 1};
        
        for (int d = 0; d < 4; d++) {
            int nr = placement.row + drs[d];
            int nc = placement.col + dcs[d];
            
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && 
                letters[nr][nc] != ' ') {
                return true;
            }
        }
    }
    
    return false;
}

bool MoveValidator::validateRackUsage(
    const TileRack& rack,
    const Move& move) {
    
    // Count tiles needed from rack
    int needed[27] = {0}; // 0-25 for A-Z, 26 for blank
    
    for (const auto& placement : move.placements) {
        if (placement.is_blank) {
            needed[26]++;
        } else {
            int idx = placement.letter - 'A';
            if (idx >= 0 && idx < 26) {
                needed[idx]++;
            }
        }
    }
    
    // Count tiles available in rack
    int available[27] = {0};
    for (const Tile& tile : rack) {
        if (tile.is_blank) {
            available[26]++;
        } else {
            int idx = toupper(tile.letter) - 'A';
            if (idx >= 0 && idx < 26) {
                available[idx]++;
            }
        }
    }
    
    // Check if we have enough tiles
    for (int i = 0; i < 27; i++) {
        if (needed[i] > available[i]) {
            LOG_DEBUG("Rack validation failed: need ", needed[i], 
                     " of ", (i == 26 ? '?' : static_cast<char>('A' + i)),
                     ", have ", available[i]);
            return false;
        }
    }
    
    return true;
}

ValidationResult MoveValidator::validateMove(
    const Board& bonusBoard,
    const LetterBoard& letters,
    const BlankBoard& blanks,
    const TileRack& rack,
    const Move& move) {
    
    ValidationResult result;
    result.isValid = false;
    result.score = 0;
    
    // Check rack usage
    if (!validateRackUsage(rack, move)) {
        result.errorMessage = "Move uses tiles not in rack";
        return result;
    }
    
    // Check connectivity
    if (!validateConnectivity(letters, move)) {
        result.errorMessage = "Move does not connect properly";
        return result;
    }
    
    // Simulate move on temporary board
    LetterBoard tempLetters = letters;
    BlankBoard tempBlanks = blanks;
    
    for (const auto& placement : move.placements) {
        tempLetters[placement.row][placement.col] = placement.letter;
        tempBlanks[placement.row][placement.col] = placement.is_blank;
    }
    
    // Extract all formed words
    result.formedWords = extractFormedWords(tempLetters, move);
    
    // Validate all words are in dictionary
    vector<string> invalidWords;
    for (const string& word : result.formedWords) {
        string normalized;
        for (char ch : word) {
            if (isalpha(ch)) {
                normalized += static_cast<char>(toupper(ch));
            }
        }
        
        if (!isValidWord(normalized)) {
            invalidWords.push_back(normalized);
        }
    }
    
    if (!invalidWords.empty()) {
        result.errorMessage = "Invalid words formed: ";
        for (const string& word : invalidWords) {
            result.errorMessage += word + " ";
        }
        result.invalidWords = invalidWords;
        return result;
    }
    
    // Recompute score from scratch using centralized ruleset
    result.score = Ruleset::getInstance().scoreMove(bonusBoard, letters, blanks, move);
    
    result.isValid = true;
    return result;
}
