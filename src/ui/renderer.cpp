#include "renderer.hpp"
#include "../util/utf8.hpp"
#include "../util/i18n.hpp"
#include "jsx.hpp"
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <spdlog/spdlog.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <iostream>
#include <chrono>

// Scroll state for candidate overflow
namespace {
struct ScrollState {
    bool active = false;
    size_t last_candidate_count = 0;
    int max_offset = 0;      // Maximum scroll offset (total chars to scroll)
    int current_offset = 0;  // Current display offset
    int direction = 1;       // 1 = scrolling left, -1 = scrolling back, 0 = paused
    std::chrono::steady_clock::time_point last_tick;
    int pause_counter = 0;

    void reset() {
        active = false;
        last_candidate_count = 0;
        max_offset = 0;
        current_offset = 0;
        direction = 1;
        pause_counter = 0;
    }
};
ScrollState g_scroll;

// Tick interval: 3 render calls per scroll step (renders ~60fps → ~20 steps/sec)
const int TICKS_PER_STEP = 3;
const int PAUSE_AT_END_STEPS = 30;    // Pause ~1.5s at full scroll
const int PAUSE_AT_START_STEPS = 30;  // Pause ~1.5s at start before re-scrolling
int g_tick_counter = 0;
}  // namespace

Renderer::Renderer() = default;

Renderer::~Renderer() {
    if (initialized_) {
        restore();
    }
}

void Renderer::init() {
    tty_fd_ = STDIN_FILENO;

    // Check if we're running in a terminal that supports alternate screen.
    // If not (e.g., piped input, systemd service), we can't use raw mode
    // or alternate screen, so mark as uninitialized but don't fail.
    if (!isatty(tty_fd_)) {
        spdlog::warn("Not a TTY; alternate screen and raw mode unavailable");
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

    // Set a scroll region that excludes the last row (status bar): DECSTBM
    // ESC[1;<row-1>r. Shell output then scrolls only within rows 1..row-1 and
    // never overwrites the status bar, so we don't have to repaint it on every
    // PTY byte (monkey finding F4). Resize handlers must re-issue this.
    {
        struct winsize ws;
        if (ioctl(tty_fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 1) {
            printf("\x1b[1;%dr", ws.ws_row - 1);
            printf("\x1b[H");
            fflush(stdout);
        }
    }

    initialized_ = true;
}

void Renderer::update_scroll_region() {
    if (!initialized_)
        return;
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 1) {
        printf("\x1b[1;%dr", ws.ws_row - 1);
        fflush(stdout);
    }
}

void Renderer::restore() {
    if (!initialized_)
        return;

    // Clear screen
    printf("\033[2J\033[H");
    fflush(stdout);

    // Reset scroll region to full screen (undo DECSTBM from init)
    printf("\x1b[r");
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

void Renderer::render(const Screen& /*screen*/) {
    // 直接转发 PTY 数据，不再重绘整个屏幕
}

void Renderer::forward_output(const char* data, size_t len) {
    // 直接转发 PTY 输出到终端
    fwrite(data, 1, len, stdout);
    fflush(stdout);

    // Scan for sequences that can erase/overwrite the status bar (which lives
    // on the last row, outside the scroll region). The scroll region (DECSTBM)
    // keeps normal shell scrolling off that row, but a shell `clear` (ESC[2J),
    // a line/scroll erase (ESC[J / ESC[K / ESC[<n>J / ESC[<n>K), or an absolute
    // cursor move onto the last row will clobber it. When any such sequence is
    // seen, flag the bar dirty so render_candidates() repaints it next frame
    // instead of skipping via the dedup (which otherwise hides it for up to 16
    // PTY bytes — the EN-mode "status bar disappears" bug).
    struct winsize ws;
    int last_row = 0;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        last_row = ws.ws_row;
    }
    for (size_t i = 0; i < len; ++i) {
        uint8_t c = static_cast<uint8_t>(data[i]);
        if (c != 0x1b)  // ESC
            continue;
        // Need at least ESC [ ... <final byte>. Final bytes: @A-Z[\]^_`a-z{|}~
        size_t j = i + 1;
        if (j >= len || data[j] != '[')
            continue;
        // Walk parameters (digits, ';', '?') to the final byte.
        size_t k = j + 1;
        while (k < len) {
            uint8_t p = static_cast<uint8_t>(data[k]);
            if ((p >= '0' && p <= '9') || p == ';' || p == '?') {
                ++k;
                continue;
            }
            break;
        }
        if (k >= len)
            break;
        uint8_t final = static_cast<uint8_t>(data[k]);
        std::string params(data + j + 1, data + k);
        // Full-screen / line erases wipe the bar regardless of row.
        if ((final == 'J' && (params.empty() || params == "0" || params == "2" || params == "3")) ||
            (final == 'K' && (params.empty() || params == "0" || params == "2"))) {
            bar_dirty_ = true;
            break;
        }
        // Absolute cursor positioning onto the last row: CSI <row> ; <col> H
        // or CSI <row> ; <col> f — if <row> == last_row, the shell is writing
        // on the status bar row.
        if ((final == 'H' || final == 'f') && last_row > 0) {
            // parse leading row number
            int row = 0;
            size_t p = 0;
            while (p < params.size() && params[p] >= '0' && params[p] <= '9') {
                row = row * 10 + (params[p] - '0');
                ++p;
            }
            if (row == last_row) {
                bar_dirty_ = true;
                break;
            }
        }
    }
}

void Renderer::redraw_shell(const Screen& screen) {
    if (!initialized_)
        return;
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }
    // Save cursor, clear the scroll region (the shell area, rows 1..row-1),
    // repaint each cell from the Screen grid, then restore cursor. The status
    // bar (last row) is repainted separately by render_candidates().
    printf("\x1b[s");     // save cursor
    printf("\x1b[1;1H");  // home within scroll region
    printf("\x1b[2J");    // clear (scroll region is set, but 2J clears whole screen)
    int rows = std::min(screen.rows(), static_cast<int>(ws.ws_row) - 1);
    int cols = std::min(screen.cols(), static_cast<int>(ws.ws_col));
    for (int r = 0; r < rows; ++r) {
        printf("\x1b[%d;1H", r + 1);
        std::string line;
        line.reserve(cols);
        for (int c = 0; c < cols; ++c) {
            Cell cell = screen.get(r, c);
            if (cell.ch == 0)
                line.push_back(' ');
            else
                line += utf8::encode(cell.ch);
        }
        fwrite(line.data(), 1, line.size(), stdout);
    }
    // Move the shell cursor back to where the Screen thinks it is (clamped to
    // the scroll region), so resumed shell output continues from the right spot.
    int cr = std::min(screen.cursor_row(), rows - 1);
    int cc = std::min(screen.cursor_col(), cols - 1);
    if (cr < 0)
        cr = 0;
    if (cc < 0)
        cc = 0;
    printf("\x1b[%d;%dH", cr + 1, cc + 1);
    printf("\x1b[u");  // restore cursor (redundant with explicit move, kept for safety)
    fflush(stdout);
}

