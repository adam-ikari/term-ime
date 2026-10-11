#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace fs = std::filesystem;

namespace {

// Every key in this file is read through the helpers below rather than through
// json::value(), which throws type_error.302 when the key is present with the
// wrong type. AppConfig::load reports any exception as "config failed to load" and
// returns an entirely default config, so one mistyped value used to cost the user
// every other setting they had made. A wrong type is now treated like an absent
// key: keep the default, and name the key in the notes so a value the program
// ignored is something the user can find instead of guess at.
// A null is the user writing "unset", so it takes the default quietly; anything
// else of the wrong type is a typo worth naming.
void note_ignored(std::vector<std::string>* ignored, const char* key, const json& value) {
    if (!ignored || value.is_null())
        return;
    if (std::find(ignored->begin(), ignored->end(), std::string(key)) == ignored->end())
        ignored->push_back(key);
}

std::string string_or(const json& j, const char* key, const std::string& fallback, std::vector<std::string>* ignored) {
    if (j.contains(key)) {
        const json& value = j.at(key);
        if (value.is_string())
            return value.get<std::string>();
        note_ignored(ignored, key, value);
    }
    return fallback;
}

bool bool_or(const json& j, const char* key, bool fallback, std::vector<std::string>* ignored) {
    if (j.contains(key)) {
        const json& value = j.at(key);
        if (value.is_boolean())
            return value.get<bool>();
        note_ignored(ignored, key, value);
    }
    return fallback;
}

// Entries are checked one by one, because an array with a single bad element is
// almost always a hand-edit of an otherwise good list.
std::vector<std::string> string_array_or(const json& j, const char* key, const std::vector<std::string>& fallback,
                                         std::vector<std::string>* ignored) {
    if (!j.contains(key))
        return fallback;
    const json& value = j.at(key);
    if (!value.is_array()) {
        note_ignored(ignored, key, value);
        return fallback;
    }
    std::vector<std::string> out;
    for (const json& element : value) {
        if (element.is_string())
            out.push_back(element.get<std::string>());
        else
            note_ignored(ignored, key, element);
    }
    return out;
}

}  // namespace

// LanguageConfig implementation
json LanguageConfig::to_json() const {
    return json{{"id", id}, {"name", name}, {"schema", schema}, {"enabled", enabled}};
}

LanguageConfig LanguageConfig::from_json(const json& j) {
    LanguageConfig cfg;
    cfg.id = string_or(j, "id", "", nullptr);
    cfg.name = string_or(j, "name", "", nullptr);
    cfg.schema = string_or(j, "schema", "", nullptr);
    cfg.enabled = bool_or(j, "enabled", true, nullptr);
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

    j["max_candidates"] = max_candidates;
    j["fuzzy_groups"] = fuzzy_groups;

    j["rime_shared_data_dir"] = rime_shared_data_dir;
    j["rime_user_data_dir"] = rime_user_data_dir;

    j["show_mode_indicator"] = show_mode_indicator;
    j["candidate_bar_position"] = candidate_bar_position;

    j["log_level"] = log_level;
    j["log_file"] = log_file;

    return j;
}

std::string AppConfig::default_shell() {
    // The login shell when the environment advertises one, else bash. Used
    // whenever the config leaves "shell" absent or empty, so running term-ime
    // from a zsh session gets zsh without any configuration.
    if (const char* env_shell = getenv("SHELL"); env_shell && *env_shell) {
        return env_shell;
    }
    return "/bin/bash";
}

