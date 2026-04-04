cmake_minimum_required(VERSION 3.16)
project(AlternatePi VERSION 0.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# =========================================================
# SOURCE FILES - Organized by module
# =========================================================

# Engine (Core game logic - no AI dependency)
set(ENGINE_SOURCES
    src/engine/board.cpp
    src/engine/dictionary.cpp
    src/engine/game_director.cpp
    src/engine/mechanics.cpp
    src/engine/rack.cpp
    src/engine/referee.cpp
    src/engine/tiles.cpp
)

# Kernel (Move generation - no spectre dependency)
set(KERNEL_SOURCES
    src/kernel/greedy_engine.cpp
    src/kernel/move_generator.cpp
)

# Spectre (AI intelligence layer)
set(SPECTRE_SOURCES
    src/spectre/engine_config.cpp
    src/spectre/general.cpp
    src/spectre/judge.cpp
    src/spectre/leave_evaluator.cpp
    src/spectre/opponent_model.cpp
    src/spectre/search_strategy.cpp
    src/spectre/spy.cpp
    src/spectre/treasurer.cpp
    src/spectre/vanguard.cpp
)

# Interface
set(INTERFACE_SOURCES
    src/interface/renderer.cpp
)

# Player Controllers
set(PLAYER_SOURCES
    src/ai_player.cpp
    src/human_player.cpp
    src/choices.cpp
)

# Game Modes
set(MODE_SOURCES
    src/modes/AiAi/aiai.cpp
    src/modes/Home/home.cpp
    src/modes/PvE/pve.cpp
    src/modes/PvP/pvp.cpp
)

# =========================================================
# BUILD TARGET
# =========================================================

add_executable(lexi_pi
    src/main.cpp
    ${ENGINE_SOURCES}
    ${KERNEL_SOURCES}
    ${SPECTRE_SOURCES}
    ${INTERFACE_SOURCES}
    ${PLAYER_SOURCES}
    ${MODE_SOURCES}
)

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(lexi_pi Threads::Threads)

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(lexi_pi PRIVATE -O3 -march=native -flto)
    target_link_options(lexi_pi PRIVATE -flto)
endif()
