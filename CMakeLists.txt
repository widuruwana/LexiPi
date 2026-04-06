cmake_minimum_required(VERSION 4.0)
project(scrabblePi)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
if(MSVC)
    add_compile_options(/arch:AVX2)
else()
    add_compile_options(-O3 -march=native)
endif()

add_executable(scrabblePi src/engine/board.cpp
        include/engine/board.h
        include/engine/tiles.h
        src/engine/tiles.cpp
        include/engine/rack.h
        src/engine/rack.cpp
        src/main.cpp
        include/move.h
        src/choices.cpp
        include/choices.h
        src/modes/PvP/pvp.cpp
        include/modes/PvP/pvp.h
        src/modes/Home/home.cpp
        include/modes/Home/home.h
        include/player_controller.h
        src/human_player.cpp
        include/human_player.h
        include/engine/dictionary.h
        src/engine/dictionary.cpp
        include/kernel/heuristics.h
        include/kernel/fast_constraints.h
        include/ai_player.h
        src/ai_player.cpp
        include/modes/PvE/pve.h
        src/modes/PvE/pve.cpp
        src/modes/AiAi/aiai.cpp
        include/modes/AiAi/aiai.h
        include/tile_tracker.h
        include/kernel/move_generator.h
        src/kernel/move_generator.cpp
        include/spectre/vanguard.h
        src/spectre/vanguard.cpp
        include/spectre/spy.h
        src/spectre/spy.cpp
        include/spectre/judge.h
        src/spectre/judge.cpp
        include/spectre/logger.h
        include/engine/types.h
        include/engine/state.h
        include/engine/referee.h
        src/engine/referee.cpp
        include/interface/renderer.h
        src/interface/renderer.cpp
        include/engine/mechanics.h
        src/engine/mechanics.cpp
        include/engine/game_director.h
        src/engine/game_director.cpp
        include/spectre/treasurer.h
        src/spectre/treasurer.cpp
        include/spectre/spectre_engine.h
        src/spectre/spectre_engine.cpp
        include/kernel/greedy_engine.h
        src/kernel/greedy_engine.cpp
        include/spectre/general.h
        src/spectre/general.cpp
        include/interface/console_observer.h
        include/spectre/spectre_types.h
        src/spectre/council.cpp
)
