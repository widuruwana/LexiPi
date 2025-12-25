#include "../include/ai_player.h"
#include "../include/heuristics.h" // For scoring
#include "../include/tile_tracker.h"
#include "../include/endgame_solver.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <future>

using namespace std;
using namespace std::chrono;

// Helper to calculate the Rack Mask (which letters do we physically have?)
uint32_t getRackMask(const string &rackStr) {
    uint32_t mask = 0;
    bool hasBlank = false;
    for (char c : rackStr) {
        if (c == '?') {
            hasBlank = true;
        } else if (isalpha(c)) {
            mask |= (1 << (toupper(c) - 'A'));
        }
    }
    // If we have a blank, we can form any letter
    if (hasBlank) return 0xFFFFFFFF; // Blank can be anything

    return mask;
}

// Optimization: Prune the search space to boundingBox + 1
struct SearchRange {
    int minRow, maxRow, minCol, maxCol;
    bool isEmpty; // track empty board state
};

// Helper to calculate score for a specific move
int calculateTrueScore(const MoveCandidate &move, const LetterBoard& letters, const Board &bonusBoard) {

    int totalScore = 0;
    int mainWordScore = 0;
    int mainWordMultiplier = 1;
    int tilesPlacedCount = 0;

    int r = move.row;
    int c = move.col;
    int dr = move.isHorizontal ? 0 : 1;
    int dc = move.isHorizontal ? 1 : 0;

    // 1. Scoring the main word
    for (char letter : move.word) {
        // Safeguard
        if (r < 0 || r > 14 || c < 0 || c > 14) {
            return -1000;
        }

        int letterScore = getTileValue(letter);
        bool isNewlyPlaced = (letters[r][c] == ' ');

        if (isNewlyPlaced) {
            tilesPlacedCount++;

            // Apply Letter Multipliers (only for new tiles)
            CellType bonus = bonusBoard[r][c];

            if (bonus == CellType::DLS) letterScore *= 2;
            else if (bonus == CellType::TLS) letterScore *= 3;

            if (bonus == CellType::DWS) mainWordMultiplier *= 2;
            else if (bonus == CellType::TWS) mainWordMultiplier *= 3;
        }

        mainWordScore += letterScore;

        // 2. Score cross words
        if (isNewlyPlaced) {
            // Checks perpendicular direction
            int pdr = move.isHorizontal ? 1 : 0;
            int pdc = move.isHorizontal ? 0 : 1;

            bool hasNeighbour = false;

            // Checks bounds dynamically based on where we are looking
            int checkR1 = r - pdr;
            int checkC1 = c - pdc;
            if (checkR1 >= 0 && checkR1 < 15 && checkC1 >= 0 && checkC1 < 15 && letters[checkR1][checkC1] != ' ') {
                hasNeighbour = true;
            }

            int checkR2 = r + pdr;
            int checkC2 = c + pdc;
            if (checkR2 >= 0 && checkR2 < 15 && checkC2 >= 0 && checkC2 < 15 && letters[checkR2][checkC2] != ' ') {
                hasNeighbour = true;
            }

            if (hasNeighbour) {
                // Find the start of the cross word
                int currR = r;
                int currC = c;

                // Scan backwards
                while (currR - pdr >= 0 && currC - pdc >= 0 && letters[currR-pdr][currC-pdc] != ' ') {
                    currR -= pdr;
                    currC -= pdc;
                }

                int crossScore = 0;
                int crossMult = 1;

                // Scan forwards to score the entier cross word
                while (currR < 15 && currC < 15) {
                    char cellLetter = letters[currR][currC];

                    if (currR == r && currC == c) {
                        // In the tile we just placed, so bonuses apply
                        int crossLetterScore = getTileValue(letter);
                        CellType crossBonus = bonusBoard[currR][currC];

                        if (crossBonus == CellType::DLS) crossLetterScore *= 2;
                        else if (crossBonus == CellType::TLS) crossLetterScore *= 3;

                        if (crossBonus == CellType::DWS) crossMult *= 2;
                        else if (crossBonus == CellType::TWS) crossMult *= 3;

                        crossScore += crossLetterScore;
                    } else if (cellLetter != ' ') {
                        // Existing tile. Adding value (no bonuses)
                        crossScore += getTileValue(cellLetter);
                    } else {
                        break; // End of the word (empty square)
                    }
                    currR += pdr;
                    currC += pdc;
                }
                totalScore += (crossScore * crossMult);
            }
        }
        r += dr;
        c += dc;
    }
    totalScore += (mainWordScore * mainWordMultiplier);

    // 3. Bingo bonus (50+ for using all 7 tiles)
    if (tilesPlacedCount == 7) {
        totalScore += 50;
    }
    
    // 4. Positional Heuristics (Strategic Bonuses)
    // Bonus for keeping the board open or blocking (simplified)
    // Bonus for playing near center (early game) or edges (late game)
    // This is a simple heuristic to encourage better board usage
    
    // Center control bonus (if not already covered by opening book)
    // We want to encourage playing towards the center or keeping lines open
    // For now, let's add a small bonus for each tile placed on a multiplier square
    // to encourage using them up (denying opponent)
    
    // (Already handled in main loop via multipliers, but we can add extra "denial" value)
    
    // Vowel/Consonant balance check is done in rack leave, so we don't need it here.
    
    return totalScore;
}


SearchRange getActiveBoardArea(const LetterBoard &letters) {
    int minR = 14;
    int maxR = 0;
    int minC = 14;
    int maxC = 0;

    bool empty = true;

    // 225 iterations that fits in L1 cache
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (letters[r][c] != ' ') {
                if (r < minR) minR = r;
                if (c < minC) minC = c;
                if (r > maxR) maxR = r;
                if (c > maxC) maxC = c;
                empty = false;
            }
        }
    }

    if (empty) {
        return {7, 7, 7, 7, true};
    }

    // Expand by one to include the "Inflence Zone" (parallel plays)
    // Using min/max to ensure no going out of bounds.
    return{
        max(0, minR - 1),
        min(14, maxR + 1),
        max(0, minC - 1),
        min(14, maxC + 1),
        false
    };
}

AIPlayer::DifferentialMove AIPlayer::calculateEngineMove(const LetterBoard &letters, const MoveCandidate &bestMove) {

    DifferentialMove diff;
    diff.row = -1;
    diff.col = -1;
    diff.word = "";

    int r = bestMove.row;
    int c = bestMove.col;
    int dr = bestMove.isHorizontal ? 0 : 1;
    int dc = bestMove.isHorizontal ? 1 : 0;

    // Scan for full word against the board.
    for (char letter : bestMove.word) {
        // Is the current square empty?
        if (letters[r][c] == ' ') {
            // Yes, we must place a tile here.

            // If this is the first empty square. then this is "Engine start"
            if (diff.row == -1 && diff.col == -1) {
                diff.row = r;
                diff.col = c;
            }

            // Add the letter to the string we send back to engine
            diff.word += letter;
        }

        //advance to next board cell
        r += dr;
        c += dc;
    }

    return diff;
}

