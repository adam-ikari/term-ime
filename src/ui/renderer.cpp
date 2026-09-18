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
    // The status bar is laid out from the terminal width, so a resize must
    // force the next render_candidates() to repaint it. Without this the mode
    // signature still matches and the dedup skips the draw, leaving the bar
    // rendered for the previous width while the rest of the screen is redrawn.
    last_bar_sig_.clear();
    bar_skip_count_ = 0;
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
    // a line/scroll erase (ESC[J / ESC[K / ESC[<n>J / ESC[<n>K), or a cursor
    // move onto the last row (absolute H/f, relative A/B) will clobber it.
    // When any such sequence is seen, flag the bar dirty so render_candidates()
    // repaints it next frame instead of skipping via the dedup (which otherwise
    // hides it for up to 16 PTY bytes — the EN-mode "status bar disappears" bug).
    // A CSI cut off by a chunk boundary is stitched back via pending_csi_.
    struct winsize ws;
    int last_row = 0;
    if (ioctl(tty_fd_, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) {
        last_row = ws.ws_row;
    }
    std::string scan;
    scan.reserve(pending_csi_.size() + len);
    scan += pending_csi_;
    scan.append(data, len);
    pending_csi_.clear();
    for (size_t i = 0; i < scan.size(); ++i) {
        uint8_t c = static_cast<uint8_t>(scan[i]);
        // LF inside the scroll region (rows 1..last_row-1) advances the tracked
        // cursor row, keeping the relative-move check below accurate.
        if (c == '\n' && last_row > 0 && cur_row_ < last_row - 1) {
            ++cur_row_;
            continue;
        }
        if (c != 0x1b)  // ESC
            continue;
        size_t j = i + 1;
        if (j >= scan.size()) {
            // Chunk ends on a lone ESC — likely the start of a split sequence.
            pending_csi_ = scan.substr(i);
            break;
        }
        if (scan[j] != '[')
            continue;  // not CSI (OSC, charset select, stray ESC)
        // Walk parameters (digits, ';', '?') to the final byte.
        size_t k = j + 1;
        while (k < scan.size()) {
            uint8_t p = static_cast<uint8_t>(scan[k]);
            if ((p >= '0' && p <= '9') || p == ';' || p == '?') {
                ++k;
                continue;
            }
            break;
        }
        if (k >= scan.size()) {
            // CSI not complete in this chunk: carry the bytes so the final byte
            // (arriving in a later chunk) is still scanned.
            pending_csi_ = scan.substr(i);
            break;
        }
        uint8_t final = static_cast<uint8_t>(scan[k]);
        std::string params(scan.data() + j + 1, k - (j + 1));
        // First numeric parameter; missing or empty means the CSI default (1).
        auto nparam = [&]() {
            int n = 0;
            bool any = false;
            for (char p : params) {
                if (p < '0' || p > '9')
                    break;
                n = n * 10 + (p - '0');
                any = true;
            }
            return any ? n : 1;
        };
        // Full-screen / line erases wipe the bar regardless of row.
        if ((final == 'J' && (params.empty() || params == "0" || params == "2" || params == "3")) ||
            (final == 'K' && (params.empty() || params == "0" || params == "2"))) {
            bar_dirty_ = true;
            continue;
        }
        // From-start erases (J=1 / K=1) reach the bar only when the cursor is
        // already sitting on the bar row.
        if (params == "1" && (final == 'J' || final == 'K') && last_row > 0 && cur_row_ == last_row) {
            bar_dirty_ = true;
            continue;
        }
        // Absolute cursor positioning: CSI <row> ; <col> H / f — a row equal to
        // last_row means the shell is about to write on the status-bar row.
        if ((final == 'H' || final == 'f') && last_row > 0) {
            cur_row_ = nparam();
            if (cur_row_ == last_row) {
                bar_dirty_ = true;
                continue;
            }
        }
        // Relative vertical moves (CUU/CUD): the scroll region constrains
        // scrolling, not cursor motion, so a move onto the bar row clobbers it.
        if ((final == 'A' || final == 'B') && last_row > 0) {
            int n = nparam();
            cur_row_ = (final == 'A') ? (cur_row_ - n < 1 ? 1 : cur_row_ - n)
                                      : (cur_row_ + n > last_row ? last_row : cur_row_ + n);
            if (cur_row_ == last_row) {
                bar_dirty_ = true;
                continue;
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
    // Clear the scroll region (the shell area, rows 1..row-1), repaint each
    // cell from the Screen grid, then move the cursor to where the Screen
    // thinks it is. The status bar (last row) is repainted separately by
    // render_candidates().
    printf("\x1b[1;1H");  // home within scroll region
    printf("\x1b[2J");    // clear (scroll region is set, but 2J clears whole screen)
    // The 2J above also wiped the status-bar row (the bar is not part of the
    // Screen grid). Invalidate the dedup signature here, or the next
    // render_candidates() call is skipped and the bar stays blank after the
    // settings panel closes.
    last_bar_sig_.clear();
    bar_skip_count_ = 0;
    int rows = std::min(screen.rows(), static_cast<int>(ws.ws_row) - 1);
    int cols = std::min(screen.cols(), static_cast<int>(ws.ws_col));
    // Rendered attributes of one cell: everything that maps to an SGR sequence.
    // Extended colors join the 16-color fields so "same color, same SGR head".
    struct Attr {
        uint8_t fg = 7;
        uint8_t bg = 0;
        bool bright = false;
        bool bg_bright = false;
        bool reverse = false;
        bool fg_extended = false;
        bool bg_extended = false;
        bool fg_truecolor = false;
        bool bg_truecolor = false;
        uint8_t fg_index = 0;
        uint8_t bg_index = 0;
        Rgb fg_rgb{0, 0, 0};
        Rgb bg_rgb{0, 0, 0};
        bool operator==(const Attr& o) const {
            return fg == o.fg && bg == o.bg && bright == o.bright && bg_bright == o.bg_bright && reverse == o.reverse &&
                   fg_extended == o.fg_extended && bg_extended == o.bg_extended && fg_truecolor == o.fg_truecolor &&
                   bg_truecolor == o.bg_truecolor && fg_index == o.fg_index && bg_index == o.bg_index &&
                   fg_rgb.r == o.fg_rgb.r && fg_rgb.g == o.fg_rgb.g && fg_rgb.b == o.fg_rgb.b &&
                   bg_rgb.r == o.bg_rgb.r && bg_rgb.g == o.bg_rgb.g && bg_rgb.b == o.bg_rgb.b;
        }
    };
    const Attr kDefault{};
    // Full SGR select for an attribute set (16-color, 256-color and truecolor).
    auto sgr_for = [](const Attr& a) {
        std::string s = "\x1b[";
        bool first = true;
        auto add = [&](int code) {
            if (!first)
                s += ';';
            first = false;
            s += std::to_string(code);
        };
        if (a.reverse)
            add(7);
        if (a.fg_extended) {
            if (a.fg_truecolor) {
                add(38);
                add(2);
                add(a.fg_rgb.r);
                add(a.fg_rgb.g);
                add(a.fg_rgb.b);
            } else {
                add(38);
                add(5);
                add(a.fg_index);
            }
        } else {
            add(a.bright ? 90 + a.fg : 30 + a.fg);
        }
        if (a.bg_extended) {
            if (a.bg_truecolor) {
                add(48);
                add(2);
                add(a.bg_rgb.r);
                add(a.bg_rgb.g);
                add(a.bg_rgb.b);
            } else {
                add(48);
                add(5);
                add(a.bg_index);
            }
        } else {
            add(a.bg_bright ? 100 + a.bg : 40 + a.bg);
        }
        s += 'm';
        return s;
    };
    for (int r = 0; r < rows; ++r) {
        std::string line;
        line.reserve(cols + 16);
        line += "\x1b[";
        line += std::to_string(r + 1);
        line += ";1H";
        line += "\x1b[0m";  // start each row at the default pen
        Attr cur = kDefault;
        for (int c = 0; c < cols; ++c) {
            Cell cell = screen.get(r, c);
            Attr a{cell.fg,          cell.bg,          cell.bright,       cell.bg_bright,    cell.reverse,
                   cell.fg_extended, cell.bg_extended, cell.fg_truecolor, cell.bg_truecolor, cell.fg_index,
                   cell.bg_index,    cell.fg_rgb,      cell.bg_rgb};
            if (!(a == cur)) {
                // Consecutive cells sharing attributes emit one SGR head only.
                line += (a == kDefault) ? std::string("\x1b[0m") : sgr_for(a);
                cur = a;
            }
            if (cell.ch == 0) {
                line.push_back(' ');
            } else {
                line += utf8::encode(cell.ch);
                // A wide glyph spans two grid columns: the left half carries the
                // character (wide=true), the right half is ch==0 && wide=true.
                // Skip the right half so the glyph is not followed by a stray
                // space (which would misalign everything after it).
                if (cell.wide)
                    ++c;
            }
        }
        line += "\x1b[0m";  // never let a row's color bleed past its end
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
    cur_row_ = cr + 1;
    // Re-show the cursor: a fullscreen overlay (or forwarded shell sequence)
    // may have sent ?25l, and without this the cursor stays invisible after
    // the repaint.
    printf("\x1b[?25h");
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
                                 const std::string& mode, int max_items) {
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
        // Also consume any pending bar_dirty_ (the repaint below restores the
        // bar) so it does not linger and force a redundant draw later.
        last_bar_sig_.clear();
        bar_skip_count_ = 0;
        bar_dirty_ = false;
    }

    // 使用 JSX 风格组件构建 UI
    auto element = ui::MainBar({.mode = mode,
                                .lang_name = "",  // Not used in current design
                                .candidates = candidates,
                                .selected = selected,
                                .buffer = buffer,
                                .term_width = static_cast<int>(ws.ws_col),
                                .max_items = max_items});

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