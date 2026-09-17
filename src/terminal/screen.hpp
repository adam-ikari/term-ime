#pragma once

#include <vector>
#include <cstdint>

// SGR pen attributes carried into each cell. 16-color mode: fg/bg hold a
// palette index 0-7 (default fg 7, default bg 0); `bright` lifts fg into
// 90-97, `bg_bright` lifts bg into 100-107; `reverse` swaps fg/bg on render.
struct Cell {
    char32_t ch = U' ';
    uint8_t fg = 7;
    uint8_t bg = 0;
    bool wide = false;
    bool bright = false;
    bool bg_bright = false;
    bool reverse = false;
};

// Persistent graphic rendition state (the SGR "pen"). Kept separate from Cell
// so erase ops and the parser share one representation of the current pen.
struct Pen {
    uint8_t fg = 7;
    uint8_t bg = 0;
    bool bright = false;
    bool bg_bright = false;
    bool reverse = false;
};

class Screen {
   public:
    Screen(int rows, int cols);

    // Store the character and the pen attributes into the cell at (row, col).
    void put(char32_t ch, int row, int col, const Pen& pen = {});
    Cell get(int row, int col) const;

    void move_cursor(int row, int col);
    int cursor_row() const;
    int cursor_col() const;

    void scroll_up(int n = 1);
    // Blank cells are reset to the default pen (fg 7 / bg 0) regardless of the
    // active pen, matching how a cell erased by the current pen reads back.
    void clear();
    void clear_line();

    // Erase in display (ED) / erase in line (EL) with the SGR parameters that
    // select the erased region, filled using `pen`.
    void erase_display(int mode, const Pen& pen);
    void erase_line(int mode, const Pen& pen);

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
