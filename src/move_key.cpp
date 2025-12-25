#include "../include/move_key.h"
#include "../include/board_state.h"
#include <algorithm>
#include <cctype>

uint32_t MoveKeyGenerator::hashCombine(uint32_t h1, uint32_t h2) {
    // Boost's hash_combine
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
}

uint64_t MoveKeyGenerator::hashCombine64(uint64_t h1, uint64_t h2) {
    // 64-bit version
    return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
}

uint64_t MoveKeyGenerator::hashRack(const TileRack& rack) {
    // Count tiles (order-independent)
    int counts[27] = {0}; // 0-25 for A-Z, 26 for blank
    
    for (const Tile& tile : rack) {
        if (tile.is_blank) {
            counts[26]++;
        } else {
            int idx = toupper(tile.letter) - 'A';
            if (idx >= 0 && idx < 26) {
                counts[idx]++;
            }
        }
    }
    
    // Hash the counts
    uint64_t h = 0;
    for (int i = 0; i < 27; i++) {
        if (counts[i] > 0) {
            h = hashCombine64(h, static_cast<uint64_t>(i) * 31 + counts[i]);
        }
    }
    
    return h;
}

uint32_t MoveKeyGenerator::hashPlacements(const std::vector<TilePlacement>& placements) {
    // Sort placements for stable order
    std::vector<TilePlacement> sorted = placements;
    std::sort(sorted.begin(), sorted.end(), 
        [](const TilePlacement& a, const TilePlacement& b) {
            if (a.row != b.row) return a.row < b.row;
            if (a.col != b.col) return a.col < b.col;
            return a.letter < b.letter;
        });
    
    // Hash the sorted placements
    uint32_t h = 0;
    for (const auto& placement : sorted) {
        int letterIdx = (placement.is_blank ? 26 : (placement.letter - 'A'));
        h = hashCombine(h, static_cast<uint32_t>(letterIdx));
        h = hashCombine(h, static_cast<uint32_t>(placement.row));
        h = hashCombine(h, static_cast<uint32_t>(placement.col));
    }
    
    return h;
}

MoveKey MoveKeyGenerator::generate(
    const LetterBoard& letters,
    const BlankBoard& blanks,
    const TileRack& rack,
    const std::vector<TilePlacement>& placements) {
    
    MoveKey key;
    key.boardHash = BoardStateManager().getHash(letters, blanks);
    key.rackHash = hashRack(rack);
    key.placementHash = hashPlacements(placements);
    
    return key;
}
