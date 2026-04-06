#include "../../include/engine/mechanics.h"
#include "../../include/engine/rack.h"
#include <iostream>
#include <vector>
#include <algorithm>

namespace Mechanics {

    /**
     * @brief Applies a full string move to the physical game state.
     * @pre THE FULL STRING DOCTRINE: `move.word` MUST be a fully overlapping string.
     * @post The board, blanks matrix, player score, and player rack are mutated. Tile bag is drawn from.
     * @invariant No std::string heap allocations are used.
     */
    void applyMove(GameState& state, const Move& move, int score) {
        if (move.type != MoveType::PLAY) return;

        int dr = move.horizontal ? 0 : 1;
        int dc = move.horizontal ? 1 : 0;
        int r = move.row;
        int c = move.col;

        TileRack& rack = state.players[state.currentPlayerIndex].rack;

        for (int i = 0; i < 15 && move.word[i] != '\0'; i++) {
            char letter = move.word[i];

            // ANOMALY TRAP 1: Spatial Hallucination (Bounds Check)
            if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) {
                std::cerr << "\n[CRITICAL ERROR] Mechanics::applyMove - Out of Bounds! Word: "
                          << move.word << " at [" << r << "," << c << "]\n";
                break;
            }

            // FULL STRING LOGIC: Only consume a tile if the physical board square is currently empty
            if (state.board[r][c] == ' ') {
                // Write EXACT character to board (preserves lowercase 'a'-'z' for blanks)
                state.board[r][c] = letter;
                state.blanks[r][c] = (letter >= 'a' && letter <= 'z');

                // EXACT MATCH TARGETING: Prevent 'Eager Blank Consumption'
                bool tileFound = false;
                bool isBlankReq = (letter >= 'a' && letter <= 'z');
                char target = isBlankReq ? '?' : letter;

                for (auto it = rack.begin(); it != rack.end(); ++it) {
                    if (it->letter == target) {
                        rack.erase(it);
                        tileFound = true;
                        break;
                    }
                }

                // ANOMALY TRAP 2: Rack Desynchronization
                if (!tileFound) {
                    std::cerr << "\n[CRITICAL ERROR] Mechanics::applyMove - Rack Desync on '" << letter << "'\n";
                }
            }

            // If the square was NOT empty, it is an existing board tile.
            // We do nothing, simply advancing the pointers to maintain spatial alignment.
            r += dr; c += dc;
        }

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

        if (state.bag.size() < 7) {
            // ANOMALY TRAP 3: Illegal Exchange
            std::cerr << "\n[WARNING] Mechanics::attemptExchange - Illegal exchange attempted with < 7 tiles in bag.\n";
            return false;
        }

        if (exchangeRack(p.rack, move.exchangeLetters, state.bag)) {
            p.passCount++;
            return true;
        }

        // ANOMALY TRAP 4: Ghost Exchange
        std::cerr << "\n[CRITICAL ERROR] Mechanics::attemptExchange - Failed to exchange tiles. Player does not own requested tiles.\n";
        return false;
    }

    void applySixPassPenalty(GameState& state) {
        for (int i = 0; i < 2; i++) {
            int rackVal = 0;
            for (const auto &tile : state.players[i].rack) {
                rackVal += getPointValue(tile.letter);
            }
            state.players[i].score -= (rackVal * 2);
        }
    }

    void applyEmptyRackBonus(GameState& state, int winnerIdx) {
        int loserIdx = 1 - winnerIdx;
        int loserRackVal = 0;
        for (const auto& tile : state.players[loserIdx].rack) {
            loserRackVal += getPointValue(tile.letter);
        }
        state.players[winnerIdx].score += (loserRackVal * 2);
    }

    /**
     * @brief High-performance state mutation specifically for MCTS and Endgame rollouts.
     * @pre THE FULL STRING DOCTRINE: `cand.word` is the full overlapping string.
     */
    void applyCandidateMove(GameState& state, const kernel::MoveCandidate& cand) {
        if (cand.word[0] == '\0') return; // Pass

        int dr = cand.isHorizontal ? 0 : 1;
        int dc = cand.isHorizontal ? 1 : 0;
        int r = cand.row;
        int c = cand.col;

        TileRack& rack = state.players[state.currentPlayerIndex].rack;

        for (int i = 0; i < 15 && cand.word[i] != '\0'; i++) {
            char letter = cand.word[i];

            // ANOMALY TRAP 1: Spatial Hallucination (Bounds Check)
            if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) {
                std::cerr << "\n[CRITICAL ERROR] Mechanics::applyCandidateMove - Out of Bounds! Word: "
                          << cand.word << " at [" << r << "," << c << "]\n";
                break;
            }

            // FULL STRING LOGIC: If the square is empty, it's a new tile we must place
            if (state.board[r][c] == ' ') {
                state.board[r][c] = letter;
                state.blanks[r][c] = (letter >= 'a' && letter <= 'z');

                // EXACT MATCH TARGETING
                bool tileFound = false;
                bool isBlankReq = (letter >= 'a' && letter <= 'z');
                char target = isBlankReq ? '?' : letter;

                for (auto it = rack.begin(); it != rack.end(); ++it) {
                    if (it->letter == target) {
                        rack.erase(it);
                        tileFound = true;
                        break;
                    }
                }

                // Anomaly Trap
                if (!tileFound) {
                    std::cerr << "\n[CRITICAL ERROR] Mechanics::applyCandidateMove - Rack Desync on '" << letter << "'\n";
                }
            }

            // Unconditionally advance pointers (Safe because of Full String Doctrine)
            r += dr; c += dc;
        }

        state.players[state.currentPlayerIndex].score += cand.score;
        state.players[state.currentPlayerIndex].passCount = 0;

        if (rack.size() < 7 && !state.bag.empty()) {
            drawTiles(state.bag, rack, static_cast<int>(7 - rack.size()));
        }
    }
}