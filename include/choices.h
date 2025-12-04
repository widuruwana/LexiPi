#pragma once

#include "../include/move.h"
#include "../include/board.h"
#include "../include/tiles.h"
#include "../include/rack.h"
#include "../include/dict.h"

#include <iostream>

using namespace std;

// Returns true if game ended and scores are printed.
bool handleSixPassEndGame(Player players[2]);

bool handleEmptyRackEndGame(Board &bonusBoard,
                            LetterBoard &letters,
                            BlankBoard &blanks,
                            TileBag &bag,
                            Player players[2],
                            GameSnapshot &lastSnapShot,
                            LastMoveInfo &lastMove,
                            int &currentPlayer,
                            bool &canChallenge,
                            bool &dictActive);

// Handle Pass command
// - Increments players passCount
// - Clears challenge window
// - Switches the current player
void passTurn(Player players[2], int &currentPlayer, bool &canChallenge, LastMoveInfo &lastMove);

// Show tile set from current player's prespective
// unseen tiles are bag + opponent rack, reveal opponenet rack when bag <= 7 (Tile tracking)
void showTileSet(const TileBag &bag, const Player players[2], int currentPlayer);

// Handle a CHALLENGE command;
// Uses lastSnapshot to undo the last move on successful challenge.
// Does NOT change current player.
void challengeMove(Board &bonusBoard,
                   LetterBoard &letters,
                   BlankBoard &blanks,
                   TileBag &bag,
                   Player players[2],
                   GameSnapshot &lastSnapShot,
                   LastMoveInfo &lastMove,
                   int &currentPlayer,
                   bool &canChallenge,
                   bool &dictActive);

// Show unseen tiles
void showUnseenTiles(const TileBag &bag,
                     const Player players[2],
                     int currentPlayer);

// Handle resignation
bool handleQuit(const Player players[2], int currentPlayer);

// handle rack command (swap/shuffle/exchange)
void handleRackChoice(Board &bonusBoard,
                      LetterBoard &letters,
                      TileBag &bag,
                      Player players[2],
                      int &currentPlayer,
                      bool &canChallenge,
                      LastMoveInfo &lastMove);

// Handle playing a move
void handleMoveChoice(Board &bonusBoard,
                      LetterBoard &letters,
                      BlankBoard &blanks,
                      TileBag &bag,
                      Player players[2],
                      GameSnapshot &lastSnapShot,
                      LastMoveInfo &lastMove,
                      int &currentPlayer,
                      bool &canChallenge);




















