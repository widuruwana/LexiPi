#pragma once

#include <vector>


using namespace std;

// Represents a single scrabble Tile
typedef struct {
    char letter; //"A to Z" (always uppercase)
    int points; //score value
    bool is_blank; // true if this is a blank tile
}Tile;

// Tile bag
using TileBag = vector<Tile>;

// Create the standard 100-tile english scrabble bag
TileBag createStandardTileBag();

// Print the tile bag
void printTileBag(const TileBag &bag, const vector<Tile> &opponentRack, bool revealOpponentRack);

// Shuffle the tile bag
void shuffleTileBag(TileBag &bag);
