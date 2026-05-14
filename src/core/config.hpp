#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Language configuration for multi-language support
struct LanguageConfig {
    std::string id;           // Language identifier (zh-CN, ja, ko, etc.)
    std::string name;         // Display name
    std::string schema;       // librime schema id
    bool enabled = true;      // Whether this language is enabled

    json to_json() const;
    static LanguageConfig from_json(const json& j);
};

// Neural ranker configuration (optional smart candidate ranking)
struct NeuralRankerConfig {
    bool enabled = false;
    std::string model_path;   // Path to ONNX model
    int max_candidates = 10;  // Max candidates to rank
    float threshold = 0.5f;   // Confidence threshold

    json to_json() const;
    static NeuralRankerConfig from_json(const json& j);
};

struct AppConfig {
    // Shell settings
    std::string shell = "/bin/bash";

    // Language settings (replaces hardcoded chinese/english mode)
    std::vector<LanguageConfig> languages;
    std::string active_language = "zh-CN";  // Current active language

    // IME settings
    std::string dict_path = "data/pinyin.dict";
    std::vector<std::string> extra_dicts;
    int page_size = 5;

    // Display settings
    bool show_mode_indicator = true;
    std::string candidate_bar_position = "bottom";  // "bottom" or "top"

    // Neural ranker settings (optional)
    NeuralRankerConfig neural_ranker;

    // Logging
    std::string log_level = "warn";  // "debug", "info", "warn", "error"
    std::string log_file = "";  // empty = no file logging

    // Load from file
    static AppConfig load(const std::string& path);

    // Save to file
    void save(const std::string& path) const;

    // Get default config path
    static std::string default_path();

    // Convert to/from JSON
    json to_json() const;
    static AppConfig from_json(const json& j);

    // Get default languages
    static std::vector<LanguageConfig> default_languages();
};