void Renderer::render_element(const ui::Element& element) {
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    // 创建 FTXUI Screen 并渲染
    auto ftxui_screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(ws.ws_col), ftxui::Dimension::Fixed(1));
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

void Renderer::render_candidates(const std::vector<Candidate>& candidates, size_t selected, const std::string& buffer,
                                 const std::string& mode) {
    // Get terminal width
    struct winsize ws;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) < 0 || ws.ws_row == 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
    }

    // Dedup: when the IME is inactive (no candidates, empty buffer) the status
    // bar content is fully determined by mode, which rarely changes. Shell
    // output drives render_candidates_bar() on every PTY byte, so without dedup
    // a high-frequency shell echo (e.g. zle per-char echo, or a forwarded
    // escape sequence) repaints the whole bar N times (monkey finding F4).
    // Skip identical repaints, but force one every BAR_FORCE_REDRAW_EVERY calls
    // so a shell `clear` (which wipes the bar) is recovered shortly.
    bool ime_active = !candidates.empty() || !buffer.empty();
    if (!ime_active) {
        // If the shell just emitted a sequence that erased the bar, force a
        // repaint now regardless of the dedup signature (see forward_output).
        bool force = bar_dirty_;
        bar_dirty_ = false;
        std::string sig = mode;
        if (!force && sig == last_bar_sig_ && bar_skip_count_ < BAR_FORCE_REDRAW_EVERY) {
            ++bar_skip_count_;
            return;
        }
        last_bar_sig_ = sig;
        bar_skip_count_ = 0;
    } else {
        // IME active: always repaint (candidate bar changes), and invalidate the
        // inactive signature so the next inactive state forces a fresh draw.
        last_bar_sig_.clear();
        bar_skip_count_ = 0;
    }

    // ---- Scroll state management ----
    // Reset scroll if candidates changed
    if (candidates.size() != g_scroll.last_candidate_count) {
        g_scroll.reset();
        g_scroll.last_candidate_count = candidates.size();
    }

    int scroll_off = 0;

    if (!candidates.empty() && selected < candidates.size()) {
        // Calculate if even a single candidate overflows
        auto u32_to_utf8_str = [](const std::u32string& s) -> std::string {
            std::string result;
            for (char32_t c : s) {
                if (c != 0)
                    result += utf8::encode(c);
            }
            return result;
        };

        std::string sel_text = u32_to_utf8_str(candidates[selected].text);
        int sel_text_width = utf8::string_width(sel_text);
        // Candidate item format: " [N.XXXX] "
        // Prefix: " [N." = 4 cols
        // Suffix: "] " = 2 cols
        int cand_item_overhead = 6;  // " [N." + "] "

        // Fixed parts: mode + pinyin + cancel
        std::string mode_text = mode;
        int mode_w = 2 + utf8::string_width(mode_text) + 2;
        std::string pinyin_prefix = " " + I18n::t("status.pinyin") + ": ";
        int pinyin_w = utf8::string_width(pinyin_prefix) + utf8::string_width(buffer) + 1;
        int fixed_w = mode_w + pinyin_w;

        int available_for_text = ws.ws_col - fixed_w - cand_item_overhead;

        if (available_for_text < sel_text_width) {
            // Need scrolling
            g_scroll.active = true;
            // Count total characters in the selected text
            {
                size_t total_chars = 0;
                {
                    size_t bp = 0;
                    while (bp < sel_text.size()) {
                        int clen = utf8::char_len(static_cast<uint8_t>(sel_text[bp]));
                        if (clen < 1)
                            clen = 1;
                        bp += clen;
                        total_chars++;
                    }
                }
                g_scroll.max_offset = static_cast<int>(total_chars);
                if (g_scroll.max_offset < 1)
                    g_scroll.max_offset = 1;
            }

            // Advance scroll position
            g_tick_counter++;
            if (g_tick_counter >= TICKS_PER_STEP) {
                g_tick_counter = 0;

                if (g_scroll.direction == 1) {
                    // Scrolling left (showing later characters)
                    g_scroll.current_offset++;
                    if (g_scroll.current_offset >= g_scroll.max_offset) {
                        g_scroll.current_offset = g_scroll.max_offset;
                        g_scroll.direction = 0;  // pause at end
                        g_scroll.pause_counter = PAUSE_AT_END_STEPS;
                    }
                } else if (g_scroll.direction == -1) {
                    // Scrolling back to start
                    g_scroll.current_offset--;
                    if (g_scroll.current_offset <= 0) {
                        g_scroll.current_offset = 0;
                        g_scroll.direction = 0;  // pause at start
                        g_scroll.pause_counter = PAUSE_AT_START_STEPS;
                    }
                } else if (g_scroll.direction == 0) {
                    // Paused
                    g_scroll.pause_counter--;
                    if (g_scroll.pause_counter <= 0) {
                        if (g_scroll.current_offset >= g_scroll.max_offset) {
                            // Was at end, now scroll back
                            g_scroll.direction = -1;
                        } else {
                            // Was at start, now scroll forward
                            g_scroll.direction = 1;
                        }
                    }
                }
            }

            scroll_off = g_scroll.current_offset;
        } else {
            g_scroll.reset();
            g_scroll.last_candidate_count = candidates.size();
        }
    }

    // 使用 JSX 风格组件构建 UI
    auto element = ui::MainBar({.mode = mode,
                                .lang_name = "",  // Not used in current design
                                .candidates = candidates,
                                .selected = selected,
                                .buffer = buffer,
                                .term_width = static_cast<int>(ws.ws_col),
                                .scroll_offset = scroll_off});

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
    auto ftxui_screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(ws.ws_col), ftxui::Dimension::Fixed(ws.ws_row));

    // Render settings panel
    auto element = ui::SettingsPanel(state);
    ftxui::Render(ftxui_screen, element);

    // Clear screen and render
    printf("\x1b[2J\x1b[H");
    std::string output = ftxui_screen.ToString();
    fwrite(output.c_str(), 1, output.size(), stdout);
    fflush(stdout);
}