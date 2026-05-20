#include "app.hpp"
#include "event_loop.hpp"
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdexcept>

App::App() = default;

App::~App() {
    try {
        if (initialized_) {
            renderer_.restore();
        }
        delete screen_;
        screen_ = nullptr;
        delete parser_;
        parser_ = nullptr;
    } catch (const std::exception& e) {
        spdlog::error("Exception in App destructor: {}", e.what());
    }
}

bool App::init(const std::string& shell) {
    spdlog::info("App::init starting");

    // 检查是否是 TTY
    if (!isatty(STDIN_FILENO)) {
        spdlog::error("Not a TTY, cannot run");
        return false;
    }

    try {
        // Initialize renderer
        spdlog::info("Initializing renderer");
        renderer_.init();

        // Spawn shell in PTY
        spdlog::info("Spawning PTY");
        if (!pty_.spawn(shell)) {
            spdlog::error("Failed to spawn shell: {}", shell);
            renderer_.restore();
            return false;
        }

        // Get terminal size
        struct winsize ws;
        if (ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws) < 0 ||
            ws.ws_row == 0 || ws.ws_row > 1000 ||
            ws.ws_col == 0 || ws.ws_col > 1000) {
            spdlog::warn("Failed to get terminal size, using defaults");
            ws.ws_row = 24;
            ws.ws_col = 80;
        }

        // Create screen and parser
        spdlog::info("Creating screen {}x{}", ws.ws_row - 1, ws.ws_col);
        screen_ = new Screen(ws.ws_row - 1, ws.ws_col);
        if (!screen_) {
            spdlog::error("Failed to create screen");
            renderer_.restore();
            return false;
        }

        parser_ = new Parser(*screen_);
        if (!parser_) {
            spdlog::error("Failed to create parser");
            delete screen_;
            screen_ = nullptr;
            renderer_.restore();
            return false;
        }

        // Initialize Rime IME
        spdlog::info("Initializing Rime IME");
        if (!ime_.initialize()) {
            spdlog::warn("Failed to initialize Rime IME, continuing without IME");
        } else {
            spdlog::info("Rime IME initialized");
        }

        initialized_ = true;
        spdlog::info("App::init complete");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception during init: {}", e.what());
        renderer_.restore();
        return false;
    }
}

void App::on_pty_data(const char* data, size_t len) {
    // 直接转发 PTY 输出到终端，不解析
    // 这样可以获得和原生终端一样的体验
    renderer_.forward_output(data, len);

    // 同时更新内部屏幕状态（用于光标位置跟踪等）
    if (parser_) {
        parser_->feed(reinterpret_cast<const uint8_t*>(data), len);
    }

    // 显示候选词栏（覆盖最后一行）
    std::string mode = (ime_.mode() == ImeMode::Chinese) ? "中文" : "EN";
    renderer_.render_candidates(ime_.candidates(), selected_candidate_, ime_.buffer(), mode);
}

void App::on_keyboard_data(const char* data, size_t len) {
    if (len == 0) return;

    int ch = static_cast<int>(data[0]);

    // 转义序列缓冲区
    static std::string escape_buffer;

    // tmux-style prefix key: Ctrl+A (char 1) then command key
    // Ctrl+A = ASCII 1 (0x01), same as GNU Screen default
    if (ch == 1) {  // Ctrl+A
        prefix_pending_ = true;
        spdlog::info("Prefix key Ctrl+A received, waiting for command");
        return;
    }

    // Handle command after prefix
    if (prefix_pending_) {
        prefix_pending_ = false;
        if (ch == ' ') {  // Ctrl+A + Space = toggle mode
            spdlog::info("Ctrl+A + Space detected, toggling mode");
            ime_.toggle_mode();
            render();
            return;
        }
        // Unknown command after prefix, forward both keys to shell
        pty_.write(std::vector<uint8_t>{1});  // Send Ctrl+A first
        pty_.write(std::vector<uint8_t>(data, data + len));
        return;
    }

    // 处理 ESC 序列
    if (ch == 27) {
        escape_buffer.clear();
        escape_buffer += static_cast<char>(ch);
        return;
    }

    if (!escape_buffer.empty()) {
        escape_buffer += static_cast<char>(ch);

        // 功能键序列：ESC [ ... 或 ESC O ...
        if (escape_buffer.size() == 2) {
            char second = escape_buffer[1];
            if (second == '[' || second == 'O') {
                return;  // 继续收集
            }
            // 不是功能键，转发给 shell
            pty_.write(std::vector<uint8_t>(escape_buffer.begin(), escape_buffer.end()));
            escape_buffer.clear();
            return;
        }

        if (escape_buffer.size() >= 3) {
            char last = escape_buffer.back();
            if ((last >= 'A' && last <= 'Z') || (last >= 'a' && last <= 'z') || last == '~') {
                spdlog::debug("Forwarding escape sequence: {}", escape_buffer);
                pty_.write(std::vector<uint8_t>(escape_buffer.begin(), escape_buffer.end()));
                escape_buffer.clear();
                return;
            }
            if (escape_buffer.size() > 10) {
                pty_.write(std::vector<uint8_t>(escape_buffer.begin(), escape_buffer.end()));
                escape_buffer.clear();
            }
        }
        return;
    }

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
        } else if (ch == ' ') {
            // Space selects first candidate (common behavior for non-English IMEs)
            auto result = ime_.select(0);
            if (!result.empty()) {
                std::string utf8;
                for (char32_t c : result) {
                    utf8 += utf8::encode(c);
                }
                pty_.write(std::vector<uint8_t>(utf8.begin(), utf8.end()));
            }
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

    spdlog::debug("render: starting");
    renderer_.render(*screen_);
    spdlog::debug("render: screen done");

    // 总是显示候选词栏（包括空状态提示）
    std::string mode = (ime_.mode() == ImeMode::Chinese) ? "中文" : "EN";
    renderer_.render_candidates(ime_.candidates(), selected_candidate_, ime_.buffer(), mode);
    spdlog::debug("render: candidates done");
}

int App::pty_fd() const {
    return pty_.fd();
}

int App::tty_fd() const {
    return renderer_.get_tty_fd();
}