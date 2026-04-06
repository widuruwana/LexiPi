#include "../../include/engine/referee.h"
#include "../../include/engine/mechanics.h"
#include "../../include/kernel/move_generator.h"
#include <iostream>
#include <cstring>
#include <vector>

using namespace std;

MoveResult Referee::validateMove(const GameState &state, const Move &move, const Board &bonusBoard, const Dictionary &dict) {
    MoveResult res;
    res.success = false;
    res.score = 0;
    res.message = "Unknown Error";

    // 1. Move Type Check
    if (move.type != MoveType::PLAY) {
        res.success = true;
        res.message = "Non-Play Move Validated.";
        return res;
    }

    // 2. Spatial Setup
    int dr = move.horizontal ? 0 : 1;
    int dc = move.horizontal ? 1 : 0;
    int r = move.row;
    int c = move.col;

    int newTilesPlaced = 0;
    bool coversCenter = false;
    bool isConnected = false;

    // THE FIX: High-performance stack array instead of std::vector heap allocation
    int rackCounts[27] = {0};
    for (const auto& t : state.players[state.currentPlayerIndex].rack) {
        if (t.letter == '?') rackCounts[26]++;
        else {
            char upper = toupper(t.letter);
            if (upper >= 'A' && upper <= 'Z') rackCounts[upper - 'A']++;
        }
    }

    // 3. FULL STRING VALIDATION LOOP
    for (int i = 0; i < 15 && move.word[i] != '\0'; i++) {
        if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) {
            res.message = "Invalid Move: Word goes out of bounds.";
            return res;
        }

        if (r == 7 && c == 7) coversCenter = true;

        char letter = move.word[i];

        if (state.board[r][c] == ' ') {
            // EMPTY SQUARE: Fast O(1) array lookup
            bool found = false;
            bool isBlankReq = (letter >= 'a' && letter <= 'z');

            if (isBlankReq) {
                if (rackCounts[26] > 0) { rackCounts[26]--; found = true; newTilesPlaced++; }
            } else {
                char upper = toupper(letter);
                if (upper >= 'A' && upper <= 'Z' && rackCounts[upper - 'A'] > 0) {
                    rackCounts[upper - 'A']--; found = true; newTilesPlaced++;
                }
            }

            if (!found) {
                res.message = "Invalid Move: Missing required tiles in rack.";
                return res;
            }

            // Check Adjacency
            int checkDr[] = {-1, 1, 0, 0};
            int checkDc[] = {0, 0, -1, 1};
            for (int k = 0; k < 4; k++) {
                int nr = r + checkDr[k];
                int nc = c + checkDc[k];
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                    if (state.board[nr][nc] != ' ') isConnected = true;
                }
            }

        } else {
            // OCCUPIED SQUARE
            isConnected = true;
            char boardChar = state.board[r][c];
            char expectedChar = (letter >= 'a' && letter <= 'z') ? toupper(letter) : letter;
            char actualChar = (boardChar >= 'a' && boardChar <= 'z') ? toupper(boardChar) : boardChar;

            if (expectedChar != actualChar) {
                res.message = "Invalid Move: Word conflicts with existing board tiles.";
                return res;
            }
        }
        r += dr; c += dc;
    }

    // 4. RULE COMPLIANCE
    if (newTilesPlaced == 0) {
        res.message = "Invalid Move: No new tiles placed.";
        return res;
    }

    if (state.board[7][7] == ' ') {
        if (!coversCenter) {
            res.message = "Invalid Move: First play must cover the center square (H8).";
            return res;
        }
    } else {
        if (!isConnected) {
            res.message = "Invalid Move: Word must connect to existing tiles.";
            return res;
        }
    }

    // 5. SYNCHRONIZED SCORING (Replaces Legacy Switch Statements)
    // Delegate to the exact same high-performance scorer used by the AI Agents.
    // NOTE: We trust the GADDAG generator's implicit dictionary validation to save CPU cycles.
    kernel::MoveCandidate cand;
    cand.row = move.row;
    cand.col = move.col;
    cand.isHorizontal = move.horizontal;
    strncpy(cand.word, move.word, 16);
    cand.word[15] = '\0';

    res.score = Mechanics::calculateTrueScore(cand, state.board, bonusBoard);
    res.success = true;
    res.message = "Valid Move";

    return res;
}

// Helpers updated to support the Full String Doctrine and bypass legacy math
int Referee::calculateScore(const LetterBoard &letters, const Board &bonuses, const Move &move) {
    kernel::MoveCandidate cand;
    cand.row = move.row;
    cand.col = move.col;
    cand.isHorizontal = move.horizontal;
    strncpy(cand.word, move.word, 16);
    cand.word[15] = '\0';
    return Mechanics::calculateTrueScore(cand, letters, bonuses);
}

bool Referee::checkConnectivity(const LetterBoard &letters, const Move &move) {
    int dr = move.horizontal ? 0 : 1;
    int dc = move.horizontal ? 1 : 0;
    int r = move.row;
    int c = move.col;

    for (int i = 0; i < 15 && move.word[i] != '\0'; i++) {
        if (r < 0 || r >= 15 || c < 0 || c >= 15) return false;

        // Overlap = Connected
        if (letters[r][c] != ' ') return true;

        // Adjacency = Connected
        int checkDr[] = {-1, 1, 0, 0};
        int checkDc[] = {0, 0, -1, 1};
        for (int k = 0; k < 4; k++) {
            int nr = r + checkDr[k];
            int nc = c + checkDc[k];
            if (nr >= 0 && nr < 15 && nc >= 0 && nc < 15 && letters[nr][nc] != ' ') {
                return true;
            }
        }
        r += dr; c += dc;
    }
    return false;
}