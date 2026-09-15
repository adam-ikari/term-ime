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

// AppConfig implementation
std::vector<LanguageConfig> AppConfig::default_languages() {
    return {{"zh-Hans", "简体中文", "luna_pinyin_simp", true}};
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
    j["max_candidates"] = max_candidates;
    j["fuzzy_pinyin"] = fuzzy_pinyin;

    j["rime_shared_data_dir"] = rime_shared_data_dir;
    j["rime_user_data_dir"] = rime_user_data_dir;

    j["show_mode_indicator"] = show_mode_indicator;
    j["candidate_bar_position"] = candidate_bar_position;

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
    // "page_size" is the pre-rename key; configs written by earlier versions
    // still load.
    cfg.max_candidates = j.value("max_candidates", j.value("page_size", 9));
    cfg.fuzzy_pinyin = j.value("fuzzy_pinyin", false);

    cfg.rime_shared_data_dir = j.value("rime_shared_data_dir", "");
    cfg.rime_user_data_dir = j.value("rime_user_data_dir", "");

    cfg.show_mode_indicator = j.value("show_mode_indicator", true);
    cfg.candidate_bar_position = j.value("candidate_bar_position", "bottom");

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

// Never throws: callers (e.g. settings close, which runs from a libuv callback)
// must not have filesystem errors escape into the event loop.
bool AppConfig::save(const std::string& path) const {
    try {
        fs::path p(path);

        // Create parent directories if needed
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }

        std::ofstream file(p);
        if (!file) {
            spdlog::error("Failed to save config: cannot open {} for writing", path);
            return false;
        }
        file << to_json().dump(4);
        if (!file) {
            spdlog::error("Failed to save config: write error on {}", path);
            return false;
        }
        spdlog::info("Saved config to: {}", path);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to save config to {}: {}", path, e.what());
        return false;
    }
}