AppConfig AppConfig::from_json(const json& j) {
    AppConfig cfg;
    std::vector<std::string>* ignored = &cfg.load_notes;
    // An explicit "shell" is respected even when it equals the built-in default;
    // to_json always writes the key, so a config saved by an earlier run must
    // not be re-derived from $SHELL behind the user's back.
    const std::string configured = string_or(j, "shell", "", ignored);
    cfg.shell = configured.empty() ? default_shell() : configured;

    if (j.contains("languages") && j["languages"].is_array()) {
        for (const auto& lang : j["languages"]) {
            // id selects the language and schema selects the rime input method, so
            // an entry missing either cannot be used for anything. Dropping it is
            // also safer than keeping it: LanguageManager indexes this list without
            // an emptiness check, and an unusable entry would be the active one.
            LanguageConfig entry = LanguageConfig::from_json(lang);
            if (entry.id.empty() || entry.schema.empty()) {
                note_ignored(ignored, "languages", lang);
                continue;
            }
            cfg.languages.push_back(entry);
        }
    }
    if (cfg.languages.empty()) {
        cfg.languages = default_languages();
    }

    cfg.active_language = string_or(j, "active_language", "zh-Hans", ignored);
    cfg.ui_language = string_or(j, "ui_language", "zh-CN", ignored);

    // Clamp the candidate cap to the single-digit selector keys [1,9] that
    // parse_key indexes ("123456789"); an out-of-range value is UB there. Read
    // it type-safely so a malformed value (e.g. a string) sanitizes to the
    // default instead of throwing and discarding the whole file. "page_size" is
    // the pre-rename key; configs written by earlier versions still load.
    int cap = 9;
    if (j.contains("max_candidates") && j["max_candidates"].is_number_integer()) {
        cap = j["max_candidates"].get<int>();
    } else if (j.contains("page_size") && j["page_size"].is_number_integer()) {
        cap = j["page_size"].get<int>();
    }
    cfg.max_candidates = std::max(1, std::min(9, cap));
    if (j.contains("fuzzy_groups")) {
        cfg.fuzzy_groups = string_array_or(j, "fuzzy_groups", cfg.fuzzy_groups, ignored);
    } else if (j.contains("fuzzy_pinyin")) {
        // Legacy bool key: true = all groups, false = precise spelling.
        static const std::vector<std::string> kAllFuzzy = {"zh_z", "n_l", "r", "hu_f", "nose"};
        cfg.fuzzy_groups = bool_or(j, "fuzzy_pinyin", true, ignored) ? kAllFuzzy : std::vector<std::string>{};
    }

    cfg.rime_shared_data_dir = string_or(j, "rime_shared_data_dir", "", ignored);
    cfg.rime_user_data_dir = string_or(j, "rime_user_data_dir", "", ignored);

    cfg.show_mode_indicator = bool_or(j, "show_mode_indicator", true, ignored);
    // Sanitize to the two legal positions; unknown/malformed -> "bottom", and a
    // non-string value sanitizes rather than throwing the whole config away.
    cfg.candidate_bar_position = "bottom";
    if (j.contains("candidate_bar_position") && j["candidate_bar_position"].is_string()) {
        const std::string pos = j["candidate_bar_position"].get<std::string>();
        if (pos == "top") cfg.candidate_bar_position = "top";
    }

    // Sanitize log_level to the legal set; anything else (unknown string or
    // wrong type) -> "warn" without discarding the config.
    cfg.log_level = "warn";
    if (j.contains("log_level") && j["log_level"].is_string()) {
        const std::string lvl = j["log_level"].get<std::string>();
        if (lvl == "debug" || lvl == "info" || lvl == "error") cfg.log_level = lvl;
    }
    cfg.log_file = string_or(j, "log_file", "", ignored);

    // One line, not one per key: the user needs to know which names were rejected,
    // and a stack of notes would bury the "failed to load" message that matters
    // more. load() appends its own note when the file does not parse at all.
    if (!cfg.load_notes.empty()) {
        std::string keys;
        for (const std::string& key : cfg.load_notes) {
            if (!keys.empty())
                keys += ", ";
            keys += key;
        }
        cfg.load_notes = {"ignored config key(s) with a value of the wrong type: " + keys};
    }

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
        // Same route as a parsed file, so the defaults (including the $SHELL
        // lookup) cannot drift apart from a real config.
        return from_json(json::object());
    }

    try {
        std::ifstream file(p);
        json j;
        file >> j;
        return from_json(j);
    } catch (const std::exception& e) {
        // Carried out with the result rather than logged here: a config that
        // will not parse is the one message the user must find in the log file
        // they just configured, and that destination is only chosen later.
        AppConfig fallback = from_json(json::object());
        fallback.load_notes.push_back("config " + path + " failed to load: " + e.what() +
                                      " -- running with defaults");
        return fallback;
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