// Simulate a move and return the resulting state (OPTIMIZED)
AIPlayer::SimulatedState AIPlayer::simulateMove(const MoveCandidate &move,
                                                 const Board &bonusBoard,
                                                 const LetterBoard &letters,
                                                 const BlankBoard &blanks,
                                                 const TileBag &bag,
                                                 const TileRack &rack) {
    SimulatedState state;
    // OPTIMIZATION: Only copy what we need to modify
    state.letters = letters;
    state.blanks = blanks;
    // OPTIMIZATION: Don't copy the entire bag for simulation - we don't need it for opponent move gen
    // state.bag = bag;  // Commented out - not needed for 2-ply evaluation
    state.myRack = rack;
    state.success = false;
    state.scoreGained = 0;
    
    // Convert candidate to engine format
    DifferentialMove engineMove = calculateEngineMove(letters, move);
    
    if (engineMove.row == -1 || engineMove.word.empty()) {
        return state; // Invalid move
    }
    
    // OPTIMIZATION: Use a dummy empty bag to prevent refilling the rack.
    // This ensures we evaluate the rack LEAVE (remaining tiles), not the refilled rack.
    // It also saves copying the bag.
    TileBag dummyBag;
    
    // Apply the move using playWord
    MoveResult result = playWord(
        bonusBoard,
        state.letters,
        state.blanks,
        dummyBag,
        state.myRack,
        engineMove.row,
        engineMove.col,
        move.isHorizontal,
        engineMove.word
    );
    
    state.success = result.success;
    state.scoreGained = result.score;
    // Don't store the bag in state - we don't need it
    
    return state;
}

// Evaluate a move with 2-ply lookahead (OPTIMIZED)
float AIPlayer::evaluate2Ply(const MoveCandidate &myMove,
                              const Board &bonusBoard,
                              const LetterBoard &letters,
                              const BlankBoard &blanks,
                              const TileBag &bag,
                              const TileRack &myRack,
                              const TileRack &oppRack) {
    
    // Simulate my move
    SimulatedState afterMyMove = simulateMove(myMove, bonusBoard, letters, blanks, bag, myRack);
    
    if (!afterMyMove.success) {
        return -10000.0f; // Invalid move
    }
    
    // Calculate my rack leave value
    // string myRemainingRack = "";
    // for (const Tile &t : afterMyMove.myRack) {
    //     myRemainingRack += t.letter;
    // }
    // float myLeaveValue = calculateRackLeave(myRemainingRack);
    
    // Use EvaluationModel
    EvaluationModel& model = (externalModel != nullptr) ? *externalModel : evalModel;
    float myEval = model.evaluate(afterMyMove.scoreGained, afterMyMove.myRack, afterMyMove.letters);

    // OPTIMIZATION 1: Early exit if opponent has no tiles (endgame)
    if (oppRack.empty()) {
        return myEval;
    }
    
    // OPTIMIZATION 2: Generate opponent moves with early termination
    // We only need the BEST opponent move, not all of them
    // THREAD-SAFE: Create a temporary AIPlayer instance to avoid race conditions
    AIPlayer tempAI;
    tempAI.findAllMoves(afterMyMove.letters, oppRack);
    vector<MoveCandidate> oppCandidates = tempAI.candidates;
    
    // OPTIMIZATION 3: Aggressive move limiting and early termination
    int oppBestScore = 0;
    const int MAX_OPP_MOVES_TO_EVALUATE = 30; // Reduced from 50 for speed
    
    // OPTIMIZATION: If opponent has many moves, use even more aggressive filtering
    int numToCheck = min(MAX_OPP_MOVES_TO_EVALUATE, static_cast<int>(oppCandidates.size()));
    if (oppCandidates.size() > 500) {
        numToCheck = min(20, static_cast<int>(oppCandidates.size())); // Very aggressive for large move sets
    }
    
    // Sort opponent candidates by a quick heuristic (word length * 10)
    // Longer words tend to score higher
    for (auto &oppCand : oppCandidates) {
        oppCand.score = static_cast<int>(oppCand.word.length()) * 10;
    }
    std::sort(oppCandidates.begin(), oppCandidates.end(),
              [](const MoveCandidate &a, const MoveCandidate &b) {
                  return a.score > b.score;
              });
    
    // Evaluate only the most promising opponent moves
    for (int i = 0; i < numToCheck; i++) {
        int score = calculateTrueScore(oppCandidates[i], afterMyMove.letters, bonusBoard);
        if (score > oppBestScore) {
            oppBestScore = score;
        }
        
        // OPTIMIZATION 4: More aggressive early termination
        // If opponent can score 70+ points, that's already very strong
        if (oppBestScore >= 70) {
            break;
        }
    }
    
    // Evaluation: my_eval - (weight * opponent_best_reply)
    // We reduce the penalty for opponent score slightly to encourage high-scoring exchanges
    // This is risky but necessary for 600+ scores (offensive playstyle)
    float eval = myEval - (0.8f * static_cast<float>(oppBestScore));
    
    // Apply tile tracking adjustments for strategic depth
    eval = evaluateWithTileTracking(eval, oppRack);
    
    return eval;
}

