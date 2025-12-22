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
#include "../../../include/modes/Home/home.h"

using namespace std;

void runPvP() {

    //creating and printing the board
    Board bonusBoard = createBoard();
    LetterBoard letters;
    clearLetterBoard(letters);
    BlankBoard blanks;
    clearBlankBoard(blanks);

    //Create, shuffle bag and draw a rack
    TileBag bag = createStandardTileBag();
    shuffleTileBag(bag);

    Player players[2];
    PlayerController* controllers[2];

    for (int i=0; i < 2; i++) {
        drawTiles(bag, players[i].rack, 7);
        players[i].score = 0;
        players[i].passCount = 0;
        controllers[i] = new HumanPlayer();
    }

    int currentPlayer = 0; // 0 -> Player 1, 1 -> Player 2

    GameSnapshot lastSnapShot;
    LastMoveInfo lastMove;

    lastMove.exists = false;
    lastMove.playerIndex = -1;

    bool canChallenge = false;

    printTitle();
    cout << "Welcome to Terminal Crossword Game (2-player mode)\n";

    bool dictActive = true;

    if (!loadDictionary("csw24.txt")) {
        cout << "WARNING: Dictionary not loaded, challenge feature will not work.\n";
        dictActive = false;
    }

    printBoard(bonusBoard, letters);
    cout << "Scores: Player 1 = " << players[0].score << " | Player 2 = " << players[1].score << endl;
    cout << "Player " << (currentPlayer + 1) << "'s Rack" << endl;

    printRack(players[currentPlayer].rack);

    while (true) {

        if (handleSixPassEndGame(players)) {
            //Game over
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
            //Game over
            break;
        }

        // Getting the move from the controller
        Move move = controllers[currentPlayer]->getMove(bonusBoard,
                                                        letters,
                                                        blanks,
                                                        bag,
                                                        players[currentPlayer],
                                                        players[1 - currentPlayer],
                                                        currentPlayer + 1);

        // Executing the move
        if (move.type == MoveType::PASS) {
            passTurn(players, currentPlayer, canChallenge, lastMove);
            printBoard(bonusBoard, letters);
            cout << "Scores: Player 1 = " << players[0].score
                 << " | Player 2 = " << players[1].score << endl;
            printRack(players[currentPlayer].rack);
            continue;
        }

        if (move.type == MoveType::QUIT) {
            if (handleQuit(players, currentPlayer)) {
                break;
            }
            continue;
        }

        if (move.type == MoveType::CHALLENGE) {
            challengeMove(bonusBoard, letters, blanks, bag, players,
                          lastSnapShot, lastMove, currentPlayer, canChallenge, dictActive);
            continue;
        }

        if (move.type == MoveType::EXCHANGE) {
            bool success = executeExchangeMove(bag, players[currentPlayer], move);

            if (success) {
                // Exchange counts as a turn
                // Reset challenge right
                lastMove.exists = false;
                canChallenge = false;

                printBoard(bonusBoard, letters);
                cout << "Scores: Player 1 = " << players[0].score
                     << " | Player 2 = " << players[1].score << endl;

                // switch turn
                currentPlayer = 1 - currentPlayer;
                cout << "\nPlayer " << (currentPlayer + 1) << "'s Rack" << endl;
                printRack(players[currentPlayer].rack);
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
                cout << "Scores: Player 1 = " << players[0].score
                     << " | Player 2 = " << players[1].score << endl;

                currentPlayer = 1 - currentPlayer;
                cout << "\nNow it's Player " << (currentPlayer + 1) << "'s turn" << endl;
                printRack(players[currentPlayer].rack);
            }
        }
    }

    delete controllers[0];
    delete controllers[1];
    waitForQuitKey();
    clearScreen();
}
























