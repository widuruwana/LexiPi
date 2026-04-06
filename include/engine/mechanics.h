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

    /**
     * @brief Evaluates the raw point value of a single Scrabble tile.
     * @pre `c` is a valid ASCII character.
     * @post Returns 0 for blanks (lowercase letters or '?') and correct points for 'A'-'Z'.
     */
    static inline int getPointValue(char c) {
        // Lowercase letters on the board represent played blanks. They score 0.
        if (c >= 'a' && c <= 'z') return 0;

        // Filter non-alpha symbols and unplayed blanks ('?', ' ')
        if (c < 'A' || c > 'Z') return 0;

        return MECHANICS_POINTS[c - 'A'];
    }

    /**
     * @brief Computes the true Scrabble score of a full word placement, including all cross-words and board multipliers.
     * @pre `move.word` MUST contain the FULL overlapping word (not just differential tiles).
     * @post Returns the exact integer score. Leaves the board and move parameters completely unmodified.
     * @invariant Zero heap allocations. $O(L)$ complexity where L is word length.
     */
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

        for (int i = 0; i < 15 && move.word[i] != '\0'; i++) {
            char letter = move.word[i];
            wordLen++;

            int letterScore = getPointValue(letter);

            // If square is empty, it's a newly placed tile
            if (letters[r][c] == ' ') {
                tilesPlacedCount++;
                CellType bonus = bonusBoard[r][c];

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
                    int currR = r; int currC = c;
                    while (currR - pdr >= 0 && currC - pdc >= 0 && letters[currR-pdr][currC-pdc] != ' ') {
                        currR -= pdr; currC -= pdc;
                    }

                    int crossScore = 0;
                    int crossMult = 1;

                    while (currR < 15 && currC < 15) {
                        char cellLetter = letters[currR][currC];
                        if (cellLetter == ' ' && (currR != r || currC != c)) break;

                        int pts = 0;
                        if (currR == r && currC == c) {
                            pts = getPointValue(letter);
                            CellType crossBonus = bonusBoard[currR][currC];
                            if (crossBonus == CellType::DLS) pts *= 2;
                            else if (crossBonus == CellType::TLS) pts *= 3;
                            if (crossBonus == CellType::DWS) crossMult *= 2;
                            else if (crossBonus == CellType::TWS) crossMult *= 3;
                        } else {
                            pts = getPointValue(cellLetter);
                        }

                        crossScore += pts;
                        currR += pdr; currC += pdc;
                    }
                    totalScore += (crossScore * crossMult);
                }
            }
            // If square is NOT empty, it's an existing tile.
            // getPointValue(letter) automatically handles the raw score safely without bonuses.

            mainWordScore += letterScore;
            r += dr; c += dc;
        }

        if (wordLen > 1) totalScore += (mainWordScore * mainWordMultiplier);
        if (tilesPlacedCount == 7) totalScore += 50;

        return totalScore;
    }

    void applyMove(GameState& state, const Move& move, int score);
    /**
     * @brief High-performance state mutation specifically for MCTS rollouts.
     * @pre `cand.word` is the full overlapping string.
     * @post Mutates board and rack correctly without differential string translation.
     */
    void applyCandidateMove(GameState& state, const kernel::MoveCandidate& cand);
    void commitSnapshot(GameState& backup, const GameState& current);
    void restoreSnapshot(GameState& current, const GameState& backup);
    bool attemptExchange(GameState& state, const Move& move);
    void applySixPassPenalty(GameState& state);
    void applyEmptyRackBonus(GameState& state, int winnerIdx);

    /**
     * @brief Wrapper to route TrueScore calculation at compile time based on orientation.
     */
    static inline int calculateTrueScore(const kernel::MoveCandidate &move, const LetterBoard& letters, const Board &bonusBoard) {
        if (move.isHorizontal)
            return calculateTrueScoreFast<true>(move, letters, bonusBoard);
        else
            return calculateTrueScoreFast<false>(move, letters, bonusBoard);
    }
}