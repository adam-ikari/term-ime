#include "screen.hpp"
#include "../util/utf8.hpp"
#include <algorithm>
#include <utility>

Screen::Screen(int rows, int cols) {
    resize(rows, cols);
}

namespace {

// True when `c` is the left half of a wide pair: it carries the glyph and
// claims the very next column as its (ch == 0) right half.
bool is_wide_left(const Cell& c) {
    return c.wide && c.ch != 0;
}

// True when column `col` of `line` is the left half of a wide pair, bounds
// checked so a caller can probe past the right margin without guarding.
bool cell_is_wide_left(const std::vector<Cell>& line, int col) {
    if (col < 0 || col >= static_cast<int>(line.size()))
        return false;
    return is_wide_left(line[static_cast<size_t>(col)]);
}

// True when column `col` of `line` is the right half of a wide pair -- i.e. the
// glyph it belongs to lives at col-1. Requiring that left neighbour to check out
// is deliberate: it stops a cell that merely happens to hold ch == 0 from being
// read as half a glyph, so any inconsistency heals instead of spreading.
bool is_wide_right(const std::vector<Cell>& line, int col) {
    if (col <= 0 || col >= static_cast<int>(line.size()))
        return false;
    const Cell& c = line[static_cast<size_t>(col)];
    return c.wide && c.ch == 0 && is_wide_left(line[static_cast<size_t>(col - 1)]);
}

// Widen [first, last] so that blanking it can never split a wide pair: a range
// starting on a right half has to reach that glyph's left half, and a range
// ending on a left half has to reach its right half. Only the boundaries can
// split a pair -- one lying wholly inside the range is erased either way, and
// widening is idempotent, so erase_display can call this per row.
void expand_wide_range(const std::vector<Cell>& line, int cols, int& first, int& last) {
    if (is_wide_right(line, first))
        --first;
    if (last < cols && is_wide_left(line[static_cast<size_t>(last)]))
        ++last;
}

}  // namespace

void Screen::put(char32_t ch, int row, int col, const Pen& pen) {
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_) {
        return;
    }

    auto& line = grid_[static_cast<size_t>(row)];

    // What the columns around `col` look like *before* anything is written.
    // Read up front: each fix below overwrites a neighbour, so testing a cell
    // after it has been cleared can no longer tell what it used to hold.
    const bool over_wide_right = is_wide_right(line, col);
    const bool over_wide_left = col + 1 < cols_ && is_wide_left(line[static_cast<size_t>(col)]);
    // The column we will claim as our own right half may itself be the left
    // half of another pair, which sits one column further right.
    const bool right_neighbour_is_wide_left = cell_is_wide_left(line, col + 1);

    Cell cell;
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
    cell.ch = ch;
    // Use utf8proc for proper width detection (CJK, emojis, etc.)
    cell.wide = (utf8::width(ch) == 2);

    // redraw_shell() acts on `wide` alone -- it skips the next column whenever
    // it sees the flag -- so `wide` must mean exactly one thing: "this glyph
    // owns the next column too". Any write that lands on, or splits, one half of
    // a pair therefore has to retire the other half, or the cell just written is
    // skipped as if invisible and a stale left half keeps claiming a column that
    // no longer holds its right half.
    if (over_wide_right) {
        // Half of someone else's glyph is being overwritten: that glyph loses
        // its right half here, so its left half has to go too rather than sit
        // there still flagged wide.
        line[static_cast<size_t>(col - 1)] = Cell{};
    }
    if (over_wide_left) {
        // The glyph being overwritten had a right half that nothing claims now.
        line[static_cast<size_t>(col + 1)] = Cell{};
    }
    if (cell.wide && right_neighbour_is_wide_left) {
        // Our right half lands on another pair's left half, orphaning *its*
        // right half one column over.
        line[static_cast<size_t>(col + 2)] = Cell{};
    }

    line[static_cast<size_t>(col)] = cell;
    if (cell.wide && col + 1 < cols_) {
        // Right half of a double-width glyph: same pen, empty char, wide
        // marker. The renderer sees ch == 0 && wide and skips the cell
        // instead of shifting the rest of the row; never written past
        // the right margin.
        line[static_cast<size_t>(col + 1)] = cell;
        line[static_cast<size_t>(col + 1)].ch = 0;
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
    first = std::max(0, first);
    last = std::min(last, cols_ - 1);
    if (first > last) {
        return;
    }
    // Never blank half a wide glyph: the surviving half would keep its `wide`
    // flag and redraw_shell would skip a column that no longer holds one.
    expand_wide_range(grid_[static_cast<size_t>(cursor_row_)], cols_, first, last);
    for (int c = first; c <= last; ++c) {
        grid_[static_cast<size_t>(cursor_row_)][static_cast<size_t>(c)] = blank;
    }
}

void Screen::erase_cells(int row, int col, int count, const Pen& pen) {
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_ || count <= 0) {
        return;
    }
    const Cell blank = blank_cell(pen);
    int first = col;
    int last = std::min(col + count - 1, cols_ - 1);
    expand_wide_range(grid_[static_cast<size_t>(row)], cols_, first, last);
    for (int c = first; c <= last; ++c) {
        grid_[static_cast<size_t>(row)][static_cast<size_t>(c)] = blank;
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
        int c0 = (r == first_row) ? std::max(0, first_col) : 0;
        int c1 = (r == last_row) ? std::min(last_col, cols_ - 1) : cols_ - 1;
        if (c0 > c1) {
            continue;
        }
        // Each row widens independently: a wide pair never spans two rows, so
        // the expansion cannot leak into a neighbour line.
        expand_wide_range(grid_[static_cast<size_t>(r)], cols_, c0, c1);
        for (int c = c0; c <= c1; ++c) {
            grid_[static_cast<size_t>(r)][static_cast<size_t>(c)] = blank;
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
        // Narrowing can cut a wide pair in half: the left half survives into the
        // new row while its right half is dropped, leaving a glyph flagged wide
        // with nothing to skip. Real terminals blank the orphaned half rather
        // than render a lone cell the width of two.
        if (copy_cols > 0 && is_wide_left(next[static_cast<size_t>(r)][static_cast<size_t>(copy_cols - 1)])) {
            next[static_cast<size_t>(r)][static_cast<size_t>(copy_cols - 1)] = Cell{};
        }
    }

    grid_ = std::move(next);
    rows_ = rows;
    cols_ = cols;
    move_cursor(cursor_row_, cursor_col_);  // clamp into the new bounds
}
