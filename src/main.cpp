#include <iostream>
#include <string>

#include "../include/board.h"
#include "../include/move.h"
#include "../include/tiles.h"
#include "../include/rack.h"
#include "../include/dict.h"
#include "../include/choices.h"

using namespace std;

int main() {

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
            continue;
        }

        if (choice == 'B') {
            printBoard(bonusBoard, letters);
            continue;
        }

        if (choice == 'T') {
            int opponent = 1 - currentPlayer;
            bool revealOpponent = (static_cast<int>(bag.size()) <= 7);

            printTileBag(bag, players[opponent].rack, revealOpponent);
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
            cout << "Confirm Resignation (Y/N): ";
            char quit;
            cin >> quit;
            quit = static_cast<char>(toupper(static_cast<unsigned char>(quit)));

            if (quit != 'Y') {
                cout << "Player " << (currentPlayer + 1) << " is still in game\n";
                continue;
            }

            cout << "\nGame Over.\n";
            cout << "Final Scores:\n";
            cout << "Player 1: " << players[0].score << endl;
            cout << "Player 2: " << players[1].score << endl;

            if (players[0].score > players[1].score) {
                cout << "Player 1 wins!\n";
            } else if (players[1].score > players[0].score) {
                cout << "Player 2 wins!\n";
            } else {
                cout << "Match is a tie!\n";
            }
            break;
        }

        // Refernce for current player's rack
        TileRack &currentRack = players[currentPlayer].rack;

        if (choice == 'R') {
            cout << R"(Enter rack command ("4-7" to swap, "0" to shuffle, "X" to exchange): )";
            string command;
            cin >> command;

            bool ok = applyRackCommand(bag, currentRack, command);

            if (ok) {
                cout << "Rack Updated";
                printRack(currentRack);

                // After an exchange the turn is over
                if (command == "X" || command == "x") {
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
                }
            } else {
                cout << "\nInvalid rack command\n";
            }

            //printRack(currentRack);
            continue;
        }

        if (choice == 'M') {
            cout << "Enter move in format <RowCol> <H/V> <WordFromRack>\n";
            cout << "Example: A10 V RETINAS\n\n";
            cout << "RowCol: ";
            string pos;
            cin >> pos;

            cout << "Direction (H/V): ";
            string dirStr;
            cin >> dirStr;

            cout << "Word (from rack only): ";
            string word;
            cin >> word;

            if (pos.size() < 2 || pos.size() > 3) {
                cout << "Invalid Position";
                continue;
            }

            char rowChar = static_cast<char>(toupper(static_cast<unsigned char>(pos[0])));
            int row = rowChar - 'A';
            if (row < 0 || row >= BOARD_SIZE) {
                cout << "Row out of range";
                continue;
            }

            int col = 0;

            // C++ Learning
            // pos.substr(1) takes the string from index 1 foward
            // ex : "A10" -> "10"
            // and stoi turns that to an int ( "10" -> 10)
            // catch block runs if stoi() throws as exception ( "AAA" )
            try {
                col = stoi(pos.substr(1)) - 1; // 1-based to 0-based
            } catch (...) {
                cout << "Invalid column number";
                continue;
            }

            if (col < 0 || col >= BOARD_SIZE) {
                cout << "Column out of range";
                continue;
            }

            bool horizontal = (toupper(static_cast<unsigned char>(dirStr[0])) == 'H');

            // Preview on copies
            LetterBoard previewLetters = letters;
            BlankBoard previewBlanks = blanks;
            TileRack previewRack = currentRack;
            TileBag previewBag = bag;

            MoveResult preview = playWord(
                bonusBoard,
                previewLetters,
                previewBlanks,
                previewBag,
                previewRack,
                row,
                col,
                horizontal,
                word
            );

            if (!preview.success) {
                cout << "Move Failed " << preview.errorMessage << endl;
                continue;
            }

            cout << "\nProposed move score (main word + cross words): " << preview.score << endl;

            /*
            cout << "\nPreview board:\n";
            printBoard(bonusBoard, previewLetters);

            cout << "\nPreview rack:\n";
            printRack(previewRack);
            */

            cout << "\nConfirm move? (y/n): ";
            char confirm;
            cin >> confirm;
            confirm = static_cast<char>(toupper(static_cast<unsigned char>(confirm)));

            if (confirm != 'Y') {
                cout << "Move cancelled.\n";
                continue;
            }

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
                currentRack,
                row,
                col,
                horizontal,
                word
            );

            if (!finalResult.success) {
                // Should not normally happen since preview succeeded
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

        cout << "Unknown Command\n";
    }

    return 0;
}