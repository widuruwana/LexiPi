#include <iostream>
#include <string>
#include <unordered_set>
#include <fstream>
#include <cctype>

#include "../include/board.h"
#include "../include/move.h"
#include "../include/tiles.h"
#include "../include/rack.h"
#include "../include/dict.h"

using namespace std;

unordered_set<string> gDictionary;

bool loadDictionary(const string &filename) {
    ifstream in(filename);
    if (!in) {
        cout << "Failed to open dictionary file: " << filename << "\n";
        return false;
    }

    string word;
    while (in >> word) {
        for (char &c : word) {
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
        gDictionary.insert(word);
    }

    cout << "\nLoaded " << gDictionary.size() << " words from " << filename << "\n";
    return true;
}

bool isValidWord(const string &word) {
    string up = word;
    for (char &c : up) {
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    }
    // C++ Learning
    // gDictionary.find(up) looks for up in the dictionary, and if not found returns gDictionary.end()
    // which basically means the word is not there.
    return gDictionary.find(up) != gDictionary.end();
}

// Get the full word from the board (for challenging)
string extractMainWord(const LetterBoard &letters, int row, int col, bool horizonatl) {
    int dr = horizonatl ? 0 : 1;
    int dc = horizonatl ? 1 : 0;

    int r = row;
    int c = col;

    // Move backwards to the start of the word
    while (r - dr >= 0 && r - dr < BOARD_SIZE && c - dc >= 0 && c - dc < BOARD_SIZE && letters[r - dr][c - dc] != ' ') {
        r = r - dr;
        c = c - dc;
    }

    // move forwards, collecting letters
    string word;
    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && letters[r][c] != ' ') {
        word.push_back(letters[r][c]);
        r += dr;
        c += dc;
    }

    return word;
}



























