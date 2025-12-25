#include "../include/logger.h"
#include <unordered_map>

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : currentLevel(LogLevel::INFO) {}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex);
    currentLevel = level;
}

LogLevel Logger::getLevel() const {
    return currentLevel;
}

const char* Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::TRACE: return "TRACE";
        default: return "UNKNOWN";
    }
}

void Logger::incrementCounter(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    counters[key]++;
}

void Logger::printCounters() {
    std::lock_guard<std::mutex> lock(mutex);
    if (counters.empty()) return;
    
    std::cout << "[INFO] === Aggregated Counters ===" << std::endl;
    for (const auto& pair : counters) {
        std::cout << "[INFO] " << pair.first << ": " << pair.second << std::endl;
    }
    std::cout << "[INFO] ============================" << std::endl;
}

void Logger::resetCounters() {
    std::lock_guard<std::mutex> lock(mutex);
    counters.clear();
}
