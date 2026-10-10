#include "ime/rime_data.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// The dir search is what decides whether pinyin works at all: a system
// librime data dir satisfies exists() but has no term-ime schema, and the app
// then shows [拼] while typing yields nothing.

namespace {

// Creates <root>/<rel> and, when `with_marker`, the schema file that marks a
// dir as term-ime's own. Returns the path as a string.
std::string make_dir(const fs::path& root, const std::string& rel, bool with_marker) {
    fs::path p = root / rel;
    fs::create_directories(p);
    if (with_marker) {
        std::ofstream(p / kRimeDataMarker) << "# schema\n";
    }
    return p.string();
}

class RimeDataDirTest : public ::testing::Test {
  protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() /
                (std::string("term-ime-rime-data-") +
                 ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(root_);
        fs::create_directories(root_);
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }
    fs::path root_;
};

TEST_F(RimeDataDirTest, MarkerDirWinsOverAnEarlierSystemLikeDir) {
    // Mirrors the real order: the data next to the binary comes later in the
    // list than nothing here, but a marker-less dir listed FIRST must lose.
    std::string decoy = make_dir(root_, "usr/share/rime-data", false);
    std::string bundle = make_dir(root_, "app/share/term-ime/rime-data", true);

    EXPECT_EQ(select_shared_data_dir("", {decoy, bundle}, "/nonexistent-fallback"), bundle);
    // Same two dirs, reversed order: the marker still decides, not the order.
    EXPECT_EQ(select_shared_data_dir("", {bundle, decoy}, "/nonexistent-fallback"), bundle);
}

TEST_F(RimeDataDirTest, ExplicitConfigWinsEvenWithoutTheMarker) {
    std::string custom = make_dir(root_, "custom", false);
    std::string bundle = make_dir(root_, "bundle", true);

    EXPECT_EQ(select_shared_data_dir(custom, {bundle}, "/nonexistent-fallback"), custom);
}

TEST_F(RimeDataDirTest, MarkerlessLayoutStillRunsRatherThanVanishing) {
    // A hand-made install with no term-ime schema anywhere: keep the old
    // behaviour (first existing dir) so the caller can warn about it instead of
    // pointing librime at a path that does not exist.
    std::string first = make_dir(root_, "first", false);
    std::string second = make_dir(root_, "second", false);

    EXPECT_EQ(select_shared_data_dir("", {first, second}, "/nonexistent-fallback"), first);
}

TEST_F(RimeDataDirTest, NothingPresentYieldsTheFallback) {
    std::vector<std::string> candidates{std::string(root_ / "absent"), std::string()};

    EXPECT_EQ(select_shared_data_dir("", candidates, "/nonexistent-fallback"), "/nonexistent-fallback");
}

TEST_F(RimeDataDirTest, AFileIsNotTreatedAsADirectory) {
    fs::create_directories(root_ / "as-file");
    std::ofstream file(root_ / "as-file" / "not-a-dir");
    // The candidate is a regular file: exists() is true, is_directory() is not.
    EXPECT_EQ(select_shared_data_dir("", {(root_ / "as-file" / "not-a-dir").string()}, "/nonexistent-fallback"),
              "/nonexistent-fallback");
}

namespace {

// Values of "opencc_config: <name>.json" across the bundled schemas, skipping
// comment lines so a documented example cannot satisfy the check by itself.
std::vector<std::string> collect_opencc_configs(const fs::path& dir) {
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".yaml")
            continue;
        std::ifstream in(entry.path());
        std::string line;
        while (std::getline(in, line)) {
            size_t key = line.find("opencc_config:");
            if (key == std::string::npos)
                continue;
            size_t hash = line.find('#');
            if (hash != std::string::npos && hash < key)
                continue;
            size_t begin = line.find_first_not_of(" \t", key + strlen("opencc_config:"));
            if (begin == std::string::npos)
                continue;
            size_t end = line.find_first_of(" \t#", begin);
            names.push_back(line.substr(begin, end - begin));
        }
    }
    return names;
}

