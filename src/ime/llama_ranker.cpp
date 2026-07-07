#include "llama_ranker.hpp"
#include "../util/utf8.hpp"
#include <spdlog/spdlog.h>
#include <llama.h>
#include <filesystem>
#include <algorithm>

LlamaRanker::LlamaRanker() = default;

LlamaRanker::~LlamaRanker() {
    running_ = false;
    queue_cv_.notify_all();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    unload_model();
}

bool LlamaRanker::initialize(const LlamaRankerConfig& config) {
    config_ = config;

    if (!config_.enabled) {
        spdlog::info("Llama ranker disabled in config");
        return false;
    }

    if (config_.model_path.empty()) {
        spdlog::warn("Llama ranker model path not specified");
        return false;
    }

    // Expand ~ in model path
    std::string model_path = config_.model_path;
    if (!model_path.empty() && model_path[0] == '~') {
        const char* home = getenv("HOME");
        if (home) {
            model_path = std::filesystem::path(home) / model_path.substr(2);
        }
    }

    // Check if model file exists
    if (!std::filesystem::exists(model_path)) {
        spdlog::warn("Llama model not found: {}", model_path);
        return false;
    }

    config_.model_path = model_path;

    // Start worker thread for async processing
    running_ = true;
    worker_thread_ = std::thread(&LlamaRanker::worker_loop, this);

    available_ = true;
    spdlog::info("Llama ranker initialized (model: {}, threads: {})", config_.model_path, config_.n_threads);
    return true;
}

void LlamaRanker::rank(std::vector<Candidate>& candidates, const std::string& context) {
    if (!is_available() || candidates.empty()) {
        return;
    }

    // Lazy load model on first use
    if (!is_model_loaded()) {
        load_model();
    }

    if (!is_model_loaded()) {
        return;
    }

    auto ranked = do_rank(candidates, context, "");
    candidates = std::move(ranked);
}

void LlamaRanker::rank_async(std::vector<Candidate> candidates, const std::string& context, const std::string& pinyin,
                             std::function<void(std::vector<Candidate>)> callback) {
    if (!is_available() || candidates.empty()) {
        if (callback)
            callback(candidates);
        return;
    }

    // Queue task for async processing
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push({std::move(candidates), context, pinyin, std::move(callback)});
    }
    queue_cv_.notify_one();
}

bool LlamaRanker::is_available() const {
    return available_;
}

bool LlamaRanker::load_model() {
    if (model_loaded_)
        return true;

    spdlog::info("Loading llama model: {} (backend: {})", config_.model_path, config_.backend);

    // Redirect llama.cpp's own logging away from stderr (it prints reams of
    // `repack:`/`llama_model_loader:`/`load_tensors:` lines that overwrite the
    // TTY screen when AI ranking is enabled). Route through spdlog instead so
    // it lands in the file log, and drop the chatty INFO-level model-load spam.
    static bool log_redirected = false;
    if (!log_redirected) {
        llama_log_set(
            [](ggml_log_level level, const char* text, void*) {
                if (!text)
                    return;
                std::string msg(text);
                while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
                    msg.pop_back();
                if (msg.empty())
                    return;
                if (level >= GGML_LOG_LEVEL_ERROR) {
                    spdlog::error("llama: {}", msg);
                } else if (level >= GGML_LOG_LEVEL_WARN) {
                    spdlog::warn("llama: {}", msg);
                }
                // INFO/DEBUG silently dropped — keep the TTY clean.
            },
            nullptr);
        log_redirected = true;
    }

    // Initialize llama backend
    llama_backend_init();

    // Load model with appropriate GPU layers based on backend
    llama_model_params model_params = llama_model_default_params();

    if (config_.backend == "cuda" || config_.backend == "vulkan" || config_.backend == "metal") {
        model_params.n_gpu_layers = config_.n_gpu_layers > 0 ? config_.n_gpu_layers : 35;
        spdlog::info("Using {} backend with {} GPU layers", config_.backend, model_params.n_gpu_layers);
    } else {
        model_params.n_gpu_layers = 0;  // CPU only
        spdlog::info("Using CPU backend with {} threads", config_.n_threads);
    }

    model_ = llama_load_model_from_file(config_.model_path.c_str(), model_params);
    if (!model_) {
        spdlog::error("Failed to load llama model: {}", config_.model_path);
        return false;
    }

    // Create context
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_threads = config_.n_threads;
    ctx_params.n_threads_batch = config_.n_threads;
    ctx_params.n_ctx = 512;  // Small context for ranking

    ctx_ = llama_new_context_with_model(model_, ctx_params);
    if (!ctx_) {
        spdlog::error("Failed to create llama context");
        llama_free_model(model_);
        model_ = nullptr;
        return false;
    }

    model_loaded_ = true;
    spdlog::info("Llama model loaded successfully");
    return true;
}

void LlamaRanker::unload_model() {
    if (!model_loaded_)
        return;

    if (ctx_) {
        llama_free(ctx_);
        ctx_ = nullptr;
    }
    if (model_) {
        llama_free_model(model_);
        model_ = nullptr;
    }

    llama_backend_free();

    model_loaded_ = false;
    spdlog::info("Llama model unloaded");
}

