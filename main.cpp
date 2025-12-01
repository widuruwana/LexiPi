#include <iostream>
#include <string>

#include "board.h"
#include "move.h"
#include "tiles.h"
#include "rack.h"

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

    TileRack playerRack;
    drawTiles(bag, playerRack, 7);

    printTitle();
    cout << "Welcome to Terminal Crossword Game (engine test)\n";
    printBoard(bonusBoard, letters);
    printRack(playerRack);

    while (true) {
        cout << "\nCommands:\n"
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
            }

            printBoard(bonusBoard, letters);
            printRack(playerRack);
            continue;
        }
    }

    return 0;
}