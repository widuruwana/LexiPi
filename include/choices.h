#pragma once

#include "../include/move.h"
#include "../include/board.h"
#include "../include/tiles.h"
#include "../include/rack.h"
#include "../include/dict.h"
#include "../include/player_controller.h"

#include <iostream>
#include <string>
#include <vector>

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
                            bool &dictActive,
                            PlayerController* controller);

// Handle Pass command
// - Increments players passCount
// - Clears challenge window
// - Switches the current player
void passTurn(Player players[2], int &currentPlayer, bool &canChallenge, LastMoveInfo &lastMove);

// Show tile set from current player's prespective
// unseen tiles are bag + opponent rack, reveal opponenet rack when bag <= 7 (Tile tracking)
void showTileSet(const TileBag &bag, const Player players[2], int currentPlayer);

// Show unseen tiles
void showUnseenTiles(const TileBag &bag, const Player players[2], int currentPlayer);

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

// Handle resignation
bool handleQuit(const Player players[2], int currentPlayer);

/*
// handle rack command (swap/shuffle/exchange)
// DELETED: handleRackChoice

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
*/

bool executePlayMove(Board &bonusBoard,
                     LetterBoard &letters,
                     BlankBoard &blanks,
                     TileBag &bag,
                     Player players[2],
                     Player currentPlayer,
                     const Move &move,
                     GameSnapshot &lastSnapShot);

bool executeExchangeMove(TileBag &bag, Player &players, const Move &move);





















