#include "screen.hpp"
#include "../util/utf8.hpp"
#include <algorithm>
#include <utility>

Screen::Screen(int rows, int cols) {
    resize(rows, cols);
}

void Screen::put(char32_t ch, int row, int col, const Pen& pen) {
    if (row >= 0 && row < rows_ && col >= 0 && col < cols_) {
        auto& cell = grid_[row][col];
        cell.ch = ch;
        cell.fg = pen.fg;
        cell.bg = pen.bg;
        cell.bright = pen.bright;
        cell.bg_bright = pen.bg_bright;
        cell.reverse = pen.reverse;
        cell.fg_extended = pen.fg_extended;
        cell.bg_extended = pen.bg_extended;
        cell.fg_truecolor = pen.fg_truecolor;
        cell.bg_truecolor = pen.bg_truecolor;
        cell.fg_index = pen.fg_index;
        cell.bg_index = pen.bg_index;
        cell.fg_rgb = pen.fg_rgb;
        cell.bg_rgb = pen.bg_rgb;
        // Use utf8proc for proper width detection (CJK, emojis, etc.)
        cell.wide = (utf8::width(ch) == 2);
        if (cell.wide && col + 1 < cols_) {
            // Right half of a double-width glyph: same pen, empty char, wide
            // marker. The renderer sees ch == 0 && wide and skips the cell
            // instead of shifting the rest of the row; never written past
            // the right margin.
            grid_[row][col + 1] = cell;
            grid_[row][col + 1].ch = 0;
        }
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

namespace {
// A blank cell carrying `pen`'s attributes (ED/EL keep the active colors so a
// colored erase paints a uniformly colored region instead of default bg).
Cell blank_cell(const Pen& pen) {
    Cell cell;
    cell.ch = U' ';
    cell.fg = pen.fg;
    cell.bg = pen.bg;
    cell.bright = pen.bright;
    cell.bg_bright = pen.bg_bright;
    cell.reverse = pen.reverse;
    cell.fg_extended = pen.fg_extended;
    cell.bg_extended = pen.bg_extended;
    cell.fg_truecolor = pen.fg_truecolor;
    cell.bg_truecolor = pen.bg_truecolor;
    cell.fg_index = pen.fg_index;
    cell.bg_index = pen.bg_index;
    cell.fg_rgb = pen.fg_rgb;
    cell.bg_rgb = pen.bg_rgb;
    cell.wide = false;
    return cell;
}
}  // namespace

void Screen::erase_line(int mode, const Pen& pen) {
    if (cursor_row_ < 0 || cursor_row_ >= rows_) {
        return;
    }
    const Cell blank = blank_cell(pen);
    int first = 0, last = cols_ - 1;
    switch (mode) {
    case 0:
        first = cursor_col_;
        break;
    case 1:
        last = cursor_col_;
        break;
    default:  // 2 (and anything else) erases the whole line
        break;
    }
    for (int c = std::max(0, first); c <= std::min(last, cols_ - 1); ++c) {
        grid_[cursor_row_][c] = blank;
    }
}

void Screen::erase_cells(int row, int col, int count, const Pen& pen) {
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_ || count <= 0) {
        return;
    }
    const Cell blank = blank_cell(pen);
    const int last = std::min(col + count - 1, cols_ - 1);
    for (int c = col; c <= last; ++c) {
        grid_[row][c] = blank;
    }
}

void Screen::erase_display(int mode, const Pen& pen) {
    const Cell blank = blank_cell(pen);
    int first_row = 0, first_col = 0, last_row = rows_ - 1, last_col = cols_ - 1;
    switch (mode) {
    case 0:
        first_row = cursor_row_;
        first_col = cursor_col_;
        break;
    case 1:
        last_row = cursor_row_;
        last_col = cursor_col_;
        break;
    default:  // 2 and 5: whole screen (no scrollback in this project)
        break;
    }
    for (int r = std::max(0, first_row); r <= std::min(last_row, rows_ - 1); ++r) {
        const int c0 = (r == first_row) ? std::max(0, first_col) : 0;
        const int c1 = (r == last_row) ? std::min(last_col, cols_ - 1) : cols_ - 1;
        for (int c = c0; c <= c1; ++c) {
            grid_[r][c] = blank;
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

    // Copy the overlapping top-left region instead of blanking the grid: a
    // real terminal keeps its content when the window changes size.
    std::vector<std::vector<Cell>> next(rows, std::vector<Cell>(cols));
    const int copy_rows = std::min(rows, rows_);
    const int copy_cols = std::min(cols, cols_);
    for (int r = 0; r < copy_rows; ++r) {
        for (int c = 0; c < copy_cols; ++c) {
            next[r][c] = grid_[r][c];
        }
    }

    grid_ = std::move(next);
    rows_ = rows;
    cols_ = cols;
    move_cursor(cursor_row_, cursor_col_);  // clamp into the new bounds
}
