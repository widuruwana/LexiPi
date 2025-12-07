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
        src/controller.cpp
        include/controller.h)
