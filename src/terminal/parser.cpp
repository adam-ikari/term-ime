#include "parser.hpp"
#include "../util/utf8.hpp"
#include <cctype>
#include <utility>
#include <vector>

namespace {
// Parse a decimal parameter substring; empty or out-of-range strings map to
// `def` (the terminal default), so "ESC[H" and malformed numbers stay safe.
int parse_param(const std::string& s, int def) {
    if (s.empty())
        return def;
    try {
        size_t used = 0;
        int v = std::stoi(s, &used);
        if (used != s.size() || v < 0 || v > 1000)
            return def;
        return v;
    } catch (const std::exception&) {
        return def;
    }
}
}  // namespace

Parser::Parser(Screen& screen) : screen_(screen) {}

void Parser::feed(const uint8_t* data, size_t len) {
    // Join the tail of a sequence left incomplete by an earlier feed() call so
    // that no character is decoded in two halves.
    std::string joined;
    if (!pending_.empty()) {
        joined = std::move(pending_);
        pending_.clear();
        joined.append(reinterpret_cast<const char*>(data), len);
        data = reinterpret_cast<const uint8_t*>(joined.data());
        len = joined.size();
    }

    size_t pos = 0;
    while (pos < len) {
        uint8_t byte = data[pos];

        if (byte < 0x80) {
            handle_char(static_cast<char>(byte));
            ++pos;
            continue;
        }

        size_t need = static_cast<size_t>(utf8::char_len(byte));
        if (need <= 1) {
            // Stray continuation byte or impossible lead byte.
            emit_char(U'\uFFFD');
            ++pos;
            continue;
        }
        if (pos + need > len) {
            // Truncated at the end of this chunk; decode once the rest arrives.
            pending_.assign(reinterpret_cast<const char*>(data + pos), len - pos);
            return;
        }

        size_t seq_pos = 0;
        char32_t ch = utf8::decode(data + pos, need, seq_pos);
        if (seq_pos == need) {
            emit_char(ch);
            pos += need;
            continue;
        }
        // Malformed sequence: replace the maximal subpart (lead byte plus the
        // continuation bytes after it) with one U+FFFD and resync there, so a
        // following ASCII byte is not swallowed.
        size_t subpart = 1;
        while (subpart < need && (data[pos + subpart] & 0xC0) == 0x80) {
            ++subpart;
        }
        emit_char(U'\uFFFD');
        pos += subpart;
    }
}

int Parser::next_row(int row) {
    if (row + 1 >= screen_.rows()) {
        screen_.scroll_up();
        return screen_.rows() - 1;
    }
    return row + 1;
}

void Parser::emit_char(char32_t ch) {
    int width = utf8::width(ch);
    int row = screen_.cursor_row();
    int col = screen_.cursor_col();

    if (width > 0 && wrap_pending_) {
        wrap_pending_ = false;
        row = next_row(row);
        col = 0;
    }
    if (width > 0 && col + width > screen_.cols()) {
        // Not enough room left for the glyph (a double-width char in the last
        // column would be split): wrap first.
        row = next_row(row);
        col = 0;
    }

    screen_.move_cursor(row, col);
    screen_.put(ch, row, col, pen_);
    if (width <= 0) {
        return;  // combining mark: occupies the cell without advancing
    }

    col += width;
    if (col >= screen_.cols()) {
        // Deferred wrap: park on the right margin, wrap on the next glyph.
        col = screen_.cols() - 1;
        wrap_pending_ = true;
    }
    screen_.move_cursor(row, col);
}

void Parser::handle_char(char c) {
    switch (state_) {
    case State::Normal:
        if (c == '\x1b') {
            state_ = State::Escape;
        } else if (c == '\r') {
            wrap_pending_ = false;
            screen_.move_cursor(screen_.cursor_row(), 0);
        } else if (c == '\n') {
            wrap_pending_ = false;
            screen_.move_cursor(next_row(screen_.cursor_row()), screen_.cursor_col());
        } else if (c == '\b') {
            wrap_pending_ = false;
            if (screen_.cursor_col() > 0) {
                screen_.move_cursor(screen_.cursor_row(), screen_.cursor_col() - 1);
            }
        } else if (c == '\t') {
            // Tab advances to the next 8-column stop; skipped columns stay
            // blank (a real terminal shows whitespace). Clamped at the right
            // margin, parking there defers the wrap to the next glyph exactly
            // like a character written in the last column.
            const int col = screen_.cursor_col();
            int next = ((col / 8) + 1) * 8;
            if (next > screen_.cols() - 1)
                next = screen_.cols() - 1;
            wrap_pending_ = (next == screen_.cols() - 1);
            screen_.move_cursor(screen_.cursor_row(), next);
        } else if (static_cast<unsigned char>(c) >= 0x20) {
            emit_char(static_cast<char32_t>(c));
        }
        break;

    case State::Escape:
        if (c == '[') {
            state_ = State::CSI;
            csi_params_.clear();
            csi_private_ = false;
            csi_intermediate_ = false;
        } else if (c == ']' || c == 'P' || c == 'X' || c == '^' || c == '_') {
            // OSC / DCS / SOS / PM / APC: swallow the body, it is not text.
            state_ = State::OSC;
        } else {
            state_ = State::Normal;
        }
        break;

    case State::CSI:
        handle_csi(c);
        break;

    case State::OSC:
        if (c == '\x07') {
            state_ = State::Normal;  // BEL terminates
        } else if (c == '\x1b') {
            state_ = State::OSCEsc;  // possible ST
        }
        break;

    case State::OSCEsc:
        // ESC '\' is ST; any other byte resumes the control string body.
        state_ = (c == '\\') ? State::Normal : State::OSC;
        break;
    }
}

