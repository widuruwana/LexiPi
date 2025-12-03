#include <iostream>
#include <string>
#include <vector>
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
string extractMainWord(const LetterBoard &letters, int row, int col, bool horizontal) {
    int dr = horizontal ? 0 : 1;
    int dc = horizontal ? 1 : 0;

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

// Returns a list of all the cross words formed.
vector<string> crossWordList(const LetterBoard &letters, int row, int col, bool mainHorizontal) {
    vector<string> words;

    // perpendicular direction
    int crossDr = mainHorizontal ? 1 : 0;
    int crossDc = mainHorizontal ? 0 : 1;

    // for going from tile to tile
    int mainDr = mainHorizontal ? 0 : 1;
    int mainDc = mainHorizontal ? 1 : 0;

    int r = row;
    int c = col;

    while (r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE && letters[r][c] != ' ') {
        // First check if there is any neighbor in that perpendicular direction
        bool hasNeighbor = false;

        // checking either side
        int nr = r - crossDr;
        int nc = c - crossDc;
        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && letters[nr][nc] != ' ') {
            hasNeighbor = true;
        }

        nr = r + crossDr;
        nc = c + crossDc;
        if (!hasNeighbor && nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE && letters[nr][nc] != ' ') {
            hasNeighbor = true;
        }

        // Single tile with no adjacent letter in cross direction
        if (hasNeighbor) {
            // Finding start of the cross word.
            int cr = r;
            int cc = c;

            while (cr - crossDr >= 0 && cr - crossDr < BOARD_SIZE && cc - crossDc >= 0 &&
                   cc - crossDc < BOARD_SIZE && letters[cr - crossDr][cc - crossDc] != ' ') {
                cr = cr - crossDr;
                cc = cc - crossDc;
            }

            string word;
            // Walk forward along the cross-word direction
            int wr = cr;
            int wc = cc;
            while (wr >= 0 && wr < BOARD_SIZE && wc >= 0 &&
                   wc < BOARD_SIZE && letters[wr][wc] != ' ') {
                word.push_back(letters[wr][wc]);
                wr += crossDr;
                wc += crossDc;
            }

            if (word.size() > 1) {
                words.push_back(word);
            }
        }

        // Advance to the next cell along the main word.
        r += mainDr;
        c += mainDc;

    }

    cout << "No of cross words found: " << words.size() << "\n";

    return words;
}