#include "../include/board_state.h"
#include "../include/random.h"
#include "../include/logger.h"
#include <random>
#include <algorithm>

BoardStateManager::BoardStateManager() {
    initializeZobrist();
    hash = 0;
}

void BoardStateManager::initializeZobrist() {
    LOG_DEBUG("Initializing Zobrist hash tables");
    
    Random& rng = Random::getInstance();
    rng.seed(42); // Fixed seed for deterministic hash initialization
    
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            for (int i = 0; i < 27; i++) {
                pieceHash[r][c][i] = random64();
            }
            blankHash[r][c] = random64();
        }
    }
}

uint64_t BoardStateManager::random64() {
    static std::mt19937_64 rng(42);
    static std::uniform_int_distribution<uint64_t> dist;
    return dist(rng);
}

void BoardStateManager::makeMove(
    LetterBoard& letters,
    BlankBoard& blanks,
    const std::vector<TilePlacement>& placements) {
    
    for (const auto& placement : placements) {
        BoardChange change;
        change.row = placement.row;
        change.col = placement.col;
        change.oldLetter = letters[placement.row][placement.col];
        change.oldBlank = blanks[placement.row][placement.col];
        change.wasEmpty = (letters[placement.row][placement.col] == ' ');
        
        // Update hash: remove old piece
        if (!change.wasEmpty) {
            int oldIdx = (change.oldBlank ? 26 : (change.oldLetter - 'A'));
            if (oldIdx >= 0 && oldIdx < 27) {
                hash ^= pieceHash[change.row][change.col][oldIdx];
            }
            if (change.oldBlank) {
                hash ^= blankHash[change.row][change.col];
            }
        }
        
        // Apply placement
        letters[placement.row][placement.col] = placement.letter;
        blanks[placement.row][placement.col] = placement.isBlank;
        
        // Update hash: add new piece
        int newIdx = (placement.isBlank ? 26 : (placement.letter - 'A'));
        if (newIdx >= 0 && newIdx < 27) {
            hash ^= pieceHash[placement.row][placement.col][newIdx];
        }
        if (placement.isBlank) {
            hash ^= blankHash[placement.row][placement.col];
        }
        
        changeStack.push(change);
    }
}

void BoardStateManager::unmakeMove(
    LetterBoard& letters,
    BlankBoard& blanks) {
    
    while (!changeStack.empty()) {
        BoardChange change = changeStack.top();
        changeStack.pop();
        
        // Update hash: remove new piece
        int newIdx = (blanks[change.row][change.col] ? 26 : 
                     (letters[change.row][change.col] - 'A'));
        if (newIdx >= 0 && newIdx < 27) {
            hash ^= pieceHash[change.row][change.col][newIdx];
        }
        if (blanks[change.row][change.col]) {
            hash ^= blankHash[change.row][change.col];
        }
        
        // Restore old state
        letters[change.row][change.col] = change.oldLetter;
        blanks[change.row][change.col] = change.oldBlank;
        
        // Update hash: restore old piece
        if (!change.wasEmpty) {
            int oldIdx = (change.oldBlank ? 26 : (change.oldLetter - 'A'));
            if (oldIdx >= 0 && oldIdx < 27) {
                hash ^= pieceHash[change.row][change.col][oldIdx];
            }
            if (change.oldBlank) {
                hash ^= blankHash[change.row][change.col];
            }
        }
    }
}

void BoardStateManager::clear() {
    while (!changeStack.empty()) {
        changeStack.pop();
    }
    hash = 0;
}

uint64_t BoardStateManager::getHash(
    const LetterBoard& letters,
    const BlankBoard& blanks) const {
    
    uint64_t h = 0;
    for (int r = 0; r < 15; r++) {
        for (int c = 0; c < 15; c++) {
            if (letters[r][c] != ' ') {
                int idx = (blanks[r][c] ? 26 : (letters[r][c] - 'A'));
                if (idx >= 0 && idx < 27) {
                    h ^= pieceHash[r][c][idx];
                }
                if (blanks[r][c]) {
                    h ^= blankHash[r][c];
                }
            }
        }
    }
    return h;
}
