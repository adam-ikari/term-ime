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
    enum class State { Normal, Escape, CSI };
    State state_ = State::Normal;
    std::string csi_params_;

    void handle_char(char c);
    void handle_csi(char c);
};
