#pragma once

#include "engine.hpp"
#include <string>
#include <vector>
#include <memory>

// Abstract interface for candidate ranking
// Implementations can use different strategies (statistical, neural, etc.)
class CandidateRanker {
   public:
    virtual ~CandidateRanker() = default;

    // Rank/reorder candidates based on context
    // context: preceding text for context-aware ranking
    // This modifies the candidates vector in place
    virtual void rank(std::vector<Candidate>& candidates, const std::string& context) = 0;

    // Check if ranker is available and initialized
    virtual bool is_available() const = 0;

    // Get ranker name for logging
    virtual std::string name() const = 0;
};

// Factory for creating rankers
class RankerFactory {
   public:
    // Create a ranker by type
    // Currently only "none" is available
    static std::unique_ptr<CandidateRanker> create(const std::string& type);

    // Get list of available ranker types
    static std::vector<std::string> available_types();
};

// No-op ranker (pass through)
class NullRanker : public CandidateRanker {
   public:
    void rank(std::vector<Candidate>& candidates, const std::string& context) override {
        // No-op: candidates unchanged
    }

    bool is_available() const override { return true; }
    std::string name() const override { return "null"; }
};
