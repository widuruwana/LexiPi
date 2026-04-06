#include "../../include/spectre/spectre_engine.h"
#include "../../include/spectre/judge.h"
#include "../../include/engine/dictionary.h"
#include "../../include/kernel/move_generator.h"
#include "../../include/engine/mechanics.h"

#include <cstring>
#include <iostream>
#include <algorithm>
#include <random>

namespace spectre {

    SpectreEngine::SpectreEngine() {
        reset(); // Initialize agents on creation
    }

    void SpectreEngine::reset() {
        spy = std::make_unique<Spy>();
        vanguard = std::make_unique<Vanguard>();
    }

    kernel::MoveCandidate SpectreEngine::deliberate(const GameState& state, const Board& bonusBoard, bool isSpeediPi) {

        kernel::MoveCandidate bestMove;
        bestMove.word[0] = '\0';
        bestMove.score = -10000;
        const Player& me = state.players[state.currentPlayerIndex];

        // >>> SPEEDI_PI (O(1) Array Conversion) <<<
        if (isSpeediPi) {
            int rackCounts[27] = {0};
            for (const auto& t : me.rack) {
                if (t.letter == '?') rackCounts[26]++;
                else if (isalpha(t.letter)) rackCounts[toupper(t.letter) - 'A']++;
            }

            auto greedyConsumer = [&](kernel::MoveCandidate& cand, int* leave) -> bool {
                int currentScore = Mechanics::calculateTrueScore(cand, state.board, bonusBoard);
                if (currentScore > bestMove.score) {
                    cand.score = currentScore;
                    bestMove = cand;
                }
                return true;
            };

            kernel::MoveGenerator::generate_raw(state.board, rackCounts, gDictionary, greedyConsumer);
            return bestMove;
        }

        // >>> CUTIE_PI (Multi-Agent System) <<<
        spy->updateGroundTruth(state.board, me.rack, state.bag);

        if (state.bag.empty()) {
            // DELEGATION: The engine no longer knows the particle thresholds.
            // It just asks the Spy for the opponent's rack.
            TileRack oppRack = spy->inferOpponentRack();
            bestMove = Judge::solveEndgame(state.board, bonusBoard, me.rack, oppRack, gDictionary);
        } else {
            std::vector<kernel::MoveCandidate> candidates = kernel::MoveGenerator::generate(state.board, me.rack, gDictionary);
            for(auto& cand : candidates) {
                cand.score = Mechanics::calculateTrueScore(cand, state.board, bonusBoard);
            }

            bestMove = vanguard->select_best_move(state, bonusBoard, candidates, *spy);
        }

        return bestMove;
    }

    void SpectreEngine::notify_move(const Move& move, const LetterBoard& board, const Board& bonusBoard) {
        spy->observeOpponentMove(move, board, bonusBoard);
    }
}