#include <iostream>

#include "../include/dict.h"
#include "../include/modes/PvP/pvp.h"
#include "../include/modes/Home/home.h"
#include "../include/modes/Test/test_mode.h"

using namespace std;

int main() {

    // selection menu
    while (true) {

        printTitleScreen();

        char mode;

        loadDictionary("csw24.txt");

        cout << "=========================================\n";
        cout << "           Welcome to LEXI_PI\n";
        cout << "         Terminal Crossword Game\n";
        cout << "               Version 0.1\n";
        cout << "=========================================\n\n";

        cout << "(1) Player vs Player\n"
             << "(2) Player vs AI\n"
             << "(3) Word Wizard\n"
             << "(4) How to play\n"
             << "(5) About\n"
             << "(6) Quit\n"
             << "(7) Test Mode\n"
             << "Please select an option: ";
        cin >> mode;

        if (mode == '1') {
            runPvP();
            break;
        } else if (mode == '2') {
            cout << " Under Construction \n" << endl;
        } else if (mode == '3') {
            string word = "XI";
            wordWizard(word);
        } else if (mode == '4') {
            showHowToPlayScreen();
        } else if (mode == '5') {
            showAboutScreen();
        } else if (mode == '6') {
            cout << " May your racks be fruitful and your bingos plentiful!\n" << endl;
            break;
        } else if (mode == '7') {
            runTestMode();
            break;
        }
    }

    return 0;
}