// Evaluate a move with 3-ply lookahead (DEEP SEARCH)
float AIPlayer::evaluate3Ply(const MoveCandidate &myMove,
                              const Board &bonusBoard,
                              const LetterBoard &letters,
                              const BlankBoard &blanks,
                              const TileBag &bag,
                              const TileRack &myRack,
                              const TileRack &oppRack) {
    
    // 1. Simulate my move
    SimulatedState afterMyMove = simulateMove(myMove, bonusBoard, letters, blanks, bag, myRack);
    
    if (!afterMyMove.success) return -10000.0f;
    
    // Calculate my rack leave
    // string myRemainingRack = "";
    // for (const Tile &t : afterMyMove.myRack) myRemainingRack += t.letter;
    // float myLeaveValue = calculateRackLeave(myRemainingRack);
    
    EvaluationModel& model = (externalModel != nullptr) ? *externalModel : evalModel;
    float myEval = model.evaluate(afterMyMove.scoreGained, afterMyMove.myRack, afterMyMove.letters);

    if (oppRack.empty()) {
        return myEval;
    }
    
    // 2. Generate opponent's best reply (similar to 2-ply but we need the actual move)
    AIPlayer tempAI;
    tempAI.findAllMoves(afterMyMove.letters, oppRack);
    vector<MoveCandidate> oppCandidates = tempAI.candidates;
    
    if (oppCandidates.empty()) {
        // Opponent passes
        return myEval;
    }
    
    // Sort and pick top opponent moves
    for (auto &oppCand : oppCandidates) {
        oppCand.score = calculateTrueScore(oppCand, afterMyMove.letters, bonusBoard);
    }
    std::sort(oppCandidates.begin(), oppCandidates.end(),
              [](const MoveCandidate &a, const MoveCandidate &b) {
                  return a.score > b.score;
              });
              
    // We assume opponent plays their best move (minimax)
    // But to be safe, we average the top 3 or take the absolute best
    // Let's take the best one for standard minimax
    const MoveCandidate& bestOppMove = oppCandidates[0];
    int oppScore = bestOppMove.score;
    
    // 3. Simulate opponent's move
    // We need a bag for simulation, even if empty/approximate
    TileBag tempBag = bag;
    // Note: simulateMove doesn't actually draw tiles for the opponent in the returned state
    // because we don't know what they draw. We just use the board state.
    
    SimulatedState afterOppMove = simulateMove(bestOppMove, bonusBoard, afterMyMove.letters, afterMyMove.blanks, tempBag, oppRack);
    
    // 4. Find MY best counter-reply
    // I need to know what tiles I have.
    // In 3-ply, I would have drawn tiles after my first move.
    // Since I don't know what I drew, I have to work with my remaining rack (from step 1)
    // plus "average" tiles or just the remaining rack if bag is empty.
    
    // For simplicity and speed, we'll evaluate my best move using ONLY my remaining rack.
    // This is a conservative estimate (lower bound).
    
    AIPlayer myCounterAI;
    myCounterAI.findAllMoves(afterOppMove.letters, afterMyMove.myRack);
    
    int myCounterScore = 0;
    if (!myCounterAI.candidates.empty()) {
        for (auto &cand : myCounterAI.candidates) {
            int score = calculateTrueScore(cand, afterOppMove.letters, bonusBoard);
            if (score > myCounterScore) myCounterScore = score;
        }
    }
    
    // Evaluation: my_score + leave - opp_score + my_counter_score
    // We might want to weight the future scores less
    float eval = myEval
                 - (0.8f * static_cast<float>(oppScore))
                 + (0.9f * static_cast<float>(myCounterScore)); // Be more optimistic about future scoring
                 
    return eval;
}

// Monte Carlo Simulation
float AIPlayer::evaluateMonteCarlo(const MoveCandidate &myMove,
                                   const Board &bonusBoard,
                                   const LetterBoard &letters,
                                   const BlankBoard &blanks,
                                   const TileBag &bag,
                                   const TileRack &myRack,
                                   const TileRack &oppRack,
                                   int numSimulations) {
    
    // 1. Simulate my move
    SimulatedState afterMyMove = simulateMove(myMove, bonusBoard, letters, blanks, bag, myRack);
    if (!afterMyMove.success) return -10000.0f;

    float totalScoreDiff = 0.0f;
    int wins = 0;

    // Run simulations
    for (int i = 0; i < numSimulations; i++) {
        // Clone state for this simulation
        LetterBoard simLetters = afterMyMove.letters;
        BlankBoard simBlanks = afterMyMove.blanks;
        
        // Infer opponent rack using TileTracker
        // Note: In a real game, we don't know the opponent's rack.
        // We generate a likely one based on unseen tiles.
        // For this simulation, we need a non-const reference to the tracker if we were updating it,
        // but here we just use it to generate a rack.
        // Since evaluateMonteCarlo is a member function, we can access 'tileTracker'.
        // However, 'tileTracker' tracks the GLOBAL state. We need a temporary tracker or just use the current one's knowledge.
        // The current 'tileTracker' has the correct unseen tiles state for the start of the turn.
        
        // We need to account for the tiles I just played in 'afterMyMove'.
        // But 'tileTracker' hasn't been updated with my move yet (that happens at end of getMove).
        // So we should use a temporary tracker that includes my move's tiles as "seen".
        
        TileTracker simTracker = this->tileTracker;
        simTracker.markPlayed(calculateEngineMove(letters, myMove).word);
        
        TileRack simOppRack = simTracker.generateOpponentRack(oppRack.size());
        
        // Simulate a few turns (e.g., 2-4 plies) or until end of game
        // For speed, we'll do a shallow rollout (1-ply opponent response)
        // A full game rollout is too slow for Scrabble without optimized move gen (GADDAG).
        
        AIPlayer oppAI;
        oppAI.findAllMoves(simLetters, simOppRack);
        
        int oppScore = 0;
        if (!oppAI.candidates.empty()) {
             // Pick a random good move or best move
             // For Monte Carlo, we often assume opponent plays well but maybe not perfect
             // Let's pick the best move to be robust (pessimistic simulation)
             
             // Sort top 5
             int num = min(5, (int)oppAI.candidates.size());
             partial_sort(oppAI.candidates.begin(), oppAI.candidates.begin() + num, oppAI.candidates.end(),
                [&](const MoveCandidate &a, const MoveCandidate &b) {
                    return calculateTrueScore(a, simLetters, bonusBoard) > calculateTrueScore(b, simLetters, bonusBoard);
                });
             
             // Pick one of the top moves randomly? Or just the best?
             // Let's stick to best response (Minimax style rollout)
             oppScore = calculateTrueScore(oppAI.candidates[0], simLetters, bonusBoard);
        }
        
        // Score difference for this simulation
        // My Score (from initial move) - Opponent Score (response)
        // We could extend this further but it gets expensive.
        totalScoreDiff += (afterMyMove.scoreGained - oppScore);
    }

    return totalScoreDiff / numSimulations;
}

