#include <gtest/gtest.h>
#include "util/utf8.hpp"
#include "terminal/parser.hpp"
#include "terminal/screen.hpp"

namespace {

// Read one row of the grid as a UTF-8 string.
std::string row_text(const Screen& screen, int row) {
    std::string out;
    for (int c = 0; c < screen.cols(); ++c) {
        out += utf8::encode(screen.get(row, c).ch);
    }
    return out;
}

void feed(Parser& parser, const std::string& bytes) {
    parser.feed(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
}

}  // namespace

class Utf8Test : public ::testing::Test {
   protected:
    void SetUp() override {}
};

TEST_F(Utf8Test, EncodeAscii) {
    EXPECT_EQ(utf8::encode(U'A'), "A");
    EXPECT_EQ(utf8::encode(U'Z'), "Z");
    EXPECT_EQ(utf8::encode(U'0'), "0");
}

TEST_F(Utf8Test, EncodeChinese) {
    // "中" in UTF-8 is E4 B8 AD
    std::string result = utf8::encode(U'中');
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ((unsigned char)result[0], 0xE4);
    EXPECT_EQ((unsigned char)result[1], 0xB8);
    EXPECT_EQ((unsigned char)result[2], 0xAD);
}

TEST_F(Utf8Test, EncodeEmoji) {
    // 😀 U+1F600 in UTF-8 is F0 9F 98 80
    std::string result = utf8::encode(U'😀');
    EXPECT_EQ(result.size(), 4);
}

TEST_F(Utf8Test, DecodeAscii) {
    const uint8_t data[] = {'A', 'B', 'C'};
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(data, 3, pos), U'A');
    EXPECT_EQ(pos, 1);
    EXPECT_EQ(utf8::decode(data, 3, pos), U'B');
    EXPECT_EQ(pos, 2);
}

TEST_F(Utf8Test, DecodeChinese) {
    // "中" in UTF-8
    const uint8_t data[] = {0xE4, 0xB8, 0xAD};
    size_t pos = 0;
    EXPECT_EQ(utf8::decode(data, 3, pos), U'中');
    EXPECT_EQ(pos, 3);
}

TEST_F(Utf8Test, RoundTrip) {
    char32_t chars[] = {U'A', U'中', U'日', U'😀', U'€'};
    for (char32_t ch : chars) {
        std::string encoded = utf8::encode(ch);
        const uint8_t* data = reinterpret_cast<const uint8_t*>(encoded.data());
        size_t pos = 0;
        char32_t decoded = utf8::decode(data, encoded.size(), pos);
        EXPECT_EQ(decoded, ch) << "Round trip failed for character";
    }
}

// ---- Parser: UTF-8 split across feed() boundaries ----
TEST_F(Utf8Test, ParserJoinsSequenceSplitAcrossFeeds) {
    Screen screen(3, 10);
    Parser parser(screen);
    const std::string chinese = utf8::encode(U'中');  // E4 B8 AD
    ASSERT_EQ(chinese.size(), 3u);

    for (char byte : chinese) {
        parser.feed(reinterpret_cast<const uint8_t*>(&byte), 1);
    }

    EXPECT_EQ(screen.get(0, 0).ch, U'中') << "row: " << row_text(screen, 0);
    // Double-width advance: the cursor skips the second column of the glyph.
    EXPECT_EQ(screen.cursor_col(), 2);
}

TEST_F(Utf8Test, ParserSplitsFourByteEmoji) {
    Screen screen(2, 10);
    Parser parser(screen);
    const std::string emoji = utf8::encode(U'😀');  // F0 9F 98 80
    ASSERT_EQ(emoji.size(), 4u);

    parser.feed(reinterpret_cast<const uint8_t*>(emoji.data()), 2);  // F0 9F
    EXPECT_EQ(screen.get(0, 0).ch, U' ') << "half a sequence must not be written";
    parser.feed(reinterpret_cast<const uint8_t*>(emoji.data()) + 2, 2);

    EXPECT_EQ(screen.get(0, 0).ch, U'😀');
}

TEST_F(Utf8Test, ParserReplacesSequenceTruncatedForever) {
    Screen screen(2, 10);
    Parser parser(screen);
    const std::string chinese = utf8::encode(U'中');
    parser.feed(reinterpret_cast<const uint8_t*>(chinese.data()), 2);  // E4 B8
    feed(parser, "A");                                                 // E4 B8 41 is invalid

    EXPECT_EQ(screen.get(0, 0).ch, U'\uFFFD');
    EXPECT_EQ(screen.get(0, 1).ch, U'A');
}

