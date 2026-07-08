#include "screen.hpp"
#include "../util/utf8.hpp"

Screen::Screen(int rows, int cols) {
    resize(rows, cols);
}

void Screen::put(char32_t ch, int row, int col) {
    if (row >= 0 && row < rows_ && col >= 0 && col < cols_) {
        grid_[row][col].ch = ch;
        // Use utf8proc for proper width detection (CJK, emojis, etc.)
        grid_[row][col].wide = (utf8::width(ch) == 2);
    }
}

Cell Screen::get(int row, int col) const {
    if (row >= 0 && row < rows_ && col >= 0 && col < cols_) {
        return grid_[row][col];
    }
    return {};
}

void Screen::move_cursor(int row, int col) {
    cursor_row_ = std::max(0, std::min(row, rows_ - 1));
    cursor_col_ = std::max(0, std::min(col, cols_ - 1));
}

int Screen::cursor_row() const {
    return cursor_row_;
}
int Screen::cursor_col() const {
    return cursor_col_;
}

void Screen::scroll_up(int n) {
    for (int i = 0; i < n; ++i) {
        grid_.erase(grid_.begin());
        grid_.emplace_back(cols_);
    }
}

void Screen::clear() {
    for (auto& row : grid_) {
        for (auto& cell : row) {
            cell = Cell{};
        }
    }
}

void Screen::clear_line() {
    if (cursor_row_ >= 0 && cursor_row_ < rows_) {
        for (auto& cell : grid_[cursor_row_]) {
            cell = Cell{};
        }
    }
}

int Screen::rows() const {
    return rows_;
}
int Screen::cols() const {
    return cols_;
}

void Screen::resize(int rows, int cols) {
    // Safety check
    if (rows <= 0)
        rows = 24;
    if (cols <= 0)
        cols = 80;

    rows_ = rows;
    cols_ = cols;
    grid_.assign(rows, std::vector<Cell>(cols));
}
