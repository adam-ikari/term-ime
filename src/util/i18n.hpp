#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Internationalization support
class I18n {
public:
    // Supported languages
    enum class Lang {
        EN,     // English
        ZH_CN,  // Simplified Chinese
        ZH_TW,  // Traditional Chinese
        JA      // Japanese
    };

    // Initialize with language
    static void init(Lang lang);

    // Get current language
    static Lang current_lang();

    // Set language
    static void set_lang(Lang lang);

    // Get translated string
    static const std::string& get(const std::string& key);

    // Shorthand for get
    static const std::string& t(const std::string& key) { return get(key); }

    // Get available languages
    static std::vector<std::pair<Lang, std::string>> available_languages();

private:
    static Lang current_lang_;
    static std::unordered_map<std::string, std::string> translations_;

    static void load_translations(Lang lang);
};

// Convenience macro for translation
#define TR(key) I18n::t(key)