TEST_F(Utf8Test, ParserReplacesStrayContinuationByte) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, std::string("\x80", 1) + "B");

    EXPECT_EQ(screen.get(0, 0).ch, U'\uFFFD');
    EXPECT_EQ(screen.get(0, 1).ch, U'B');
}

// ---- Parser: control sequences must never reach the grid ----
TEST_F(Utf8Test, ParserSwallowsPrivateCsi) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[?25l");
    feed(parser, "A");

    EXPECT_EQ(row_text(screen, 0), "A         ");
    EXPECT_EQ(screen.cursor_col(), 1);
}

TEST_F(Utf8Test, ParserSwallowsPrivateCsiWithParameters) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[>0;276;0c");
    feed(parser, "A");

    EXPECT_EQ(row_text(screen, 0), "A         ");
}

TEST_F(Utf8Test, ParserSwallowsOscWithBelAndSt) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b]0;title\x07");
    feed(parser, "A");
    feed(parser, "\x1b]2;other\x1b\\");
    feed(parser, "B");

    EXPECT_EQ(row_text(screen, 0), "AB        ");
}

TEST_F(Utf8Test, ParserKeepsKnownCsiBehaviour) {
    Screen screen(4, 10);
    Parser parser(screen);
    feed(parser, "abc\x1b[2;3HX");  // cursor position still works
    EXPECT_EQ(row_text(screen, 1), "  X       ");
    feed(parser, "\x1b[2J");  // erase display
    EXPECT_EQ(row_text(screen, 1), "          ");
}

// ---- Parser: auto-wrap at the right margin ----
TEST_F(Utf8Test, ParserWrapsToNextLine) {
    Screen screen(3, 5);
    Parser parser(screen);
    feed(parser, "abcdef");

    EXPECT_EQ(row_text(screen, 0), "abcde");
    EXPECT_EQ(row_text(screen, 1), "f    ");
    EXPECT_EQ(screen.cursor_row(), 1);
    EXPECT_EQ(screen.cursor_col(), 1);
}

TEST_F(Utf8Test, ParserWrapsInsteadOfSplittingWideChar) {
    Screen screen(3, 5);
    Parser parser(screen);
    feed(parser, "abcd");
    feed(parser, utf8::encode(U'中'));

    EXPECT_EQ(screen.get(0, 3).ch, U'd');
    EXPECT_EQ(screen.get(0, 4).ch, U' ') << "no half glyph in the last column";
    EXPECT_EQ(screen.get(1, 0).ch, U'中');
}

TEST_F(Utf8Test, ParserWrapAtBottomScrolls) {
    Screen screen(2, 3);
    Parser parser(screen);
    feed(parser, "abcdefg");

    EXPECT_EQ(row_text(screen, 0), "def");
    EXPECT_EQ(row_text(screen, 1), "g  ");
}

// ---- Screen: resize keeps content ----
TEST_F(Utf8Test, ScreenResizePreservesOverlap) {
    Screen screen(3, 5);
    screen.put(U'X', 0, 0);
    screen.put(U'Y', 2, 4);
    screen.move_cursor(2, 4);

    screen.resize(5, 8);
    EXPECT_EQ(screen.get(0, 0).ch, U'X');
    EXPECT_EQ(screen.get(2, 4).ch, U'Y');
    EXPECT_EQ(screen.get(4, 7).ch, U' ');
    EXPECT_EQ(screen.cursor_row(), 2);
    EXPECT_EQ(screen.cursor_col(), 4);

    screen.resize(2, 3);  // shrink: overlap kept, cursor clamped
    EXPECT_EQ(screen.get(0, 0).ch, U'X');
    EXPECT_EQ(screen.cursor_row(), 1);
    EXPECT_EQ(screen.cursor_col(), 2);
}

// ---- Parser: SGR pen (16 colors) ----
TEST_F(Utf8Test, SgrForegroundAndReset) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[31mX");
    EXPECT_EQ(screen.get(0, 0).ch, U'X');
    EXPECT_EQ(screen.get(0, 0).fg, 1);
    EXPECT_EQ(screen.get(0, 0).bright, false);

    feed(parser, "\x1b[0mY");
    EXPECT_EQ(screen.get(0, 1).ch, U'Y');
    EXPECT_EQ(screen.get(0, 1).fg, 7);  // default pen after reset
}