// Files an opencc config references ("file": "..."), for any dict type.
std::vector<std::string> collect_opencc_refs(const fs::path& config) {
    std::ifstream in(config);
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> refs;
    size_t pos = 0;
    while ((pos = json.find("\"file\"", pos)) != std::string::npos) {
        size_t open = json.find('"', json.find(':', pos));
        if (open == std::string::npos)
            break;
        size_t close = json.find('"', open + 1);
        if (close == std::string::npos)
            break;
        refs.push_back(json.substr(open + 1, close - open - 1));
        pos = close;
    }
    return refs;
}

}  // namespace

// The opencc tables are shipped data, not build output: librime resolves
// opencc_config under <shared_data_dir>/opencc/ and, when the file is absent,
// SimplifierComponent::Create returns nullptr -- the filter quietly does not
// exist and the user sees unconverted candidates with no error anywhere.
TEST(BundledOpenccData, EverySchemaConfigIsBundled) {
    const fs::path opencc_dir = fs::path(RIME_BUNDLED_DATA_DIR) / "opencc";
    auto names = collect_opencc_configs(RIME_BUNDLED_DATA_DIR);
    ASSERT_FALSE(names.empty()) << "no opencc_config found in " << RIME_BUNDLED_DATA_DIR;
    for (const auto& name : names) {
        EXPECT_TRUE(fs::exists(opencc_dir / name))
            << opencc_dir / name << " is referenced by a schema but not bundled";
    }
}

TEST(BundledOpenccData, EveryConfigReferenceIsBundled) {
    const fs::path opencc_dir = fs::path(RIME_BUNDLED_DATA_DIR) / "opencc";
    ASSERT_TRUE(fs::is_directory(opencc_dir));
    int checked = 0;
    for (const auto& entry : fs::directory_iterator(opencc_dir)) {
        if (entry.path().extension() != ".json")
            continue;
        for (const auto& ref : collect_opencc_refs(entry.path())) {
            EXPECT_TRUE(fs::exists(opencc_dir / ref))
                << ref << " is referenced by " << entry.path().filename() << " but not bundled";
            ++checked;
        }
    }
    EXPECT_GT(checked, 0) << "no references parsed -- the walker broke, not the data";
}

// The bundled data is not all MIT: essay.txt is LGPL-3.0 and the opencc tables
// are Apache-2.0, so the grant has to travel with the bytes. Before this was
// wired up the tarball shipped the data with no attribution anywhere in it.
TEST(BundledRimeDataLicense, PerFileGrantShipsWithTheData) {
    const fs::path license = fs::path(RIME_BUNDLED_DATA_DIR) / "LICENSE";
    ASSERT_TRUE(fs::exists(license))
        << license << " missing: the data dir mixes licenses and must carry the per-file grant";
    std::ifstream in(license);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("LGPL"), std::string::npos) << "grant text does not name the LGPL-3.0 essay.txt";
    EXPECT_NE(text.find("Apache"), std::string::npos) << "grant text does not name the Apache-2.0 opencc tables";
}

// The schemas in the same dir come from data/rime-data/ and are LGPL-3.0 /
// BSD-3-Clause, a different set of grants from dict/. Each covered file must
// have its grant named, or the package silently omits one source's terms.
TEST(BundledRimeDataLicense, SchemaGrantShipsWithTheSchemas) {
    std::ifstream in(fs::path(RIME_BUNDLED_DATA_DIR) / "LICENSE-schemas.txt");
    ASSERT_TRUE(in) << "LICENSE-schemas.txt missing: the schemas' grants do not ship";
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (const char* name : {"pinyin.yaml", "luna_pinyin.schema.yaml",
                             "luna_pinyin_simp.schema.yaml", "default.yaml"}) {
        EXPECT_TRUE(fs::exists(fs::path(RIME_BUNDLED_DATA_DIR) / name))
            << name << " is not bundled at all";
        EXPECT_NE(text.find(name), std::string::npos)
            << "grant text does not name " << name;
    }
    EXPECT_NE(text.find("LGPL-3.0"), std::string::npos) << "does not name the LGPL-3.0 schemas";
    EXPECT_NE(text.find("BSD-3-Clause"), std::string::npos) << "does not name the BSD-3-Clause default.yaml";
}

}  // namespace
