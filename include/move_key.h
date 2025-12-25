#pragma once

#include "board_state.h"
#include "rack.h"
#include <cstdint>
#include <string>
#include <vector>

// Canonical move key for caching/transposition tables
// Based on: placements (stable order), board hash, rack multiset
struct MoveKey {
    uint64_t boardHash;      // Zobrist hash of board before move
    uint64_t rackHash;       // Hash of rack tiles before move
    uint32_t placementHash;   // Hash of placements (stable order)
    
    bool operator==(const MoveKey& other) const {
        return boardHash == other.boardHash &&
               rackHash == other.rackHash &&
               placementHash == other.placementHash;
    }
    
    bool operator!=(const MoveKey& other) const {
        return !(*this == other);
    }
    
    bool operator<(const MoveKey& other) const {
        if (boardHash != other.boardHash) return boardHash < other.boardHash;
        if (rackHash != other.rackHash) return rackHash < other.rackHash;
        return placementHash < other.placementHash;
    }
};

// Move key generator
class MoveKeyGenerator {
public:
    // Generate canonical key from move
    static MoveKey generate(
        const LetterBoard& letters,
        const BlankBoard& blanks,
        const TileRack& rack,
        const std::vector<TilePlacement>& placements
    );
    
    // Generate hash of rack tiles (order-independent)
    static uint64_t hashRack(const TileRack& rack);
    
    // Generate hash of placements (order-independent)
    static uint32_t hashPlacements(const std::vector<TilePlacement>& placements);
    
private:
    // Simple hash function
    static uint32_t hashCombine(uint32_t h1, uint32_t h2);
    static uint64_t hashCombine64(uint64_t h1, uint64_t h2);
};
