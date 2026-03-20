#pragma once

#include "state.h"
#include "types.h"
#include "../kernel/move_generator.h"
#include "../move.h"

// SAFE LOOKUP TABLE (0-25 = A-Z).
// No external dependencies. Compiler will inline this.
static constexpr int MECHANICS_POINTS[] = {
    1, 3, 3, 2, 1, 4, 2, 4, 1, 8, 5, 1, 3, 1, 1, 3, 10, 1, 1, 1, 1, 4, 4, 8, 4, 10
};

namespace Mechanics {

    // SAFE & FAST Point Lookup
    static inline int getPointValue(char c) {
        // Handle blanks, spaces, and safety checks
        if (c < 'A' || c > 'z') return 0; // Filter non-alpha (includes '?' and ' ')

        // Normalize to 0-25
        char upper = (c >= 'a') ? (c ^ 0x20) : c;
        if (upper < 'A' || upper > 'Z') return 0;

        return MECHANICS_POINTS[upper - 'A'];
    }

    // --- TEMPLATED SCORER ---
    template <bool IS_HORIZONTAL>
    static inline int calculateTrueScoreFast(const kernel::MoveCandidate &move,
                                             const LetterBoard& letters,
                                             const Board &bonusBoard)
    {
        int totalScore = 0;
        int mainWordScore = 0;
        int mainWordMultiplier = 1;
        int tilesPlacedCount = 0;
        int wordLen = 0;

        int r = move.row;
        int c = move.col;

        constexpr int dr = IS_HORIZONTAL ? 0 : 1;
        constexpr int dc = IS_HORIZONTAL ? 1 : 0;
        constexpr int pdr = IS_HORIZONTAL ? 1 : 0; // Perpendicular dr
        constexpr int pdc = IS_HORIZONTAL ? 0 : 1; // Perpendicular dc

        // Iterate through the CANDIDATE word (which is the FULL word)
        for (char letter : move.word) {
            if (letter == '\0') break;
            wordLen++;

            // 1. Calculate Score for this letter (Safe Lookup)
            int letterScore = getPointValue(letter);

            // 2. Check Board Status
            if (letters[r][c] == ' ') {
                // TILE PLACEMENT (Empty Square)
                tilesPlacedCount++;
                CellType bonus = bonusBoard[r][c];

                // Apply Board Bonuses
                if (bonus != CellType::Normal) {
                    if (bonus == CellType::DLS) letterScore *= 2;
                    else if (bonus == CellType::TLS) letterScore *= 3;
                    else if (bonus == CellType::DWS) mainWordMultiplier *= 2;
                    else if (bonus == CellType::TWS) mainWordMultiplier *= 3;
                }

                // --- CROSS WORD CHECK ---
                bool hasNeighbor = false;
                if (r-pdr >= 0 && letters[r-pdr][c-pdc] != ' ') hasNeighbor = true;
                else if (r+pdr < 15 && letters[r+pdr][c+pdc] != ' ') hasNeighbor = true;

                if (hasNeighbor) {
                    // Backtrack to start of cross-word
                    int currR = r; int currC = c;
                    while (currR - pdr >= 0 && currC - pdc >= 0 && letters[currR-pdr][currC-pdc] != ' ') {
                        currR -= pdr; currC -= pdc;
                    }

                    int crossScore = 0;
                    int crossMult = 1;

                    // Forward Scan of Cross Word
                    while (currR < 15 && currC < 15) {
                        char cellLetter = letters[currR][currC];

                        // If empty AND it's not the tile we are placing right now, stop.
                        if (cellLetter == ' ' && (currR != r || currC != c)) break;

                        int pts = 0;
                        if (currR == r && currC == c) {
                            // This is the tile we just placed
                            pts = getPointValue(letter);

                            CellType crossBonus = bonusBoard[currR][currC];
                            if (crossBonus == CellType::DLS) pts *= 2;
                            else if (crossBonus == CellType::TLS) pts *= 3;

                            if (crossBonus == CellType::DWS) crossMult *= 2;
                            else if (crossBonus == CellType::TWS) crossMult *= 3;
                        } else {
                            // Existing tile
                            pts = getPointValue(cellLetter);
                        }

                        crossScore += pts;
                        currR += pdr; currC += pdc;
                    }
                    totalScore += (crossScore * crossMult);
                }
            } else {
                // EXISTING TILE (Board not empty)
                // We use the letterScore derived from the word, but without bonuses
                // Note: getPointValue is safe, so we don't need to read 'letters[r][c]' for score
            }

            mainWordScore += letterScore;
            r += dr; c += dc;
        }

        if (wordLen > 1) totalScore += (mainWordScore * mainWordMultiplier);
        if (tilesPlacedCount == 7) totalScore += 50;

        return totalScore;
    }

    // Function Declarations
    void applyMove(GameState& state, const Move& move, int score);
    void commitSnapshot(GameState& backup, const GameState& current);
    void restoreSnapshot(GameState& current, const GameState& backup);
    bool attemptExchange(GameState& state, const Move& move);
    void applySixPassPenalty(GameState& state);
    void applyEmptyRackBonus(GameState& state, int winnerIdx);

    // Wrapper
    static inline int calculateTrueScore(const kernel::MoveCandidate &move, const LetterBoard& letters, const Board &bonusBoard) {
        if (move.isHorizontal)
            return calculateTrueScoreFast<true>(move, letters, bonusBoard);
        else
            return calculateTrueScoreFast<false>(move, letters, bonusBoard);
    }
}