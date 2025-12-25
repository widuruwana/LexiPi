#include <iostream>
#include <string>
#include <sstream>

#include "../../../include/board.h"
#include "../../../include/move.h"
#include "../../../include/tiles.h"
#include "../../../include/rack.h"
#include "../../../include/dict.h"
#include "../../../include/choices.h"
#include "../../../include/human_player.h"
#include "../../../include/ai_player.h"
#include "../../../include/modes/Home/home.h"

using namespace std;

void runPvE() {

    // 1. Setup Board
    Board bonusBoard = createBoard();
    LetterBoard letters;
    clearLetterBoard(letters);
    BlankBoard blanks;
    clearBlankBoard(blanks);

    // 2. Setup Bag
    TileBag bag = createStandardTileBag();
    shuffleTileBag(bag);

    // 3. Setup Players
    Player players[2];
    PlayerController* controllers[2];

    // Player 1 (Human)
    drawTiles(bag, players[0].rack, 7);
    players[0].score = 0;
    players[0].passCount = 0;
    controllers[0] = new HumanPlayer();

    // Player 2 (AI)
    drawTiles(bag, players[1].rack, 7);
    players[1].score = 0;
    players[1].passCount = 0;
    controllers[1] = new AIPlayer();

    int currentPlayer = 0; // 0 -> Human, 1 -> AI

    GameSnapshot lastSnapShot;
    LastMoveInfo lastMove;
    lastMove.exists = false;
    lastMove.playerIndex = -1;

    bool canChallenge = false;
    bool dictActive = true;

    if (!loadDictionary("csw24.txt")) {
        cout << "WARNING: Dictionary not loaded, AI might misbehave.\n";
        dictActive = false;
    }

    printTitle();
    cout << "Starting Match: Player vs CUTIE_PI (AI)\n";
    cout << "Good luck, hooman.\n\n";

    printBoard(bonusBoard, letters);
    cout << "Scores: You = " << players[0].score << " | Cutie_Pi = " << players[1].score << "\n";

    // Only show rack in human's turn
    if (currentPlayer == 0) {
        cout << "Your Rack:" << endl;
        printRack(players[currentPlayer].rack);
    }

    // Game loop
    while (true) {
        if (handleSixPassEndGame(players)) {
            break;
        }

        if (handleEmptyRackEndGame(bonusBoard,
                                   letters,
                                   blanks,
                                   bag,
                                   players,
                                   lastSnapShot,
                                   lastMove,
                                   currentPlayer,
                                   canChallenge,
                                   dictActive,
                                   controllers[currentPlayer])) {
            break;
        }

        // Get Move  (Human input)
        Move move = controllers[currentPlayer]->getMove(bonusBoard,
                                                        letters,
                                                        blanks,
                                                        bag,
                                                        players[currentPlayer], // Me
                                                        players[1 - currentPlayer], // Opponent
                                                        currentPlayer + 1);

        // move execution
        if (move.type == MoveType::PASS) {
            passTurn(players, currentPlayer, canChallenge, lastMove);
            //printBoard(bonusBoard, letters);
            //cout << "Scores: You = " << players[0].score << " | Cutie_Pi = " << players[1].score << endl;

            if (currentPlayer == 0) {
                cout << "\nYour Turn:" << endl;
                printRack(players[currentPlayer].rack);
            }
            continue;
        }

        if (move.type == MoveType::QUIT) {
            // Only Human can quit via menu
            if (handleQuit(players, currentPlayer)) {
                break;
            }
            continue;
        }

        if (move.type == MoveType::CHALLENGE) {
            // AI likely won't challenge in V1, but logic remains
            challengeMove(bonusBoard, letters, blanks, bag, players,
                          lastSnapShot, lastMove, currentPlayer, canChallenge, dictActive);
            continue;
        }

        if (move.type == MoveType::EXCHANGE) {
            bool success = executeExchangeMove(bag, players[currentPlayer], move);
            if (success) {
                lastMove.exists = false;
                canChallenge = false;

                // Increment pass count for exchange
                players[currentPlayer].passCount++;

                printBoard(bonusBoard, letters);
                cout << "Scores: You = " << players[0].score << " | Cutie_Pi = " << players[1].score << endl;

                // Notify the OTHER player (who is about to become current)
                AIPlayer* aiOpponent = dynamic_cast<AIPlayer*>(controllers[1 - currentPlayer]);
                if (aiOpponent) {
                    aiOpponent->observeOpponentMove(move);
                }

                currentPlayer = 1 - currentPlayer;
                if (currentPlayer == 0) {
                    cout << "\nYour Turn:" << endl;
                    printRack(players[currentPlayer].rack);
                }
            }
            continue;
        }

        if (move.type == MoveType::PLAY) {
            bool success = executePlayMove(bonusBoard, letters, blanks, bag, players,
                                           players[currentPlayer], move, lastSnapShot);

            if (success) {
                lastMove.exists = true;
                lastMove.playerIndex = currentPlayer;
                lastMove.startRow = move.row;
                lastMove.startCol = move.col;
                lastMove.horizontal = move.horizontal;
                canChallenge = true;

                printBoard(bonusBoard, letters);
                cout << "Scores: You = " << players[0].score << " | Cutie_Pi = " << players[1].score << endl;

                currentPlayer = 1 - currentPlayer;
                if (currentPlayer == 0) {
                    cout << "\nYour Turn:" << endl;
                    printRack(players[currentPlayer].rack);
                }
            } else {
                if (currentPlayer == 1) {
                    cout << "[Critical Error] AI played an invalid move. Force passing to prevent an infinity loop\n";
                    passTurn(players, currentPlayer, canChallenge, lastMove);
                    if (currentPlayer == 0) {
                        cout << "\nYour Turn:" << endl;
                        printRack(players[currentPlayer].rack);
                    }
                } else {
                    // Human can retry
                    cout << "Invalid move. Try again.\n";
                }
            }
        }
    }

    // Clean up
    delete controllers[0];
    delete controllers[1];
    waitForQuitKey();
    clearScreen();
}





















