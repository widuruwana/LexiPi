#include "../../include/kernel/greedy_engine.h"
#include "../../include/kernel/heuristics.h"
#include "../../include/engine/mechanics.h"
#include "../../include/engine/dictionary.h" // For gDictionary

namespace kernel {

    MoveCandidate GreedyEngine::generate_best_move(const GameState& state, const Board& bonusBoard) {
        
        MoveCandidate bestMove;
        bestMove.word[0] = '\0';
        bestMove.score = -10000;

        const Player& me = state.players[state.currentPlayerIndex];

        // 1. Convert Rack to Histogram (Stack)
        int rackCounts[27] = {0};
        for (const auto& t : me.rack) {
            if (t.letter == '?') rackCounts[26]++;
            else if (t.letter >= 'A' && t.letter <= 'Z')
                rackCounts[t.letter - 'A']++;
            else if (t.letter >= 'a' && t.letter <= 'z')
                rackCounts[t.letter - 'a']++;
        }

        // 2. Define Zero-Alloc Consumer
        auto consumer = [&](kernel::MoveCandidate& cand, int* remainingRack) -> bool {

            // OPTIMIZATION: Use Templated Scorer
            int boardScore = 0;
            if (cand.isHorizontal)
                boardScore = Mechanics::calculateTrueScoreFast<true>(cand, state.board, bonusBoard);
            else
                boardScore = Mechanics::calculateTrueScoreFast<false>(cand, state.board, bonusBoard);

            // B. Fast Leave Score (Direct Array Access)
            float leavePenalty = 0.0f;
            for(int i=0; i<26; i++) {
                if (remainingRack[i] > 0) {
                    leavePenalty += (kernel::Heuristics::LEAVE_VALUES[i] * remainingRack[i]);
                }
            }

            cand.score = boardScore + (int)leavePenalty;

            if (cand.score > bestMove.score) {
                bestMove = cand;
            }
            return true;
        };

        // 3. Execute Pipeline
        kernel::MoveGenerator::generate_raw(state.board, rackCounts, gDictionary, consumer);

        return bestMove;
    }

}