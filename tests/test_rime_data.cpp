#include "ime/rime_data.hpp"
#include <gtest/gtest.h>
#include <filesystem>
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

}  // namespace
