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
    spdlog::info("App::init starting");

    // Initialize renderer
    spdlog::info("Initializing renderer");
    renderer_.init();

    // Spawn shell in PTY
    spdlog::info("Spawning PTY");
    if (!pty_.spawn(shell)) {
        spdlog::error("Failed to spawn shell");
        renderer_.restore();
        return false;
    }

    // Get terminal size
    struct winsize ws;
    if (ioctl(renderer_.get_tty_fd(), TIOCGWINSZ, &ws) < 0 ||
        ws.ws_row == 0 || ws.ws_row > 1000 ||
        ws.ws_col == 0 || ws.ws_col > 1000) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    // Create screen and parser
    spdlog::info("Creating screen {}x{}", ws.ws_row - 1, ws.ws_col);
    screen_ = new Screen(ws.ws_row - 1, ws.ws_col);
    parser_ = new Parser(*screen_);

    // Initialize Rime IME
    spdlog::info("Initializing Rime IME");
    if (!ime_.initialize()) {
        spdlog::warn("Failed to initialize Rime IME");
    } else {
        spdlog::info("Rime IME initialized");
    }

    initialized_ = true;
    spdlog::info("App::init complete");
    return true;
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

    // 转义序列缓冲区（用于检测功能键序列）
    static std::string escape_buffer;

    // Shift+Space 检测：ESC + Space
    // 同时处理其他 ESC 序列
    if (ch == 27) {
        escape_buffer.clear();
        escape_buffer += static_cast<char>(ch);
        return;
    }

    // 处理 ESC 后续字符
    if (!escape_buffer.empty()) {
        escape_buffer += static_cast<char>(ch);

        // ESC + Space = Shift+Space，切换模式
        if (escape_buffer == "\x1b ") {
            ime_.toggle_mode();
            escape_buffer.clear();
            render();
            return;
        }

        // ESC 单独按下（超时后只有一个 ESC 字符）
        // 如果正在输入中文，取消输入；否则转发给 shell
        if (escape_buffer.size() == 2) {
            char second = escape_buffer[1];
            if (second == '[' || second == 'O') {
                // 功能键序列开始，继续收集
                return;
            }
            // 不是功能键序列，检查是否是 Shift+Space
            if (second == ' ') {
                ime_.toggle_mode();
                escape_buffer.clear();
                render();
                return;
            }
            // 其他情况：如果正在输入中文，取消输入
            if (ime_.state() == ImeState::Composing || ime_.state() == ImeState::Selecting) {
                ime_.cancel();
                escape_buffer.clear();
                render();
                return;
            }
            // 转发 ESC 和当前字符给 shell
            pty_.write(std::vector<uint8_t>(escape_buffer.begin(), escape_buffer.end()));
            escape_buffer.clear();
            return;
        }

        // 收集功能键序列直到完成
        // 功能键序列格式：ESC [ <params> <final> 或 ESC O <final>
        if (escape_buffer.size() >= 3) {
            char last = escape_buffer.back();
            // 序列以字母结尾（A-Z, a-z）
            if ((last >= 'A' && last <= 'Z') || (last >= 'a' && last <= 'z')) {
                pty_.write(std::vector<uint8_t>(escape_buffer.begin(), escape_buffer.end()));
                escape_buffer.clear();
                return;
            }
            // 序列以 ~ 结尾
            if (last == '~') {
                pty_.write(std::vector<uint8_t>(escape_buffer.begin(), escape_buffer.end()));
                escape_buffer.clear();
                return;
            }
            // 继续收集
            if (escape_buffer.size() > 10) {
                // 太长了，直接转发
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