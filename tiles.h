#pragma once

#include <vector>

using namespace std;

// Represents a single scrabble Tile
typedef struct {
    char letter; //"A to Z" or "?" for blank
    int poins; //score value
}Tile;

// Tile bag
using TileBag = vector<Tile>;

// Create the standard 100-tile english scrabble bag
TileBag createStandardTileBag();

// Shuffle the tile bag
void shuffleTileBag(TileBag &bag);