TEST_F(Utf8Test, SgrBoldAndBackground) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[1mA");
    EXPECT_EQ(screen.get(0, 0).ch, U'A');
    EXPECT_EQ(screen.get(0, 0).bright, true);
    EXPECT_EQ(screen.get(0, 0).fg, 7);  // bold lifts, it does not recolor

    feed(parser, "\x1b[22;42mB");
    EXPECT_EQ(screen.get(0, 1).ch, U'B');
    EXPECT_EQ(screen.get(0, 1).bright, false);
    EXPECT_EQ(screen.get(0, 1).bg, 2);
}

TEST_F(Utf8Test, SgrBrightPaletteAndReverse) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[90mX");  // bright black fg
    EXPECT_EQ(screen.get(0, 0).fg, 0);
    EXPECT_EQ(screen.get(0, 0).bright, true);

    feed(parser, "\x1b[0m\x1b[7mR");  // reverse video
    EXPECT_EQ(screen.get(0, 1).ch, U'R');
    EXPECT_EQ(screen.get(0, 1).reverse, true);

    feed(parser, "\x1b[27m\x1b[104mS");  // bright blue bg
    EXPECT_EQ(screen.get(0, 2).reverse, false);
    EXPECT_EQ(screen.get(0, 2).bg, 4);
    EXPECT_EQ(screen.get(0, 2).bg_bright, true);

    feed(parser, "\x1b[49mT");  // bg back to default
    EXPECT_EQ(screen.get(0, 3).bg, 0);
    EXPECT_EQ(screen.get(0, 3).bg_bright, false);
}

TEST_F(Utf8Test, SgrMultipleParametersCombine) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[31;42;1mX");  // red fg, green bg, bright
    EXPECT_EQ(screen.get(0, 0).fg, 1);
    EXPECT_EQ(screen.get(0, 0).bg, 2);
    EXPECT_EQ(screen.get(0, 0).bright, true);
}

TEST_F(Utf8Test, SgrEmptyFormAndTruecolorAreIgnored) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[31mA");
    feed(parser, "\x1b[mB");  // empty SGR means reset
    EXPECT_EQ(screen.get(0, 0).fg, 1);
    EXPECT_EQ(screen.get(0, 1).fg, 7);

    // 38;5;196 must not leak its sub-arguments into the 16-color pen.
    feed(parser, "\x1b[38;5;196;48;2;1;2;3mC");
    EXPECT_EQ(screen.get(0, 2).ch, U'C');
    EXPECT_EQ(screen.get(0, 2).fg, 7);
    EXPECT_EQ(screen.get(0, 2).bg, 0);
    EXPECT_EQ(screen.get(0, 2).bg_bright, false);
}

// ---- Parser: SGR extended colors (256-color / 24bit truecolor) ----
TEST_F(Utf8Test, Sgr256ColorForeground) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[38;5;196mA");
    EXPECT_EQ(screen.get(0, 0).ch, U'A');
    EXPECT_EQ(screen.get(0, 0).fg_extended, true);
    EXPECT_EQ(screen.get(0, 0).fg_truecolor, false);
    EXPECT_EQ(screen.get(0, 0).fg_index, 196);
    // Extended colors leave the 16-color fields untouched.
    EXPECT_EQ(screen.get(0, 0).fg, 7);
    EXPECT_EQ(screen.get(0, 0).bright, false);
}

TEST_F(Utf8Test, SgrTruecolorForeground) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[38;2;10;20;30mB");
    EXPECT_EQ(screen.get(0, 0).ch, U'B');
    EXPECT_EQ(screen.get(0, 0).fg_extended, true);
    EXPECT_EQ(screen.get(0, 0).fg_truecolor, true);
    EXPECT_EQ(screen.get(0, 0).fg_rgb.r, 10);
    EXPECT_EQ(screen.get(0, 0).fg_rgb.g, 20);
    EXPECT_EQ(screen.get(0, 0).fg_rgb.b, 30);
}

