#include "components.hpp"
#include "../util/utf8.hpp"
#include "../util/i18n.hpp"
#include <ftxui/dom/elements.hpp>

namespace ui {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// Refined 24-bit color palette - green status bar background.
namespace color {
// Status bar background - deep green
const FtxuiColor kBarBg = FtxuiColor::Palette256::DarkGreen;
// Mode indicator text - bright white on green
const FtxuiColor kMode = FtxuiColor::White;
const FtxuiColor kBracket = FtxuiColor::Palette256::DarkSeaGreen;
// Pinyin buffer - bright sky blue
const FtxuiColor kPinyin = FtxuiColor::Palette256::DeepSkyBlue1;
// Candidates - warm gold
const FtxuiColor kCandidate = FtxuiColor::Palette256::Gold1;
// Selected candidate - bright green highlight
const FtxuiColor kSelectedBg = FtxuiColor::Palette256::SpringGreen1;
const FtxuiColor kSelectedFg = FtxuiColor::Palette256::DarkGreen;
// Hints - light green-gray
const FtxuiColor kHint = FtxuiColor::Palette256::DarkSeaGreen;
const FtxuiColor kSeparator = FtxuiColor::Palette256::DarkSeaGreen4;
}  // namespace color

std::string u32_to_utf8(const std::u32string& s) {
    std::string result;
    for (char32_t c : s) {
        if (c != 0) {
            result += utf8::encode(c);
        }
    }
    return result;
}

// Single mode color (no per-language switching).
FtxuiColor GetModeColor(const std::string& /*mode*/) {
    return color::kMode;
}

}  // namespace

// ============================================================================
// ModeIndicator
// ============================================================================

Element ModeIndicator(const ModeIndicatorProps& props) {
    return HBox({Text(" [") | Dim() | TextColor(color::kBracket), Text(props.mode) | Bold() | TextColor(color::kMode),
                 Text("] ") | Dim() | TextColor(color::kBracket)});
}

// ============================================================================
// CandidateItem
// ============================================================================

Element CandidateItem(const CandidateItemProps& props) {
    std::string text_str = u32_to_utf8(props.text);
    // Apply scrolling for selected candidate: show a substring window
    std::string display_text;
    int offset = props.scroll_offset;
    if (props.scroll_offset > 0) {
        // Scroll the text: skip N characters (not bytes) from the beginning
        size_t char_count = 0;
        size_t byte_pos = 0;
        while (byte_pos < text_str.size() && char_count < static_cast<size_t>(offset)) {
            int clen = utf8::char_len(static_cast<uint8_t>(text_str[byte_pos]));
            if (clen < 1)
                clen = 1;
            byte_pos += clen;
            char_count++;
        }
        display_text = text_str.substr(byte_pos);
        // Prepend ellipsis to indicate scrolling
        if (byte_pos > 0) {
            display_text = "…" + display_text;
        }
    } else {
        display_text = text_str;
    }

    std::string label = std::to_string(props.index) + "." + display_text;

    if (props.selected) {
        return Text(" " + label + " ") | Bold() | BgColor(color::kSelectedBg) | TextColor(color::kSelectedFg);
    } else {
        return Text(" " + label + " ") | TextColor(color::kCandidate);
    }
}

// ============================================================================
// CandidateBar
// ============================================================================

Element CandidateBar(const CandidateBarProps& props) {
    Elements items;

    // Mode indicator
    bool is_chinese = props.mode.find("拼") != std::string::npos;
    items.push_back(ModeIndicator({.mode = props.mode, .is_chinese = is_chinese}));

    // Pinyin buffer - bright blue, no underline
    items.push_back(Text(" " + props.buffer + " ") | TextColor(color::kPinyin));

    // Candidates
    for (size_t i = 0; i < props.candidates.size() && i < 9; ++i) {
        items.push_back(CandidateItem(
            {.index = static_cast<int>(i + 1), .text = props.candidates[i].text, .selected = (i == props.selected)}));
    }

    return HBox(std::move(items)) | BgColor(color::kBarBg) | Height(1);
}

// ============================================================================
// EmptyBar
// ============================================================================

Element EmptyBar(const EmptyBarProps& props) {
    bool is_chinese = props.mode.find("拼") != std::string::npos;

    Elements items;

    // Mode indicator
    items.push_back(ModeIndicator({.mode = props.mode, .is_chinese = is_chinese}));

    // Filler
    items.push_back(Filler());

    // Hints
    items.push_back(HintsBar());

    return HBox(std::move(items)) | BgColor(color::kBarBg) | Height(1);
}

// ============================================================================
// StatusBar
// ============================================================================

