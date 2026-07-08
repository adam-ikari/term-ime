#pragma once

#include <vector>
#include <cstdint>

struct Cell {
    char32_t ch = U' ';
    uint8_t fg = 7;
    uint8_t bg = 0;
    bool wide = false;
};

class Screen {
   public:
    Screen(int rows, int cols);

    void put(char32_t ch, int row, int col);
    Cell get(int row, int col) const;

    void move_cursor(int row, int col);
    int cursor_row() const;
    int cursor_col() const;

    void scroll_up(int n = 1);
    void clear();
    void clear_line();

    int rows() const;
    int cols() const;
    void resize(int rows, int cols);

   private:
    std::vector<std::vector<Cell>> grid_;
    int cursor_row_ = 0;
    int cursor_col_ = 0;
    int rows_ = 0;
    int cols_ = 0;
};
