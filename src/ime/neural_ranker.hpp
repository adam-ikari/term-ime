#pragma once

#include "ranker.hpp"
#include "core/config.hpp"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <functional>

// Neural network-based candidate ranker using ONNX Runtime
// Features:
// - Async ranking to avoid blocking input
// - Lazy loading to minimize memory usage
// - INT8 quantized model support for CPU efficiency
class NeuralRanker : public CandidateRanker {
public:
    explicit NeuralRanker();
    ~NeuralRanker();

    // Initialize with config
    bool initialize(const NeuralRankerConfig& config);

    // Rank candidates (async, results delivered via callback)
    // This is non-blocking - results are delivered asynchronously
    void rank(std::vector<Candidate>& candidates,
              const std::string& context) override;

    // Async ranking with result callback
    // The callback will be invoked when ranking is complete
    void rank_async(std::vector<Candidate> candidates,
                    const std::string& context,
                    std::function<void(std::vector<Candidate>)> callback);

    bool is_available() const override;
    std::string name() const override { return "neural"; }

    // Load model (lazy, on first use)
    void load_model();

    // Unload model to free memory
    void unload_model();

    // Check if model is loaded
    bool is_model_loaded() const;

private:
    struct RankTask {
        std::vector<Candidate> candidates;
        std::string context;
        std::function<void(std::vector<Candidate>)> callback;
    };

    NeuralRankerConfig config_;
    std::atomic<bool> available_{false};
    std::atomic<bool> model_loaded_{false};

    // Async processing
    std::thread worker_thread_;
    std::queue<RankTask> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> running_{false};

    void worker_loop();
    std::vector<Candidate> do_rank(const std::vector<Candidate>& candidates,
                                   const std::string& context);

    // ONNX Runtime handles (opaque pointers to avoid header dependency)
    void* session_ = nullptr;
    void* env_ = nullptr;
};