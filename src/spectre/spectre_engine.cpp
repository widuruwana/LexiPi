#include "../../include/spectre/spectre_engine.h"
#include "../../include/spectre/vanguard.h"
#include "../../include/spectre/spy.h"
#include "../../include/spectre/judge.h"
#include "../../include/engine/dictionary.h"
#include "../../include/kernel/move_generator.h"
#include "../../include/engine/mechanics.h" // Needed for score calculation

#include <memory>
#include <cstring>
#include <iostream>

using namespace spectre;

namespace SpectreEngine {

    // --- PERSISTENT AGENTS ---
    static thread_local std::unique_ptr<spectre::Spy> spy = std::make_unique<spectre::Spy>();
    static thread_local std::unique_ptr<spectre::Vanguard> vanguard = std::make_unique<spectre::Vanguard>();

    kernel::MoveCandidate deliberate(const GameState& state, const Board& bonusBoard) {

        kernel::MoveCandidate bestMove;
        bestMove.word[0] = '\0';
        bestMove.score = -10000;

        const Player& me = state.players[state.currentPlayerIndex];

        // 1. UPDATE INTELLIGENCE
        spy->updateGroundTruth(state.board, me.rack, state.bag);

        // 2. DECISION FORK
        if (state.bag.empty()) {
            // >>> THE JUDGE (Endgame Solver) <<<

            std::vector<char> inferredOppChars;
            const std::vector<char>& rawPool = spy->getUnseenPool();
            size_t poolSize = rawPool.size();

            // [FIX] BYPASS SPY LOGIC AT ENDGAME
            if (poolSize < 9) {
                // CASE A: Perfect Information (<= 7 tiles)
                // If there are 7 or fewer tiles left, the opponent holds exactly these.
                if (poolSize <= 7) {
                    inferredOppChars = rawPool;
                }
                // CASE B: Near-Perfect Information (8 tiles)
                // Bypass particle weights. Just take a random sample from the raw pool.
                else {
                    inferredOppChars = rawPool;
                    static thread_local std::mt19937 rng(std::random_device{}());
                    std::shuffle(inferredOppChars.begin(), inferredOppChars.end(), rng);
                    // Resize to standard rack size (7) if needed, dropping 1 random tile
                    if (inferredOppChars.size() > 7) inferredOppChars.resize(7);
                }
            }
            else {
                // CASE C: Deep Endgame / Midgame Transition
                // Use Spy's sophisticated particle filtering
                inferredOppChars = spy->generateWeightedRack();
            }

            // Convert chars to Tiles with points
            TileRack oppRack;
            for(char c : inferredOppChars) {
                Tile t; t.letter = c; t.points = Mechanics::getPointValue(c);
                oppRack.push_back(t);
            }

            bestMove = Judge::solveEndgame(state.board, bonusBoard, me.rack, oppRack, gDictionary);

        }
        else {
            // >>> THE VANGUARD (Midgame Strategy) <<<

            // A. Generate Candidates
            std::vector<kernel::MoveCandidate> candidates = kernel::MoveGenerator::generate(
                state.board,
                me.rack,
                gDictionary
            );

            int debugCount = 0;
            for(auto& cand : candidates) {
                cand.score = Mechanics::calculateTrueScore(cand, state.board, bonusBoard);
            }

            // B. Execute Guided MCTS
            bestMove = vanguard->select_best_move(
                state,
                bonusBoard,
                candidates,
                *spy
            );
        }

        return bestMove;
    }

    void notify_move(const Move& move, const LetterBoard& board) {
        spy->observeOpponentMove(move, board);
    }
}