Element StatusBar(const StatusBarProps& props) {
    Elements items;

    // Language and mode
    items.push_back(Text(" [" + props.lang_name + " " + props.mode + "]") | Bold());

    return HBox(std::move(items)) | TextColor(GetModeColor(props.mode));
}

// ============================================================================
// HintsBar
// ============================================================================

Element HintItem(const HintItemProps& props) {
    return Text(" " + props.key + " " + props.action + " ") | Dim() | TextColor(color::kHint);
}

Element HintsBar() {
    return HBox({HintItem({.key = "^A Space", .action = I18n::t("hint.toggle_mode")}),
                 Text("|") | TextColor(color::kHint),
                 HintItem({.key = "^A S", .action = I18n::t("settings.title")})});
}

// ============================================================================
// MainBar
// ============================================================================

Element MainBar(const MainBarProps& props) {
    // 即使没有候选词，也要显示拼音（buffer 可能非空）
    if (props.candidates.empty() && props.buffer.empty()) {
        return EmptyBar({.mode = props.mode});
    }

    int term_w = props.term_width;
    if (term_w <= 0)
        term_w = 80;

    // ---- Width calculation helpers ----

    // Fixed elements widths (don't shrink)
    // ModeIndicator: " [中文] " → space + [ + mode + ] + space
    std::string mode_text = props.mode;
    int mode_w = 1 /*空格*/ + 1 /*[*/ + utf8::string_width(mode_text) + 1 /*]*/ + 1 /*空格*/;

    // Pinyin: " " + buffer + " " (no label, blue)
    int pinyin_w = 1 /*前空格*/ + utf8::string_width(props.buffer) + 1 /*后空格*/;

    // Total fixed width (filler takes remaining, so we don't add filler width)
    int fixed_w = mode_w + pinyin_w;

    // Candidate item width: " N.text " or " [N.text] "
    auto candidate_width = [](const std::u32string& text, bool selected) -> int {
        // " N." = 1+1+1 = 3 (space + digit + dot)
        // text width = utf8 width of the string
        // " " (normal) = 1,  " [] " (selected) = 1+1+1 = 3
        std::string text_utf8;
        for (char32_t c : text) {
            if (c != 0)
                text_utf8 += utf8::encode(c);
        }
        int text_w = utf8::string_width(text_utf8);
        return 3 + text_w + (selected ? 3 : 1);
    };

    int scroll_off = props.scroll_offset;

    // ---- Determine how many candidates fit ----
    size_t max_display = std::min(props.candidates.size(), size_t(9));
    size_t display_count = max_display;
    bool need_scroll = false;

    // Try to fit as many as possible within term width.
    // If candidates overflow, they're accessible via page down (rime's page system).
    // Never show fewer than 1 candidate; single candidate text may be truncated.
    while (display_count > 1) {
        int total_w = fixed_w;
        for (size_t i = 0; i < display_count; ++i) {
            bool sel = (i == props.selected);
            total_w += candidate_width(props.candidates[i].text, sel);
        }

        if (total_w <= term_w) {
            break;
        }
        display_count--;
    }

    // If even 1 candidate doesn't fit, enable text scrolling for it
    if (display_count >= 1 && props.selected < props.candidates.size()) {
        int single_w = fixed_w + candidate_width(props.candidates[props.selected].text, true);
        if (single_w > term_w) {
            need_scroll = true;
        }
    }

    // Always show at least the selected candidate
    if (display_count == 0 && !props.candidates.empty()) {
        display_count = 1;
        need_scroll = true;
    }

    // ---- Build items ----
    Elements items;

    // Mode indicator
    bool is_chinese = props.mode.find("拼") != std::string::npos;
    items.push_back(ModeIndicator({.mode = props.mode, .is_chinese = is_chinese}));

    // Pinyin buffer - bright blue, no underline
    items.push_back(Text(" " + props.buffer + " ") | TextColor(color::kPinyin));

    // Candidates
    if (display_count == 1 && need_scroll && props.selected < props.candidates.size()) {
        // Only show the selected candidate with scrolling
        items.push_back(CandidateItem({.index = static_cast<int>(props.selected + 1),
                                       .text = props.candidates[props.selected].text,
                                       .selected = true,
                                       .scroll_offset = scroll_off}));
    } else {
        for (size_t i = 0; i < display_count; ++i) {
            items.push_back(CandidateItem({.index = static_cast<int>(i + 1),
                                           .text = props.candidates[i].text,
                                           .selected = (i == props.selected),
                                           .scroll_offset = (need_scroll && i == props.selected) ? scroll_off : 0}));
        }
    }

    // Filler
    items.push_back(Filler());

    // Cancel hint
    return HBox(std::move(items)) | BgColor(color::kBarBg) | Height(1);
}

}  // namespace ui
