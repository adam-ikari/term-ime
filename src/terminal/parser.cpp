#include "parser.hpp"
#include "../util/utf8.hpp"
#include <cctype>
#include <utility>

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
    screen_.put(ch, row, col);
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
        screen_.move_cursor(row - 1, col - 1);
        break;
    }
    case 'A':  // Cursor up
        screen_.move_cursor(screen_.cursor_row() - 1, screen_.cursor_col());
        break;
    case 'B':  // Cursor down
        screen_.move_cursor(screen_.cursor_row() + 1, screen_.cursor_col());
        break;
    case 'C':  // Cursor forward
        screen_.move_cursor(screen_.cursor_row(), screen_.cursor_col() + 1);
        break;
    case 'D':  // Cursor back
        screen_.move_cursor(screen_.cursor_row(), screen_.cursor_col() - 1);
        break;
    case 'J':  // Erase display
        if (csi_params_ == "2") {
            screen_.clear();
        }
        break;
    case 'K':  // Erase line
        screen_.clear_line();
        break;
    case 'm':  // SGR (colors) - ignore for now
        break;
    }
}
