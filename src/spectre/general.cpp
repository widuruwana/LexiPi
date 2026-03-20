#include "../../include/spectre/general.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cstring>

namespace spectre {

    General::General() {}

    // STATIC TWS COORDINATES (Standard Scrabble)
    static const int TWS_COORDS[8][2] = {
        {0, 0}, {0, 7}, {0, 14},
        {7, 0},         {7, 14},
        {14, 0}, {14, 7}, {14, 14}
    };

    float General::evaluate_topology(const GameState& state, const Move& move) {
        // 1. SIMULATION (Hypothetical World)
        // We simulate the board AFTER the move is played.
        LetterBoard tempBoard = state.board;
        simulate_move_on_board(tempBoard, move);

        // 2. SCAN
        TopologyReport report = scan_topology(tempBoard);

        // 3. DOCTRINE
        int scoreDiff = state.players[state.currentPlayerIndex].score - state.players[1 - state.currentPlayerIndex].score;
        bool isWinning = scoreDiff > 40;
        bool isDesperate = scoreDiff < -40;

        float utility = 0.5f; // Neutral Start

        if (isWinning) {
            // --- DEFENSIVE DOCTRINE (The "Boa Constrictor") ---
            // Goal: Starve the opponent of hooks and TWS.

            // Penalty for Openness: We want a closed board.
            // If openness is 0.5, we subtract 0.1. If 0.1, we subtract 0.0.
            utility -= (report.openness * 0.4f);

            // Severe Penalty for exposing TWS
            if (report.attackable_tws > 0) utility -= 0.3f;
            if (report.attackable_tws > 1) utility -= 0.5f;

        } else if (isDesperate) {
            // --- OFFENSIVE DOCTRINE (The "Chaos Agent") ---
            // Goal: Maximize variance. We need TWS access.

            // Reward Openness
            utility += (report.openness * 0.4f);

            // Huge Bonus for TWS
            if (report.attackable_tws > 0) utility += 0.3f;

        } else {
            // --- NEUTRAL DOCTRINE ---
            // Slight preference for openness to keep flow going
            utility += (report.openness * 0.1f);
        }

        // Clamp to 0.0 - 1.0
        return std::max(0.0f, std::min(1.0f, utility));
    }

    TopologyReport General::scan_topology(const LetterBoard& board) {
        TopologyReport report;
        report.open_anchors = 0;
        report.attackable_tws = 0;
        report.is_fractured = false;

        // 1. Scan for Open Anchors (The "Perimeter")
        // An empty square is an anchor if it touches a tile.
        int totalPerimeter = 0;

        for (int r = 0; r < 15; r++) {
            for (int c = 0; c < 15; c++) {
                if (board[r][c] == ' ') {
                    bool isAnchor = false;
                    // Check 4 neighbors
                    if (r > 0 && board[r-1][c] != ' ') isAnchor = true;
                    else if (r < 14 && board[r+1][c] != ' ') isAnchor = true;
                    else if (c > 0 && board[r][c-1] != ' ') isAnchor = true;
                    else if (c < 14 && board[r][c+1] != ' ') isAnchor = true;

                    if (isAnchor) {
                        report.open_anchors++;
                    }
                } else {
                    // It's a tile
                }
            }
        }

        // Normalize Openness (Max practical anchors is ~60 in a wide open game)
        report.openness = std::min(1.0f, (float)report.open_anchors / 50.0f);

        // 2. Scan for Triple Word Scores (Hotspots)
        for (int i = 0; i < 8; i++) {
            int r = TWS_COORDS[i][0];
            int c = TWS_COORDS[i][1];

            // If TWS is empty...
            if (board[r][c] == ' ') {
                // ...check if it is reachable (adjacent to a tile)
                // Note: This is a simplification. Real reachability requires line-of-sight.
                // But generally, if you are next to it, you can probably hit it.
                bool accessible = false;
                if (r > 0 && board[r-1][c] != ' ') accessible = true;
                else if (r < 14 && board[r+1][c] != ' ') accessible = true;
                else if (c > 0 && board[r][c-1] != ' ') accessible = true;
                else if (c < 14 && board[r][c+1] != ' ') accessible = true;

                if (accessible) {
                    report.attackable_tws++;
                }
            }
        }

        return report;
    }

    void General::simulate_move_on_board(LetterBoard& board, const Move& move) {
        int r = move.row;
        int c = move.col;
        int dr = move.horizontal ? 0 : 1;
        int dc = move.horizontal ? 1 : 0;

        for (int i = 0; move.word[i] != '\0'; i++) {
            if (r >= 15 || c >= 15) break;
            // If the board is empty there, we place the tile
            if (board[r][c] == ' ') {
                board[r][c] = move.word[i];
            }
            // If occupied, we skip (it's already there)
            r += dr;
            c += dc;
        }
    }

    void General::report_topology(const GameState& state, const Move& move) {
        LetterBoard tempBoard = state.board;
        simulate_move_on_board(tempBoard, move);
        TopologyReport report = scan_topology(tempBoard);
        float score = evaluate_topology(state, move);

        std::cout << "[GENERAL REPORT] Move: " << move.word
                  << "\n  > Openness: " << std::fixed << std::setprecision(2) << report.openness
                  << " (" << report.open_anchors << " anchors)"
                  << "\n  > Attackable TWS: " << report.attackable_tws
                  << "\n  > FINAL SCORE: " << score
                  << std::endl;
    }

}