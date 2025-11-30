#include "rack.h"

#include <iostream>
#include <algorithm>
#include <random>
#include <ctime>

using namespace std;

// Center a short string inside a field of give width
static string centerText(const string &text, int width) {
    int padding = width - static_cast<int>(text.size());
    if (padding <= 0) {
        return text;
    }

    int left = padding/2;
    int right = padding - left;
    return string(left, ' ' ) + text + string(right, ' ');
}

// Draw up to 'count' tiles from the bag into the rack.
// Returns how many tiles were actually drawn.
int drawTiles(TileBag &bag, TileRack &rack, int count) {
    int drawn = 0;
    while (drawn < count && !bag.empty()) {
        rack.push_back(bag.back()); // take from end
        bag.pop_back();
        ++drawn;
    }
    return drawn;
}

//Print the rack like 1:A(1) 2:T(1) 3:Q(10)
void printRack(const TileRack &rack) {
    if (rack.empty()) {
        cout << "Rack is empty." << endl;
        return;
    }

    const int innerWidth = 5; //width inside the tile (between | |)
    const int tileWidth = innerWidth + 2; //including borders

    string topLine = "  ";
    string letterLine = "  ";
    string pointsLine = "  ";
    string bottomLine = "  ";
    string indexLine = "  ";

    for (size_t i = 0; i < rack.size(); ++i) {
        const Tile &t = rack[i];

        topLine += "+-----";
        bottomLine += "+-----";

        // Letter (centered).
        char letterChar = (t.letter == '?') ? ' ' : t.letter;
        string letterStr(1, letterChar);
        string centeredLetter = centerText(letterStr, innerWidth);
        letterLine += "|" + centeredLetter;

        // Points (Bottom - right)
        string pointsStr = to_string(t.points);
        int spacesBefore = innerWidth - static_cast<int>(pointsStr.size());
        if (spacesBefore < 0) {
            spacesBefore = 0;
        }
        string pointsInner(spacesBefore, ' ');
        pointsInner += pointsStr;
        pointsLine += "|" + pointsInner;

        // Indices under tiles (starting from 1)
        string indexStr = to_string(static_cast<int>(i)+1);
        indexLine += centerText(indexStr, innerWidth+1);
    }

    topLine += "+";
    bottomLine += "+";
    letterLine += "|";
    pointsLine += "|";

    cout << "\nYour rack:\n";
    cout << topLine << endl;
    cout << letterLine << endl;
    cout << pointsLine << endl;
    cout << bottomLine << endl;
    cout << indexLine << endl;
}

//Swap two tiles in the rack (based on player view)
bool handleSwapCommand(TileRack &rack, int index1, int index2) {

    // Covert to 0 based
    int i = index1 - 1;
    int j = index2 - 1;

    //validate indexes
    if (i < 0 || j < 0 || i >= static_cast<int>(rack.size()) || j >= static_cast<int>(rack.size())) {
        return false;
    }

    swap(rack[i], rack[j]);
    return true;
}

//Shuffle the rack play order randomly
void shuffleRack(TileRack &rack) {
    static mt19937 rng(static_cast<unsigned int>(time(nullptr)));
    shuffle(rack.begin(), rack.end(), rng);
}

// Parse a player command for the rack.
// - "4-7" → swap tile 4 and 7
// - "0"   → shuffle rack
// Returns true if command was valid and applied.
bool applyRackCommand(TileRack &rack, const string &command) {
    if (command == "0") {
        shuffleRack(rack);
        return true;
    }

    //Expect format "x-y"
    size_t dashPos = command.find('-');
    // above find the "-" in the string and returns its position.
    // ex: "4-7", the position index of "-" is 1.
    // npos is a special constant that means "not found in the string" (big number like 18231234564321...)
    if (dashPos == string::npos) {
        return false;
    }

    string left = command.substr(0, dashPos);
    //meaning take everything between 0 and dashpos(1), in this case 4.
    string right = command.substr(dashPos + 1);
    //These two lines splits the string from "-"

    if (left.empty() || right.empty()) {
        return false;
    }

    //stoi -> string to int
    int index1 = stoi(right);
    int index2 = stoi(left);

    return handleSwapCommand(rack, index1, index2);
}
