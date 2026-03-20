#include "../../include/engine/referee.h"
#include "../../include/engine/rack.h"
#include <algorithm>

using namespace std;

static inline char fastUpper(char c) {
    return (c >= 'a' && c <= 'z') ? (c ^ 0x20) : c;
}

static int letterPoints(char ch) {
    ch = fastUpper(ch);
    switch (ch) {
        case 'A': case 'E': case 'I': case 'O': case 'U':
        case 'N': case 'R': case 'T': case 'L': case 'S': return 1;
        case 'D': case 'G': return 2;
        case 'B': case 'C': case 'M': case 'P': return 3;
        case 'F': case 'H': case 'V': case 'W': case 'Y': return 4;
        case 'K': return 5;
        case 'J': case 'X': return 8;
        case 'Q': case 'Z': return 10;
        default: return 0;
    }
}

static int findTileInRackStack(const Tile* rack, int rackSize, const bool* used, char letter) {
    char upper = fastUpper(letter);
    for (int i = 0; i < rackSize; i++) {
        if (!used[i] && fastUpper(rack[i].letter) == upper) return i;
    }
    for (int i = 0; i < rackSize; i++) {
        if (!used[i] && rack[i].letter == '?') return i;
    }
    return -1;
}

static int getWordScore(const Board &bonusBoard, const LetterBoard &letters,
                        const BlankBoard &blanks, const bool newTileMap[15][15],
                        int r, int c, int dr, int dc) {
    int score = 0;
    int wordMult = 1;
    int length = 0;

    while (r - dr >= 0 && c - dc >= 0 && letters[r - dr][c - dc] != ' ') {
        r -= dr; c -= dc;
    }

    while (r < 15 && c < 15 && letters[r][c] != ' ') {
        int letterScore = letterPoints(letters[r][c]);
        if (blanks[r][c]) letterScore = 0;

        if (newTileMap[r][c]) {
            CellType cell = bonusBoard[r][c];
            if (cell == CellType::DLS) letterScore *= 2;
            else if (cell == CellType::TLS) letterScore *= 3;
            else if (cell == CellType::DWS) wordMult *= 2;
            else if (cell == CellType::TWS) wordMult *= 3;
        }

        score += letterScore;
        r += dr; c += dc;
        length++;
    }

    return (length > 1) ? (score * wordMult) : 0;
}

MoveResult Referee::validateMove(const GameState &state, const Move &move,
                                 const Board &bonusBoard, Dictionary &dict) {
    MoveResult res;
    res.success = false;
    res.score = 0;

    if (move.type != MoveType::PLAY) {
        res.message = "Referee only validates PLAY moves.";
        return res;
    }

    int len = 0;
    while (move.word[len] != '\0' && len < 15) len++;
    if (len == 0) { res.message = "Empty word."; return res; }

    struct PlacedTile { int r, c; char letter; bool isBlank; };
    PlacedTile placed[15];
    int placedCount = 0;

    bool usedRackIndices[7] = {false};
    Tile localRack[7];
    int rackSize = 0;
    for (const auto& t : state.players[state.currentPlayerIndex].rack) {
        if (rackSize < 7) localRack[rackSize++] = t;
    }

    int r = move.row;
    int c = move.col;
    int dr = move.horizontal ? 0 : 1;
    int dc = move.horizontal ? 1 : 0;
    int wIdx = 0;

    if (state.board[r][c] != ' ') { res.message = "Start position occupied."; return res; }

    while (wIdx < len) {
        if (r >= 15 || c >= 15) { res.message = "Word out of bounds."; return res; }

        if (state.board[r][c] == ' ') {
            char letter = fastUpper(move.word[wIdx]);
            int idx = findTileInRackStack(localRack, rackSize, usedRackIndices, letter);

            if (idx == -1) { res.message = "Missing tiles in rack."; return res; }
            usedRackIndices[idx] = true;

            placed[placedCount++] = {r, c, letter, (localRack[idx].letter == '?')};
            wIdx++;
        }
        r += dr; c += dc;
    }

    if (placedCount == 0) { res.message = "Must place at least one tile."; return res; }

    bool boardEmpty = true;
    for(int i=0; i<15 && boardEmpty; i++)
        for(int j=0; j<15; j++) if(state.board[i][j] != ' ') { boardEmpty = false; break; }

    if (boardEmpty) {
        bool centerCovered = false;
        for (int i = 0; i < placedCount; i++) {
            if (placed[i].r == 7 && placed[i].c == 7) { centerCovered = true; break; }
        }
        if (!centerCovered) { res.message = "First move must cover center (H8)."; return res; }
    } else {
        bool connected = false;
        int checkDr[] = {-1, 1, 0, 0};
        int checkDc[] = {0, 0, -1, 1};
        for (int i = 0; i < placedCount; i++) {
            for (int k = 0; k < 4; k++) {
                int nr = placed[i].r + checkDr[k];
                int nc = placed[i].c + checkDc[k];
                if (nr >= 0 && nr < 15 && nc >= 0 && nc < 15) {
                    if (state.board[nr][nc] != ' ') { connected = true; break; }
                }
            }
            if (connected) break;
        }
        if (!connected) { res.message = "Word must connect to existing tiles."; return res; }
    }

    LetterBoard tempBoard = state.board;
    BlankBoard tempBlanks = state.blanks;
    bool newTileMap[15][15] = {false};

    for (int i = 0; i < placedCount; i++) {
        tempBoard[placed[i].r][placed[i].c] = placed[i].letter;
        tempBlanks[placed[i].r][placed[i].c] = placed[i].isBlank;
        newTileMap[placed[i].r][placed[i].c] = true;
    }

    int score = 0;
    score += getWordScore(bonusBoard, tempBoard, tempBlanks, newTileMap, move.row, move.col, dr, dc);

    for (int i = 0; i < placedCount; i++) {
        int crossScore = getWordScore(bonusBoard, tempBoard, tempBlanks, newTileMap, placed[i].r, placed[i].c, dc, dr);
        score += crossScore;
    }

    if (placedCount == 7) score += 50;

    res.success = true;
    res.score = score;
    return res;
}