Move AIPlayer::getMove(const Board &bonusBoard,
                       const LetterBoard &letters,
                       const BlankBoard &blankBoard,
                       const TileBag &bag,
                       const Player &me,
                       const Player &opponent,
                       int PlayerNum) {

    // Load weights if not already loaded (or reload to allow tuning)
    // For now, we load once or every turn? Loading every turn allows live tuning.
    if (externalModel == nullptr && !weightsLoaded) {
        evalModel.loadWeights("data/weights.txt");
        weightsLoaded = true;
    }

    // To measure the time spent processing each turn
    auto startTime = high_resolution_clock::now();

    candidates.clear();

    // Extract the rack from player object
    const TileRack &myRack = me.rack;
    const TileRack &oppRack = opponent.rack;

    // Debug: Print what is used by Cutie_Pi
    string rackStr ="";
    for (const Tile& t: myRack) {
        rackStr += t.letter;
    }

    cout << "[AI] Cutie_Pi is thinking... (Rack: " << rackStr << ")" << endl;
    
    // Track my rack tiles as seen
    tileTracker.markDrawn(rackStr);
    
    // Check for special game states
    bool isEndgameState = isEndgame(bag, myRack, oppRack);
    bool isCritical = isCriticalPosition(me, opponent, bag);
    
    if (isEndgameState) {
        cout << "[AI] ENDGAME detected - optimizing final moves" << endl;
        EndgameSolver solver(bonusBoard);
        // Assuming we know opponent's rack perfectly in endgame for now (or use tile tracker to guess)
        // For true endgame solver, we need to know opponent's rack.
        // If we don't know it perfectly, we can use the tile tracker's unseen tiles as the opponent's rack
        // if the bag is empty.
        
        TileRack estimatedOppRack = oppRack;
        if (bag.empty() && oppRack.empty()) {
             // If we don't have opponent rack info passed in (it might be hidden), reconstruct it
             // But Player object usually has the rack.
             // If it's hidden, we use tile tracker.
             // For now, assume we can see it or it's passed correctly.
        }
        
        return solver.solveEndgame(letters, blankBoard, myRack, oppRack, me.score, opponent.score);
    }
    if (isCritical) {
        cout << "[AI] CRITICAL position - using deeper analysis" << endl;
    }

    // 1. Find all possible moves using the LETTER BOARD ( ignore bonuses for logic )
    // Use GADDAG if available (it is now)
    findAllMoves(letters, myRack);

    // 2. Pick the best one
    if (candidates.empty()) {
        cout << "[AI] No moves found. Passing." << endl;
        Move passMove;
        passMove.type = MoveType::PASS;
        return passMove;
    }

    // OPENING BOOK: Special handling for first move
    bool isOpeningMove = true;
    for (int r = 0; r < 15 && isOpeningMove; r++) {
        for (int c = 0; c < 15; c++) {
            if (letters[r][c] != ' ') {
                isOpeningMove = false;
                break;
            }
        }
    }

    if (isOpeningMove) {
        cout << "[AI] Opening move - applying opening book strategy" << endl;
        
        // Opening strategy: Prioritize long words through center with premium square usage
        for (auto &cand : candidates) {
            // Check if word goes through center (7,7)
            bool throughCenter = false;
            int centerTiles = 0;
            int r = cand.row;
            int c = cand.col;
            int dr = cand.isHorizontal ? 0 : 1;
            int dc = cand.isHorizontal ? 1 : 0;
            
            for (size_t i = 0; i < cand.word.length(); i++) {
                if (r == 7 && c == 7) {
                    throughCenter = true;
                }
                // Count tiles near center (6-8 range)
                if (r >= 6 && r <= 8 && c >= 6 && c <= 8) {
                    centerTiles++;
                }
                r += dr;
                c += dc;
            }
            
            if (throughCenter) {
                // Massive bonus for going through center
                cand.score += 100;
                
                // Extra bonus for length (longer opening = better board control)
                cand.score += static_cast<int>(cand.word.length()) * 15;
                
                // Bonus for using multiple center squares
                cand.score += centerTiles * 10;
                
                // Bonus for 5+ letter words (strong opening)
                if (cand.word.length() >= 5) {
                    cand.score += 50;
                }
                
                // Huge bonus for 7-letter opening (rare but powerful)
                if (cand.word.length() == 7) {
                    cand.score += 100;
                }
            }
        }
    }

    // OPTIMIZATION: Quick pre-scoring without full simulation
    // Use a fast heuristic: base score + word length bonus + premium square bonus + bingo bonus
    for (auto &cand : candidates) {
        int baseScore = calculateTrueScore(cand, letters, bonusBoard);
        
        // Count tiles placed (for bingo detection)
        int tilesPlaced = 0;
        int r = cand.row;
        int c = cand.col;
        int dr = cand.isHorizontal ? 0 : 1;
        int dc = cand.isHorizontal ? 1 : 0;
        
        // BALANCED AGGRESSION: Premium squares are valuable but not overwhelming
        int premiumBonus = 0;
        for (size_t i = 0; i < cand.word.length(); i++) {
            if (r >= 0 && r < 15 && c >= 0 && c < 15) {
                if (letters[r][c] == ' ') {
                    tilesPlaced++;
                    CellType bonus = bonusBoard[r][c];
                    if (bonus == CellType::TWS) premiumBonus += 30;      // Balanced
                    else if (bonus == CellType::DWS) premiumBonus += 15; // Balanced
                    else if (bonus == CellType::TLS) premiumBonus += 10; // Balanced
                    else if (bonus == CellType::DLS) premiumBonus += 5;  // Balanced
                }
            }
            r += dr;
            c += dc;
        }
        
        // BALANCED: Bingos are important but not overwhelming
        int bingoBonus = (tilesPlaced == 7) ? 60 : 0;  // Balanced bonus
        
        // BALANCED: Longer words are better
        int lengthBonus = static_cast<int>(cand.word.length()) * 4;  // Balanced
        
        cand.score = baseScore + lengthBonus + premiumBonus + bingoBonus;
    }

    // Sort by quick heuristic (descending)
    std::sort(candidates.begin(), candidates.end(),
              [](const MoveCandidate &a, const MoveCandidate &b) {
                  return a.score > b.score;
    });
    
    // OPTIMIZATION: Calculate accurate leave values only for top candidates
    // Reduced from TOP_K * 2 to TOP_K * 1.5 for speed
    int numForAccurateLeave = min(static_cast<int>(TOP_K_CANDIDATES * 1.5), static_cast<int>(candidates.size()));
    for (int i = 0; i < numForAccurateLeave; i++) {
        auto &cand = candidates[i];
        int baseScore = calculateTrueScore(cand, letters, bonusBoard);
        
        // Calculate remaining rack after this move
        SimulatedState simState = simulateMove(cand, bonusBoard, letters, blankBoard, bag, myRack);
        
        // Use EvaluationModel for sorting candidates too
        float evalScore = 0.0f;
        if (simState.success) {
            EvaluationModel& model = (externalModel != nullptr) ? *externalModel : evalModel;
            evalScore = model.evaluate(baseScore, simState.myRack, simState.letters);
        }
        
        // Store enhanced score (base + leave)
        cand.score = static_cast<int>(evalScore);
    }
    
    // Re-sort with accurate scores for top candidates
    std::sort(candidates.begin(), candidates.end(),
              [](const MoveCandidate &a, const MoveCandidate &b) {
                  return a.score > b.score;
    });

    // 3. Apply 2-ply evaluation to top K candidates (PARALLEL)
    // In critical positions, evaluate more candidates
    int numToEvaluate = isCritical ?
                        min(TOP_K_CANDIDATES + 5, static_cast<int>(candidates.size())) :
                        min(TOP_K_CANDIDATES, static_cast<int>(candidates.size()));
    
    if (isCritical) {
        cout << "[AI] Evaluating " << numToEvaluate << " candidates (critical position)" << endl;
    }
    
    // Parallel evaluation using std::async
    vector<future<float>> futures;
    futures.reserve(numToEvaluate);
    
    for (int i = 0; i < numToEvaluate; i++) {
        // Capture candidates[i] by value to avoid race condition
        MoveCandidate candidate = candidates[i];
        
        // Use Monte Carlo for top candidates
        // Always use MC for the very best candidate (heuristic) to ensure robustness
        // In critical positions, use it for top 3
        bool useMonteCarlo = (i == 0) || (isCritical && i < 3);
        bool use3Ply = (isCritical && i < 8) && !useMonteCarlo;
        
        futures.push_back(async(launch::async, [this, candidate, useMonteCarlo, use3Ply, &bonusBoard, &letters, &blankBoard, &bag, &myRack, &oppRack]() {
            if (useMonteCarlo) {
                // Run 30 simulations for top moves
                return evaluateMonteCarlo(candidate, bonusBoard, letters, blankBoard, bag, myRack, oppRack, 30);
            } else if (use3Ply) {
                return evaluate3Ply(candidate, bonusBoard, letters, blankBoard, bag, myRack, oppRack);
            } else {
                return evaluate2Ply(candidate, bonusBoard, letters, blankBoard, bag, myRack, oppRack);
            }
        }));
    }
    
    // Collect results
    float bestEval = -100000.0f;
    int bestIdx = 0;
    
    for (int i = 0; i < numToEvaluate; i++) {
        float eval = futures[i].get();
        if (eval > bestEval) {
            bestEval = eval;
            bestIdx = i;
        }
    }
    
    const MoveCandidate &bestMove = candidates[bestIdx];
    
    // Stop the timer
    auto stopTime = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stopTime - startTime);

    // Generate correct move string
    // We only send the tiles we are placing, not the full word.
    DifferentialMove engineMove = calculateEngineMove(letters, bestMove);

    if (engineMove.row == -1 || engineMove.word.empty()) {
        cout << "[AI] Error: Move exists, but its already on board and require no Tiles. Passing" << endl;
        Move passMove;
        passMove.type = MoveType::PASS;
        return passMove;
    }

    // Track the tiles we're playing
    tileTracker.markPlayed(engineMove.word);
    
    // Print status
    cout << "[AI] Found " << candidates.size() << " moves, evaluated top "
         << numToEvaluate << " (MC/3-ply/2-ply) in " << duration.count() << " microseconds." << endl;
    cout << "[AI] Play: " << bestMove.word
         << " (Send: " << engineMove.word << " @" << engineMove.row << "," << engineMove.col << ")"
         << " (eval: " << bestEval << ")" << endl;
    
    // Show tile tracking info in critical situations
    if (isCritical || isEndgameState) {
        cout << "[AI] Tiles remaining unseen: " << tileTracker.getTotalUnseen() << endl;
        if (tileTracker.hasBlankUnseen()) {
            cout << "[AI] WARNING: Blank(s) still unseen!" << endl;
        }
    }

    // Construct the Move object
    Move result;
    result.type = MoveType::PLAY;
    result.row = engineMove.row;
    result.col = engineMove.col;
    result.word = engineMove.word;
    result.horizontal = bestMove.isHorizontal;
    return result;
}

