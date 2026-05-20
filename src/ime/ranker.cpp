#include "ranker.hpp"
#include <spdlog/spdlog.h>

std::unique_ptr<CandidateRanker> RankerFactory::create(const std::string& type) {
    if (type == "none" || type.empty()) {
        return std::make_unique<NullRanker>();
    }

    spdlog::warn("Unknown ranker type: {}, using null ranker", type);
    return std::make_unique<NullRanker>();
}

std::vector<std::string> RankerFactory::available_types() {
    return {"none"};
}
