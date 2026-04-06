#pragma once

#include <string>
#include "engine/types.h"

using namespace std;

/**
 * @brief Attempts to play a word on the physical board.
 * * @pre THE FULL STRING DOCTRINE: `rackWord` MUST be the full, completely overlapping string.
 * Differential strings will cause exact-match targeting failures and rack desyncs.
 * * @param bonusBoard Static geometry of DWS, TWS, etc.
 * @param letters The physical characters on the board.
 * @param blanks The physical boolean tracking of played blanks.
 * @param bag The global tile bag.
 * @param rack The physical rack of the current player.
 * @param startRow The precise starting row of the FIRST letter in `rackWord`.
 * @param startCol The precise starting column of the FIRST letter in `rackWord`.
 * @param horizontal True if playing left-to-right, False if top-to-bottom.
 * @param rackWord The FULL string (e.g., "BIRDMaN").
 * * @return MoveResult containing success flag and calculated score.
 */
MoveResult playWord(
    const Board &bonusBoard,
    LetterBoard &letters,
    BlankBoard &blanks,
    TileBag &bag,
    TileRack &rack,
    int startRow,
    int startCol,
    bool horizontal,
    const string &rackWord
);