Move AIPlayer::getEndGameDecision() {
    // Placeholder, just pass for now
    Move m;
    m.type = MoveType::PASS;
    return m;
}

// Check if we're in endgame (bag empty, few tiles left)
bool AIPlayer::isEndgame(const TileBag &bag, const TileRack &myRack, const TileRack &oppRack) const {
    return bag.empty() && (myRack.size() + oppRack.size()) <= 14;
}

// Check if this is a critical position requiring deeper search
bool AIPlayer::isCriticalPosition(const Player &me, const Player &opponent, const TileBag &bag) const {
    int scoreDiff = abs(me.score - opponent.score);
    int tilesLeft = bag.size();
    
    // Critical if:
    // 1. Close game (within 50 points) and late game (< 20 tiles)
    // 2. Very close game (within 30 points) and mid-late game (< 30 tiles)
    // 3. Approaching endgame (< 10 tiles left)
    return (scoreDiff < 50 && tilesLeft < 20) ||
           (scoreDiff < 30 && tilesLeft < 30) ||
           (tilesLeft < 10);
}

// Adjust evaluation based on tile tracking
float AIPlayer::evaluateWithTileTracking(float baseEval, const TileRack &oppRack) const {
    float adjustment = 0.0f;
    
    // Estimate opponent's threat level
    float oppThreat = tileTracker.estimateOpponentThreat();
    
    // If opponent likely has bingo potential, be more defensive
    if (tileTracker.opponentHasBingoPotential(oppRack.size())) {
        adjustment -= 2.0f;  // Significantly reduced defensive bonus to encourage open play
    }
    
    // Adjust based on unseen high-value tiles
    if (tileTracker.hasBlankUnseen()) {
        adjustment -= 1.0f;  // Reduced caution
    }
    
    // Scale threat by how many tiles opponent has
    float threatFactor = static_cast<float>(oppRack.size()) / 7.0f;
    adjustment -= (oppThreat * threatFactor * 0.01f); // Minimal threat scaling
    
    return baseEval + adjustment;
}

void AIPlayer::setEvaluationModel(EvaluationModel* model) {
    this->externalModel = model;
}

