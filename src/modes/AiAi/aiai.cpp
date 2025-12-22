#include <iostream>
#include <vector>
#include <numeric>
#include <thread>
#include <chrono>

#include "../../../include/modes/AiAi/aiai.h"
#include "../../../include/board.h"
#include "../../../include/move.h"
#include "../../../include/tiles.h"
#include "../../../include/rack.h"
#include "../../../include/dict.h"
#include "../../../include/choices.h"
#include "../../../include/ai_player.h"
#include "../../../include/modes/Home/home.h"

using namespace std;

// Stats
struct MatchResult {
    int scoreP1;
    int scoreP2;
    int winner; // 0 = P1, 1 = P2, -1 = Draw
};

void runAiAi() {
    int numGames;
    bool verbose;

    cout << "\n=========================================\n";
    cout << "      CUTIE_PI vs EVIL CUTIE_PI\n";
    cout << "=========================================\n";
    cout << "Enter number of games to simulate: ";
    cin >> numGames;
    cout << "Watch the games? (1 = Yes, 0 = No/Fast): ";
    cin >> verbose;

    if (!loadDictionary("csw24.txt")) {
        cout << "Error: Dictionary not found.\n";
        return;
    }

    vector<MatchResult> results;
    auto startTotal = chrono::high_resolution_clock::now();

    for (int game = 1; game <= numGames; game++) {
        if (!verbose) {
            cout << "Simulating game " << game << "/" << numGames << flush;
        } else {
            cout << "Simulating game " << game << "/" << numGames;
        }


        // 1. Setup Board
        Board bonusBoard = createBoard();
        LetterBoard letters;
        clearLetterBoard(letters);
        BlankBoard blanks;
        clearBlankBoard(blanks);

        // 2. Setup Bag
        TileBag bag = createStandardTileBag();
        shuffleTileBag(bag);

        // 3. Setup AIs
        Player players[2];
        PlayerController* controllers[2];

        // Player 1 - Cutie_Pi
        drawTiles(bag, players[0].rack, 7);
        players[0].score = 0;
        players[0].passCount = 0;
        controllers[0] = new AIPlayer();

        // Player 2 - Evil Cutie_Pi
        drawTiles(bag, players[1].rack, 7);
        players[1].score = 0;
        players[1].passCount = 0;
        controllers[1] = new AIPlayer();

        int currentPlayer = 0;

        GameSnapshot lastSnapShot;
        LastMoveInfo lastMove;
        lastMove.exists = false;
        lastMove.playerIndex = -1;

        bool canChallenge = false; // AI assumes valid dictionary
        bool dictActive = true;

        // Game loop
        bool gameOver = false;
        while (!gameOver) {

            if (handleSixPassEndGame(players)) {
                gameOver = true;
                break;
            }

            if (handleEmptyRackEndGame(bonusBoard,
                                       letters,
                                       blanks,
                                       bag,
                                       players,
                                       lastSnapShot,
                                       lastMove,
                                       currentPlayer,
                                       canChallenge,
                                       dictActive,
                                       controllers[currentPlayer])) {
                gameOver = true;
                break;
                                       }

            if (verbose) {
                cout << "\n" << (currentPlayer == 0 ? "Cutie_Pi" : "Evil_Pi") << "is thinking...";
            }

            Move move = controllers[currentPlayer]->getMove(bonusBoard,
                                                            letters,
                                                            blanks,
                                                            bag,
                                                            players[currentPlayer],
                                                            players[1 - currentPlayer],
                                                            currentPlayer + 1);

            // move execution
            if (move.type == MoveType::PASS) {
                passTurn(players, currentPlayer, canChallenge, lastMove);
                //printBoard(bonusBoard, letters);
                //cout << "Scores: You = " << players[0].score << " | Cutie_Pi = " << players[1].score << endl;
                continue;
            }

            /*
            if (move.type == MoveType::CHALLENGE) {
                // AI likely won't challenge in V1, but logic remains
                challengeMove(bonusBoard, letters, blanks, bag, players,
                              lastSnapShot, lastMove, currentPlayer, canChallenge, dictActive);
                continue;
            }

            if (move.type == MoveType::EXCHANGE) {
                bool success = executeExchangeMove(bag, players[currentPlayer], move);
                if (success) {
                    lastMove.exists = false;
                    canChallenge = false;

                    players[currentPlayer].passCount++;

                    printBoard(bonusBoard, letters);
                    cout << "Scores: You = " << players[0].score << " | Cutie_Pi = " << players[1].score << endl;

                    currentPlayer = 1 - currentPlayer;
                    if (currentPlayer == 0) {
                        cout << "\nYour Turn:" << endl;
                        printRack(players[currentPlayer].rack);
                    }
                }
                continue;
            }
            */

            if (move.type == MoveType::PLAY) {
                bool success = executePlayMove(bonusBoard, letters, blanks, bag, players,
                                               players[currentPlayer], move, lastSnapShot);

                if (success) {
                    lastMove.exists = true;
                    lastMove.playerIndex = currentPlayer;
                    lastMove.startRow = move.row;
                    lastMove.startCol = move.col;
                    lastMove.horizontal = move.horizontal;
                    canChallenge = true;

                    if (verbose) {
                        printBoard(bonusBoard, letters);
                        cout << "Scores: Cutie_Pi = " << players[0].score << " | Evil_Pi = " << players[1].score << endl;
                    }

                    currentPlayer = 1 - currentPlayer;

                } else {
                    cout << "[Critical Error] AI played an invalid move. Force passing to prevent an infinity loop\n";
                    passTurn(players, currentPlayer, canChallenge, lastMove);
                }
            }
        }

        // Determining the winner
        int winner = -1;
        if (players[0].score > players[1].score) winner = 0;
        else if (players[1].score > players[0].score) winner = 1;

        results.push_back({players[0].score, players[1].score, winner});

        delete controllers[0];
        delete controllers[1];
    }

    auto endTotal = chrono::high_resolution_clock::now();
    double elapsedMs = chrono::duration_cast<chrono::milliseconds>(endTotal-startTotal).count();

    // Statistics
    long long totalP1 = 0, totalP2 = 0;
    int winsP1 = 0, winsP2 = 0, draws = 0;

    for (const auto& res: results) {
        totalP1 += res.scoreP1;
        totalP2 += res.scoreP2;
        if (res.winner == 0) {
            winsP1++;
        }
        else if (res.winner == 1) {
            winsP2++;
        }else {
            draws++;
        }
    }

    cout << "\n\n=========================================\n";
    cout << "           SIMULATION RESULTS            \n";
    cout << "=========================================\n";

    cout << "Games played: " << numGames << endl;
    cout << "Time Elpased: " << (elapsedMs / 1000.0) << " seconds\n" << endl;

    cout << "Cutie_Pi (P1) Wins: " << winsP1 << " (" << (winsP1 * 100.0 / numGames) << "%)\n";
    cout << "Evil_Pi  (P2) Wins: " << winsP2 << " (" << (winsP2 * 100.0 / numGames) << "%)\n";
    cout << "Draws: " << draws << "\n\n";

    cout << "Avg Score (P1): " << (totalP1 / (double)numGames) << "\n";
    cout << "Avg Score (P2): " << (totalP2 / (double)numGames) << "\n";
    cout << "Combined Avg: " << ((totalP1 + totalP2) / (double)numGames / 2.0) << "\n";
    cout << "=========================================\n";

    waitForQuitKey();
    clearScreen();
}