bool LlamaRanker::is_model_loaded() const {
    return model_loaded_;
}

void LlamaRanker::worker_loop() {
    // Preload the model as soon as the worker starts (at App::init time, when
    // enabled), rather than lazily on the first ranking task. Otherwise the
    // first candidate after enabling AI ranking stalls for several seconds
    // while 330MB of model is loaded + repacked (monkey finding F6).
    load_model();

    while (running_) {
        RankTask task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !task_queue_.empty() || !running_; });

            if (!running_)
                break;

            if (task_queue_.empty())
                continue;

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        // Process task (model already preloaded above; re-load only if it was
        // unloaded between tasks, e.g. by unload_model()).
        if (!is_model_loaded()) {
            load_model();
        }

        // Process task
        auto result = do_rank(task.candidates, task.context, task.pinyin);
        if (task.callback) {
            task.callback(std::move(result));
        }
    }
}

std::string LlamaRanker::build_prompt(const std::vector<Candidate>& candidates, const std::string& context,
                                      const std::string& pinyin) const {
    // Build candidate list string
    std::string cand_list;
    for (size_t i = 0; i < candidates.size() && i < 9; ++i) {
        std::string text;
        for (char32_t ch : candidates[i].text) {
            text += utf8::encode(ch);
        }
        cand_list += std::to_string(i + 1) + "." + text + " ";
    }

    // Build prompt for ranking
    std::string prompt;
    if (!context.empty()) {
        prompt = "根据上下文\"" + context + "\"和拼音\"" + pinyin +
                 "\"，"
                 "从以下候选词中选择最合适的（只输出序号）：\n" +
                 cand_list;
    } else {
        prompt = "根据拼音\"" + pinyin +
                 "\"，"
                 "从以下候选词中选择最合适的（只输出序号）：\n" +
                 cand_list;
    }

    return prompt;
}

std::vector<int> LlamaRanker::parse_ranking(const std::string& output, int num_candidates) const {
    std::vector<int> ranking;

    // Find numbers in output
    for (char c : output) {
        if (c >= '1' && c <= '9') {
            int idx = c - '1';
            if (idx < num_candidates) {
                ranking.push_back(idx);
            }
        }
    }

    // Fill remaining candidates in original order
    for (int i = 0; i < num_candidates; ++i) {
        if (std::find(ranking.begin(), ranking.end(), i) == ranking.end()) {
            ranking.push_back(i);
        }
    }

    return ranking;
}

std::vector<Candidate> LlamaRanker::do_rank(const std::vector<Candidate>& candidates, const std::string& context,
                                            const std::string& pinyin) {
    if (candidates.empty() || !ctx_)
        return candidates;

    // Build prompt
    std::string prompt = build_prompt(candidates, context, pinyin);

    // Tokenize
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    std::vector<llama_token> tokens;
    tokens.resize(prompt.size() + 1);
    int n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), true, true);
    if (n_tokens < 0) {
        spdlog::error("Failed to tokenize prompt");
        return candidates;
    }
    tokens.resize(n_tokens);

    // Create batch
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);

    // Run inference
    int decode_result = llama_decode(ctx_, batch);
    if (decode_result != 0) {
        spdlog::error("Failed to decode: {}", decode_result);
        return candidates;
    }

    // Get output tokens
    std::string output;
    llama_token last_token = llama_vocab_eos(vocab);
    for (int i = 0; i < config_.max_tokens; ++i) {
        // Sample next token
        auto* logits = llama_get_logits_ith(ctx_, batch.n_tokens - 1);
        if (!logits)
            break;

        // RAII buffer for candidate token data. Previously this was a raw
        // new[]/delete[] pair, but several early `break` paths below skipped
        // the delete[], leaking on every ranking iteration (monkey finding F8).
        // The buffer is now stack-managed via std::vector and released at end
        // of the iteration regardless of which break fires.
        const size_t n_vocab = llama_vocab_n_tokens(vocab);
        std::vector<llama_token_data> cand_buf(n_vocab);

        // Simple greedy sampling
        float max_logit = -INFINITY;
        llama_token best_token = last_token;
        for (size_t j = 0; j < n_vocab; ++j) {
            if (logits[j] > max_logit) {
                max_logit = logits[j];
                best_token = static_cast<llama_token>(j);
            }
        }

        if (best_token == last_token)
            break;

        // Decode token to text
        char buf[16];
        int len = llama_token_to_piece(vocab, best_token, buf, sizeof(buf), 0, false);
        if (len > 0) {
            output += std::string(buf, len);
        }

        // Check for EOS
        if (llama_vocab_is_eog(vocab, best_token))
            break;

        // Continue with this token
        batch = llama_batch_get_one(&best_token, 1);
        decode_result = llama_decode(ctx_, batch);
        if (decode_result != 0)
            break;
        // cand_buf freed automatically at end of iteration — no leak.
    }

    // Parse ranking from output
    auto ranking = parse_ranking(output, candidates.size());

    // Reorder candidates
    std::vector<Candidate> ranked_result;
    for (int idx : ranking) {
        if (idx >= 0 && idx < static_cast<int>(candidates.size())) {
            ranked_result.push_back(candidates[idx]);
        }
    }

    return ranked_result;
}