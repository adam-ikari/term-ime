#include "components.hpp"
#include "../util/utf8.hpp"
#include "../util/i18n.hpp"
#include <ftxui/dom/elements.hpp>

namespace ui {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

std::string u32_to_utf8(const std::u32string& s) {
    std::string result;
    for (char32_t c : s) {
        if (c != 0) {
            result += utf8::encode(c);
        }
    }
    return result;
}

FtxuiColor GetModeColor(const std::string& mode) {
    return (mode.find("中文") != std::string::npos) ? FtxuiColor::Green : FtxuiColor::Cyan;
}

}  // namespace

// ============================================================================
// ModeIndicator
// ============================================================================

Element ModeIndicator(const ModeIndicatorProps& props) {
    FtxuiColor mode_color = props.is_chinese ? FtxuiColor::Green : FtxuiColor::Cyan;

    return HBox({
        Text(" 【") | Dim(),
        Text(props.mode) | Bold() | TextColor(mode_color),
        Text("】 ") | Dim()
    });
}

// ============================================================================
// CandidateItem
// ============================================================================

Element CandidateItem(const CandidateItemProps& props) {
    std::string text_str = u32_to_utf8(props.text);
    std::string label = std::to_string(props.index) + "." + text_str;

    if (props.selected) {
        return Text(" [" + label + "] ") | Bold() | BgColor(FtxuiColor::Blue);
    } else {
        return Text(" " + label + " ") | TextColor(FtxuiColor::Yellow);
    }
}

// ============================================================================
// CandidateBar
// ============================================================================

Element CandidateBar(const CandidateBarProps& props) {
    Elements items;

    // Mode indicator
    bool is_chinese = props.mode.find("中文") != std::string::npos;
    items.push_back(ModeIndicator({
        .mode = props.mode,
        .is_chinese = is_chinese
    }));

    // Pinyin buffer
    items.push_back(Text(" " + I18n::t("status.pinyin") + ": " + props.buffer + " ") | Bold());

    // Candidates
    for (size_t i = 0; i < props.candidates.size() && i < 9; ++i) {
        items.push_back(CandidateItem({
            .index = static_cast<int>(i + 1),
            .text = props.candidates[i].text,
            .selected = (i == props.selected)
        }));
    }

    // Cancel hint
    items.push_back(Text("  Esc " + I18n::t("hint.cancel") + " ") | Dim() | TextColor(FtxuiColor::GrayDark));

    return HBox(std::move(items)) | Inverted() | Height(1);
}

// ============================================================================
// EmptyBar
// ============================================================================

Element EmptyBar(const EmptyBarProps& props) {
    bool is_chinese = props.mode.find("中文") != std::string::npos;

    Elements items;

    // Mode indicator
    items.push_back(ModeIndicator({
        .mode = props.mode,
        .is_chinese = is_chinese
    }));

    // Filler
    items.push_back(Filler());

    // Hints
    items.push_back(HintsBar());

    return HBox(std::move(items)) | Inverted() | Height(1);
}

// ============================================================================
// AIIndicator
// ============================================================================

Element AIIndicator(const AIIndicatorProps& props) {
    if (props.downloading) {
        std::string progress = std::to_string(props.download_progress);
        return Text(" [下载模型 " + progress + "%]") | TextColor(FtxuiColor::Yellow);
    }

    if (props.loading) {
        // Use spinner animation
        static size_t frame = 0;
        frame = (frame + 1) % SpinnerFrames().size();
        return Text(" [" + SpinnerFrame(frame) + " AI...]") | TextColor(FtxuiColor::Cyan);
    }

    if (props.enabled) {
        return Text(" [AI]") | TextColor(FtxuiColor::Green);
    }

    return Empty();
}

// ============================================================================
// StatusBar
// ============================================================================

Element StatusBar(const StatusBarProps& props) {
    Elements items;

    // Language and mode
    items.push_back(Text(" 【" + props.lang_name + " " + props.mode + "】") | Bold());

    // AI indicator
    items.push_back(AIIndicator({
        .enabled = props.ai_enabled,
        .loading = props.ai_loading,
        .downloading = props.downloading,
        .download_progress = props.download_progress
    }));

    return HBox(std::move(items)) | TextColor(GetModeColor(props.mode));
}

// ============================================================================
// HintsBar
// ============================================================================

Element HintItem(const HintItemProps& props) {
    return Text(" " + props.key + " " + props.action + " ") | Dim() | TextColor(FtxuiColor::GrayDark);
}

Element HintsBar() {
    return HBox({
        HintItem({.key = "Ctrl+A,Space", .action = I18n::t("hint.toggle_mode")}),
        Text("|") | TextColor(FtxuiColor::GrayDark),
        HintItem({.key = "1-9", .action = I18n::t("hint.select")}),
        Text("|") | TextColor(FtxuiColor::GrayDark),
        HintItem({.key = "Esc", .action = I18n::t("hint.cancel")}),
        Text("|") | TextColor(FtxuiColor::GrayDark),
        HintItem({.key = "Ctrl+A,A", .action = I18n::t("hint.ai_toggle")}),
        Text("|") | TextColor(FtxuiColor::GrayDark),
        HintItem({.key = "Ctrl+A,S", .action = I18n::t("settings.title")})
    });
}

// ============================================================================
// MainBar
// ============================================================================

Element MainBar(const MainBarProps& props) {
    if (props.candidates.empty()) {
        return EmptyBar({.mode = props.mode});
    }

    // Build main bar with candidates
    Elements items;

    // Mode indicator
    bool is_chinese = props.mode.find("中文") != std::string::npos;
    items.push_back(ModeIndicator({
        .mode = props.mode,
        .is_chinese = is_chinese
    }));

    // Pinyin buffer
    items.push_back(Text(" " + I18n::t("status.pinyin") + ": " + props.buffer + " ") | Bold());

    // Candidates
    for (size_t i = 0; i < props.candidates.size() && i < 9; ++i) {
        items.push_back(CandidateItem({
            .index = static_cast<int>(i + 1),
            .text = props.candidates[i].text,
            .selected = (i == props.selected)
        }));
    }

    // AI indicator
    items.push_back(AIIndicator({
        .enabled = props.ai_enabled,
        .loading = props.ai_loading,
        .downloading = props.downloading,
        .download_progress = props.download_progress
    }));

    // Filler
    items.push_back(Filler());

    // Cancel hint
    items.push_back(Text(" Esc " + I18n::t("hint.cancel") + " ") | Dim() | TextColor(FtxuiColor::GrayDark));

    return HBox(std::move(items)) | Inverted() | Height(1);
}

}  // namespace ui
