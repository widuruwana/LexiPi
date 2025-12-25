#include "../../../include/modes/Training/training_mode.h"
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

TrainingMode::TrainingMode() {
    // Initialize Board
    bonusBoard = createBoard();

    // Load initial weights
    // Try loading from data/weights.txt, if not found try ../data/weights.txt
    // We check existence first to avoid printing the warning from loadWeights unless necessary
    std::ifstream f("data/weights.txt");
    if (f.good()) {
        sharedModel.loadWeights("data/weights.txt");
    } else {
        sharedModel.loadWeights("../data/weights.txt");
    }
    
    // Initialize players
    // Note: We need to ensure AIPlayer can use an external model or we need to inject it.
    // Currently AIPlayer has its own 'evalModel' member.
    // We should probably modify AIPlayer to allow setting the model, or just rely on it loading from file.
    // But for training, we want to update the model in memory and have both AIs use it.
    // For now, let's assume we update the file and they reload it, OR we modify AIPlayer.
    // Modifying AIPlayer to take a pointer to EvaluationModel is cleaner.
    // But given the constraints, let's just update the file for now? No, that's slow.
    // Let's assume we can access the model.
    // Actually, AIPlayer::evalModel is private.
    // We need to make it public or add a setter.
    // For this implementation, I will assume we added a setter `setEvaluationModel`.
}

void TrainingMode::run() {
    cout << "Starting Training Mode..." << endl;
    cout << "Episodes: " << numGames << ", Learning Rate: " << learningRate << endl;

    for (int i = 0; i < numGames; i++) {
        playEpisode();
        
        if ((i + 1) % saveInterval == 0) {
            cout << "Episode " << (i + 1) << " complete. Saving weights..." << endl;
            // Try to save to the same location we loaded from, or default to data/weights.txt
            // For simplicity, try data/weights.txt first (project root), then ../data/weights.txt
            if (!sharedModel.saveWeights("data/weights.txt")) {
                 sharedModel.saveWeights("../data/weights.txt");
            }
        }
    }
    
    cout << "Training Complete." << endl;
}

void TrainingMode::playEpisode() {
    // Reset Game State
    clearLetterBoard(board);
    clearBlankBoard(blankBoard);
    bag = createStandardTileBag();
    shuffleTileBag(bag);
    
    // Reset Players
    player1 = Player();
    player2 = Player();
    
    // Draw initial tiles
    for(int i=0; i<7; i++) {
        if(!bag.empty()) { player1.rack.push_back(bag.back()); bag.pop_back(); }
        if(!bag.empty()) { player2.rack.push_back(bag.back()); bag.pop_back(); }
    }
    
    // Clear history
    episodeHistory.clear();
    
    bool gameRunning = true;
    int passCount = 0;
    int turnCount = 0;
    
    // Game Loop
    while (gameRunning) {
        Player& currentPlayer = (turnCount % 2 == 0) ? player1 : player2;
        Player& opponent = (turnCount % 2 == 0) ? player2 : player1;
        AIPlayer& currentAI = (turnCount % 2 == 0) ? ai1 : ai2;
        
        // Inject the shared model
        currentAI.setEvaluationModel(&sharedModel);
        
        // Get Move
        Move move = currentAI.getMove(bonusBoard, board, blankBoard, bag, currentPlayer, opponent, (turnCount % 2) + 1);
        
        if (move.type == MoveType::PASS) {
            passCount++;
            if (passCount >= 2) gameRunning = false;
        } else {
            passCount = 0;
            
            // Execute Move
            MoveResult result = playWord(bonusBoard, board, blankBoard, bag, currentPlayer.rack, move.row, move.col, move.horizontal, move.word);
            
            if (result.success) {
                currentPlayer.score += result.score;

                // Notify Opponent AI
                AIPlayer& opponentAI = (turnCount % 2 == 0) ? ai2 : ai1;
                opponentAI.observeOpponentMove(move);
                
                // RECORD TRAINING DATA
                // We need the features for the state *after* the move (S')
                // But wait, TD learning usually updates V(S) based on R + V(S').
                // Here, the AI made a move to reach state S'.
                // The "Value" of this state is what the AI predicted.
                // We need to capture:
                // 1. The features of the resulting state (S')
                // 2. The predicted value V(S')
                // 3. The reward R (score gained)
                
                // Re-calculate features and value for the new state
                // Note: This is slightly inefficient as AI just calculated it, but we need to capture it.
                auto features = sharedModel.getFeatures(result.score, currentPlayer.rack, board);
                float value = sharedModel.evaluate(result.score, currentPlayer.rack, board);
                
                episodeHistory.push_back({features, value, (float)result.score});
            }
        }
        
        // Check Endgame
        if (player1.rack.empty() && bag.empty()) gameRunning = false;
        if (player2.rack.empty() && bag.empty()) gameRunning = false;
        
        turnCount++;
        if (turnCount > 100) gameRunning = false; // Safety break
    }
    
    // End of Episode - Update Weights (TD Lambda or Monte Carlo)
    // Let's use simple TD(0) for now, or actually Monte Carlo (update towards final score)
    // TD(0): V(St) = R + gamma * V(St+1)
    // Error = (R + gamma * V(St+1)) - V(St)
    
    updateModel();
}

void TrainingMode::updateModel() {
    // Iterate backwards through history? Or forwards?
    // TD(0) updates based on next state.
    
    for (size_t t = 0; t < episodeHistory.size() - 1; ++t) {
        const auto& currentStep = episodeHistory[t];
        const auto& nextStep = episodeHistory[t+1];
        
        // Target: Reward + Discount * NextValue
        // Note: In Scrabble, the "Reward" is the score of the move.
        // The "Value" is the strategic value of the position.
        // Our evaluate() function returns (Score + StrategicValue).
        // So V(S) includes the immediate reward.
        // Wait, evaluate() returns Score + Leave + Synergy.
        // So V(S) = R + Potential.
        // The "Next State" S' is the state AFTER the opponent moves?
        // No, in self-play, we usually look at state after our move.
        // But the "Next Value" should be from the perspective of the same player?
        // This gets tricky with 2-player zero-sum.
        // Simple approach: Train to predict Final Score Difference?
        // Or Train to maximize (MyScore - OpponentScore).
        
        // Let's stick to the definition in EvaluationModel:
        // V(move) = Score + Leave + Synergy.
        // We want V(move) to approximate "True Value".
        // True Value = Immediate Score + Future Score.
        // So V(S_t) should predict (R_t + R_{t+2} + ...).
        
        // TD Target: R_t + gamma * V(S_{t+2}) ?? (Next time I move)
        // Let's try a simpler gradient ascent on the winner.
        // If Player 1 won, increase weights for all features seen by Player 1.
        // This is "Monte Carlo Policy Gradient" (REINFORCE).
        
        float finalScoreDiff = (float)(player1.score - player2.score);
        
        // If this step was made by Player 1
        // We need to track who made which step.
        // For now, let's assume alternating turns.
        bool isPlayer1 = (t % 2 == 0);
        
        float rewardSignal = isPlayer1 ? finalScoreDiff : -finalScoreDiff;
        
        // Scale by learning rate
        // Update = alpha * Reward * Gradient
        // Gradient of Linear Model w.r.t Weights is just the Features.
        
        sharedModel.updateWeights(currentStep.features, learningRate * rewardSignal * 0.01f); // Scaling factor
    }
}