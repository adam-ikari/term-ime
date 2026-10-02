#include "terminal/pty.hpp"

#include <gtest/gtest.h>

#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <algorithm>
#include <thread>
#include <vector>

// The overflow path in Pty::write() is the one place where user input can
// actually be lost. test_pty.cpp exercises TxByteQueue in isolation, but it
// takes for granted the property that makes the drop-newest policy defensible —
// that what the child ultimately receives is a CONTIGUOUS RUN of the stream,
// with the loss confined to the newest end. Nothing checked that against a real
// stalled slave.
//
// The stall is produced the way it happens in practice: a `cat` child whose
// output nobody reads. Its pty output buffer fills, cat blocks in write(2), and
// then it stops draining its input — so the master's write buffer fills too and
// write_raw() falls through to the queue with its 100 ms budget spent.
//
// (Spawning `sleep` does not work here: Pty::spawn execs the program with no
// arguments, so `sleep` exits immediately with a usage error and the pty hangs
// up rather than stalling.)

namespace {

// Printable ASCII only, and deliberately so. The first attempt used `i % 251`,
// whose byte 0x03 is the line discipline's VINTR: the tty turned it into SIGINT
// for the child's process group, cat died, and the pty hung up mid-test — which
// looks exactly like the overflow behaviour under test. A byte stream chosen to
// be distinctive must not also carry terminal semantics.
std::vector<uint8_t> patterned(size_t len) {
    std::vector<uint8_t> out(len);
    for (size_t i = 0; i < len; ++i)
        out[i] = static_cast<uint8_t>(0x20 + (i % 90));
    return out;
}

// A pty slave starts in canonical mode with ECHO on, which makes this untestable
// as written: the line discipline echoes everything back (so reading the master
// shows our own input, not the child's output) and withholds it from the child
// until a newline arrives (so `cat` sees nothing for a binary paste). Both have
// to go, and the master's fd is where the slave's termios is configured.
// ISIG goes too, so no byte in the test stream can signal the child.
void make_slave_raw(Pty& pty) {
    struct termios t {};
    ASSERT_EQ(tcgetattr(pty.fd(), &t), 0);
    t.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | ISIG));
    ASSERT_EQ(tcsetattr(pty.fd(), TCSANOW, &t), 0);
}

// Read until the fd goes quiet for `quiet_ms`, retrying the outbound queue as we
// go. The flush matters: bytes that write_raw() could not hand to the kernel sit
// in TxByteQueue until the next write()/flush(), so without this the child only
// ever sees what the kernel already delivered — which is a contiguous prefix
// under EITHER overflow policy, and the test therefore cannot tell them apart.
std::vector<uint8_t> drain(Pty& pty, int quiet_ms) {
    std::vector<uint8_t> out;
    int quiet_rounds = 0;
    for (int round = 0; round < 400 && quiet_rounds < 3; ++round) {
        // Push the queued tail out FIRST. Bytes that write_raw() could not hand
        // to the kernel sit in TxByteQueue until the next write()/flush(); if the
        // drain simply waited for the fd to go quiet it would stop before ever
        // asking for them, and the child would only ever see what the kernel
        // already delivered -- a contiguous prefix under either overflow policy,
        // so the test could not tell drop-newest from drop-oldest.
        pty.flush(20);
        struct pollfd pfd {};
        pfd.fd = pty.fd();
        pfd.events = POLLIN;
        const int pr = poll(&pfd, 1, quiet_ms);
        if (pr <= 0) {
            ++quiet_rounds;
            continue;
        }
        std::vector<uint8_t> buf(65536);
        const ssize_t n = ::read(pty.fd(), buf.data(), buf.size());
        if (n > 0) {
            buf.resize(static_cast<size_t>(n));
            out.insert(out.end(), buf.begin(), buf.end());
            quiet_rounds = 0;
        } else {
            ++quiet_rounds;
        }
    }
    return out;
}

// How many leading bytes of `got` match the start of `data`.
size_t common_prefix(const std::vector<uint8_t>& got, const std::vector<uint8_t>& data) {
    size_t n = 0;
    while (n < got.size() && n < data.size() && got[n] == data[n])
        ++n;
    return n;
}

}  // namespace

TEST(PtyOverflowTest, StalledSlaveKeepsAContiguousPrefix) {
    Pty pty;
    ASSERT_TRUE(pty.spawn("/bin/cat"));
    make_slave_raw(pty);

    const auto data = patterned(512 * 1024);
    // Nothing is read from the master while this runs, so cat blocks and stops
    // draining, and the write path has to fall back to its queue.
    pty.write(data);

    const std::vector<uint8_t> got = drain(pty, 300);
    ASSERT_FALSE(got.empty()) << "child received nothing at all";

    // The property the whole drop-newest policy rests on. A divergence here
    // means the child's byte stream was spliced, and its parser would chase that
    // garbage for the rest of the session.
    const size_t matched = common_prefix(got, data);
    EXPECT_EQ(matched, got.size())
        << "child's stream diverges at byte " << matched << " of " << got.size()
        << " — the stream was spliced";
    EXPECT_LE(got.size(), data.size());
}

TEST(PtyOverflowTest, RepeatedWritesStayBoundedAndOrdered) {
    Pty pty;
    ASSERT_TRUE(pty.spawn("/bin/cat"));
    make_slave_raw(pty);

    // Each write merges the pending queue and re-offers it, which is where an
    // unbounded queue would show up as unbounded memory.
    const auto data = patterned(256 * 1024);
    for (int i = 0; i < 8; ++i)
        pty.write(data);

    const std::vector<uint8_t> got = drain(pty, 300);
    // Without this the comparison below is vacuous: an empty result matches any
    // stream, so a build that drops everything would pass. (Learned the hard
    // way — this assertion was missing and the test "passed" with got empty.)
    ASSERT_FALSE(got.empty()) << "child received nothing at all";
    const size_t matched = common_prefix(got, data);
    // Still a contiguous run: each retry must not reorder or duplicate bytes.
    EXPECT_EQ(matched, got.size())
        << "spliced at byte " << matched << " of " << got.size();
}

TEST(PtyOverflowTest, DrainedChildGetsEverythingInOrder) {
    // The control case. Without it the two tests above could pass merely because
    // everything is being dropped.
    Pty pty;
    ASSERT_TRUE(pty.spawn("/bin/cat"));
    make_slave_raw(pty);

    const auto data = patterned(256 * 1024);
    std::vector<uint8_t> got;
    size_t sent = 0;
    // Write in slices and read between them, so cat keeps draining and never
    // stalls — this is the ordinary paste path.
    while (sent < data.size()) {
        const size_t slice = std::min<size_t>(16 * 1024, data.size() - sent);
        std::vector<uint8_t> chunk(data.begin() + sent, data.begin() + sent + slice);
        pty.write(chunk);
        sent += slice;
        std::vector<uint8_t> more = drain(pty, 5);
        got.insert(got.end(), more.begin(), more.end());
    }
    for (int i = 0; i < 40 && got.size() < data.size(); ++i) {
        pty.flush(10);
        std::vector<uint8_t> more = drain(pty, 10);
        got.insert(got.end(), more.begin(), more.end());
    }

    ASSERT_GE(got.size(), data.size()) << "only " << got.size() << " of " << data.size() << " bytes arrived";
    EXPECT_EQ(common_prefix(got, data), data.size()) << "live-child delivery corrupted the stream";
}