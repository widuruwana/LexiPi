#include "../../include/spectre/vanguard.h"
#include "../../include/engine/mechanics.h"
#include <cstring>

namespace spectre {

    double Vanguard::consult_council(const GameState& state, const kernel::MoveCandidate& cand) {
        Move m;
        m.row = cand.row;
        m.col = cand.col;
        m.horizontal = cand.isHorizontal;
        m.type = MoveType::PLAY;

        // FULL STRING PASS-THROUGH: Council agents natively skip existing board tiles
        std::strncpy(m.word, cand.word, 16);
        m.word[15] = '\0';

        float topoScore = general.evaluate_topology(state, m);
        double topoBias = (topoScore - 0.5) * 40.0;
        float nav = treasurer.evaluate_equity(state, m, cand.score);

        int myScore = state.players[state.currentPlayerIndex].score;
        int oppScore = state.players[1 - state.currentPlayerIndex].score;
        int scoreDiff = myScore - oppScore;

        return treasurer.normalize_to_winprob(scoreDiff, nav + topoBias, state.bag.size());
    }

}