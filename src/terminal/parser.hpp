#pragma once

#include "screen.hpp"
#include <cstdint>
#include <string>

class Parser {
   public:
    Parser(Screen& screen);

    void feed(const uint8_t* data, size_t len);

   private:
    Screen& screen_;
    // Normal -> Escape -> CSI/OSCEsc; OSC covers every ESC-terminated string
    // control (OSC/DCS/SOS/PM/APC) whose body must never reach the grid.
    enum class State { Normal, Escape, CSI, OSC, OSCEsc };
    State state_ = State::Normal;
    std::string csi_params_;
    bool csi_private_ = false;       // 0x3C-0x3F prefix seen ('?', '>', '!', '<')
    bool csi_intermediate_ = false;  // 0x20-0x2F intermediate byte seen
    // Tail of a UTF-8 sequence split across two feed() calls. Bounded by the
    // longest UTF-8 sequence (4 bytes) minus one.
    std::string pending_;
    // Cursor parked on the right margin by deferred auto-wrap: the wrap itself
    // happens when the next printable glyph arrives.
    bool wrap_pending_ = false;

    void handle_char(char c);
    void handle_csi(char c);
    void emit_char(char32_t ch);

    // Row below `row`, scrolling the screen when already on the last row.
    int next_row(int row);
};
