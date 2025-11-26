#include <iostream>
#include <string>

#include "board.h"
#include "tiles.h"
#include "rack.h"

using namespace std;

int main() {

    //creating and printing the board
    Board board = createBoard();
    printBoard(board);

    //Create, shuffle bag and draw a rack
    TileBag bag = createStandardTileBag();
    shuffleTileBag(bag);

    TileRack playerRack;
    drawTiles(bag, playerRack, 7);

    printRack(playerRack);

    //Test loop
    string command;
    while (true) {
        cout << "\nEnter your rack command (e.g. 4-7 to swap, 0 to shuffle, q to quit): ";
        cin >> command;
        if (command == "q" || command == "Q") {
            break;
        }

        if (applyRackCommand(playerRack, command)) {
            cout << "Rack updated.\n";
        } else {
            cout << "Invalid rack command.\n";
        }

        printRack(playerRack);
    }

    return 0;
}