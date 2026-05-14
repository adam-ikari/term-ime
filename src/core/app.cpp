#include "app.hpp"
#include "event_loop.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <sys/ioctl.h>

App::App() = default;

App::~App() {
    if (initialized_) {
        renderer_.restore();
    }
    delete screen_;
    delete parser_;
}

bool App::init(const std::string& shell) {
    // Initialize renderer
    renderer_.init();

    // Spawn shell in PTY
    if (!pty_.spawn(shell)) {
        spdlog::error("Failed to spawn shell");
        renderer_.restore();
        return false;
    }

    // Get terminal size
    struct winsize ws;
    ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws);

    // Create screen and parser
    screen_ = new Screen(ws.ws_row - 1, ws.ws_col);
    parser_ = new Parser(*screen_);

    // Initialize Rime IME
    if (!ime_.initialize()) {
        spdlog::warn("Failed to initialize Rime IME");
    } else {
        spdlog::info("Rime IME initialized");
    }

    initialized_ = true;
    return true;
}

void App::on_pty_data(const char* data, size_t len) {
    if (parser_) {
        parser_->feed(reinterpret_cast<const uint8_t*>(data), len);
        render();
    }
}

void App::on_keyboard_data(const char* data, size_t len) {
    if (len == 0) return;

    int ch = static_cast<int>(data[0]);

    // Check if IME should handle this
    if (ime_.state() == ImeState::Composing || ime_.state() == ImeState::Selecting) {
        if (ch >= '1' && ch <= '9') {
            int idx = ch - '1';
            auto result = ime_.select(idx);
            if (!result.empty()) {
                std::string utf8;
                for (char32_t c : result) {
                    utf8 += utf8::encode(c);
                }
                pty_.write(std::vector<uint8_t>(utf8.begin(), utf8.end()));
            }
            render();
        } else if (ch == 27) {
            ime_.cancel();
            render();
        } else if (ch == '\b' || ch == 127) {
            ime_.cancel();
            render();
        } else if (ch >= 'a' && ch <= 'z') {
            ime_.input(static_cast<char>(ch));
            selected_candidate_ = 0;
            render();
        } else {
            pty_.write(std::vector<uint8_t>(data, data + len));
        }
    } else {
        if (ime_.mode() == ImeMode::Chinese && ch >= 'a' && ch <= 'z') {
            ime_.input(static_cast<char>(ch));
            selected_candidate_ = 0;
            render();
        } else {
            pty_.write(std::vector<uint8_t>(data, data + len));
        }
    }
}

void App::on_resize(int signum) {
    (void)signum;
    struct winsize ws;
    ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws);

    if (screen_) {
        screen_->resize(ws.ws_row - 1, ws.ws_col);
    }
    pty_.resize(ws.ws_row - 1, ws.ws_col);

    render();
}

void App::on_quit(int signum) {
    (void)signum;
    renderer_.restore();
    initialized_ = false;
}

void App::render() {
    if (!screen_) return;

    renderer_.render(*screen_);

    if (ime_.state() != ImeState::Inactive) {
        renderer_.render_candidates(ime_.candidates(), selected_candidate_, ime_.buffer());
    }
}

int App::pty_fd() const {
    return pty_.fd();
}

int App::tty_fd() const {
    return renderer_.get_tty_fd();
}