TEST_F(Utf8Test, Sgr256ColorBackground) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[48;5;236mC");
    EXPECT_EQ(screen.get(0, 0).ch, U'C');
    EXPECT_EQ(screen.get(0, 0).bg_extended, true);
    EXPECT_EQ(screen.get(0, 0).bg_truecolor, false);
    EXPECT_EQ(screen.get(0, 0).bg_index, 236);
}

TEST_F(Utf8Test, SgrTruecolorBackground) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[48;2;1;2;3mD");
    EXPECT_EQ(screen.get(0, 0).ch, U'D');
    EXPECT_EQ(screen.get(0, 0).bg_extended, true);
    EXPECT_EQ(screen.get(0, 0).bg_truecolor, true);
    EXPECT_EQ(screen.get(0, 0).bg_rgb.r, 1);
    EXPECT_EQ(screen.get(0, 0).bg_rgb.g, 2);
    EXPECT_EQ(screen.get(0, 0).bg_rgb.b, 3);
}

TEST_F(Utf8Test, SgrResetTurnsOffExtendedForeground) {
    Screen screen(2, 10);
    Parser parser(screen);
    // Red bg, then truecolor fg {1,2,3}, then 39: the reset turns the extended
    // foreground back to the default and leaves the background untouched.
    feed(parser, "\x1b[41;38;2;1;2;3;39mE");
    EXPECT_EQ(screen.get(0, 0).ch, U'E');
    EXPECT_EQ(screen.get(0, 0).fg_extended, false);
    EXPECT_EQ(screen.get(0, 0).fg, 7);  // default fg back
    EXPECT_EQ(screen.get(0, 0).bright, false);
    EXPECT_EQ(screen.get(0, 0).bg, 1);  // bg kept across the fg reset
}

TEST_F(Utf8Test, SgrExtendedMissingParamsDefault) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[38;5mF");  // 38;5 without an index: defaults to 0
    EXPECT_EQ(screen.get(0, 0).ch, U'F');
    EXPECT_EQ(screen.get(0, 0).fg_extended, true);
    EXPECT_EQ(screen.get(0, 0).fg_index, 0);

    feed(parser, "\x1b[38;2;1;2mG");  // 38;2 missing blue: defaults to 0
    EXPECT_EQ(screen.get(0, 1).ch, U'G');
    EXPECT_EQ(screen.get(0, 1).fg_truecolor, true);
    EXPECT_EQ(screen.get(0, 1).fg_rgb.r, 1);
    EXPECT_EQ(screen.get(0, 1).fg_rgb.g, 2);
    EXPECT_EQ(screen.get(0, 1).fg_rgb.b, 0);
}

TEST_F(Utf8Test, Sgr16ColorThen256Color) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "\x1b[32;38;5;21mF");
    EXPECT_EQ(screen.get(0, 0).ch, U'F');
    EXPECT_EQ(screen.get(0, 0).fg_extended, true);
    EXPECT_EQ(screen.get(0, 0).fg_truecolor, false);
    EXPECT_EQ(screen.get(0, 0).fg_index, 21);
    EXPECT_EQ(screen.get(0, 0).fg, 2);  // 16-color field keeps the earlier value

    // A later 16-color code wins back over the extended foreground.
    feed(parser, "\x1b[33mG");
    EXPECT_EQ(screen.get(0, 1).ch, U'G');
    EXPECT_EQ(screen.get(0, 1).fg_extended, false);
    EXPECT_EQ(screen.get(0, 1).fg, 3);
}

TEST_F(Utf8Test, EraseDisplayCarriesExtendedColors) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "XXXXXXXXXX");
    feed(parser, "\x1b[48;2;1;2;3m");  // truecolor bg
    feed(parser, "\x1b[2;1H");
    feed(parser, "\x1b[1J");
    EXPECT_EQ(screen.get(0, 0).ch, U' ');
    EXPECT_EQ(screen.get(0, 0).bg_extended, true);
    EXPECT_EQ(screen.get(0, 0).bg_truecolor, true);
    EXPECT_EQ(screen.get(0, 0).bg_rgb.r, 1);
    EXPECT_EQ(screen.get(0, 0).bg_rgb.g, 2);
    EXPECT_EQ(screen.get(0, 0).bg_rgb.b, 3);
}

