#include <iostream>
#include <string>
#include <sstream>

#include "../../../include/board.h"
#include "../../../include/move.h"
#include "../../../include/tiles.h"
#include "../../../include/rack.h"
#include "../../../include/dict.h"
#include "../../../include/choices.h"
#include "../../../include/controller.h"

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
    for (int i=0; i < 2; i++) {
        drawTiles(bag, players[i].rack, 7);
        players[i].score = 0;
        players[i].passCount = 0;
    }

    // Instantiate controllers
    HumanController human1;
    HumanController human2;
    PlayerController* controllers[2] = { &human1, &human2 };

    int currentPlayer = 0; // 0 -> Player 1, 1 -> Player 2

    GameSnapshot lastSnapShot;
    LastMoveInfo lastMove;
    lastMove.exists = false;
    lastMove.playerIndex = -1;

    bool canChallenge = false;

    printTitle();
    cout << "Welcome to Terminal Crossword Game (2-player mode)\n";

    bool dictActive = true;

    if (!loadDictionary("build/Release/data/csw24.txt")) {
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

        // Get move from controller
        string commandStr = controllers[currentPlayer]->getMove(
            bonusBoard,
            letters,
            blanks,
            bag,
            players[currentPlayer],
            currentPlayer,
            canChallenge
        );

        stringstream ss(commandStr);
        string command;
        ss >> command;

        if (command == "QUIT") {
            if (handleQuit(players, currentPlayer)) {
                break;
            }
            continue;
        }

        if (command == "PASS") {
            passTurn(players, currentPlayer, canChallenge, lastMove);
            printBoard(bonusBoard, letters);
            cout << "Scores: Player 1 = " << players[0].score << " | Player 2 = " << players[1].score << endl;
            cout << "Rack:\n";
            printRack(players[currentPlayer].rack);
            continue;
        }

        if (command == "CHALLENGE") {
            challengeMove(bonusBoard,
                          letters,
                          blanks,
                          bag,
                          players,
                          lastSnapShot,
                          lastMove,
                          currentPlayer,
                          canChallenge,
                          dictActive);
            continue;
        }

        if (command == "SHOW_TILES") {
             showUnseenTiles(bag, players, currentPlayer);
             continue;
        }

        if (command == "EXCHANGE") {
            string tilesStr;
            ss >> tilesStr;
            
            // Exchange logic
            if (exchangeRack(players[currentPlayer].rack, tilesStr, bag)) {
                cout << "Rack Updated";
                printRack(players[currentPlayer].rack);

                // after an exchange you can no longer challenge that last word.
                players[currentPlayer].passCount += 1;
                canChallenge = false;
                lastMove.exists = false;

                // exchange uses the turn, switching players.
                currentPlayer = 1 - currentPlayer;
                cout << "Exchange is used. Switching over to Player " << (currentPlayer + 1) << "'s turn\n";
                cout << "\n\nPlayer" << (currentPlayer + 1) << "'s Rack" << endl;
                printBoard(bonusBoard, letters);
                cout << "Scores: Player 1 = " << players[0].score << " | Player 2 = " << players[1].score << endl;
                printRack(players[currentPlayer].rack);
            } else {
                cout << "\nInvalid exchange: you don't have those tiles.\n";
            }
            continue;
        }

        if (command == "PLAY") {
            int row, col;
            string dirStr, word;
            ss >> row >> col >> dirStr >> word;
            bool horizontal = (dirStr == "H");

            // Taking an snapshot before applying final word
            lastSnapShot.letters = letters;
            lastSnapShot.blanks = blanks;
            lastSnapShot.bag = bag;
            lastSnapShot.players[0] = players[0];
            lastSnapShot.players[1] = players[1];

            MoveResult finalResult = playWord(
                bonusBoard,
                letters,
                blanks,
                bag,
                players[currentPlayer].rack,
                row,
                col,
                horizontal,
                word
            );

            if (!finalResult.success) {
                // Should not normally happen since preview succeeded in controller
                cout << "Unexpected error applying move: " << finalResult.errorMessage << endl;
            } else {
                cout << "Move played. Score: " << finalResult.score << endl;
                players[currentPlayer].score += finalResult.score;

                // This will revert back to previous values if the play becomes illegal.
                players[0].passCount = 0;
                players[1].passCount = 0;

                // Recording the last move for potential challenging.
                lastMove.exists = true;
                lastMove.playerIndex = currentPlayer;
                lastMove.startRow = row;
                lastMove.startCol = col;
                lastMove.horizontal = horizontal;
                canChallenge = true;
            }

            printBoard(bonusBoard, letters);
            cout << "Scores: Player 1 = " << players[0].score << " | Player 2 = " << players[1].score << endl;

            // Show next player's rack
            currentPlayer = 1 - currentPlayer;

            cout << "\nNow its Player " << (currentPlayer + 1) << "'s turn" << endl;
            cout << "Rack:\n";

            printRack(players[currentPlayer].rack);
            continue;
        }

        cout << "Unknown Command from Controller: " << commandStr << endl;
    }
}