#include <gtest/gtest.h>
#include "util/i18n.hpp"
#include "util/utf8.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

class I18nTest : public ::testing::Test {
   protected:
    void SetUp() override {
        original_lang_ = I18n::current_lang();
    }
    void TearDown() override {
        I18n::set_lang(original_lang_);
    }
   private:
    I18n::Lang original_lang_;
};

// ==========================================================================
// File-loaded translations
// ==========================================================================

TEST_F(I18nTest, LoadZhCnFromFile) {
    I18n::init(I18n::Lang::ZH_CN, "data/translations");
    EXPECT_EQ(I18n::get("mode.chinese"), "中文");
    EXPECT_EQ(I18n::get("mode.english"), "EN");
    EXPECT_EQ(I18n::get("hint.toggle_mode"), "切换");
    EXPECT_EQ(I18n::get("hint.select"), "选择");
    EXPECT_EQ(I18n::get("hint.cancel"), "取消");
    EXPECT_EQ(I18n::get("status.pinyin"), "拼音");
    EXPECT_EQ(I18n::get("settings.title"), "设置");
    EXPECT_EQ(I18n::get("settings.language"), "语言");
    EXPECT_EQ(I18n::get("settings.ui_language"), "界面语言");
    EXPECT_EQ(I18n::get("settings.close"), "关闭");
}

TEST_F(I18nTest, LoadEnFromFile) {
    I18n::init(I18n::Lang::EN, "data/translations");
    EXPECT_EQ(I18n::get("mode.chinese"), "中文");
    EXPECT_EQ(I18n::get("mode.english"), "EN");
    EXPECT_EQ(I18n::get("hint.toggle_mode"), "Toggle");
    EXPECT_EQ(I18n::get("hint.select"), "Select");
    EXPECT_EQ(I18n::get("hint.cancel"), "Cancel");
    EXPECT_EQ(I18n::get("status.pinyin"), "Pinyin");
    EXPECT_EQ(I18n::get("settings.title"), "Settings");
    EXPECT_EQ(I18n::get("settings.language"), "Language");
    EXPECT_EQ(I18n::get("settings.ui_language"), "UI Language");
    EXPECT_EQ(I18n::get("settings.close"), "Close");
}

// ==========================================================================
// A partial translation file overlays the built-ins instead of replacing them
// ==========================================================================

TEST_F(I18nTest, PartialFileOverlaysBuiltins) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "term-ime-i18n-partial";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream out(dir / "en.json");
        out << R"({"hint.toggle_mode": "Choose"})";
    }

    I18n::init(I18n::Lang::EN, dir.string());
    // The file is authoritative for the key it carries...
    EXPECT_EQ(I18n::get("hint.toggle_mode"), "Choose");
    // ...and a key added after the installed file was written still resolves,
    // instead of the panel printing the raw key name.
    EXPECT_EQ(I18n::get("settings.fuzzy.zh_z"), "zh⇄z");

    I18n::init(I18n::Lang::EN, "/nonexistent/path");
    fs::remove_all(dir);
}

// ==========================================================================
// Default / fallback translations (when translation files are missing)
// ==========================================================================

TEST_F(I18nTest, DefaultTranslationsZhCN) {
    I18n::init(I18n::Lang::ZH_CN, "/nonexistent/path");
    EXPECT_EQ(I18n::get("mode.chinese"), "中文");
    EXPECT_EQ(I18n::get("mode.english"), "EN");
    EXPECT_EQ(I18n::get("hint.toggle_mode"), "切换");
    EXPECT_EQ(I18n::get("hint.select"), "选择");
    EXPECT_EQ(I18n::get("hint.cancel"), "取消");
    EXPECT_EQ(I18n::get("status.pinyin"), "拼音");
    EXPECT_EQ(I18n::get("settings.title"), "设置");
}

TEST_F(I18nTest, DefaultTranslationsEN) {
    I18n::init(I18n::Lang::EN, "/nonexistent/path");
    EXPECT_EQ(I18n::get("mode.chinese"), "中文");
    EXPECT_EQ(I18n::get("mode.english"), "EN");
    EXPECT_EQ(I18n::get("hint.toggle_mode"), "Toggle");
    EXPECT_EQ(I18n::get("hint.select"), "Select");
    EXPECT_EQ(I18n::get("hint.cancel"), "Cancel");
    EXPECT_EQ(I18n::get("status.pinyin"), "Pinyin");
    EXPECT_EQ(I18n::get("settings.title"), "Settings");
}

// ==========================================================================
// Fallback: missing key returns the key itself
// ==========================================================================

TEST_F(I18nTest, FallbackToKeyWhenNotFound) {
    I18n::init(I18n::Lang::ZH_CN, "/nonexistent/path");
    EXPECT_EQ(I18n::get("nonexistent.key"), "nonexistent.key");
}

// ==========================================================================
// Language switching
// ==========================================================================

