#include "ranker.hpp"
#include "neural_ranker.hpp"
#include <spdlog/spdlog.h>

std::unique_ptr<CandidateRanker> RankerFactory::create(const std::string& type) {
    if (type == "none" || type.empty()) {
        return std::make_unique<NullRanker>();
    }

    if (type == "neural") {
#ifdef USE_ONNX_RUNTIME
        return std::make_unique<NeuralRanker>();
#else
        spdlog::warn("Neural ranker requested but ONNX Runtime not available");
        return std::make_unique<NullRanker>();
#endif
    }

    spdlog::warn("Unknown ranker type: {}, using null ranker", type);
    return std::make_unique<NullRanker>();
}

std::vector<std::string> RankerFactory::available_types() {
    std::vector<std::string> types = {"none"};
#ifdef USE_ONNX_RUNTIME
    types.push_back("neural");
#endif
    return types;
}
