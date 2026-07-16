#include "parser.hpp"
#include "../util/utf8.hpp"
#include <cctype>

Parser::Parser(Screen& screen) : screen_(screen) {}

void Parser::feed(const uint8_t* data, size_t len) {
    size_t pos = 0;
    while (pos < len) {
        // Check for UTF-8 multi-byte sequence
        int char_len = utf8::char_len(data[pos]);
        if (char_len > 1 && pos + char_len <= len) {
            // Decode UTF-8 character
            char32_t ch = utf8::decode(data, len, pos);
            int width = utf8::width(ch);

            screen_.put(ch, screen_.cursor_row(), screen_.cursor_col());

            // Move cursor by display width (2 for CJK, 1 for others)
            int new_col = std::min(screen_.cursor_col() + width, screen_.cols() - 1);
            screen_.move_cursor(screen_.cursor_row(), new_col);
        } else {
            char c = static_cast<char>(data[pos++]);
            handle_char(c);
        }
    }
}

void Parser::handle_char(char c) {
    switch (state_) {
    case State::Normal:
        if (c == '\x1b') {
            state_ = State::Escape;
        } else if (c == '\r') {
            screen_.move_cursor(screen_.cursor_row(), 0);
        } else if (c == '\n') {
            if (screen_.cursor_row() + 1 >= screen_.rows()) {
                screen_.scroll_up();
            } else {
                screen_.move_cursor(screen_.cursor_row() + 1, screen_.cursor_col());
            }
        } else if (c == '\b') {
            if (screen_.cursor_col() > 0) {
                screen_.move_cursor(screen_.cursor_row(), screen_.cursor_col() - 1);
            }
        } else if (static_cast<unsigned char>(c) >= 0x20) {
            screen_.put(static_cast<char32_t>(c), screen_.cursor_row(), screen_.cursor_col());
            screen_.move_cursor(screen_.cursor_row(), std::min(screen_.cursor_col() + 1, screen_.cols() - 1));
        }
        break;

    case State::Escape:
        if (c == '[') {
            state_ = State::CSI;
            csi_params_.clear();
        } else {
            state_ = State::Normal;
        }
        break;

    case State::CSI:
        handle_csi(c);
        break;
    }
}

void Parser::handle_csi(char c) {
    if (std::isdigit(c) || c == ';') {
        csi_params_ += c;
    } else {
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
        state_ = State::Normal;
    }
}
