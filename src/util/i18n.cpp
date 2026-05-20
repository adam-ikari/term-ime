#include "i18n.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

I18n::Lang I18n::current_lang_ = I18n::Lang::EN;
std::unordered_map<std::string, std::string> I18n::translations_;
std::filesystem::path I18n::translations_path_;

void I18n::init(Lang lang) {
    translations_path_ = get_default_translations_path();
    current_lang_ = lang;
    if (!load_translations(lang)) {
        load_default_translations(lang);
    }
    spdlog::info("I18n initialized with language: {} (path: {})",
                 lang_code(lang), translations_path_.string());
}

void I18n::init(Lang lang, const std::string& translations_path) {
    translations_path_ = translations_path;
    current_lang_ = lang;
    if (!load_translations(lang)) {
        load_default_translations(lang);
        spdlog::warn("Failed to load translations from {}, using defaults",
                     translations_path);
    }
    spdlog::info("I18n initialized with language: {}", lang_code(lang));
}

I18n::Lang I18n::current_lang() {
    return current_lang_;
}

void I18n::set_lang(Lang lang) {
    if (current_lang_ != lang) {
        current_lang_ = lang;
        if (!load_translations(lang)) {
            load_default_translations(lang);
        }
        spdlog::info("Language changed to: {}", lang_code(lang));
    }
}

const std::string& I18n::get(const std::string& key) {
    static const std::string empty;
    auto it = translations_.find(key);
    if (it != translations_.end()) {
        return it->second;
    }
    // Return key itself if not found (fallback)
    return key;
}

std::vector<std::pair<I18n::Lang, std::string>> I18n::available_languages() {
    return {
        {Lang::EN, "English"},
        {Lang::ZH_CN, "简体中文"},
        {Lang::ZH_TW, "繁體中文"},
        {Lang::JA, "日本語"}
    };
}

std::string I18n::lang_code(Lang lang) {
    switch (lang) {
        case Lang::ZH_CN: return "zh-CN";
        case Lang::ZH_TW: return "zh-TW";
        case Lang::JA: return "ja";
        default: return "en";
    }
}

I18n::Lang I18n::parse_lang(const std::string& code) {
    if (code == "zh-CN" || code == "zh-Hans") return Lang::ZH_CN;
    if (code == "zh-TW" || code == "zh-Hant") return Lang::ZH_TW;
    if (code == "ja") return Lang::JA;
    return Lang::EN;
}

std::filesystem::path I18n::get_default_translations_path() {
    // Try XDG_CONFIG_HOME first
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        return std::filesystem::path(xdg_config) / "term-ime" / "translations";
    }
    // Fallback to HOME/.config
    const char* home = getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config" / "term-ime" / "translations";
    }
    return std::filesystem::path("translations");
}

bool I18n::load_translations(Lang lang) {
    std::string filename = lang_code(lang) + ".json";
    std::filesystem::path file_path = translations_path_ / filename;

    if (!std::filesystem::exists(file_path)) {
        spdlog::debug("Translation file not found: {}", file_path.string());
        return false;
    }

    try {
        std::ifstream file(file_path);
        json j;
        file >> j;

        translations_.clear();
        for (auto& [key, value] : j.items()) {
            if (value.is_string()) {
                translations_[key] = value.get<std::string>();
            }
        }

        spdlog::info("Loaded {} translations from {}", translations_.size(), file_path.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load translations: {}", e.what());
        return false;
    }
}

void I18n::load_default_translations(Lang lang) {
    translations_.clear();

    // Minimal fallback translations
    switch (lang) {
        case Lang::ZH_CN:
            translations_ = {
                {"mode.chinese", "中文"},
                {"mode.english", "EN"},
                {"hint.toggle_mode", "切换"},
                {"hint.select", "选择"},
                {"hint.cancel", "取消"},
                {"hint.ai_toggle", "AI排序"},
                {"status.pinyin", "拼音"},
            };
            break;

        case Lang::ZH_TW:
            translations_ = {
                {"mode.chinese", "中文"},
                {"mode.english", "EN"},
                {"hint.toggle_mode", "切換"},
                {"hint.select", "選擇"},
                {"hint.cancel", "取消"},
                {"hint.ai_toggle", "AI排序"},
                {"status.pinyin", "拼音"},
            };
            break;

        case Lang::JA:
            translations_ = {
                {"mode.chinese", "中国語"},
                {"mode.english", "EN"},
                {"hint.toggle_mode", "切替"},
                {"hint.select", "選択"},
                {"hint.cancel", "取消"},
                {"hint.ai_toggle", "AI並替"},
                {"status.pinyin", "ピンイン"},
            };
            break;

        default:
            translations_ = {
                {"mode.chinese", "中文"},
                {"mode.english", "EN"},
                {"hint.toggle_mode", "Toggle"},
                {"hint.select", "Select"},
                {"hint.cancel", "Cancel"},
                {"hint.ai_toggle", "AI Rank"},
                {"status.pinyin", "Pinyin"},
            };
            break;
    }

    spdlog::info("Using default translations for {}", lang_code(lang));
}