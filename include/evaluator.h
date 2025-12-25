#pragma once

#include "evaluation_model.h"
#include "dawg.h"
#include "dict.h"
#include <memory>
#include <mutex>

// Singleton Evaluator that loads weights and resources once at startup
class Evaluator {
public:
    // Get singleton instance
    static Evaluator& getInstance();
    
    // Initialize with weights file (call once at startup)
    bool initialize(const std::string& weightsFile = "data/weights.txt");
    
    // Get the evaluation model
    EvaluationModel& getModel();
    
    // Get the DAWG
    Dawg& getDawg();
    
    // Check if initialized
    bool isInitialized() const;
    
    // Reload weights (dev-only, for hot-reload during tuning)
    bool reloadWeights(const std::string& weightsFile);
    
    // Prevent copying
    Evaluator(const Evaluator&) = delete;
    Evaluator& operator=(const Evaluator&) = delete;

private:
    Evaluator() = default;
    ~Evaluator() = default;
    
    EvaluationModel model;
    bool initialized = false;
    std::mutex mutex;
    
    // Track load count for rate limiting
    int weightsLoadCount = 0;
};
