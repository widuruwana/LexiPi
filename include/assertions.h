#pragma once

#include "../include/logger.h"
#include <stdexcept>
#include <cstdlib>

// Development mode assertions (hard fail on invariants)
#ifdef DEBUG_BUILD
    #define ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                LOG_ERROR("ASSERTION FAILED: ", message); \
                LOG_ERROR("  File: ", __FILE__, " Line: ", __LINE__); \
                std::abort(); \
            } \
        } while(0)
#else
    // Production mode: log error but continue safely
    #define ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                LOG_ERROR("Invariant violated: ", message); \
                LOG_ERROR("  File: ", __FILE__, " Line: ", __LINE__); \
            } \
        } while(0)
#endif

// Specific assertion macros
#define ASSERT_TILE_COUNT(expected, actual) \
    ASSERT((expected) == (actual), \
           "Tile count mismatch: expected " + std::to_string(expected) + \
           ", got " + std::to_string(actual))

#define ASSERT_VALID_POSITION(row, col) \
    ASSERT((row) >= 0 && (row) < 15 && (col) >= 0 && (col) < 15, \
           "Invalid position: (" + std::to_string(row) + "," + std::to_string(col) + ")")

#define ASSERT_NOT_NULL(ptr, name) \
    ASSERT((ptr) != nullptr, \
           "Null pointer: " + std::string(name))

#define ASSERT_RANGE(value, min, max, name) \
    ASSERT((value) >= (min) && (value) <= (max), \
           std::string(name) + " out of range: " + std::to_string(value) + \
           " (expected " + std::to_string(min) + "-" + std::to_string(max) + ")")

#define ASSERT_SCORE_MATCH(expected, actual) \
    ASSERT((expected) == (actual), \
           "Score mismatch: expected " + std::to_string(expected) + \
           ", got " + std::to_string(actual))

#define ASSERT_WORD_VALID(word) \
    ASSERT(!(word).empty(), "Empty word") \
    ASSERT((word).length() >= 2, "Word too short: " + (word))
