#include "app.hpp"
#include "event_loop.hpp"
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdio>

App::App() = default;

App::~App() {
    if (initialized_) {
        renderer_.restore();
    }
}

bool App::init(const std::string& shell) {
    // Initialize renderer
    renderer_.init();

    // Spawn shell in PTY
    if (!pty_.spawn(shell)) {
        fprintf(stderr, "Failed to spawn shell\n");
        renderer_.restore();
        return false;
    }

    // Get terminal size
    struct winsize ws;
    ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws);

    // Create screen and parser
    screen_ = new Screen(ws.ws_row - 1, ws.ws_col);
    parser_ = new Parser(*screen_);

    // Load IME
    dict_ = std::make_unique<Dict>();
    dict_->load("data/pinyin.dict");
    ime_ = std::make_unique<PinyinIme>(std::move(dict_));

    initialized_ = true;
    return true;
}

void App::on_pty_output(int fd) {
    (void)fd;
    auto data = pty_.read();
    if (data) {
        parser_->feed(data->data(), data->size());
        render();
    }
}

void App::on_keyboard_input(int fd) {
    (void)fd;
    int ch = renderer_.read_key();
    if (ch < 0) return;

    // Check if IME should handle this
    if (ime_->state() == ImeState::Composing || ime_->state() == ImeState::Selecting) {
        if (ch >= '1' && ch <= '9') {
            int idx = ch - '1';
            auto result = ime_->select(idx);
            if (!result.empty()) {
                std::string utf8;
                for (char32_t c : result) {
                    utf8 += utf8::encode(c);
                }
                pty_.write(std::vector<uint8_t>(utf8.begin(), utf8.end()));
            }
            render();
        } else if (ch == 27) {
            ime_->cancel();
            render();
        } else if (ch == '\b' || ch == 127) {
            ime_->cancel();
            render();
        } else if (ch >= 'a' && ch <= 'z') {
            ime_->input(static_cast<char>(ch));
            selected_candidate_ = 0;
            render();
        } else {
            char c = static_cast<char>(ch);
            pty_.write(std::vector<uint8_t>(&c, &c + 1));
        }
    } else {
        if (ch >= 'a' && ch <= 'z') {
            ime_->input(static_cast<char>(ch));
            selected_candidate_ = 0;
            render();
        } else {
            char c = static_cast<char>(ch);
            pty_.write(std::vector<uint8_t>(&c, &c + 1));
        }
    }
}

void App::on_resize(int signum) {
    (void)signum;
    struct winsize ws;
    ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws);

    screen_->resize(ws.ws_row - 1, ws.ws_col);
    pty_.resize(ws.ws_row - 1, ws.ws_col);

    render();
}

void App::on_quit(int signum) {
    (void)signum;
    renderer_.restore();
    initialized_ = false;
}

void App::render() {
    renderer_.render(*screen_);

    if (ime_->state() != ImeState::Inactive) {
        renderer_.render_candidates(ime_->candidates(), selected_candidate_, ime_->buffer());
    }
}

int App::pty_fd() const {
    return pty_.fd();
}

int App::tty_fd() const {
    return renderer_.get_tty_fd();
}