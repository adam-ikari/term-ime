#include "pty.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <cstring>

Pty::Pty() = default;

Pty::~Pty() {
    if (master_fd_ >= 0) {
        close(master_fd_);
    }
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        waitpid(pid_, nullptr, 0);
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
