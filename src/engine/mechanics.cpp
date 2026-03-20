#include "../../include/engine/mechanics.h"
#include "../../include/engine/rack.h"
// Removed heuristics.h include as we use internal safe scoring now
#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

namespace Mechanics {
    void applyMove(GameState& state, const Move& move, int score) {
        if (move.type != MoveType::PLAY) return;

        int dr = move.horizontal ? 0 : 1;
        int dc = move.horizontal ? 1 : 0;
        int r = move.row;
        int c = move.col;

        int startR = r;
        int startC = c;

        TileRack& rack = state.players[state.currentPlayerIndex].rack;
        string word = move.word; // Note: 'Move' struct contains only placed tiles (compressed)

        for (char letter : word) {
            // Skip existing tiles to find the next empty slot
            while (r < BOARD_SIZE && c < BOARD_SIZE && state.board[r][c] != ' ') {
                r += dr; c += dc;
            }

            if (r >= BOARD_SIZE || c >= BOARD_SIZE) break;

            // FIX: Write EXACT character to board (preserve 'a' for blanks)
            state.board[r][c] = letter;

            // KEEP the blanks array if other legacy parts need it,
            // but the Board itself MUST tell the truth.
            state.blanks[r][c] = (letter >= 'a' && letter <= 'z');

            // Remove used tile from rack
            for (auto it = rack.begin(); it != rack.end(); ++it) {

                bool isBlank = (it->letter == '?');
                bool isLetterMatch = (it->letter == letter);

                // Match blank '?' with any letter input
                bool match = isBlank || isLetterMatch;
                if (match) {
                    rack.erase(it);
                    break;
                }
            }
            r += dr; c += dc;
        }

        int endR = r;
        int endC = c;
        int span = move.horizontal ? (endC - startC) : (endR - startR);

        state.players[state.currentPlayerIndex].score += score;
        state.players[state.currentPlayerIndex].passCount = 0;

        if (rack.size() < 7 && !state.bag.empty()) {
            drawTiles(state.bag, rack, static_cast<int>(7 - rack.size()));
        }
    }

    void commitSnapshot(GameState& backup, const GameState& current) {
        backup = current;
    }

    void restoreSnapshot(GameState& current, const GameState& backup) {
        current = backup;
    }

    bool attemptExchange(GameState& state, const Move& move) {
        Player& p = state.players[state.currentPlayerIndex];
        // Ensure bag has enough tiles (Standard rule: 7 tiles needed in bag to exchange)
        // Adjust '7' to '1' if your rules allow exchanging with fewer tiles in bag
        if (state.bag.size() < 7) return false;

        if (exchangeRack(p.rack, move.exchangeLetters, state.bag)) {
            p.passCount++;
            return true;
        }
        return false;
    }

    void applySixPassPenalty(GameState& state) {
        for (int i = 0; i < 2; i++) {
            int rackVal = 0;
            for (const auto &tile : state.players[i].rack) {
                rackVal += getPointValue(tile.letter); // Use internal safe scorer
            }
            state.players[i].score -= (rackVal * 2);
        }
    }

    void applyEmptyRackBonus(GameState& state, int winnerIdx) {
        int loserIdx = 1 - winnerIdx;
        int loserRackVal = 0;
        for (const auto& tile : state.players[loserIdx].rack) {
            loserRackVal += getPointValue(tile.letter); // Use internal safe scorer
        }
        state.players[winnerIdx].score += (loserRackVal * 2);
    }
}