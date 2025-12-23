cmake_minimum_required(VERSION 4.0)
project(scrabblePi)

set(CMAKE_CXX_STANDARD 20)

add_executable(scrabblePi src/board.cpp
        include/board.h
        include/tiles.h
        src/tiles.cpp
        include/rack.h
        src/rack.cpp
        src/main.cpp
        include/move.h
        src/move.cpp
        src/dict.cpp
        include/dict.h
        src/choices.cpp
        include/choices.h
        src/modes/PvP/pvp.cpp
        include/modes/PvP/pvp.h
        src/modes/Home/home.cpp
        include/modes/Home/home.h
        include/player_controller.h
        src/human_player.cpp
        include/human_player.h
        include/dawg.h
        src/dawg.cpp
        include/heuristics.h
        include/fast_constraints.h
        src/fast_constraints.cpp
        include/ai_player.h
        src/ai_player.cpp
        include/endgame_solver.h
        src/endgame_solver.cpp
        include/modes/PvE/pve.h
        src/modes/PvE/pve.cpp
        src/modes/AiAi/aiai.cpp
        include/modes/AiAi/aiai.h)