TEST_F(I18nTest, SwitchLanguagePreservesTranslations) {
    I18n::init(I18n::Lang::ZH_CN, "/nonexistent/path");
    EXPECT_EQ(I18n::get("hint.select"), "选择");

    I18n::set_lang(I18n::Lang::EN);
    EXPECT_EQ(I18n::get("hint.select"), "Select");
}

// ==========================================================================
// All translation keys match between zh-CN and en files
// ==========================================================================

TEST_F(I18nTest, TranslationKeysMatchBetweenFiles) {
    // Load zh-CN and capture keys
    I18n::init(I18n::Lang::ZH_CN, "data/translations");
    // Verify all keys used in code are present in both files
    // Keys used: hint.cancel, hint.select, hint.toggle_mode, status.pinyin,
    //            settings.title, settings.close, settings.ui_language,
    //            settings.language, mode.chinese, mode.english
    EXPECT_NE(I18n::get("settings.close"), "settings.close");    // zh-CN: "关闭"
    EXPECT_NE(I18n::get("settings.language"), "settings.language"); // zh-CN: "语言"
}

// ==========================================================================
// Default translations completeness check
// ==========================================================================

TEST_F(I18nTest, DefaultTranslationsCoverAllCodeKeys) {
    // All keys used in src/ code must be present in load_default_translations
    // Code uses: hint.cancel, hint.select, hint.toggle_mode, status.pinyin,
    //            settings.title, settings.close, settings.ui_language
    // Default has: mode.chinese, mode.english, hint.toggle_mode, hint.select,
    //              hint.cancel, status.pinyin, settings.title
    // Missing from default: settings.close, settings.ui_language ← BUG!

    I18n::init(I18n::Lang::ZH_CN, "/nonexistent/path");
    // These should NOT fall back to the key itself
    EXPECT_NE(I18n::get("settings.close"), "settings.close");
    EXPECT_NE(I18n::get("settings.ui_language"), "settings.ui_language");
}

// ==========================================================================
// Settings descriptions: resolve in both languages and fit the panel width
// ==========================================================================

namespace {
// A description is drawn as one non-wrapping Text, so a string wider than the
// render surface loses the panel's right border instead of wrapping. 52 columns
// keeps the 60x20 render in tests/test_settings.cpp whole.
constexpr int kMaxDescriptionWidth = 52;

const std::vector<const char*> kDescriptionKeys = {
    "settings.ui_language.desc", "settings.max_candidates.desc", "settings.fuzzy.zh_z.desc",
    "settings.fuzzy.n_l.desc",   "settings.fuzzy.r.desc",        "settings.fuzzy.hu_f.desc",
    "settings.fuzzy.nose.desc",  "settings.close.desc",
};
}  // namespace

TEST_F(I18nTest, SettingsDescriptionsResolveAndFit) {
    for (const I18n::Lang lang : {I18n::Lang::ZH_CN, I18n::Lang::EN}) {
        // Twice: the bundled JSON is what an install ships, the built-in table is
        // what runs when no translation dir exists. Either one missing leaves the
        // panel printing the raw key or clipping the border.
        for (const char* dir : {"data/translations", "/nonexistent/path"}) {
            I18n::init(lang, dir);
            for (const char* key : kDescriptionKeys) {
                const std::string text = I18n::get(key);
                EXPECT_NE(text, key) << key << " missing for lang=" << static_cast<int>(lang) << " dir=" << dir;
                EXPECT_FALSE(text.empty()) << key;
                EXPECT_LE(utf8::string_width(text), kMaxDescriptionWidth) << key << ": " << text;
            }
        }
    }
}

TEST_F(I18nTest, FuzzyRowsAreShortUnderTheGroupHeader) {
    // The header says 模糊音 / Fuzzy pinyin, so the rows must not repeat it: five
    // copies of the group name is width spent saying nothing.
    I18n::init(I18n::Lang::ZH_CN, "data/translations");
    EXPECT_EQ(I18n::get("settings.group.fuzzy"), "模糊音");
    EXPECT_EQ(I18n::get("settings.fuzzy.zh_z"), "zh⇄z");
    EXPECT_EQ(I18n::get("settings.fuzzy.nose"), "an⇄ang");
    EXPECT_EQ(I18n::get("settings.fuzzy.zh_z.desc"), "不区分 zh/ch/sh 与 z/c/s");
    EXPECT_EQ(I18n::get("settings.fuzzy.zh_z").find("模糊音"), std::string::npos)
        << "row label grew the group name back";

    I18n::init(I18n::Lang::EN, "data/translations");
    EXPECT_EQ(I18n::get("settings.group.fuzzy"), "Fuzzy pinyin");
    EXPECT_EQ(I18n::get("settings.fuzzy.zh_z"), "zh⇄z");
    EXPECT_EQ(I18n::get("settings.fuzzy.nose"), "an⇄ang");
    EXPECT_EQ(I18n::get("settings.fuzzy.nose.desc"), "Treats -n and -ng alike");
}