void Parser::handle_csi(char c) {
    unsigned char byte = static_cast<unsigned char>(c);
    if (byte >= 0x30 && byte <= 0x3F) {
        // Parameter bytes, including the private prefixes '?', '>', '!', '<'.
        if (std::isdigit(byte) || c == ';') {
            // Cap the buffer: a hostile/huge CSI must not grow memory
            // unboundedly; once over the cap the tail is dropped.
            if (csi_params_.size() < 4096)
                csi_params_ += c;
        } else {
            csi_private_ = true;
        }
        return;
    }
    if (byte >= 0x20 && byte <= 0x2F) {
        csi_intermediate_ = true;  // e.g. ESC[1 q
        return;
    }
    state_ = State::Normal;
    if (byte < 0x40 || byte > 0x7E) {
        return;  // malformed or aborted (ESC/CAN/SUB) sequence: drop it
    }
    if (csi_private_ || csi_intermediate_) {
        return;  // private/unknown sequence: consumed and never written to the grid
    }
    // Final character
    switch (c) {
    case 'H':  // Cursor position
    case 'f': {
        int row = 1, col = 1;
        size_t pos = csi_params_.find(';');
        if (pos != std::string::npos) {
            try {
                row = std::stoi(csi_params_.substr(0, pos));
                col = std::stoi(csi_params_.substr(pos + 1));
            } catch (const std::exception&) {
                // Malformed CSI parameters, ignore
            }
        } else if (!csi_params_.empty()) {
            try {
                row = std::stoi(csi_params_);
            } catch (const std::exception&) {
                // Malformed CSI parameter, ignore
            }
        }
        wrap_pending_ = false;  // explicit positioning cancels deferred wrap
        screen_.move_cursor(row - 1, col - 1);
        break;
    }
    case 'A':  // Cursor up (CUU): n rows, clamped at the top margin
        wrap_pending_ = false;
        screen_.move_cursor(screen_.cursor_row() - parse_param(csi_params_, 1), screen_.cursor_col());
        break;
    case 'B':  // Cursor down (CUD): n rows, clamped at the bottom margin
        wrap_pending_ = false;
        screen_.move_cursor(screen_.cursor_row() + parse_param(csi_params_, 1), screen_.cursor_col());
        break;
    case 'C':  // Cursor forward (CUF): n columns, clamped at the right margin
        wrap_pending_ = false;
        screen_.move_cursor(screen_.cursor_row(), screen_.cursor_col() + parse_param(csi_params_, 1));
        break;
    case 'D':  // Cursor back (CUB): n columns, clamped at the left margin
        wrap_pending_ = false;
        screen_.move_cursor(screen_.cursor_row(), screen_.cursor_col() - parse_param(csi_params_, 1));
        break;
    case 'G':  // Cursor horizontal absolute (CHA): column n, row unchanged
        wrap_pending_ = false;
        screen_.move_cursor(screen_.cursor_row(), parse_param(csi_params_, 1) - 1);
        break;
    case 'd':  // Cursor vertical absolute (VPA): row n, column unchanged
        wrap_pending_ = false;
        screen_.move_cursor(parse_param(csi_params_, 1) - 1, screen_.cursor_col());
        break;
    case 'X':  // Erase character (ECH): n blank cells right of the cursor
        screen_.erase_cells(screen_.cursor_row(), screen_.cursor_col(), parse_param(csi_params_, 1), pen_);
        break;
    case 's':  // Save cursor (SCOSC)
        saved_row_ = screen_.cursor_row();
        saved_col_ = screen_.cursor_col();
        break;
    case 'u':  // Restore cursor (SCORC)
        wrap_pending_ = false;
        screen_.move_cursor(saved_row_, saved_col_);
        break;
    case 'J': {  // Erase display
        int mode = parse_param(csi_params_, 0);
        if (mode == 5)
            mode = 2;  // no scrollback in this project: 5 behaves like 2
        screen_.erase_display(mode, pen_);
        break;
    }
    case 'K':  // Erase line
        screen_.erase_line(parse_param(csi_params_, 0), pen_);
        break;
    case 'm':  // SGR
        apply_sgr();
        break;
    }
}

