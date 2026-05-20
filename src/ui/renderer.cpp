#include "renderer.hpp"
#include "../util/utf8.hpp"
#include "../util/i18n.hpp"
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/dom/elements.hpp>
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

ftxui::Element Renderer::build_empty_bar(const std::string& mode) const {
    using namespace ftxui;

    // Mode indicator
    Color mode_color = (mode.find("中文") != std::string::npos) ? Color::Green : Color::Cyan;
    auto mode_indicator = hbox({
        text(" 【") | dim,
        text(mode) | bold | color(mode_color),
        text("】 ") | dim,
    });

    // Hints with i18n
    auto hints = hbox({
        text(" Ctrl+A,Space " + I18n::t("hint.toggle_mode") + " ") | dim | color(Color::GrayDark),
        text("│") | color(Color::GrayDark),
        text(" 1-9 " + I18n::t("hint.select") + " ") | dim | color(Color::GrayDark),
        text("│") | color(Color::GrayDark),
        text(" Esc " + I18n::t("hint.cancel") + " ") | dim | color(Color::GrayDark),
        text("│") | color(Color::GrayDark),
        text(" Ctrl+A,A " + I18n::t("hint.ai_toggle") + " ") | dim | color(Color::GrayDark),
    });

    return hbox({
        mode_indicator,
        filler(),
        hints,
    }) | inverted | size(HEIGHT, EQUAL, 1);
}

ftxui::Element Renderer::build_candidate_bar(const std::vector<Candidate>& candidates,
                                              size_t selected, const std::string& buffer,
                                              const std::string& mode) const {
    using namespace ftxui;

    std::vector<Element> items;

    // Mode indicator
    Color mode_color = (mode.find("中文") != std::string::npos) ? Color::Green : Color::Cyan;
    items.push_back(hbox({
        text(" 【") | dim,
        text(mode) | bold | color(mode_color),
        text("】 ") | dim,
    }));

    // Pinyin buffer
    items.push_back(text(" " + I18n::t("status.pinyin") + ": " + buffer + " ") | bold);

    // 候选词
    for (size_t i = 0; i < candidates.size() && i < 9; ++i) {
        std::string text_str;
        for (char32_t ch : candidates[i].text) {
            if (ch != 0) {
                text_str += utf8::encode(ch);
            }
        }

        std::string label = std::to_string(i + 1) + "." + text_str;
        if (i == selected) {
            items.push_back(text(" [" + label + "] ") | bold | bgcolor(Color::Blue));
        } else {
            items.push_back(text(" " + label + " ") | color(Color::Yellow));
        }
    }

    // Cancel hint
    items.push_back(text("  Esc" + I18n::t("hint.cancel") + " ") | dim | color(Color::GrayDark));

    return hbox(items) | inverted | size(HEIGHT, EQUAL, 1);
}

void Renderer::render_candidates(const std::vector<Candidate>& candidates,
                                  size_t selected, const std::string& buffer,
                                  const std::string& mode) {
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    // 使用 FTXUI 构建 Element
    ftxui::Element element;
    if (candidates.empty()) {
        element = build_empty_bar(mode);
    } else {
        element = build_candidate_bar(candidates, selected, buffer, mode);
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