// ---- Parser: ED / EL erase ----
TEST_F(Utf8Test, EraseDisplayKeepsCursorRule) {
    Screen screen(3, 10);
    Parser parser(screen);
    feed(parser, "AAAAAA");
    feed(parser, "\x1b[2;3H");                     // cursor at (1, 2)
    feed(parser, "\x1b[0J");                       // ED 0: cursor to end of screen
    EXPECT_EQ(row_text(screen, 0), "AAAAAA    ");  // above cursor untouched
    EXPECT_EQ(row_text(screen, 1), "          ");
    EXPECT_EQ(row_text(screen, 2), "          ");

    feed(parser, "BBBBBB");  // resumes writing at the erased (1, 2)
    EXPECT_EQ(row_text(screen, 1), "  BBBBBB  ");

    feed(parser, "\x1b[2J");  // ED 2: whole screen
    EXPECT_EQ(row_text(screen, 0), "          ");
    EXPECT_EQ(row_text(screen, 1), "          ");
    EXPECT_EQ(row_text(screen, 2), "          ");
}

TEST_F(Utf8Test, EraseDisplayColoredByPen) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "XXXXXXXXXX");
    feed(parser, "\x1b[44;1m");  // blue bg, bold
    feed(parser, "\x1b[2;1H");   // cursor at (1, 0)
    feed(parser, "\x1b[1J");     // ED 1: start of screen to cursor
    // Erased cells carry the active pen, not the default one.
    EXPECT_EQ(screen.get(0, 0).ch, U' ');
    EXPECT_EQ(screen.get(0, 0).bg, 4);
    EXPECT_EQ(screen.get(0, 0).bright, true);
    EXPECT_EQ(screen.get(0, 9).ch, U' ');
    EXPECT_EQ(screen.get(0, 9).bg, 4);
    EXPECT_EQ(screen.get(1, 0).ch, U' ');  // cursor cell itself erased
    EXPECT_EQ(screen.get(1, 0).bg, 4);
}

TEST_F(Utf8Test, EraseLineModes) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "1234567890");
    feed(parser, "\x1b[2;1Habcdefghij");
    feed(parser, "\x1b[1;4H\x1b[0K");  // EL 0: cursor to end of line
    EXPECT_EQ(row_text(screen, 0), "123       ");
    EXPECT_EQ(row_text(screen, 1), "abcdefghij");
    feed(parser, "\x1b[2;4H\x1b[1K");  // EL 1: start of line to cursor
    EXPECT_EQ(row_text(screen, 1), "    efghij");
    feed(parser, "\x1b[2K");  // EL 2: whole line
    EXPECT_EQ(row_text(screen, 1), "          ");
    EXPECT_EQ(row_text(screen, 0), "123       ");  // other line untouched
}

// ---- Parser: parameterized cursor movement ----
TEST_F(Utf8Test, CursorMoveWithCounts) {
    Screen screen(5, 10);
    Parser parser(screen);
    feed(parser, "\x1b[2B");  // down 2
    EXPECT_EQ(screen.cursor_row(), 2);
    EXPECT_EQ(screen.cursor_col(), 0);
    feed(parser, "\x1b[5C");  // right 5
    EXPECT_EQ(screen.cursor_col(), 5);
    feed(parser, "\x1b[2A");  // up 2
    EXPECT_EQ(screen.cursor_row(), 0);
    feed(parser, "\x1b[3D");  // left 3
    EXPECT_EQ(screen.cursor_col(), 2);
}

TEST_F(Utf8Test, CursorMoveClampsAtMargins) {
    Screen screen(3, 5);
    Parser parser(screen);
    feed(parser, "\x1b[50A");  // up far past the top
    EXPECT_EQ(screen.cursor_row(), 0);
    feed(parser, "\x1b[50B");  // down far past the bottom
    EXPECT_EQ(screen.cursor_row(), 2);
    feed(parser, "\x1b[50D");  // left far past the left margin
    EXPECT_EQ(screen.cursor_col(), 0);
    feed(parser, "\x1b[50C");  // right far past the right margin
    EXPECT_EQ(screen.cursor_col(), 4);
}

TEST_F(Utf8Test, ChaAndVpa) {
    Screen screen(5, 10);
    Parser parser(screen);
    feed(parser, "\x1b[7G");  // CHA: column 7 (0-based 6)
    EXPECT_EQ(screen.cursor_col(), 6);
    EXPECT_EQ(screen.cursor_row(), 0);
    feed(parser, "\x1b[3d");  // VPA: row 3 (0-based 2)
    EXPECT_EQ(screen.cursor_row(), 2);
    EXPECT_EQ(screen.cursor_col(), 6);
}