vector<MoveCandidate> AIPlayer::findMovesHorizontal(const LetterBoard &letters, const TileRack &rack) {
    vector<MoveCandidate> results;
    
    // Prepare rack counts
    int rackCounts[27] = {0};
    for (const Tile &t: rack) {
        if (t.letter == '?') rackCounts[26]++;
        else rackCounts[toupper(t.letter) - 'A']++;
    }
    
    // Generate horizontal moves
    for (int r = 0; r < 15; r++) {
        RowConstraint constraints = ConstraintGenerator::generateRowConstraint(letters, r);
        
        for (int c = 0; c < 15; c++) {
            bool isAnchor = false;
            if (letters[r][c] == ' ') {
                if (r == 7 && c == 7 && getActiveBoardArea(letters).isEmpty) {
                    isAnchor = true;
                } else {
                    if (r > 0 && letters[r-1][c] != ' ') isAnchor = true;
                    else if (r < 14 && letters[r+1][c] != ' ') isAnchor = true;
                    else if (c > 0 && letters[r][c-1] != ' ') isAnchor = true;
                    else if (c < 14 && letters[r][c+1] != ' ') isAnchor = true;
                }
            }
            
            if (isAnchor) {
                // Temporary helper lambda to generate moves for this anchor
                auto genFromAnchor = [&](int anchorRow, int anchorCol, const LetterBoard &board, int counts[27], bool horizontal) {
                    int rackCountsCopy[27];
                    memcpy(rackCountsCopy, counts, sizeof(rackCountsCopy));
                    
                    uint32_t triedMask = 0;
                    for (int i = 0; i < 27; i++) {
                        if (rackCountsCopy[i] > 0) {
                            bool isBlank = (i == 26);
                            int startChar = isBlank ? 0 : i;
                            int endChar = isBlank ? 25 : i;
                            
                            for (int c = startChar; c <= endChar; c++) {
                                if (!isBlank && ((triedMask >> c) & 1)) continue;
                                if (!isBlank) triedMask |= (1 << c);
                                
                                char letter = (char)('A' + c);
                                
                                if (!constraints.isAllowed(anchorCol, letter)) continue;
                                
                                int childIdx = gDawg.nodes[gDawg.rootIndex].children[c];
                                if (childIdx != -1) {
                                    rackCountsCopy[i]--;
                                    
                                    string word = "";
                                    word += (isBlank ? (char)tolower(letter) : letter);
                                    
                                    // Go Left
                                    goLeftParallel(childIdx, anchorRow, anchorCol - 1, board, rackCountsCopy, word, anchorCol, horizontal, constraints, results);
                                    
                                    int delimIdx = gDawg.nodes[childIdx].children[26];
                                    if (delimIdx != -1) {
                                        goRightParallel(delimIdx, anchorRow, anchorCol + 1, board, rackCountsCopy, word, anchorCol, horizontal, constraints, results);
                                    }
                                    
                                    rackCountsCopy[i]++;
                                }
                            }
                        }
                    }
                };
                
                genFromAnchor(r, c, letters, rackCounts, true);
            }
        }
    }
    
    return results;
}

vector<MoveCandidate> AIPlayer::findMovesVertical(const LetterBoard &letters, const TileRack &rack) {
    vector<MoveCandidate> results;
    
    // Prepare rack counts
    int rackCounts[27] = {0};
    for (const Tile &t: rack) {
        if (t.letter == '?') rackCounts[26]++;
        else rackCounts[toupper(t.letter) - 'A']++;
    }
    
    // Transpose board for vertical moves
    LetterBoard transposed;
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            transposed[c][r] = letters[r][c];
        }
    }
    
    // Generate vertical moves (processed as horizontal on transposed board)
    for (int r = 0; r < 15; r++) {
        RowConstraint constraints = ConstraintGenerator::generateRowConstraint(transposed, r);
        
        for (int c = 0; c < 15; c++) {
            bool isAnchor = false;
            if (transposed[r][c] == ' ') {
                if (r == 7 && c == 7 && getActiveBoardArea(letters).isEmpty) {
                    isAnchor = true;
                } else {
                    if (r > 0 && transposed[r-1][c] != ' ') isAnchor = true;
                    else if (r < 14 && transposed[r+1][c] != ' ') isAnchor = true;
                    else if (c > 0 && transposed[r][c-1] != ' ') isAnchor = true;
                    else if (c < 14 && transposed[r][c+1] != ' ') isAnchor = true;
                }
            }
            
            if (isAnchor) {
                // Temporary helper lambda
                auto genFromAnchor = [&](int anchorRow, int anchorCol, const LetterBoard &board, int counts[27], bool horizontal) {
                    int rackCountsCopy[27];
                    memcpy(rackCountsCopy, counts, sizeof(rackCountsCopy));
                    
                    uint32_t triedMask = 0;
                    for (int i = 0; i < 27; i++) {
                        if (rackCountsCopy[i] > 0) {
                            bool isBlank = (i == 26);
                            int startChar = isBlank ? 0 : i;
                            int endChar = isBlank ? 25 : i;
                            
                            for (int c = startChar; c <= endChar; c++) {
                                if (!isBlank && ((triedMask >> c) & 1)) continue;
                                if (!isBlank) triedMask |= (1 << c);
                                
                                char letter = (char)('A' + c);
                                
                                if (!constraints.isAllowed(anchorCol, letter)) continue;
                                
                                int childIdx = gDawg.nodes[gDawg.rootIndex].children[c];
                                if (childIdx != -1) {
                                    rackCountsCopy[i]--;
                                    
                                    string word = "";
                                    word += (isBlank ? (char)tolower(letter) : letter);
                                    
                                    // Go Left
                                    goLeftParallel(childIdx, anchorRow, anchorCol - 1, board, rackCountsCopy, word, anchorCol, horizontal, constraints, results);
                                    
                                    int delimIdx = gDawg.nodes[childIdx].children[26];
                                    if (delimIdx != -1) {
                                        goRightParallel(delimIdx, anchorRow, anchorCol + 1, board, rackCountsCopy, word, anchorCol, horizontal, constraints, results);
                                    }
                                    
                                    rackCountsCopy[i]++;
                                }
                            }
                        }
                    }
                };
                
                genFromAnchor(r, c, transposed, rackCounts, false);
            }
        }
    }
    
    return results;
}

