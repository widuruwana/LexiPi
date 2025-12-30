#pragma once

#include "player_controller.h"
#include "dawg.h"
#include "fast_constraints.h"
#include "spectre/move_generator.h"
#include "rack.h"
#include "board.h"
#include <vector>
#include <string>

using namespace std;

enum class AIStyle {
    SPEEDI_PI, // Fast, Static Heuristics (The "Speedster")
    CUTIE_PI // The "Champion"
};

class AIPlayer : public PlayerController {
public:
    AIPlayer() = default;
    virtual ~AIPlayer() = default;

    AIPlayer(AIStyle style = AIStyle::CUTIE_PI);

    vector<spectre::MoveCandidate> candidates;

    // Main entry point called by the game loop
    Move getMove(const Board &bonusBoard,
                 const LetterBoard &letters,
                 const BlankBoard &blankBoard,
                 const TileBag &bag,
                 const Player &me,
                 const Player &opponent,
                 int PlayerNum) override;

    // [FIX] Added declaration to match src/ai_player.cpp
    std::vector<char> exchangeTiles(const std::vector<Tile>& rack) override;

    // Placeholder for now (required by pure virtual)
    Move getEndGameDecision() override;

    bool shouldChallenge(const Move &opponentMove, const LetterBoard &board) const;

    // The solver functions
    void findAllMoves(const LetterBoard &letters, const TileRack &rack);

    std::string getName() const;

private:

    AIStyle style;

    // Recursion state helpers
    int currentRow;
    bool currentIsHorizontal;

    // Helper struct for engine translation
    struct DifferentialMove {
        int row, col;
        string word;
    };

    DifferentialMove calculateEngineMove(const LetterBoard &letters, const spectre::MoveCandidate &bestMove);

    bool isRackBad(const TileRack &rack);

    std::string getTilesToExchange(const TileRack &rack);
};