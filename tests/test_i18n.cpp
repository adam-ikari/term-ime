#include <gtest/gtest.h>
#include "util/i18n.hpp"

class I18nTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Save current state
        original_lang_ = I18n::current_lang();
    }

    void TearDown() override {
        // Restore original state
        I18n::set_lang(original_lang_);
    }

   private:
    I18n::Lang original_lang_;
};

TEST_F(I18nTest, DefaultTranslationsZhCN) {
    I18n::init(I18n::Lang::ZH_CN);

    // Test existing keys
    EXPECT_EQ(I18n::get("mode.chinese"), "中文");
    EXPECT_EQ(I18n::get("mode.english"), "EN");
    EXPECT_EQ(I18n::get("hint.toggle_mode"), "切换");
    EXPECT_EQ(I18n::get("hint.select"), "选择");
    EXPECT_EQ(I18n::get("hint.cancel"), "取消");
    EXPECT_EQ(I18n::get("status.pinyin"), "拼音");

    // Test settings.title (the missing key that caused the bug)
    EXPECT_EQ(I18n::get("settings.title"), "设置");
}

TEST_F(I18nTest, DefaultTranslationsEN) {
    I18n::init(I18n::Lang::EN);

    // Test existing keys
    EXPECT_EQ(I18n::get("mode.chinese"), "中文");
    EXPECT_EQ(I18n::get("mode.english"), "EN");
    EXPECT_EQ(I18n::get("hint.toggle_mode"), "Toggle");
    EXPECT_EQ(I18n::get("hint.select"), "Select");
    EXPECT_EQ(I18n::get("hint.cancel"), "Cancel");
    EXPECT_EQ(I18n::get("status.pinyin"), "Pinyin");

    // Test settings.title (the missing key that caused the bug)
    EXPECT_EQ(I18n::get("settings.title"), "Settings");
}

TEST_F(I18nTest, FallbackToKeyWhenNotFound) {
    I18n::init(I18n::Lang::ZH_CN);

    // When key is not found, should return the key itself
    EXPECT_EQ(I18n::get("nonexistent.key"), "nonexistent.key");
}
