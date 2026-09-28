#include "core/event_loop.hpp"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <csignal>
#include <unistd.h>

// Registering a watcher can fail, and every failure here is one the app would
// otherwise never notice: a refused watch is an fd or a signal whose events stop
// arriving, which looks like a hung program rather than an error. The refusals
// are reported through spdlog at error level, so the fixture keeps the expected
// noise out of the test log.

class EventLoopWatchTest : public ::testing::Test {
   protected:
    void SetUp() override {
        saved_level_ = spdlog::get_level();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { spdlog::set_level(saved_level_); }
    spdlog::level::level_enum saved_level_;
};

namespace {
void noop_io(const char*, size_t) {}
}  // namespace

TEST_F(EventLoopWatchTest, FirstWatchSucceedsAndDuplicateIsRefused) {
    EventLoop loop;
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    EXPECT_TRUE(loop.watch_fd(fds[0], noop_io));
    EXPECT_FALSE(loop.watch_fd(fds[0], noop_io));  // would drop the live handle
    EXPECT_TRUE(loop.watch_fd(fds[1], noop_io));   // a refusal must not poison others

    loop.unwatch_fd(fds[0]);
    EXPECT_TRUE(loop.watch_fd(fds[0], noop_io));  // re-registering after unwatch is legal
    close(fds[0]);
    close(fds[1]);
}

TEST_F(EventLoopWatchTest, UnusableFdIsRefusedRatherThanSilentlyIgnored) {
    EventLoop loop;
    EXPECT_FALSE(loop.watch_fd(-1, noop_io));
    EXPECT_TRUE(loop.watch_fd(STDERR_FILENO, noop_io, /*readable=*/false));
}

TEST_F(EventLoopWatchTest, SignalWatchRefusesSecondRegistration) {
    EventLoop loop;
    EXPECT_TRUE(loop.watch_signal(SIGUSR1, [](int) {}));
    EXPECT_FALSE(loop.watch_signal(SIGUSR1, [](int) {}));
    loop.unwatch_signal(SIGUSR1);
    EXPECT_TRUE(loop.watch_signal(SIGUSR1, [](int) {}));
}
