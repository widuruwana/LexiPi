#include "../../include/spectre/opponent_model.h"
#include <random>
#include <algorithm>

namespace spectre {

    // =========================================================
    // UniformOpponentModel (Maximum Ignorance Baseline)
    // =========================================================

    UniformOpponentModel::UniformOpponentModel() {
        unseen_pool.reserve(100);
    }

    void UniformOpponentModel::updateGroundTruth(const LetterBoard& board,
                                                   const TileRack& myRack,
                                                   const std::vector<Tile>& bag) {
        // Same logic as Spy's ground truth computation
        int standard_counts[27] = {
            9, 2, 2, 4, 12, 2, 3, 2, 9, 1, 1, 4, 2, 6, 8, 2, 1, 6, 4, 6, 4, 2, 2, 1, 2, 1, 2
        };

        for (int r = 0; r < 15; r++) {
            for (int c = 0; c < 15; c++) {
                char ch = board[r][c];
                if (ch != ' ') {
                    if (ch >= 'a' && ch <= 'z') standard_counts[26]--;
                    else standard_counts[ch - 'A']--;
                }
            }
        }

        for (const auto& t : myRack) {
            if (t.letter == '?') standard_counts[26]--;
            else if (t.letter >= 'A' && t.letter <= 'Z') standard_counts[t.letter - 'A']--;
        }

        unseen_pool.clear();
        for (int i = 0; i < 26; i++) {
            for (int k = 0; k < standard_counts[i]; k++) {
                unseen_pool.push_back((char)('A' + i));
            }
        }
        for (int k = 0; k < standard_counts[26]; k++) {
            unseen_pool.push_back('?');
        }
    }

    void UniformOpponentModel::observeOpponentMove(const Move& /*move*/,
                                                     const LetterBoard& /*preMoveBoard*/) {
        // Uniform model ignores observations. Maximum ignorance by design.
    }

    std::vector<char> UniformOpponentModel::sampleOpponentRack() const {
        if (unseen_pool.empty()) return {};

        static thread_local std::mt19937 rng(std::random_device{}());

        std::vector<char> pool_copy = unseen_pool;
        std::shuffle(pool_copy.begin(), pool_copy.end(), rng);

        int rack_size = std::min((int)pool_copy.size(), 7);
        return std::vector<char>(pool_copy.begin(), pool_copy.begin() + rack_size);
    }

    const std::vector<char>& UniformOpponentModel::getUnseenPool() const {
        return unseen_pool;
    }

} // namespace spectre
