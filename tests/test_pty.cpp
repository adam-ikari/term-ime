#include "terminal/pty.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <vector>

// The queue carries an ordered byte stream to the pty slave, so the rules worth
// locking are (1) it never splices the stream -- on overflow the newest bytes go,
// never the oldest -- and (2) queued bytes always precede newer ones.

namespace {

// Patterned stream so any splice shows up as a mismatch, not as "looks like text".
std::vector<uint8_t> stream(size_t len) {
    std::vector<uint8_t> out(len);
    for (size_t i = 0; i < len; ++i)
        out[i] = static_cast<uint8_t>(i % 251);
    return out;
}

}  // namespace

TEST(TxByteQueueTest, BuffersContiguousTailWhenUnderCap) {
    const auto data = stream(4096);
    TxByteQueue q;
    EXPECT_EQ(q.buffer_tail(data.data(), data.size(), 1000), 0u);
    EXPECT_EQ(q.size(), 3096u);

    const std::vector<uint8_t> got = q.take();
    EXPECT_TRUE(std::equal(got.begin(), got.end(), data.begin() + 1000));
    EXPECT_TRUE(q.empty());  // take() drains it
}

TEST(TxByteQueueTest, OverflowDropsNewestBytesNotOldest) {
    // The kernel took 1024 of a 256 KiB write. The 1024 already-sent bytes must
    // stay adjacent to what is buffered: dropping from the front (the previous
    // behaviour) would leave the slave with "...byte 1023" followed by
    // "...byte 65535", cutting a UTF-8 or escape sequence in half.
    const auto data = stream(256 * 1024);
    TxByteQueue q;
    const size_t dropped = q.buffer_tail(data.data(), data.size(), 1024);

    EXPECT_EQ(dropped, data.size() - 1024 - TxByteQueue::kMaxBytes);
    EXPECT_EQ(q.size(), TxByteQueue::kMaxBytes);

    const std::vector<uint8_t> got = q.take();
    EXPECT_TRUE(std::equal(got.begin(), got.end(), data.begin() + 1024));
    // The cut is at the newest end.
    EXPECT_EQ(got.back(), data[1024 + TxByteQueue::kMaxBytes - 1]);
}

TEST(TxByteQueueTest, QueuedBytesGoAheadOfNewerOnes) {
    // Mirrors Pty::write(): take() the queue, append the new data, resend. The
    // result must be one uninterrupted run of the original stream.
    const auto data = stream(8192);
    TxByteQueue q;
    q.buffer_tail(data.data(), data.size(), 2000);  // 2000 sent, rest queued

    const std::vector<uint8_t> suffix = stream(64);
    std::vector<uint8_t> merged = q.take();
    const size_t queued = merged.size();
    merged.insert(merged.end(), suffix.begin(), suffix.end());

    EXPECT_EQ(merged.size(), queued + suffix.size());
    EXPECT_TRUE(std::equal(merged.begin(), merged.begin() + queued, data.begin() + 2000));
    EXPECT_TRUE(std::equal(merged.begin() + queued, merged.end(), suffix.begin()));
}

TEST(TxByteQueueTest, BufferTailReplacesRatherThanAppends) {
    // buffer_tail() is called on a buffer that already starts with the previous
    // contents (Pty::write / Pty::flush merge them first). Appending instead
    // would duplicate the stream.
    const auto data = stream(2048);
    TxByteQueue q;
    q.buffer_tail(data.data(), data.size(), 1536);
    q.buffer_tail(data.data(), data.size(), 1024);

    EXPECT_EQ(q.size(), 1024u);
    const std::vector<uint8_t> got = q.take();
    EXPECT_TRUE(std::equal(got.begin(), got.end(), data.begin() + 1024));
}

TEST(TxByteQueueTest, ClearDiscardsQueue) {
    const auto data = stream(512);
    TxByteQueue q;
    q.buffer_tail(data.data(), data.size(), 0);
    ASSERT_FALSE(q.empty());
    q.clear();
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.take().size(), 0u);
}
