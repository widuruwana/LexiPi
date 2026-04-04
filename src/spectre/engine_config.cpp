#include "../../include/spectre/engine_config.h"
#include "../../include/spectre/spy.h"

namespace spectre {

    EngineConfig EngineConfig::full_spectre() {
        EngineConfig cfg;
        auto leaves = std::make_shared<HandTunedLeaves>();
        auto spy = std::make_shared<Spy>();

        cfg.leaveEval = leaves;
        cfg.opponentModel = spy;
        cfg.searchStrategy = std::make_shared<SimulationSelect>(leaves.get(), 100, 500);
        return cfg;
    }

} // namespace spectre
