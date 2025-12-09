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
        src/game_engine.cpp
        include/game_engine.h
        src/test_player.cpp
        include/test_player.h
        src/modes/Test/test_mode.cpp
        include/modes/Test/test_mode.h)
