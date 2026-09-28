#include "pty.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>

namespace {
// How long a write() for bytes the user just produced may block the event-loop
// thread. Losing a committed word is silent data loss at the one place the user
// cannot retype, so this is generous; flush() uses a non-blocking budget.
constexpr int kWriteBudgetMs = 100;
// ~Pty drains the queue while the child is still alive, but a stuck slave must
// not hold up exit.
constexpr int kExitBudgetMs = 50;
}  // namespace

Pty::Pty() = default;

bool Pty::wait_for_exit(int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        int status = 0;
        pid_t r = waitpid(pid_, &status, WNOHANG);
        if (r == pid_ || r < 0) {
            return true;  // reaped, or no such child
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;  // timed out still running
}

Pty::~Pty() {
    if (master_fd_ >= 0) {
        // Queued bytes are input the child has not seen yet, and closing the
        // master makes them unrecoverable. The child is still alive at this
        // point, so give the queue one bounded drain.
        if (!tx_queue_.empty()) {
            const std::vector<uint8_t> queued = tx_queue_.take();
            write_raw(queued.data(), queued.size(), kExitBudgetMs);
            if (!tx_queue_.empty()) {
                spdlog::warn("pty exit: slave never took {} queued byte(s)", tx_queue_.size());
            }
        }
        close(master_fd_);
    }
    if (pid_ > 0) {
        // Reap the child, but never block forever: if the shell ignores
        // SIGTERM (e.g. a trapped TERM), escalate to SIGKILL after a bounded
        // wait. A bare blocking waitpid here hangs term-ime on exit (monkey
        // finding F2).
        kill(pid_, SIGTERM);
        if (!wait_for_exit(2000)) {  // 2s grace period
            kill(pid_, SIGKILL);
            wait_for_exit(1000);  // reap the kill
        }
    }
}

bool Pty::spawn(const std::string& shell) {
    // Ask the real controlling terminal for its geometry. A hard-coded 24x80
    // makes the child shell (and every TUI it launches) lay out for the wrong
    // size. Fall back to 24x80 when no tty is reachable.
    struct winsize ws {};
    const int probe_fds[] = {STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO};
    bool have_size = false;
    for (int fd : probe_fds) {
        if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
            have_size = true;
            break;
        }
        ws = winsize{};
    }
    if (!have_size) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    int master;
    pid_ = forkpty(&master, nullptr, nullptr, &ws);
    if (pid_ < 0) {
        return false;
    }
    master_fd_ = master;

    if (pid_ == 0) {
        // Child process
        setenv("TERM", "xterm-256color", 1);
        // execvp, not execl: a config may name the shell as a bare program
        // ("zsh"), and execl would fail on it instead of searching PATH -- the
        // child would _exit() and term-ime would close with no explanation.
        std::vector<char*> argv = {const_cast<char*>(shell.c_str()), nullptr};
        execvp(shell.c_str(), argv.data());
        _exit(1);
    }

    // Set non-blocking
    int flags = fcntl(master_fd_, F_GETFL);
    fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK);

    return true;
}

std::optional<std::vector<uint8_t>> Pty::read() {
    std::vector<uint8_t> buf(4096);
    ssize_t n = ::read(master_fd_, buf.data(), buf.size());
    if (n > 0) {
        buf.resize(n);
        return buf;
    }
    return std::nullopt;
}

bool Pty::write(const std::vector<uint8_t>& data) {
    // Queued bytes are older than `data`, so they go out first.
    if (tx_queue_.empty())
        return write_raw(data.data(), data.size(), kWriteBudgetMs);
    std::vector<uint8_t> merged = tx_queue_.take();
    merged.insert(merged.end(), data.begin(), data.end());
    return write_raw(merged.data(), merged.size(), kWriteBudgetMs);
}

bool Pty::flush(int budget_ms) {
    if (tx_queue_.empty())
        return true;
    const std::vector<uint8_t> queued = tx_queue_.take();
    return write_raw(queued.data(), queued.size(), budget_ms);
}

