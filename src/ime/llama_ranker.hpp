#pragma once

#include "ranker.hpp"
#include "core/config.hpp"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <functional>

// Forward declaration to avoid including llama.h in header
struct llama_model;
struct llama_context;

// LLM-based candidate ranker using llama.cpp
// Features:
// - Async ranking to avoid blocking input
// - Lazy model loading to minimize memory usage
// - Quantized model support for CPU efficiency
class LlamaRanker : public CandidateRanker {
public:
    LlamaRanker();
    ~LlamaRanker();

    // Initialize with config
    bool initialize(const LlamaRankerConfig& config);

    // Synchronous rank (may block, use rank_async for better UX)
    void rank(std::vector<Candidate>& candidates,
              const std::string& context) override;

    // Async ranking with result callback
    // The callback will be invoked when ranking is complete
    void rank_async(std::vector<Candidate> candidates,
                    const std::string& context,
                    const std::string& pinyin,
                    std::function<void(std::vector<Candidate>)> callback);

    bool is_available() const override;
    std::string name() const override { return "llama"; }

    // Load model (lazy, on first use)
    bool load_model();

    // Unload model to free memory
    void unload_model();

    // Check if model is loaded
    bool is_model_loaded() const;

private:
    struct RankTask {
        std::vector<Candidate> candidates;
        std::string context;
        std::string pinyin;
        std::function<void(std::vector<Candidate>)> callback;
    };

    LlamaRankerConfig config_;
    std::atomic<bool> available_{false};
    std::atomic<bool> model_loaded_{false};

    // Async processing
    std::thread worker_thread_;
    std::queue<RankTask> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> running_{false};

    // llama.cpp handles
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;

    void worker_loop();
    std::vector<Candidate> do_rank(const std::vector<Candidate>& candidates,
                                   const std::string& context,
                                   const std::string& pinyin);

    // Build prompt for LLM
    std::string build_prompt(const std::vector<Candidate>& candidates,
                             const std::string& context,
                             const std::string& pinyin) const;

    // Parse LLM output to get ranking
    std::vector<int> parse_ranking(const std::string& output,
                                   int num_candidates) const;
};
