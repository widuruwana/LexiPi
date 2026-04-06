#pragma once
#include "../../include/engine/game_director.h"
#include "../../include/interface/renderer.h"
#include <iostream>
#include <mutex>

static std::mutex g_console_mutex;

class ConsoleObserver : public GameObserver {
public:
    void onLogMessage(const std::string& msg) override {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        std::cout << msg << std::endl;
    }

    void onTurnStart(int gameId, int pIdx, const GameState& state, const Board& bonusBoard, const std::string& pName) override {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        Renderer::printBoard(bonusBoard, state.board);
        std::cout << "Scores: P1=" << state.players[0].score << " | P2=" << state.players[1].score << std::endl;
        
        if (pName == "Human") {
            Renderer::printRack(state.players[pIdx].rack);
        }
    }

    void onGameOver(const GameState& state, const Board& bonusBoard) override {
        std::lock_guard<std::mutex> lock(g_console_mutex);
        std::cout << "\n=========================================\n";
        std::cout << "           FINAL BOARD STATE\n";
        std::cout << "=========================================\n";
        Renderer::printBoard(bonusBoard, state.board);
        std::cout << "FINAL SCORES: P1=" << state.players[0].score << " | P2=" << state.players[1].score << "\n";
        std::cout << "=========================================\n\n";
    }
};