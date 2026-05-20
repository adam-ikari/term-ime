#include "i18n.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

I18n::Lang I18n::current_lang_ = I18n::Lang::EN;
std::unordered_map<std::string, std::string> I18n::translations_;

void I18n::init(Lang lang) {
    current_lang_ = lang;
    load_translations(lang);
    spdlog::info("I18n initialized with language: {}", static_cast<int>(lang));
}

I18n::Lang I18n::current_lang() {
    return current_lang_;
}

void I18n::set_lang(Lang lang) {
    if (current_lang_ != lang) {
        current_lang_ = lang;
        load_translations(lang);
        spdlog::info("Language changed to: {}", static_cast<int>(lang));
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

void I18n::load_translations(Lang lang) {
    translations_.clear();

    // Define translations for each language
    switch (lang) {
        case Lang::ZH_CN:
            translations_ = {
                // Mode indicators
                {"mode.chinese", "中文"},
                {"mode.english", "EN"},
                {"mode.ai", "[AI]"},
                {"mode.ai_loading", "[AI...]"},

                // Hints
                {"hint.toggle_mode", "切换"},
                {"hint.select", "选择"},
                {"hint.cancel", "取消"},
                {"hint.ai_toggle", "AI排序"},

                // Status
                {"status.pinyin", "拼音"},
                {"status.initializing", "初始化中..."},
                {"status.loading_model", "加载模型中..."},

                // Errors
                {"error.init_failed", "初始化失败"},
                {"error.model_not_found", "模型未找到"},
                {"error.config_load_failed", "配置加载失败"},

                // Kaomoji
                {"kaomoji.happy", "开心"},
                {"kaomoji.sad", "难过"},
                {"kaomoji.love", "爱"},
                {"kaomoji.angry", "生气"},
                {"kaomoji.cute", "可爱"},
                {"kaomoji.search", "搜索颜文字"},

                // Settings
                {"settings.language", "语言"},
                {"settings.ai_ranking", "AI排序"},
                {"settings.backend", "后端"},
                {"settings.threads", "线程数"}
            };
            break;

        case Lang::ZH_TW:
            translations_ = {
                // Mode indicators
                {"mode.chinese", "中文"},
                {"mode.english", "EN"},
                {"mode.ai", "[AI]"},
                {"mode.ai_loading", "[AI...]"},

                // Hints
                {"hint.toggle_mode", "切換"},
                {"hint.select", "選擇"},
                {"hint.cancel", "取消"},
                {"hint.ai_toggle", "AI排序"},

                // Status
                {"status.pinyin", "拼音"},
                {"status.initializing", "初始化中..."},
                {"status.loading_model", "載入模型中..."},

                // Errors
                {"error.init_failed", "初始化失敗"},
                {"error.model_not_found", "模型未找到"},
                {"error.config_load_failed", "設定載入失敗"},

                // Kaomoji
                {"kaomoji.happy", "開心"},
                {"kaomoji.sad", "難過"},
                {"kaomoji.love", "愛"},
                {"kaomoji.angry", "生氣"},
                {"kaomoji.cute", "可愛"},
                {"kaomoji.search", "搜尋顏文字"},

                // Settings
                {"settings.language", "語言"},
                {"settings.ai_ranking", "AI排序"},
                {"settings.backend", "後端"},
                {"settings.threads", "執行緒數"}
            };
            break;

        case Lang::JA:
            translations_ = {
                // Mode indicators
                {"mode.chinese", "中国語"},
                {"mode.english", "EN"},
                {"mode.ai", "[AI]"},
                {"mode.ai_loading", "[AI...]"},

                // Hints
                {"hint.toggle_mode", "切替"},
                {"hint.select", "選択"},
                {"hint.cancel", "取消"},
                {"hint.ai_toggle", "AI並替"},

                // Status
                {"status.pinyin", "ピンイン"},
                {"status.initializing", "初期化中..."},
                {"status.loading_model", "モデル読込中..."},

                // Errors
                {"error.init_failed", "初期化失敗"},
                {"error.model_not_found", "モデルが見つかりません"},
                {"error.config_load_failed", "設定読込失敗"},

                // Kaomoji
                {"kaomoji.happy", "嬉しい"},
                {"kaomoji.sad", "悲しい"},
                {"kaomoji.love", "愛"},
                {"kaomoji.angry", "怒り"},
                {"kaomoji.cute", "可愛い"},
                {"kaomoji.search", "顔文字検索"},

                // Settings
                {"settings.language", "言語"},
                {"settings.ai_ranking", "AI並替"},
                {"settings.backend", "バックエンド"},
                {"settings.threads", "スレッド数"}
            };
            break;

        default: // EN
            translations_ = {
                // Mode indicators
                {"mode.chinese", "中文"},
                {"mode.english", "EN"},
                {"mode.ai", "[AI]"},
                {"mode.ai_loading", "[AI...]"},

                // Hints
                {"hint.toggle_mode", "Toggle"},
                {"hint.select", "Select"},
                {"hint.cancel", "Cancel"},
                {"hint.ai_toggle", "AI Rank"},

                // Status
                {"status.pinyin", "Pinyin"},
                {"status.initializing", "Initializing..."},
                {"status.loading_model", "Loading model..."},

                // Errors
                {"error.init_failed", "Initialization failed"},
                {"error.model_not_found", "Model not found"},
                {"error.config_load_failed", "Config load failed"},

                // Kaomoji
                {"kaomoji.happy", "Happy"},
                {"kaomoji.sad", "Sad"},
                {"kaomoji.love", "Love"},
                {"kaomoji.angry", "Angry"},
                {"kaomoji.cute", "Cute"},
                {"kaomoji.search", "Search Kaomoji"},

                // Settings
                {"settings.language", "Language"},
                {"settings.ai_ranking", "AI Ranking"},
                {"settings.backend", "Backend"},
                {"settings.threads", "Threads"}
            };
            break;
    }
}
