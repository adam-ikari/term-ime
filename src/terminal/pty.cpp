#include "pty.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <cstring>
#include <chrono>
#include <thread>

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
    struct winsize ws = {24, 80, 0, 0};

    pid_ = forkpty(&master_fd_, nullptr, nullptr, &ws);
    if (pid_ < 0) {
        return false;
    }

    if (pid_ == 0) {
        // Child process
        setenv("TERM", "xterm-256color", 1);
        execl(shell.c_str(), shell.c_str(), nullptr);
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
    return ::write(master_fd_, data.data(), data.size()) == static_cast<ssize_t>(data.size());
}

int Pty::fd() const {
    return master_fd_;
}

void Pty::resize(int rows, int cols) {
    struct winsize ws;
    ws.ws_row = rows;
    ws.ws_col = cols;
    ioctl(master_fd_, TIOCSWINSZ, &ws);
}
