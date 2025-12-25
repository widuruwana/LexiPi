#include "../include/evaluator.h"
#include "../include/logger.h"

Evaluator& Evaluator::getInstance() {
    static Evaluator instance;
    return instance;
}

bool Evaluator::initialize(const std::string& weightsFile) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (initialized) {
        LOG_WARN("Evaluator already initialized. Skipping re-initialization.");
        return true;
    }
    
    LOG_INFO("Initializing Evaluator with weights file: ", weightsFile);
    
    if (!model.loadWeights(weightsFile)) {
        LOG_ERROR("Failed to load weights from: ", weightsFile);
        return false;
    }
    
    weightsLoadCount++;
    LOG_INFO("Weights loaded successfully (load #", weightsLoadCount, ")");
    initialized = true;
    return true;
}

EvaluationModel& Evaluator::getModel() {
    if (!initialized) {
        LOG_ERROR("Evaluator accessed before initialization!");
        // Return default model anyway to avoid crash
    }
    return model;
}

Dawg& Evaluator::getDawg() {
    return gDawg; // Global DAWG instance
}

bool Evaluator::isInitialized() const {
    return initialized;
}

bool Evaluator::reloadWeights(const std::string& weightsFile) {
    std::lock_guard<std::mutex> lock(mutex);
    
    LOG_INFO("Reloading weights from: ", weightsFile);
    
    if (!model.loadWeights(weightsFile)) {
        LOG_ERROR("Failed to reload weights from: ", weightsFile);
        return false;
    }
    
    weightsLoadCount++;
    LOG_INFO("Weights reloaded successfully (load #", weightsLoadCount, ")");
    return true;
}
