#include <iostream>

#include "../../../include/board.h"
#include "../../../include/move.h"
#include "../../../include/tiles.h"
#include "../../../include/rack.h"
#include "../../../include/dict.h"
#include "../../../include/choices.h"

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
                                   dictActive)) {
            //Game over
            break;
                                   }

        cout << "\nCommands (Player " << currentPlayer + 1 << "):\n"
             << " m -> play a move\n"
             << " r -> rack command (swap/shuffle/exchange)\n"
             << " c -> challenge last word\n"
             << " b -> show board\n"
             << " t -> show unseen tiles\n"
             << " p -> pass\n"
             << " q -> quit\n"
             << "Enter Choice: ";

        string input;

        if (!(cin >> input)) {
            break;
        }

        if (input.size() != 1) {
            cout << "Invalid input. Please enter only one character\n";
            continue;
        }

        char choice = input[0];

        choice = static_cast<char>(toupper(static_cast<unsigned char>(choice)));

        // Passing
        if (choice == 'P') {
            passTurn(players, currentPlayer, canChallenge, lastMove);
            printBoard(bonusBoard, letters);
            cout << "Scores: Player 1 = " << players[0].score << " | Player 2 = " << players[1].score << endl;
            cout << "Rack:\n";
            printRack(players[currentPlayer].rack);
            continue;
        }

        if (choice == 'B') {
            printBoard(bonusBoard, letters);
            cout << "Scores: Player 1 = " << players[0].score << " | Player 2 = " << players[1].score << endl;
            cout << "Rack:\n";
            printRack(players[currentPlayer].rack);
            continue;
        }

        if (choice == 'T') {
            showUnseenTiles(bag, players, currentPlayer);
            continue;
        }

        if (choice == 'C') {
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

        if (choice == 'Q') {
            if (handleQuit(players, currentPlayer)) {
                break;
            }
            continue;
        }
        if (choice == 'R') {
            handleRackChoice(bonusBoard,
                             letters,
                             bag,
                             players,
                             currentPlayer,
                             canChallenge,
                             lastMove);
            continue;
        }

        if (choice == 'M') {
            handleMoveChoice(bonusBoard,
                             letters,
                             blanks,
                             bag,
                             players,
                             lastSnapShot,
                             lastMove,
                             currentPlayer,
                             canChallenge);
            continue;
        }

        cout << "Unknown Command\n";
    }
}