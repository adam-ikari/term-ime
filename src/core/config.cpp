#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

// LanguageConfig implementation
json LanguageConfig::to_json() const {
    return json{{"id", id}, {"name", name}, {"schema", schema}, {"enabled", enabled}};
}

LanguageConfig LanguageConfig::from_json(const json& j) {
    LanguageConfig cfg;
    cfg.id = j.value("id", "");
    cfg.name = j.value("name", "");
    cfg.schema = j.value("schema", "");
    cfg.enabled = j.value("enabled", true);
    return cfg;
}

// LlamaRankerConfig implementation
json LlamaRankerConfig::to_json() const {
    return json{{"enabled", enabled},          {"model_path", model_path}, {"n_threads", n_threads},
                {"max_tokens", max_tokens},    {"timeout_ms", timeout_ms}, {"backend", backend},
                {"n_gpu_layers", n_gpu_layers}};
}

LlamaRankerConfig LlamaRankerConfig::from_json(const json& j) {
    LlamaRankerConfig cfg;
    cfg.enabled = j.value("enabled", false);
    cfg.model_path = j.value("model_path", "");
    cfg.n_threads = j.value("n_threads", 2);
    cfg.max_tokens = j.value("max_tokens", 10);
    cfg.timeout_ms = j.value("timeout_ms", 100);
    cfg.backend = j.value("backend", "cpu");
    cfg.n_gpu_layers = j.value("n_gpu_layers", 0);
    return cfg;
}

// AppConfig implementation
std::vector<LanguageConfig> AppConfig::default_languages() {
    return {{"zh-Hans", "简体中文", "luna_pinyin_simp", true},
            {"zh-Hant", "繁體中文", "luna_pinyin", true},
            {"zh-Hant-TW", "正體中文（台灣）", "terra_pinyin", false}};
}

json AppConfig::to_json() const {
    json j;
    j["shell"] = shell;

    json langs = json::array();
    for (const auto& lang : languages) {
        langs.push_back(lang.to_json());
    }
    j["languages"] = langs;
    j["active_language"] = active_language;
    j["ui_language"] = ui_language;

    j["dict_path"] = dict_path;
    j["extra_dicts"] = extra_dicts;
    j["page_size"] = page_size;

    j["rime_shared_data_dir"] = rime_shared_data_dir;
    j["rime_user_data_dir"] = rime_user_data_dir;

    j["show_mode_indicator"] = show_mode_indicator;
    j["candidate_bar_position"] = candidate_bar_position;

    j["llama_ranker"] = llama_ranker.to_json();

    j["log_level"] = log_level;
    j["log_file"] = log_file;

    return j;
}

AppConfig AppConfig::from_json(const json& j) {
    AppConfig cfg;
    cfg.shell = j.value("shell", "/bin/bash");

    if (j.contains("languages") && j["languages"].is_array()) {
        for (const auto& lang : j["languages"]) {
            cfg.languages.push_back(LanguageConfig::from_json(lang));
        }
    }
    if (cfg.languages.empty()) {
        cfg.languages = default_languages();
    }

    cfg.active_language = j.value("active_language", "zh-Hans");
    cfg.ui_language = j.value("ui_language", "zh-CN");

    cfg.dict_path = j.value("dict_path", "data/pinyin.dict");
    cfg.extra_dicts = j.value("extra_dicts", std::vector<std::string>{});
    cfg.page_size = j.value("page_size", 5);

    cfg.rime_shared_data_dir = j.value("rime_shared_data_dir", "");
    cfg.rime_user_data_dir = j.value("rime_user_data_dir", "");

    cfg.show_mode_indicator = j.value("show_mode_indicator", true);
    cfg.candidate_bar_position = j.value("candidate_bar_position", "bottom");

    if (j.contains("llama_ranker")) {
        cfg.llama_ranker = LlamaRankerConfig::from_json(j["llama_ranker"]);
    }

    cfg.log_level = j.value("log_level", "warn");
    cfg.log_file = j.value("log_file", "");

    return cfg;
}

std::string AppConfig::default_path() {
    // Try XDG_CONFIG_HOME first
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        return fs::path(xdg_config) / "term-ime" / "config.json";
    }
    // Fallback to HOME/.config
    const char* home = getenv("HOME");
    if (home) {
        return fs::path(home) / ".config" / "term-ime" / "config.json";
    }
    return "config.json";
}

AppConfig AppConfig::load(const std::string& path) {
    fs::path p(path);

    if (!fs::exists(p)) {
        spdlog::info("Config file not found: {}, using defaults", path);
        AppConfig cfg;
        cfg.languages = default_languages();
        return cfg;
    }

    try {
        std::ifstream file(p);
        json j;
        file >> j;
        spdlog::info("Loaded config from: {}", path);
        return from_json(j);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load config: {}", e.what());
        AppConfig cfg;
        cfg.languages = default_languages();
        return cfg;
    }
}

void AppConfig::save(const std::string& path) const {
    fs::path p(path);

    // Create parent directories if needed
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    try {
        std::ofstream file(p);
        file << to_json().dump(4);
        spdlog::info("Saved config to: {}", path);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config: {}", e.what());
    }
}