TEST_F(Utf8Test, EchErasesNCellsRightOfCursor) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, "abcdef");
    feed(parser, "\x1b[1;2H");  // cursor at (0, 1)
    feed(parser, "\x1b[2X");    // ECH 2
    EXPECT_EQ(row_text(screen, 0), "a  def    ");
    feed(parser, "\x1b[8X");  // ECH past the right margin: clamps
    EXPECT_EQ(row_text(screen, 0), "a         ");
    EXPECT_EQ(screen.cursor_col(), 1);  // ECH leaves the cursor in place
}

TEST_F(Utf8Test, SaveAndRestoreCursor) {
    Screen screen(5, 10);
    Parser parser(screen);
    feed(parser, "abc\x1b[s");  // save at (0, 3)
    feed(parser, "\x1b[2;3H");  // move away
    EXPECT_EQ(screen.cursor_row(), 1);
    EXPECT_EQ(screen.cursor_col(), 2);
    feed(parser, "\x1b[u");  // restore to (0, 3)
    EXPECT_EQ(screen.cursor_row(), 0);
    EXPECT_EQ(screen.cursor_col(), 3);
}

// ---- Parser: Tab advances to the next 8-column stop ----
TEST_F(Utf8Test, TabAdvancesToTabStop) {
    Screen screen(3, 10);
    Parser parser(screen);
    feed(parser, "a\tb");  // 'a' at col 0, tab to col 8, 'b' at col 8
    EXPECT_EQ(screen.get(0, 0).ch, U'a');
    EXPECT_EQ(screen.get(0, 8).ch, U'b');
    EXPECT_EQ(screen.get(0, 1).ch, U' ');  // skipped columns stay blank
    EXPECT_EQ(screen.cursor_col(), 9);
}

// ---- Screen: wide character right half is marked ----
TEST_F(Utf8Test, WideCharMarksRightHalf) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, utf8::encode(U'中'));
    EXPECT_EQ(screen.get(0, 0).ch, U'中');
    EXPECT_EQ(screen.get(0, 0).wide, true);
    EXPECT_EQ(screen.get(0, 1).ch, 0);  // right half: empty char, wide marker
    EXPECT_EQ(screen.get(0, 1).wide, true);
    EXPECT_EQ(screen.cursor_col(), 2);
}

TEST_F(Utf8Test, WideCharRightHalfClearedByErase) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, utf8::encode(U'中'));
    feed(parser, "\x1b[2K");  // erase line clears both halves
    EXPECT_EQ(screen.get(0, 0).wide, false);
    EXPECT_EQ(screen.get(0, 1).wide, false);
    EXPECT_EQ(screen.get(0, 1).ch, U' ');
}

// Contract shared with the renderer: the left half of a wide glyph carries the
// character (wide=true), the right half is ch==0 && wide=true. redraw_shell()
// skips the right half based on exactly these flags.
TEST_F(Utf8Test, WideCharRowHalvesHaveRightShapes) {
    Screen screen(2, 10);
    Parser parser(screen);
    feed(parser, utf8::encode(U'中'));
    feed(parser, "AB");
    feed(parser, utf8::encode(U'文'));
    feed(parser, "cd");  // wraps to row 1
    // Left halves carry the glyph; right halves are empty with wide=true.
    EXPECT_EQ(screen.get(0, 0).ch, U'中');
    EXPECT_EQ(screen.get(0, 0).wide, true);
    EXPECT_EQ(screen.get(0, 1).ch, 0);
    EXPECT_EQ(screen.get(0, 1).wide, true);
    EXPECT_EQ(screen.get(0, 2).ch, U'A');
    EXPECT_EQ(screen.get(0, 2).wide, false);
    EXPECT_EQ(screen.get(0, 4).ch, U'文');
    EXPECT_EQ(screen.get(0, 4).wide, true);
    EXPECT_EQ(screen.get(0, 5).ch, 0);
    EXPECT_EQ(screen.get(0, 5).wide, true);
    EXPECT_EQ(screen.get(0, 6).ch, U'c');
    EXPECT_EQ(screen.get(0, 7).ch, U'd');
    EXPECT_EQ(screen.get(0, 8).ch, U' ');
    EXPECT_EQ(screen.get(0, 8).wide, false);
}