bool Pty::write_raw(const uint8_t* data, size_t len, int budget_ms) {
    // master_fd_ is O_NONBLOCK: a single write() may accept only part of the
    // buffer (large paste, long CJK commit) or return EAGAIN. Loop until every
    // byte is handed to the kernel; dropping the tail silently lost input.
    // Callers run on the event-loop thread, so the EAGAIN poll() retries are
    // bounded by `budget_ms`: a slave buffer that never drains (shell not
    // reading) must not freeze the loop forever. Whatever the budget could not
    // send is queued for the next write()/flush().
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms);

    // Unsent bytes become the queue. Only the newest end may be dropped, since
    // splicing the stream mid-sequence is worse than losing its tail.
    auto queue_tail = [&](size_t from) {
        if (const size_t dropped = tx_queue_.buffer_tail(data, len, from)) {
            spdlog::error("pty write: queue full, dropped {} newest bytes to stay within {}, the rest of the stream is lost",
                          dropped, TxByteQueue::kMaxBytes);
        }
    };

    // The peer will never accept anything again, so a queue it cannot drain is
    // worse than an honest drop: the next flush() would keep claiming success.
    auto give_up = [&](size_t from, const char* why) {
        const size_t unsent = (len - from) + tx_queue_.size();
        tx_queue_.clear();
        spdlog::warn("pty write: {}: discarding {} unsent bytes, peer cannot take them", why, unsent);
        return false;
    };

    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(master_fd_, data + written, len - written);
        if (n > 0) {
            written += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            // SIGWINCH and SIGTERM land on this thread; retrying EINTR without
            // re-checking the budget could spin past any deadline.
            if (std::chrono::steady_clock::now() >= deadline) {
                queue_tail(written);
                return false;
            }
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                spdlog::warn("pty write: blocked >{}ms, buffering {} of {} bytes", budget_ms, len - written, len);
                queue_tail(written);
                return false;
            }
            struct pollfd pfd {};
            pfd.fd = master_fd_;
            pfd.events = POLLOUT;
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            int poll_timeout = remaining < 1 ? 1 : static_cast<int>(remaining);
            int pr = poll(&pfd, 1, poll_timeout);
            if (pr < 0) {
                if (errno == EINTR) {
                    continue;
                }
                spdlog::warn("pty write: poll failed: {}", std::strerror(errno));
                queue_tail(written);
                return false;
            }
            if (pr == 0) {
                spdlog::warn("pty write: stalled after {} of {} bytes, buffering the rest", written, len);
                queue_tail(written);
                return false;
            }
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                return give_up(written, "poll reported hangup");
            }
            continue;
        }
        return give_up(written, n < 0 ? std::strerror(errno) : "zero-length write");
    }
    return true;
}

int Pty::fd() const {
    return master_fd_;
}

void Pty::resize(int rows, int cols) {
    // winsize's fields are unsigned short, so a negative or oversized value does
    // not fail -- it wraps, and the child is told something like 65535 rows. Both
    // callers range-check the geometry they read from the tty first, so this is
    // the backstop for whoever calls next, not a live bug being papered over.
    const auto clamp = [](int v, const char* what) {
        if (v >= 1 && v <= 65535)
            return static_cast<unsigned short>(v);
        const int fixed = v < 1 ? 1 : 65535;
        spdlog::warn("pty resize: {} = {} is outside winsize's 1..65535, using {}", what, v, fixed);
        return static_cast<unsigned short>(fixed);
    };

    // Zero-initialised: TIOCSWINSZ reads ws_xpixel/ws_ypixel, and leaving them
    // as stack garbage makes the child inherit a meaningless pixel size.
    struct winsize ws {};
    ws.ws_row = clamp(rows, "rows");
    ws.ws_col = clamp(cols, "cols");
    ioctl(master_fd_, TIOCSWINSZ, &ws);
}
