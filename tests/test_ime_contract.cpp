#include <gtest/gtest.h>

#include "ime/engine.hpp"
#include "ime/rime_engine.hpp"

#include <cstdlib>
#include <unistd.h>
#include <filesystem>
#include <string>

// The ImeEngine contract, tested directly rather than through the TUI.
//
// The interface is advertised as embeddable (term-ime-lib), so its guarantees
// are a public promise, not an internal detail — but nothing asserted them
// before. The e2e suites drive the whole app, which means a contract change
// would show up as some unrelated screen assertion failing, or not at all.
//
// These are the invariants a caller is entitled to rely on:
//   - a fresh engine is Inactive and in English mode
//   - input() is refused outside a composition, and refusal is distinguishable
//     from acceptance (the app relies on exactly that distinction)
//   - state() advances Composing -> Selecting once candidates exist
//   - select() commits and reports what it committed
//   - cancel() returns to Inactive and takes the buffer with it
//   - page_up/page_down change the visible page

namespace {

// Each fixture gets its own HOME so librime deploys into a clean user dir and
// one test's dictionary cannot satisfy another's.
class ImeEngineContract : public ::testing::Test {
   protected:
    void SetUp() override {
        home_ = std::filesystem::temp_directory_path() /
                ("ti-ime-contract-" + std::to_string(::getpid()) + "-" + std::to_string(counter_++));
        std::filesystem::create_directories(home_);
        setenv("HOME", home_.c_str(), 1);
        setenv("XDG_CONFIG_HOME", (home_ / ".config").c_str(), 1);
        setenv("XDG_DATA_HOME", (home_ / ".local" / "share").c_str(), 1);
        engine_ = std::make_unique<RimeIme>();
        // Initialisation deploys the dictionary; a failure here would make every
        // assertion below meaningless, so require it.
        ASSERT_TRUE(engine_->initialize()) << "rime failed to initialize";
    }

    void TearDown() override {
        engine_.reset();
        std::error_code ec;
        std::filesystem::remove_all(home_, ec);
    }

    std::filesystem::path home_;
    std::unique_ptr<RimeIme> engine_;
    static inline int counter_ = 0;
};

TEST_F(ImeEngineContract, FreshEngineIsIdleAndInEnglish) {
    EXPECT_EQ(engine_->state(), ImeState::Inactive);
    EXPECT_EQ(engine_->mode(), ImeMode::English);
    EXPECT_TRUE(engine_->buffer().empty());
    EXPECT_TRUE(engine_->candidates().empty());
}

TEST_F(ImeEngineContract, InputOutsideACompositionIsRefused) {
    // The app's key dispatch branches on this: `input()` returning false is what
    // tells it the byte is not ours and must be forwarded to the shell. If this
    // ever started returning true outside a composition, keys would be silently
    // swallowed instead.
    EXPECT_FALSE(engine_->input('n'));
    EXPECT_EQ(engine_->state(), ImeState::Inactive);
}

TEST_F(ImeEngineContract, CompositionAdvancesToSelectingWithCandidates) {
    // input() is refused in English mode by contract, so opt in first.
    engine_->set_mode(ImeMode::Chinese);
    ASSERT_TRUE(engine_->input('n'));
    ASSERT_TRUE(engine_->input('i'));
    ASSERT_TRUE(engine_->input('h'));
    ASSERT_TRUE(engine_->input('a'));
    ASSERT_TRUE(engine_->input('o'));

    EXPECT_EQ(engine_->state(), ImeState::Selecting) << "buffer: " << engine_->buffer();
    EXPECT_FALSE(engine_->candidates().empty());
    EXPECT_EQ(engine_->buffer(), "ni hao");
}

TEST_F(ImeEngineContract, SelectCommitsAndReportsTheText) {
    engine_->set_mode(ImeMode::Chinese);
    for (char c : std::string("nihao"))
        ASSERT_TRUE(engine_->input(c));
    ASSERT_FALSE(engine_->candidates().empty());

    const std::u32string committed = engine_->select(0);
    EXPECT_FALSE(committed.empty()) << "select(0) committed nothing";

    // select() already drains the commit internally, so the queue must be empty
    // afterwards. This is the anti-double-commit invariant: the app forwards
    // select()'s return value to the shell, and if the commit were still queued
    // here the next key that called take_commit() would put the same text on the
    // wire a second time.
    EXPECT_TRUE(engine_->take_commit().empty())
        << "select() left the commit queued — it would reach the shell twice";
}

TEST_F(ImeEngineContract, CancelClearsBufferAndReturnsToInactive) {
    engine_->set_mode(ImeMode::Chinese);
    for (char c : std::string("nihao"))
        ASSERT_TRUE(engine_->input(c));
    ASSERT_NE(engine_->state(), ImeState::Inactive);

    engine_->cancel();

    EXPECT_EQ(engine_->state(), ImeState::Inactive);
    EXPECT_TRUE(engine_->buffer().empty());
    EXPECT_TRUE(engine_->candidates().empty());
}

TEST_F(ImeEngineContract, ModeToggleIsSymmetric) {
    ASSERT_EQ(engine_->mode(), ImeMode::English);
    engine_->toggle_mode();
    EXPECT_EQ(engine_->mode(), ImeMode::Chinese);
    engine_->toggle_mode();
    EXPECT_EQ(engine_->mode(), ImeMode::English);

    engine_->set_mode(ImeMode::Chinese);
    EXPECT_EQ(engine_->mode(), ImeMode::Chinese);
}

TEST_F(ImeEngineContract, SelectOutOfRangeCommitsNothing) {
    engine_->set_mode(ImeMode::Chinese);
    for (char c : std::string("nihao"))
        ASSERT_TRUE(engine_->input(c));

    // Documented refusal: librime's select_keys map single digits to at most
    // ten candidates, so index 10+ has no key. Silently selecting candidate 0
    // here would commit the wrong word.
    EXPECT_TRUE(engine_->select(10).empty());
    EXPECT_TRUE(engine_->select(-1).empty());
}

TEST_F(ImeEngineContract, BackspaceRemovesOneSyllableCharacter) {
    engine_->set_mode(ImeMode::Chinese);
    for (char c : std::string("nihao"))
        ASSERT_TRUE(engine_->input(c));
    ASSERT_EQ(engine_->buffer(), "ni hao");

    engine_->backspace();

    // One character, not the whole composition — that is the documented
    // behaviour, and the app relies on it to let the user fix a typo.
    EXPECT_NE(engine_->buffer(), "ni hao");
    EXPECT_FALSE(engine_->buffer().empty()) << "backspace cleared the whole buffer";
}
}
