#include <gtest/gtest.h>
#include "core/config.hpp"

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(ConfigTest, DefaultConfig) {
    AppConfig config;
    EXPECT_EQ(config.shell, "/bin/bash");
    EXPECT_EQ(config.page_size, 5);
    EXPECT_EQ(config.log_level, "warn");
    EXPECT_TRUE(config.show_mode_indicator);
}

TEST_F(ConfigTest, DefaultLanguages) {
    auto languages = AppConfig::default_languages();
    EXPECT_FALSE(languages.empty());

    // Should have Chinese (simplified is first)
    bool has_chinese = false;
    for (const auto& lang : languages) {
        if (lang.id == "zh-CN-simp") {
            has_chinese = true;
            EXPECT_EQ(lang.name, "简体中文");
            EXPECT_TRUE(lang.enabled);
        }
    }
    EXPECT_TRUE(has_chinese);
}

TEST_F(ConfigTest, LanguageConfigToJson) {
    LanguageConfig lang;
    lang.id = "zh-CN";
    lang.name = "中文";
    lang.schema = "luna_pinyin";
    lang.enabled = true;

    json j = lang.to_json();
    EXPECT_EQ(j["id"], "zh-CN");
    EXPECT_EQ(j["name"], "中文");
    EXPECT_EQ(j["schema"], "luna_pinyin");
    EXPECT_EQ(j["enabled"], true);
}

TEST_F(ConfigTest, LanguageConfigFromJson) {
    json j = {
        {"id", "ja"},
        {"name", "日本語"},
        {"schema", "kana"},
        {"enabled", false}
    };

    LanguageConfig lang = LanguageConfig::from_json(j);
    EXPECT_EQ(lang.id, "ja");
    EXPECT_EQ(lang.name, "日本語");
    EXPECT_EQ(lang.schema, "kana");
    EXPECT_FALSE(lang.enabled);
}

TEST_F(ConfigTest, NeuralRankerConfigDefault) {
    NeuralRankerConfig config;
    EXPECT_FALSE(config.enabled);
    EXPECT_EQ(config.max_candidates, 10);
    EXPECT_FLOAT_EQ(config.threshold, 0.5f);
}

TEST_F(ConfigTest, AppConfigToJson) {
    AppConfig config;
    config.shell = "/bin/zsh";
    config.page_size = 10;
    config.log_level = "debug";

    json j = config.to_json();
    EXPECT_EQ(j["shell"], "/bin/zsh");
    EXPECT_EQ(j["page_size"], 10);
    EXPECT_EQ(j["log_level"], "debug");
}
