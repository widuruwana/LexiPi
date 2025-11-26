cmake_minimum_required(VERSION 4.0)
project(scrabblePi)

set(CMAKE_CXX_STANDARD 20)

add_executable(scrabblePi board.cpp
        board.h
        tiles.h
        tiles.cpp
        rack.h
        rack.cpp
        main.cpp)
