#include <iostream>
#include <string>

#include "../include/board.h"
#include "../include/move.h"
#include "../include/tiles.h"
#include "../include/rack.h"

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
    }

    int currentPlayer = 0; // 0 -> Player 1, 1 -> Player 2

    printTitle();
    cout << "Welcome to Terminal Crossword Game (2-player mode)\n";

    printBoard(bonusBoard, letters);
    cout << "Scores: Player 1 = " << players[0].score << " | Player 2 = " << players[1].score << endl;
    cout << "Player " << (currentPlayer + 1) << "'s Rack" << endl;

    printRack(players[currentPlayer].rack);

    while (true) {
        cout << "\nCommands (Player " << currentPlayer + 1 << "):\n"
             << " m -> play a move\n"
             << " r -> rack command (swap/shuffle)\n"
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

        if (choice == 'Q') {
            cout << "Confirm Resignation (Y/N): ";
            char quit;
            cin >> quit;
            quit = static_cast<char>(toupper(static_cast<unsigned char>(quit)));

            if (quit != 'Y') {
                cout << (currentPlayer + 1) << " is still in game\n";
            } else {
                cout << "\nGame Over.\n";
                cout << "Final Scores:\n";
                cout << "Player 1: " << players[0].score << endl;
                cout << "Player 2: " << players[1].score << endl;

                if (players[0].score > players[1].score) {
                    cout
                }
            }

            break;
        }

        if (choice == 'R') {
            cout << "Enter rack command (\"4-7\" to swap, \"0\" to shuffle): ";
            string command;
            cin >> command;

            if (applyRackCommand(playerRack, command)) {
                cout << "Rack Updated";
            } else {
                cout << "Invalid rack command";
            }

            printRack(playerRack);
            continue;
        }

        if (choice == 'M') {
            cout << "Enter move in format <RowCol> <H/V> <WordFromRack>\n";
            cout << "Example: A10 V RETINAS\n";
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
            TileRack previewRack = playerRack;
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

            MoveResult finalResult = playWord(
                bonusBoard,
                letters,
                blanks,
                bag,
                playerRack,
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
                totalScore += finalResult.score;
            }

            printBoard(bonusBoard, letters);

            cout << "Total Score: " << totalScore << endl;

            printRack(playerRack);
            continue;
        }
    }

    return 0;
}