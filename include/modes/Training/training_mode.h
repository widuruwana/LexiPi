#pragma once

#include "../../../include/board.h"
#include "../../../include/tiles.h"
#include "../../../include/move.h"
#include "../../../include/ai_player.h"
#include "../../../include/evaluation_model.h"
#include <vector>
#include <map>

class TrainingMode {
public:
    TrainingMode();
    ~TrainingMode() = default;

    void run();

private:
    AIPlayer ai1;
    AIPlayer ai2;
    EvaluationModel sharedModel; // Both AIs share the same model being trained

    // Game State
    Board bonusBoard;
    LetterBoard board;
    BlankBoard blankBoard;
    TileBag bag;
    Player player1;
    Player player2;

    // Hyperparameters
    float learningRate = 0.001f;
    float discountFactor = 0.95f; // Gamma
    int numGames = 1000;
    int saveInterval = 100;

    // Training State
    struct TrainingStep {
        std::map<std::string, float> features; // Gradient of Value w.r.t Weights
        float predictedValue; // V(s)
        float reward; // R
    };

    std::vector<TrainingStep> episodeHistory;

    void playEpisode();
    void updateModel();
};