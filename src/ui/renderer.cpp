#include "renderer.hpp"
#include "../util/utf8.hpp"
#include "../util/i18n.hpp"
#include "jsx.hpp"
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <iostream>

Renderer::Renderer() = default;

Renderer::~Renderer() {
    if (initialized_) {
        restore();
    }
}

void Renderer::init() {
    tty_fd_ = STDIN_FILENO;

    // 检查是否是 TTY
    if (!isatty(tty_fd_)) {
        // 非 TTY 环境，不初始化
        initialized_ = false;
        return;
    }

    // Save current terminal settings
    saved_termios_ = new termios;
    if (tcgetattr(tty_fd_, saved_termios_) < 0) {
        delete saved_termios_;
        saved_termios_ = nullptr;
        initialized_ = false;
        return;
    }

    // Switch to alternate screen buffer first
    // This preserves the original screen content
    printf("\033[?1049h");
    fflush(stdout);

    // Clear screen
    // ESC[2J = clear entire screen
    // ESC[H = move cursor to home
    printf("\033[2J\033[H");
    fflush(stdout);

    // Set raw mode:
    // - 关闭 ICANON (行缓冲) - 立即读取每个字符
    // - 关闭 ECHO - PTY 会回显，不需要本地回显
    // - 关闭 ISIG - 捕获 Ctrl+C 等信号
    termios raw = *saved_termios_;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(tty_fd_, TCSAFLUSH, &raw);

    initialized_ = true;
}

void Renderer::restore() {
    if (!initialized_) return;

    // Clear screen
    printf("\033[2J\033[H");
    fflush(stdout);

    // Switch back to main screen buffer
    // This restores the original terminal content
    printf("\033[?1049l");
    fflush(stdout);

    // Restore terminal settings
    if (saved_termios_) {
        tcsetattr(tty_fd_, TCSAFLUSH, saved_termios_);
        delete saved_termios_;
        saved_termios_ = nullptr;
    }

    initialized_ = false;
}

void Renderer::render(const Screen& screen) {
    // 直接转发 PTY 数据，不再重绘整个屏幕
}

void Renderer::forward_output(const char* data, size_t len) {
    // 直接转发 PTY 输出到终端
    fwrite(data, 1, len, stdout);
    fflush(stdout);
}

void Renderer::render_element(const ui::Element& element) {
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    // 创建 FTXUI Screen 并渲染
    auto ftxui_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(ws.ws_col),
        ftxui::Dimension::Fixed(1)
    );
    ftxui::Render(ftxui_screen, element);

    // 保存当前光标位置
    printf("\x1b[s");

    // 移动到最后一行
    printf("\x1b[%d;1H", ws.ws_row);

    // 清除该行
    printf("\x1b[2K");

    // 输出 FTXUI 渲染结果
    std::string output = ftxui_screen.ToString();
    fwrite(output.c_str(), 1, output.size(), stdout);

    // 恢复光标位置
    printf("\x1b[u");
    fflush(stdout);
}

void Renderer::render_candidates(const std::vector<Candidate>& candidates,
                                  size_t selected, const std::string& buffer,
                                  const std::string& mode) {
    render_candidates_ex(candidates, selected, buffer, mode, false, false, false, 0);
}

void Renderer::render_candidates_ex(const std::vector<Candidate>& candidates,
                                     size_t selected, const std::string& buffer,
                                     const std::string& mode,
                                     bool ai_enabled, bool ai_loading,
                                     bool downloading, int download_progress) {
    // 使用 JSX 风格组件构建 UI
    auto element = ui::MainBar({
        .mode = mode,
        .lang_name = "",  // Not used in current design
        .candidates = candidates,
        .selected = selected,
        .buffer = buffer,
        .ai_enabled = ai_enabled,
        .ai_loading = ai_loading,
        .downloading = downloading,
        .download_progress = download_progress
    });

    render_element(element);
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

void Renderer::render_settings(ui::SettingsState& state) {
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    // Create FTXUI Screen for full screen
    auto ftxui_screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(ws.ws_col),
        ftxui::Dimension::Fixed(ws.ws_row)
    );

    // Render settings panel
    auto element = ui::SettingsPanel(state);
    ftxui::Render(ftxui_screen, element);

    // Clear screen and render
    printf("\x1b[2J\x1b[H");
    std::string output = ftxui_screen.ToString();
    fwrite(output.c_str(), 1, output.size(), stdout);
    fflush(stdout);
}