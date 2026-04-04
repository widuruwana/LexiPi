#pragma once

#include "leave_evaluator.h"
#include "opponent_model.h"
#include "search_strategy.h"
#include <memory>
#include <string>

namespace spectre {

    // =========================================================
    // ENGINE CONFIGURATION
    // =========================================================
    // A configuration is a composition of:
    //   1. LeaveEvaluator  — How good is my remaining rack?
    //   2. OpponentModel   — What does my opponent hold?
    //   3. SearchStrategy  — How do I pick the best move?
    //
    // Each unique (LeaveEval, OppModel, Search) tuple defines
    // a distinct "player" for ablation experiments.
    //
    // Usage:
    //   auto cfg = EngineConfig::greedy_baseline();
    //   auto cfg = EngineConfig::equity_handtuned();
    //   auto cfg = EngineConfig::full_spectre();
    // =========================================================

    struct EngineConfig {
        std::shared_ptr<LeaveEvaluator>   leaveEval;
        std::shared_ptr<OpponentModel>    opponentModel;
        std::shared_ptr<SearchStrategy>   searchStrategy;

        std::string label() const {
            std::string s = "Config[";
            if (leaveEval) s += leaveEval->name();
            s += " + ";
            if (opponentModel) s += opponentModel->name();
            s += " + ";
            if (searchStrategy) s += searchStrategy->name();
            s += "]";
            return s;
        }

        // ---------------------------------------------------------
        // PRESET FACTORIES (Each is a row in the ablation table)
        // ---------------------------------------------------------

        // Row 1: Pure score maximizer. No leave. No opponent model. No search.
        static EngineConfig greedy_baseline() {
            EngineConfig cfg;
            cfg.searchStrategy = std::make_shared<GreedySelect>();
            // No leaveEval or opponentModel needed for greedy
            return cfg;
        }

        // Row 2: Score + hand-tuned leave equity. No simulation.
        static EngineConfig equity_handtuned() {
            EngineConfig cfg;
            auto leaves = std::make_shared<HandTunedLeaves>();
            cfg.leaveEval = leaves;
            cfg.searchStrategy = std::make_shared<EquitySelect>(leaves.get());
            return cfg;
        }

        // Row 3: Score + hand-tuned leaves + uniform opponent model + simulation.
        static EngineConfig simulation_uniform() {
            EngineConfig cfg;
            auto leaves = std::make_shared<HandTunedLeaves>();
            auto opp = std::make_shared<UniformOpponentModel>();
            cfg.leaveEval = leaves;
            cfg.opponentModel = opp;
            cfg.searchStrategy = std::make_shared<SimulationSelect>(leaves.get());
            return cfg;
        }

        // Row 4: Full Spectre — trained leaves + particle filter + simulation.
        // (Placeholder until training pipeline exists)
        static EngineConfig full_spectre();
    };

} // namespace spectre