void AIPlayer::goLeftParallel(int nodeIdx, int currRow, int currCol,
                              const LetterBoard &letters, int rackCounts[27],
                              string currentWord, int anchorCol, bool isHorizontal, 
                              const RowConstraint& constraints, vector<MoveCandidate> &results) {
    
    if (currCol < 0) {
        int delimIdx = gDawg.nodes[nodeIdx].children[26];
        if (delimIdx != -1) {
            goRightParallel(delimIdx, currRow, anchorCol + 1, letters, rackCounts, currentWord, anchorCol, isHorizontal, constraints, results);
        }
        return;
    }
    
    if (letters[currRow][currCol] != ' ') {
        char existing = letters[currRow][currCol];
        int letterIdx = toupper(existing) - 'A';
        
        int childIdx = gDawg.nodes[nodeIdx].children[letterIdx];
        if (childIdx != -1) {
            string newWord = existing + currentWord;
            goLeftParallel(childIdx, currRow, currCol - 1, letters, rackCounts, newWord, anchorCol, isHorizontal, constraints, results);
            
            int delimIdx = gDawg.nodes[childIdx].children[26];
            if (delimIdx != -1) {
                goRightParallel(delimIdx, currRow, anchorCol + 1, letters, rackCounts, newWord, anchorCol, isHorizontal, constraints, results);
            }
        }
        return;
    }
    
    int delimIdx = gDawg.nodes[nodeIdx].children[26];
    if (delimIdx != -1) {
        goRightParallel(delimIdx, currRow, anchorCol + 1, letters, rackCounts, currentWord, anchorCol, isHorizontal, constraints, results);
    }
    
    bool hasTiles = false;
    for(int i=0; i<27; ++i) if(rackCounts[i] > 0) { hasTiles = true; break; }
    if (!hasTiles) return;
    
    uint32_t triedMask = 0;
    for (int i = 0; i < 27; i++) {
        if (rackCounts[i] > 0) {
            bool isBlank = (i == 26);
            int startChar = isBlank ? 0 : i;
            int endChar = isBlank ? 25 : i;
            
            for (int c = startChar; c <= endChar; c++) {
                if (!isBlank && ((triedMask >> c) & 1)) continue;
                if (!isBlank) triedMask |= (1 << c);
                
                char letter = (char)('A' + c);
                
                if (!constraints.isAllowed(currCol, letter)) continue;
                
                int childIdx = gDawg.nodes[nodeIdx].children[c];
                if (childIdx != -1) {
                    rackCounts[i]--;
                    
                    string newWord = (isBlank ? (char)tolower(letter) : letter) + currentWord;
                    
                    goLeftParallel(childIdx, currRow, currCol - 1, letters, rackCounts, newWord, anchorCol, isHorizontal, constraints, results);
                    
                    rackCounts[i]++;
                }
            }
        }
    }
}

void AIPlayer::goRightParallel(int nodeIdx, int currRow, int currCol,
                               const LetterBoard &letters, int rackCounts[27],
                               string currentWord, int startCol, bool isHorizontal, 
                               const RowConstraint& constraints, vector<MoveCandidate> &results) {
    
    if (gDawg.nodes[nodeIdx].isEndOfWord) {
        if (currCol >= 15 || letters[currRow][currCol] == ' ') {
            int finalStartCol = currCol - currentWord.length();
            int finalRow = isHorizontal ? currRow : finalStartCol;
            int finalCol = isHorizontal ? finalStartCol : currRow;
            
            if (finalRow >= 0 && finalRow < 15 && finalCol >= 0 && finalCol < 15) {
                results.push_back({finalRow, finalCol, currentWord, 0, isHorizontal});
            }
        }
    }
    
    if (currCol >= 15) return;
    
    if (letters[currRow][currCol] != ' ') {
        char existing = letters[currRow][currCol];
        int letterIdx = toupper(existing) - 'A';
        
        int childIdx = gDawg.nodes[nodeIdx].children[letterIdx];
        if (childIdx != -1) {
            goRightParallel(childIdx, currRow, currCol + 1, letters, rackCounts, currentWord + existing, startCol, isHorizontal, constraints, results);
        }
        return;
    }
    
    bool hasTiles = false;
    for(int i=0; i<27; ++i) if(rackCounts[i] > 0) { hasTiles = true; break; }
    if (!hasTiles) return;
    
    uint32_t triedMask = 0;
    for (int i = 0; i < 27; i++) {
        if (rackCounts[i] > 0) {
            bool isBlank = (i == 26);
            int startChar = isBlank ? 0 : i;
            int endChar = isBlank ? 25 : i;
            
            for (int c = startChar; c <= endChar; c++) {
                if (!isBlank && ((triedMask >> c) & 1)) continue;
                if (!isBlank) triedMask |= (1 << c);
                
                char letter = (char)('A' + c);
                
                if (!constraints.isAllowed(currCol, letter)) continue;
                
                int childIdx = gDawg.nodes[nodeIdx].children[c];
                if (childIdx != -1) {
                    rackCounts[i]--;
                    
                    string newWord = currentWord + (isBlank ? (char)tolower(letter) : letter);
                    
                    goRightParallel(childIdx, currRow, currCol + 1, letters, rackCounts, newWord, startCol, isHorizontal, constraints, results);
                    
                    rackCounts[i]++;
                }
            }
        }
    }
}

void AIPlayer::findAllMoves(const LetterBoard &letters, const TileRack &rack) {
    // PARALLELIZED: Generate horizontal and vertical moves concurrently
    auto hFuture = async(launch::async, [this, &letters, &rack]() {
        return findMovesHorizontal(letters, rack);
    });
    
    auto vFuture = async(launch::async, [this, &letters, &rack]() {
        return findMovesVertical(letters, rack);
    });
    
    // Collect results from both threads
    vector<MoveCandidate> horizontalMoves = hFuture.get();
    vector<MoveCandidate> verticalMoves = vFuture.get();
    
    // Combine results
    candidates.clear();
    candidates.reserve(horizontalMoves.size() + verticalMoves.size());
    candidates.insert(candidates.end(), horizontalMoves.begin(), horizontalMoves.end());
    candidates.insert(candidates.end(), verticalMoves.begin(), verticalMoves.end());
}

void AIPlayer::genMovesGaddag(int anchorRow, int anchorCol, const LetterBoard &letters, int rackCounts[27], bool isHorizontal, const RowConstraint& constraints) {
    // Start GADDAG traversal from Root
    // We are at an empty anchor square (anchorRow, anchorCol).
    // We must place a tile here.
    
    // Try every tile in rack
    // Also handle blanks
    
    // Optimization: Use a set to avoid duplicate starting letters from rack
    uint32_t triedMask = 0;
    
    // Iterate through A-Z and Blank
    for (int i = 0; i < 27; i++) {
        if (rackCounts[i] > 0) {
            bool isBlank = (i == 26);
            
            int startChar = isBlank ? 0 : i;
            int endChar = isBlank ? 25 : i;
            
            for (int c = startChar; c <= endChar; c++) {
                if (!isBlank && ((triedMask >> c) & 1)) continue;
                if (!isBlank) triedMask |= (1 << c);
                
                char letter = (char)('A' + c);
                
                // Check constraints
                if (!constraints.isAllowed(anchorCol, letter)) continue;
                
                // Check if this letter can start a word (has edge from root)
                int childIdx = gDawg.nodes[gDawg.rootIndex].children[c];
                if (childIdx != -1) {
                    // Backtrack: Decrement count
                    rackCounts[i]--;
                    
                    string word = "";
                    word += (isBlank ? (char)tolower(letter) : letter);
                    
                    // Go Left
                    goLeft(childIdx, anchorRow, anchorCol - 1, letters, rackCounts, word, anchorCol, isHorizontal, constraints);
                    
                    // Also, we can immediately switch to Right if we want (meaning this is the start of the word)
                    // The delimiter edge is at index 26
                    int delimIdx = gDawg.nodes[childIdx].children[26];
                    if (delimIdx != -1) {
                        goRight(delimIdx, anchorRow, anchorCol + 1, letters, rackCounts, word, anchorCol, isHorizontal, constraints);
                    }
                    
                    // Backtrack: Increment count
                    rackCounts[i]++;
                }
            }
        }
    }
}

