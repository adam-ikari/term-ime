#include "renderer.hpp"
#include "../util/utf8.hpp"
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cstdio>

Renderer::Renderer() = default;

Renderer::~Renderer() {
    if (initialized_) {
        restore();
    }
}

void Renderer::init() {
    tty_fd_ = STDIN_FILENO;

    // Save current terminal settings
    saved_termios_ = new termios;
    tcgetattr(tty_fd_, saved_termios_);

    // Set raw mode
    termios raw = *saved_termios_;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(tty_fd_, TCSAFLUSH, &raw);

    // Hide cursor, enable alternate screen
    printf("\x1b[?25l\x1b[?1049h");
    fflush(stdout);

    initialized_ = true;
}

void Renderer::restore() {
    if (!initialized_) return;

    // Restore terminal settings
    if (saved_termios_) {
        tcsetattr(tty_fd_, TCSAFLUSH, saved_termios_);
        delete saved_termios_;
        saved_termios_ = nullptr;
    }

    // Show cursor, disable alternate screen
    printf("\x1b[?25h\x1b[?1049l");
    fflush(stdout);

    initialized_ = false;
}

void Renderer::render(const Screen& screen) {
    // Move cursor to home position
    printf("\x1b[H");

    for (int row = 0; row < screen.rows(); ++row) {
        for (int col = 0; col < screen.cols(); ++col) {
            Cell cell = screen.get(row, col);
            std::string utf8 = utf8::encode(cell.ch);
            printf("%s", utf8.c_str());
        }
        if (row < screen.rows() - 1) {
            printf("\r\n");
        }
    }
    fflush(stdout);
}

void Renderer::render_candidates(const std::vector<Candidate>& candidates,
                                  size_t selected, const std::string& buffer) {
    if (candidates.empty()) return;

    // Move to bottom line
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) < 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    printf("\x1b[%d;1H", ws.ws_row);  // Last line
    printf("\x1b[7m");  // Reverse video
    fflush(stdout);

    // Show buffer
    printf(" 拼音: %s ", buffer.c_str());

    // Show candidates
    size_t idx = 0;
    for (const auto& cand : candidates) {
        std::string text;
        for (char32_t ch : cand.text) {
            text += utf8::encode(ch);
        }
        if (idx == selected) {
            printf(" [%zu.%s] ", idx + 1, text.c_str());
        } else {
            printf(" %zu.%s ", idx + 1, text.c_str());
        }
        ++idx;
    }

    printf("\x1b[0m");  // Reset attributes
    fflush(stdout);
}

int Renderer::read_key() {
    char c;
    ssize_t n = read(tty_fd_, &c, 1);
    if (n > 0) {
        return static_cast<unsigned char>(c);
    }
    return -1;
}

int Renderer::get_tty_fd() const {
    return tty_fd_;
}