// Split the ';'-separated SGR parameters and apply them to `pen_`.
void Parser::apply_sgr() {
    std::vector<int> args;
    size_t start = 0;
    for (;;) {
        const size_t sep = csi_params_.find(';', start);
        const std::string field = csi_params_.substr(start, sep == std::string::npos ? std::string::npos : sep - start);
        // An empty field means 0, so "ESC[m" and "ESC[;m" reset like "ESC[0m".
        args.push_back(field.empty() ? 0 : parse_param(field, -1));
        if (sep == std::string::npos)
            break;
        start = sep + 1;
    }

    for (size_t i = 0; i < args.size(); ++i) {
        const int n = args[i];
        if (n < 0)
            continue;  // malformed code: skipped, pen unchanged

        // 38/48 introduce 256-color (5;n) or truecolor (2;r;g;b). The sub-
        // arguments must be consumed here or "5" would read as a bg lift.
        if (n == 38 || n == 48) {
            const bool is_fg = (n == 38);
            auto& ext = is_fg ? pen_.fg_extended : pen_.bg_extended;
            auto& tc = is_fg ? pen_.fg_truecolor : pen_.bg_truecolor;
            auto& idx = is_fg ? pen_.fg_index : pen_.bg_index;
            auto& rgb = is_fg ? pen_.fg_rgb : pen_.bg_rgb;
            auto& plain = is_fg ? pen_.fg : pen_.bg;
            auto& lift = is_fg ? pen_.bright : pen_.bg_bright;
            // Sub-argument clamped to [0,255]; missing or malformed means 0.
            auto clamp8 = [&](size_t pos) -> uint8_t {
                if (pos < args.size() && args[pos] >= 0) {
                    return static_cast<uint8_t>(args[pos] > 255 ? 255 : args[pos]);
                }
                return 0;
            };
            if (i + 1 < args.size() && args[i + 1] == 2) {
                // Truecolor: 38;2;r;g;b. Missing components default to 0.
                ext = true;
                tc = true;
                rgb = Rgb{clamp8(i + 2), clamp8(i + 3), clamp8(i + 4)};
                i += 4;
            } else if (i + 1 < args.size() && args[i + 1] == 5) {
                // 256-color: 38;5;n. Missing index defaults to 0.
                ext = true;
                tc = false;
                idx = clamp8(i + 2);
                i += 2;
            } else if (i + 1 < args.size()) {
                i += 1;  // unknown sub-form: ignored, pen unchanged
            } else {
                // Bare 38/48 (no sub-arguments): restore the default like 39/49.
                i += 1;
                plain = is_fg ? 7 : 0;
                lift = false;
                ext = false;
                tc = false;
            }
            continue;
        }
        switch (n) {
        case 0:
            pen_ = Pen{};
            break;
        case 1:
            pen_.bright = true;
            break;
        case 22:
            pen_.bright = false;
            break;
        case 5:
            pen_.bg_bright = true;
            break;
        case 25:
            pen_.bg_bright = false;
            break;
        case 7:
            pen_.reverse = true;
            break;
        case 27:
            pen_.reverse = false;
            break;
        case 39:
            pen_.fg = 7;
            pen_.bright = false;
            pen_.fg_extended = false;
            pen_.fg_truecolor = false;
            break;
        case 49:
            pen_.bg = 0;
            pen_.bg_bright = false;
            pen_.bg_extended = false;
            pen_.bg_truecolor = false;
            break;
        default:
            if (n >= 30 && n <= 37) {
                pen_.fg = static_cast<uint8_t>(n - 30);
                pen_.bright = false;
                pen_.fg_extended = false;
                pen_.fg_truecolor = false;
            } else if (n >= 90 && n <= 97) {
                pen_.fg = static_cast<uint8_t>(n - 90);
                pen_.bright = true;
                pen_.fg_extended = false;
                pen_.fg_truecolor = false;
            } else if (n >= 40 && n <= 47) {
                pen_.bg = static_cast<uint8_t>(n - 40);
                pen_.bg_bright = false;
                pen_.bg_extended = false;
                pen_.bg_truecolor = false;
            } else if (n >= 100 && n <= 107) {
                pen_.bg = static_cast<uint8_t>(n - 100);
                pen_.bg_bright = true;
                pen_.bg_extended = false;
                pen_.bg_truecolor = false;
            }
            // Any other code: ignored.
            break;
        }
    }
}