void AIPlayer::goLeft(int nodeIdx, int currRow, int currCol,
                      const LetterBoard &letters, int rackCounts[27],
                      string currentWord, int anchorCol, bool isHorizontal, const RowConstraint& constraints) {
    
    // 1. Check bounds
    if (currCol < 0) {
        // Hit wall, must switch to Right
        int delimIdx = gDawg.nodes[nodeIdx].children[26];
        if (delimIdx != -1) {
            goRight(delimIdx, currRow, anchorCol + 1, letters, rackCounts, currentWord, anchorCol, isHorizontal, constraints);
        }
        return;
    }
    
    // 2. Check if square is occupied
    if (letters[currRow][currCol] != ' ') {
        // Must match existing tile
        char existing = letters[currRow][currCol];
        int letterIdx = toupper(existing) - 'A';
        
        int childIdx = gDawg.nodes[nodeIdx].children[letterIdx];
        if (childIdx != -1) {
            string newWord = existing + currentWord;
            goLeft(childIdx, currRow, currCol - 1, letters, rackCounts, newWord, anchorCol, isHorizontal, constraints);
            
            // Can we switch to Right here?
            // Yes, if we completed the "Left" part.
            int delimIdx = gDawg.nodes[childIdx].children[26];
            if (delimIdx != -1) {
                goRight(delimIdx, currRow, anchorCol + 1, letters, rackCounts, newWord, anchorCol, isHorizontal, constraints);
            }
        }
        return;
    }
    
    // 3. Square is empty: Try placing tile from rack
    // But first, we can switch to Right (finish left part)
    int delimIdx = gDawg.nodes[nodeIdx].children[26];
    if (delimIdx != -1) {
        goRight(delimIdx, currRow, anchorCol + 1, letters, rackCounts, currentWord, anchorCol, isHorizontal, constraints);
    }
    
    // Now try extending left
    // Check if we have any tiles left
    bool hasTiles = false;
    for(int i=0; i<27; ++i) if(rackCounts[i] > 0) { hasTiles = true; break; }
    if (!hasTiles) return;
    
    uint32_t triedMask = 0;
    for (int i = 0; i < 27; i++) {
        if (rackCounts[i] > 0) {
            bool isBlank = (i == 26);
            
            int startChar = isBlank ? 0 : i;
            int endChar = isBlank ? 25 : i;
            
            for (int c = startChar; c <= endChar; c++) {
                if (!isBlank && ((triedMask >> c) & 1)) continue;
                if (!isBlank) triedMask |= (1 << c);
                
                char letter = (char)('A' + c);
                
                // Check constraints (O(1) lookup)
                if (!constraints.isAllowed(currCol, letter)) continue;
                
                int childIdx = gDawg.nodes[nodeIdx].children[c];
                if (childIdx != -1) {
                    rackCounts[i]--;
                    
                    string newWord = (isBlank ? (char)tolower(letter) : letter) + currentWord;
                    
                    goLeft(childIdx, currRow, currCol - 1, letters, rackCounts, newWord, anchorCol, isHorizontal, constraints);
                    
                    rackCounts[i]++;
                }
            }
        }
    }
}

void AIPlayer::goRight(int nodeIdx, int currRow, int currCol,
                       const LetterBoard &letters, int rackCounts[27],
                       string currentWord, int startCol, bool isHorizontal, const RowConstraint& constraints) {
    
    // 1. Check if we formed a valid word
    if (gDawg.nodes[nodeIdx].isEndOfWord) {
        // Valid word found!
        // Check if we are at edge or next square is empty
        if (currCol >= 15 || letters[currRow][currCol] == ' ') {
            // Add candidate
            int finalStartCol = currCol - currentWord.length();
            int finalRow = isHorizontal ? currRow : finalStartCol;
            int finalCol = isHorizontal ? finalStartCol : currRow;
            
            // Verify bounds
            if (finalRow >= 0 && finalRow < 15 && finalCol >= 0 && finalCol < 15) {
                candidates.push_back({finalRow, finalCol, currentWord, 0, isHorizontal});
            }
        }
    }
    
    // 2. Check bounds
    if (currCol >= 15) return;
    
    // 3. Check if square is occupied
    if (letters[currRow][currCol] != ' ') {
        char existing = letters[currRow][currCol];
        int letterIdx = toupper(existing) - 'A';
        
        int childIdx = gDawg.nodes[nodeIdx].children[letterIdx];
        if (childIdx != -1) {
            goRight(childIdx, currRow, currCol + 1, letters, rackCounts, currentWord + existing, startCol, isHorizontal, constraints);
        }
        return;
    }
    
    // 4. Square is empty: Try placing tile
    bool hasTiles = false;
    for(int i=0; i<27; ++i) if(rackCounts[i] > 0) { hasTiles = true; break; }
    if (!hasTiles) return;
    
    uint32_t triedMask = 0;
    for (int i = 0; i < 27; i++) {
        if (rackCounts[i] > 0) {
            bool isBlank = (i == 26);
            
            int startChar = isBlank ? 0 : i;
            int endChar = isBlank ? 25 : i;
            
            for (int c = startChar; c <= endChar; c++) {
                if (!isBlank && ((triedMask >> c) & 1)) continue;
                if (!isBlank) triedMask |= (1 << c);
                
                char letter = (char)('A' + c);
                
                // Check constraints (O(1) lookup)
                if (!constraints.isAllowed(currCol, letter)) continue;
                
                int childIdx = gDawg.nodes[nodeIdx].children[c];
                if (childIdx != -1) {
                    rackCounts[i]--;
                    
                    string newWord = currentWord + (isBlank ? (char)tolower(letter) : letter);
                    
                    goRight(childIdx, currRow, currCol + 1, letters, rackCounts, newWord, startCol, isHorizontal, constraints);
                    
                    rackCounts[i]++;
                }
            }
        }
    }
}



















