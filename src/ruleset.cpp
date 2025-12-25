#include "../include/ruleset.h"
#include "../include/logger.h"

Ruleset& Ruleset::getInstance() {
    static Ruleset instance;
    return instance;
}

void Ruleset::initialize(
    const std::string& lexiconId,
    int bingoBonus,
    int rackSize,
    int boardSize) {
    
    if (initialized) {
        LOG_WARN("Ruleset already initialized. Skipping re-initialization.");
        return;
    }
    
    this->lexiconId = lexiconId;
    this->bingoBonus = bingoBonus;
    this->rackSize = rackSize;
    this->boardSize = boardSize;
    
    initialized = true;
    
    LOG_INFO("Ruleset initialized:");
    LOG_INFO("  Lexicon: ", lexiconId);
    LOG_INFO("  Bingo Bonus: ", bingoBonus);
    LOG_INFO("  Rack Size: ", rackSize);
    LOG_INFO("  Board Size: ", boardSize);
}
