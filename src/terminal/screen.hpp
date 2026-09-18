#pragma once

#include <vector>
#include <cstdint>

// 24bit RGB color component (truecolor).
struct Rgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

// SGR pen attributes carried into each cell. 16-color mode: fg/bg hold a
// palette index 0-7 (default fg 7, default bg 0); `bright` lifts fg into
// 90-97, `bg_bright` lifts bg into 100-107; `reverse` swaps fg/bg on render.
// Extended mode: when `fg_extended`/`bg_extended` is set, the matching 16-color
// fields above are ignored and the color comes from `fg_index`/`bg_index`
// (256-color) or `fg_rgb`/`bg_rgb` (truecolor), selected by the `*_truecolor`
// flag.
struct Cell {
    char32_t ch = U' ';
    uint8_t fg = 7;
    uint8_t bg = 0;
    bool wide = false;
    bool bright = false;
    bool bg_bright = false;
    bool reverse = false;
    bool fg_extended = false;   // foreground is 256-color or truecolor, not 16
    bool bg_extended = false;   // background is 256-color or truecolor, not 16
    bool fg_truecolor = false;  // fg_rgb valid (else fg_index valid)
    bool bg_truecolor = false;  // bg_rgb valid (else bg_index valid)
    uint8_t fg_index = 0;       // 256-color palette index, fg_extended non-truecolor
    uint8_t bg_index = 0;
    Rgb fg_rgb{0, 0, 0};
    Rgb bg_rgb{0, 0, 0};
};

// Persistent graphic rendition state (the SGR "pen"). Kept separate from Cell
// so erase ops and the parser share one representation of the current pen.
struct Pen {
    uint8_t fg = 7;
    uint8_t bg = 0;
    bool bright = false;
    bool bg_bright = false;
    bool reverse = false;
    bool fg_extended = false;
    bool bg_extended = false;
    bool fg_truecolor = false;
    bool bg_truecolor = false;
    uint8_t fg_index = 0;
    uint8_t bg_index = 0;
    Rgb fg_rgb{0, 0, 0};
    Rgb bg_rgb{0, 0, 0};
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
    // Erase characters (ECH): `count` blank cells starting at (row, col),
    // filled using `pen` like ED/EL; clamped to the right margin.
    void erase_cells(int row, int col, int count, const Pen& pen);

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
