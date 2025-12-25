#pragma once

#include <string>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <unordered_map>

enum class LogLevel {
    ERROR = 0,
    WARN = 1,
    INFO = 2,
    DEBUG = 3,
    TRACE = 4
};

class Logger {
public:
    static Logger& getInstance();
    
    void setLevel(LogLevel level);
    LogLevel getLevel() const;
    
    template<typename... Args>
    void log(LogLevel level, const char* file, int line, const char* format, Args... args) {
        if (level > currentLevel) return;
        
        std::lock_guard<std::mutex> lock(mutex);
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::cout << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        std::cout << "." << std::setfill('0') << std::setw(3) << ms.count();
        std::cout << "] [" << levelToString(level) << "]";
        
        if (level <= LogLevel::DEBUG) {
            std::cout << " [" << file << ":" << line << "]";
        }
        
        std::cout << " ";
        
        ((std::cout << args), ...);
        std::cout << std::endl;
    }
    
    // Rate-limited logging - only log once per N calls
    template<typename... Args>
    void logRateLimited(LogLevel level, int& counter, int limit,
                     const char* file, int line, const char* format, Args... args) {
        if (level > currentLevel) return;
        
        counter++;
        if (counter % limit == 0) {
            log(level, file, line, format, args...);
        }
    }
    
    // Aggregate counts for repetitive messages
    void incrementCounter(const std::string& key);
    void printCounters();
    void resetCounters();

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    LogLevel currentLevel;
    std::mutex mutex;
    std::unordered_map<std::string, int> counters;
    
    const char* levelToString(LogLevel level) const;
};

// Convenience macros
#define LOG_ERROR(...) Logger::getInstance().log(LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  Logger::getInstance().log(LogLevel::WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  Logger::getInstance().log(LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) Logger::getInstance().log(LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_TRACE(...) Logger::getInstance().log(LogLevel::TRACE, __FILE__, __LINE__, __VA_ARGS__)

// Rate-limited logging (logs every N calls)
#define LOG_RATE_LIMITED(level, counter, limit, ...) \
    Logger::getInstance().logRateLimited(level, counter, limit, __FILE__, __LINE__, __VA_ARGS__)

// Counter aggregation
#define LOG_INCREMENT(key) Logger::getInstance().incrementCounter(key)
#define LOG_PRINT_COUNTERS() Logger::getInstance().printCounters()
