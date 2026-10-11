#include <gtest/gtest.h>
#include "core/config.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>

class ConfigTest : public ::testing::Test {
   protected:
    void SetUp() override {}
};

TEST_F(ConfigTest, DefaultConfig) {
    AppConfig config;
    EXPECT_EQ(config.shell, "/bin/bash");
    EXPECT_EQ(config.max_candidates, 9);
    EXPECT_EQ(config.log_level, "warn");
    EXPECT_TRUE(config.show_mode_indicator);
}

TEST_F(ConfigTest, DefaultLanguages) {
    auto languages = AppConfig::default_languages();
    EXPECT_FALSE(languages.empty());

    // Should have Chinese (simplified is first)
    bool has_chinese = false;
    for (const auto& lang : languages) {
        if (lang.id == "zh-Hans") {
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
    json j = {{"id", "zh-Hans"}, {"name", "简体中文"}, {"schema", "luna_pinyin_simp"}, {"enabled", true}};

    LanguageConfig lang = LanguageConfig::from_json(j);
    EXPECT_EQ(lang.id, "zh-Hans");
    EXPECT_EQ(lang.name, "简体中文");
    EXPECT_EQ(lang.schema, "luna_pinyin_simp");
    EXPECT_TRUE(lang.enabled);
}

TEST_F(ConfigTest, AppConfigToJson) {
    AppConfig config;
    config.shell = "/bin/zsh";
    config.max_candidates = 8;
    config.log_level = "debug";

    json j = config.to_json();
    EXPECT_EQ(j["shell"], "/bin/zsh");
    EXPECT_EQ(j["max_candidates"], 8);
    EXPECT_EQ(j["log_level"], "debug");
}

TEST_F(ConfigTest, RimeDataDirs) {
    AppConfig config;
    EXPECT_TRUE(config.rime_shared_data_dir.empty());
    EXPECT_TRUE(config.rime_user_data_dir.empty());

    config.rime_shared_data_dir = "/custom/rime-data";
    json j = config.to_json();
    EXPECT_EQ(j["rime_shared_data_dir"], "/custom/rime-data");
}

// The candidate cap is user-configurable, and configs written before the
// rename (key "page_size") must keep working.
TEST_F(ConfigTest, MaxCandidatesLoadsAndLegacyKeyStillWorks) {
    const std::string path = "/tmp/term-ime-test-config.json";
    {
        std::ofstream out(path);
        out << R"({"max_candidates": 3})";
    }
    EXPECT_EQ(AppConfig::load(path).max_candidates, 3);
    {
        std::ofstream out(path);
        out << R"({"page_size": 2})";
    }
    EXPECT_EQ(AppConfig::load(path).max_candidates, 2);
    std::remove(path.c_str());
}

TEST_F(ConfigTest, FuzzyGroupsRoundTrip) {
    AppConfig config;
    EXPECT_EQ(config.fuzzy_groups.size(), 5u);  // all groups on by default
    config.fuzzy_groups = {"n_l", "zh_z"};
    EXPECT_EQ(config.to_json()["fuzzy_groups"].size(), 2u);

    const std::string path = "/tmp/term-ime-test-fuzzy.json";
    {
        std::ofstream out(path);
        out << config.to_json().dump();
    }
    AppConfig loaded = AppConfig::load(path);
    ASSERT_EQ(loaded.fuzzy_groups.size(), 2u);
    EXPECT_EQ(loaded.fuzzy_groups[0], "n_l");
    EXPECT_EQ(loaded.fuzzy_groups[1], "zh_z");
    std::remove(path.c_str());
}

TEST_F(ConfigTest, LegacyFuzzyPinyinBoolStillLoads) {
    // A config written by the pre-group version ("fuzzy_pinyin": false) must
    // map to precise spelling instead of dropping the file or defaulting on.
    const std::string path = "/tmp/term-ime-test-fuzzy-legacy.json";
    {
        std::ofstream out(path);
        out << "{\"fuzzy_pinyin\": false}\n";
    }
    AppConfig loaded = AppConfig::load(path);
    EXPECT_TRUE(loaded.fuzzy_groups.empty());
    std::remove(path.c_str());
}

// M1: config is never trusted blindly. An out-of-range cap is clamped to the
// single-digit selector range [1,9], and a malformed (non-integer) cap falls
// back to the default WITHOUT discarding the rest of the file (previously a bad
// type threw and reset everything to defaults).
TEST_F(ConfigTest, MaxCandidatesIsClampedAndTypeSafe) {
    const std::string path = "/tmp/term-ime-test-clamp.json";
    auto load_cap = [&](const std::string& body) {
        {
            std::ofstream out(path);
            out << body;
        }
        return AppConfig::load(path);
    };

    EXPECT_EQ(load_cap(R"({"max_candidates": 20})").max_candidates, 9);
    EXPECT_EQ(load_cap(R"({"max_candidates": 0})").max_candidates, 1);
    EXPECT_EQ(load_cap(R"({"max_candidates": -5})").max_candidates, 1);
    EXPECT_EQ(load_cap(R"({"page_size": 99})").max_candidates, 9);  // legacy key too
    EXPECT_EQ(load_cap(R"({"max_candidates": 3})").max_candidates, 3);  // in range kept

    // A string cap is malformed: default to 9, and the sibling field survives.
    AppConfig survived = load_cap(R"({"max_candidates": "lots", "shell": "/bin/zsh"})");
    EXPECT_EQ(survived.max_candidates, 9);
    EXPECT_EQ(survived.shell, "/bin/zsh");

    std::remove(path.c_str());
}

// The M1 pass above sanitized three keys and left the rest on json::value(), which
// throws type_error.302 on a type mismatch — and load() reports that as "the config
// failed to load", replacing the FILE with defaults. So a single mistyped value cost
// the user every setting they had actually made. This is the shape of that bug, with
// the exact body that reproduced it: shell and fuzzy_groups were both correct in the
// file and both disappeared.
TEST_F(ConfigTest, OneWrongTypedKeyDoesNotDiscardTheFile) {
    const std::string path = "/tmp/term-ime-test-types.json";
    auto load_body = [&](const std::string& body) {
        {
            std::ofstream out(path);
            out << body;
        }
        return AppConfig::load(path);
    };

    AppConfig typed = load_body(R"({"ui_language": 7, "shell": "/bin/zsh", "fuzzy_groups": ["zh_z", "r"]})");
    EXPECT_EQ(typed.ui_language, "zh-CN");  // bad type -> default
    EXPECT_EQ(typed.shell, "/bin/zsh");     // the siblings survive
    ASSERT_EQ(typed.fuzzy_groups.size(), 2u);
    EXPECT_EQ(typed.fuzzy_groups[0], "zh_z");
    ASSERT_EQ(typed.load_notes.size(), 1u);
    EXPECT_NE(typed.load_notes[0].find("ui_language"), std::string::npos);
    EXPECT_EQ(typed.take_load_notes().size(), 1u);

    // Booleans and paths, which have no enum validation to hide behind.
    AppConfig misc = load_body(
        R"({"show_mode_indicator": "yes", "log_file": [1], "rime_user_data_dir": 3, "shell": "/bin/dash"})");
    EXPECT_TRUE(misc.show_mode_indicator);
    EXPECT_EQ(misc.log_file, "");
    EXPECT_EQ(misc.rime_user_data_dir, "");
    EXPECT_EQ(misc.shell, "/bin/dash");

    // The legacy boolean key has the same failure mode.
    AppConfig legacy = load_body(R"({"fuzzy_pinyin": "auto", "shell": "/bin/dash"})");
    EXPECT_EQ(legacy.fuzzy_groups.size(), 5u);  // default is all groups on
    EXPECT_EQ(legacy.shell, "/bin/dash");

    // One bad element in an array must not throw away the elements beside it.
    AppConfig partial = load_body(R"({"fuzzy_groups": ["zh_z", 5, "n_l"]})");
    ASSERT_EQ(partial.fuzzy_groups.size(), 2u);
    EXPECT_EQ(partial.fuzzy_groups[0], "zh_z");
    EXPECT_EQ(partial.fuzzy_groups[1], "n_l");

    // A language entry is only usable with both an id (what selects it) and a
    // schema (what rime loads for it); LanguageManager indexes the list without
    // checking, so an unusable entry is dropped rather than kept and made active.
    AppConfig langs = load_body(
        R"({"languages": [{"id": "zh-Hans", "schema": "luna_pinyin_simp"}, {"id": 7, "schema": "x"}, {"id": "en"}]})");
    ASSERT_EQ(langs.languages.size(), 1u);
    EXPECT_EQ(langs.languages[0].id, "zh-Hans");

    // All entries unusable still has to leave a loadable list, and a file whose top
    // level is not an object has no keys to read rather than a parse error.
    AppConfig empty = load_body(R"({"languages": [{"schema": "x"}]})");
    ASSERT_EQ(empty.languages.size(), 1u);
    EXPECT_EQ(empty.languages[0].id, "zh-Hans");
    AppConfig scalar = load_body("[1, 2, 3]");
    EXPECT_EQ(scalar.shell, AppConfig::default_shell());
    EXPECT_EQ(scalar.max_candidates, 9);

    std::remove(path.c_str());
}

// A config that cannot be parsed at all is the one diagnostic that must reach
// the log file the *user* configured, not whichever logger happened to exist
// while loading; so the loader hands the note back instead of logging it.
TEST_F(ConfigTest, UnparseableConfigReportsThroughTheResult) {
    const std::string path = "/tmp/term-ime-test-broken.json";
    {
        std::ofstream out(path);
        out << R"({"shell": "/bin/zsh",)";  // truncated: not valid JSON
    }
    AppConfig broken = AppConfig::load(path);
    EXPECT_EQ(broken.shell, AppConfig::default_shell());  // fell back to defaults
    ASSERT_EQ(broken.load_notes.size(), 1u);
    EXPECT_NE(broken.load_notes[0].find("failed to load"), std::string::npos);

    // take_load_notes() drains, so a replay cannot repeat on every consumer.
    EXPECT_EQ(broken.take_load_notes().size(), 1u);
    EXPECT_TRUE(broken.load_notes.empty());

    {
        std::ofstream out(path);
        out << R"({"shell": "/bin/zsh"})";
    }
    EXPECT_TRUE(AppConfig::load(path).load_notes.empty());  // healthy file, no notes
    std::remove(path.c_str());
}

// The shell comes from the config when configured, else from $SHELL, else from
// /bin/bash. An explicit "/bin/bash" must survive a different $SHELL: the
// member default used to be indistinguishable from an absent key, so $SHELL
// overrode whatever the user had written.
TEST_F(ConfigTest, ShellResolutionPrefersExplicitValue) {
    const char* saved = getenv("SHELL");
    const std::string restore = saved ? saved : "";

    const std::string path = "/tmp/term-ime-test-shell.json";
    auto load = [&](const std::string& body) {
        {
            std::ofstream out(path);
            out << body;
        }
        return AppConfig::load(path);
    };

    setenv("SHELL", "/bin/zsh", 1);
    EXPECT_EQ(load(R"({"shell": "/bin/bash"})").shell, "/bin/bash");
    EXPECT_EQ(load(R"({"shell": "/usr/bin/fish"})").shell, "/usr/bin/fish");
    EXPECT_EQ(load(R"({})").shell, "/bin/zsh");             // $SHELL fills in
    EXPECT_EQ(load(R"({"shell": ""})").shell, "/bin/zsh");  // blank falls back too

    unsetenv("SHELL");
    EXPECT_EQ(load(R"({})").shell, "/bin/bash");

    std::remove(path.c_str());
    if (!restore.empty()) setenv("SHELL", restore.c_str(), 1);
}

// M1: log_level is one of debug/info/warn/error. An unknown string or a
// non-string value falls back to the default WITHOUT discarding the rest of
// the file.
TEST_F(ConfigTest, LogLevelIsSanitized) {
    const std::string path = "/tmp/term-ime-test-loglevel.json";
    auto load = [&](const std::string& body) {
        {
            std::ofstream out(path);
            out << body;
        }
        return AppConfig::load(path);
    };

    EXPECT_EQ(load(R"({"log_level": "debug"})").log_level, "debug");
    EXPECT_EQ(load(R"({"log_level": "info"})").log_level, "info");
    EXPECT_EQ(load(R"({"log_level": "warn"})").log_level, "warn");
    EXPECT_EQ(load(R"({"log_level": "error"})").log_level, "error");
    EXPECT_EQ(load(R"({"log_level": "trace"})").log_level, "warn");   // unknown -> default
    EXPECT_EQ(load(R"({"log_level": ""})").log_level, "warn");         // blank -> default
    EXPECT_EQ(load(R"({"log_level": 5})").log_level, "warn");          // wrong type -> default
    EXPECT_EQ(load(R"({"log_level": null})").log_level, "warn");

    // A malformed level must not discard a sibling field.
    AppConfig survived = load(R"({"log_level": "chatty", "shell": "/bin/zsh"})");
    EXPECT_EQ(survived.log_level, "warn");
    EXPECT_EQ(survived.shell, "/bin/zsh");

    std::remove(path.c_str());
}

// M1: candidate_bar_position is "bottom" or "top". Anything else falls back to
// the default WITHOUT discarding the rest of the file.
TEST_F(ConfigTest, CandidateBarPositionIsSanitized) {
    const std::string path = "/tmp/term-ime-test-barpos.json";
    auto load = [&](const std::string& body) {
        {
            std::ofstream out(path);
            out << body;
        }
        return AppConfig::load(path);
    };

    EXPECT_EQ(load(R"({"candidate_bar_position": "bottom"})").candidate_bar_position, "bottom");
    EXPECT_EQ(load(R"({"candidate_bar_position": "top"})").candidate_bar_position, "top");
    EXPECT_EQ(load(R"({"candidate_bar_position": "middle"})").candidate_bar_position, "bottom");
    EXPECT_EQ(load(R"({"candidate_bar_position": ""})").candidate_bar_position, "bottom");
    EXPECT_EQ(load(R"({"candidate_bar_position": 3})").candidate_bar_position, "bottom");

    AppConfig survived = load(R"({"candidate_bar_position": "side", "shell": "/bin/zsh"})");
    EXPECT_EQ(survived.candidate_bar_position, "bottom");
    EXPECT_EQ(survived.shell, "/bin/zsh");

    std::remove(path.c_str());
}
