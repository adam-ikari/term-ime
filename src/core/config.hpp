#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Language configuration for multi-language support
struct LanguageConfig {
    std::string id;       // Language identifier (zh-CN, ja, ko, etc.)
    std::string name;     // Display name
    std::string schema;   // librime schema id
    bool enabled = true;  // Whether this language is enabled

    json to_json() const;
    static LanguageConfig from_json(const json& j);
};

struct AppConfig {
    // Shell settings
    std::string shell = "/bin/bash";

    // Language settings (replaces hardcoded chinese/english mode)
    std::vector<LanguageConfig> languages;
    std::string active_language = "zh-Hans";  // Current active language

    // UI language for i18n
    std::string ui_language = "zh-CN";  // UI display language: "en", "zh-CN"

    // IME settings
    std::string dict_path = "data/pinyin.dict";
    std::vector<std::string> extra_dicts;
    // Candidate bar: upper bound on how many candidates are shown per page
    // (1-9; 9 is the highest single-digit selector key). The number actually
    // shown adapts to the terminal width — see ui::FitCandidateBar.
    int max_candidates = 9;

    // Fuzzy pinyin groups enabled. Each is one settings toggle:
    //   zh_z 平翘舌 (zh/ch/sh ↔ z/c/s)
    //   n_l  n/l 互换
    //   r    r/l、r/y（r 系）
    //   hu_f h/f（hu ↔ f）
    //   nose 前后鼻音 (en/eng、in/ing、an/ang)
    // Rules live in the bundled luna_pinyin_simp_fuzzy.schema.yaml. Empty =
    // precise spelling; all five = the bundled all-on fuzzy schema; any other
    // subset is materialised as a per-combination schema at startup (RimeIme).
    std::vector<std::string> fuzzy_groups = {"zh_z", "n_l", "r", "hu_f", "nose"};

    // Rime data directories (optional)
    std::string rime_shared_data_dir;  // System rime-data directory
    std::string rime_user_data_dir;    // User config directory

    // Display settings
    bool show_mode_indicator = true;
    std::string candidate_bar_position = "bottom";  // "bottom" or "top"

    // Logging
    std::string log_level = "warn";  // "debug", "info", "warn", "error"
    std::string log_file = "";       // empty = no file logging

    // Load from file
    static AppConfig load(const std::string& path);

    // Save to file. Returns false (and logs) on any filesystem/IO failure;
    // never throws.
    bool save(const std::string& path) const;

    // Get default config path
    static std::string default_path();

    // Convert to/from JSON
    json to_json() const;
    static AppConfig from_json(const json& j);

    // Get default languages
    static std::vector<LanguageConfig> default_languages();
};
