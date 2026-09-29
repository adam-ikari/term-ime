#include "settings.hpp"
#include "components.hpp"
#include "../util/i18n.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>

namespace ui {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

Element SettingsRow(const SettingsItem& item, bool focused) {
    Elements row;

    // Label
    auto label_style = focused ? Bold() : Dim();
    row.push_back(Text("  " + item.label + ": ") | label_style);

    // Value with selection indicator (use display_value if available)
    std::string value_display = item.display_value.empty() ? item.value : item.display_value;
    if (focused) {
        value_display = "[" + value_display + "]";
        row.push_back(Text(value_display) | Bold() | BgColor(FtxuiColor::Blue));
    } else {
        row.push_back(Text(value_display) | TextColor(FtxuiColor::Cyan));
    }

    // Options hint (if multiple options)
    if (item.options.size() > 1 && focused) {
        row.push_back(Text(" < ") | Dim());
        row.push_back(Text(std::to_string(item.selected_index + 1) + "/" + std::to_string(item.options.size())) |
                      Dim());
    }

    return HBox(row);
}

Element SettingsMenuItem(const std::string& label, bool selected) {
    if (selected) {
        return Text("> " + label + " <") | Bold() | BgColor(FtxuiColor::Blue);
    }
    return Text("  " + label + "  ") | Dim();
}

// The registered on_change callback may re-enter settings_init() and rebuild
// state.items (App::on_settings_change does exactly that for ui_language), which
// would invalidate a reference to the item being reported. Hand it copies.
void NotifyChange(const SettingsState& state, const SettingsItem& item) {
    if (!state.on_change) {
        return;
    }
    const std::string key = item.key;
    const std::string value = item.value;
    state.on_change(key, value);
}

}  // namespace

// ============================================================================
// Settings Panel Component
// ============================================================================

Element SettingsPanel(SettingsState& state) {
    Elements content;

    // Title
    content.push_back(Text(""));
    content.push_back(Text("  " + I18n::t("settings.title") + "  ") | Bold() | Inverted());
    content.push_back(Text(""));

    // The fuzzy items form one titled group (an ftxui window) at the tail of
    // the items list; the first member carries the title in group_header.
    // The description is a single fixed slot after all items so the window's
    // height never grows with focus — a taller-when-focused box would shove
    // the Close row off the 19-row budget (see settings-panel-row-budget).
    Elements group_rows;
    bool in_group = false;
    std::string group_title;
    for (size_t i = 0; i < state.items.size(); ++i) {
        const SettingsItem& item = state.items[i];
        const bool focused = (static_cast<int>(i) == state.focus_index);
        if (!item.group_header.empty()) {
            in_group = true;
            group_title = item.group_header;
        }
        if (in_group) {
            group_rows.push_back(SettingsRow(item, focused));
        } else {
            content.push_back(SettingsRow(item, focused));
        }
    }
    if (in_group) {
        content.push_back(ftxui::window(Text(group_title), VBox(std::move(group_rows))));
    }
    // Fixed description slot: whichever item holds focus (or Close).
    const int fi = state.focus_index;
    const std::string desc = (fi >= 0 && fi < static_cast<int>(state.items.size()))
                                 ? state.items[fi].description
                                 : I18n::t("settings.close.desc");
    content.push_back(Text("    " + desc) | Dim());

    // Separator
    content.push_back(Text(""));
    content.push_back(Text("  " + std::string(30, '-') + "  ") | Dim());

    // Instructions, one line: the panel has to fit a 20-row terminal.
    content.push_back(Text("  " + I18n::t("hint.select") + " ↑↓   " + I18n::t("hint.toggle_mode") +
                           " ←→/Enter   " + I18n::t("hint.cancel") + " Esc/Tab  ") |
                      Dim());

    // Close button
    content.push_back(Text(""));
    bool close_focused = (state.focus_index >= static_cast<int>(state.items.size()));
    content.push_back(SettingsMenuItem(I18n::t("settings.close"), close_focused));

    // Wrap in border
    auto inner = VBox(std::move(content));
    auto bordered = ftxui::border(inner);
    auto colored = bordered | bgcolor(FtxuiColor::Black);

    return VBox({Filler(), HBox({Filler(), colored, Filler()}), Filler()});
}

// ============================================================================
// Settings Menu Actions
// ============================================================================

bool settings_handle_key(SettingsState& state, int key) {
    const int item_count = static_cast<int>(state.items.size()) + 1;  // +1 for close button

    switch (key) {
    case 'k':
    case 'A':  // Up arrow (CSI A)
        state.focus_index = (state.focus_index - 1 + item_count) % item_count;
        return true;

    case 'j':
    case 'B':  // Down arrow (CSI B)
        state.focus_index = (state.focus_index + 1) % item_count;
        return true;

    case 'h':
    case 'D':  // Left arrow (CSI D)
        if (state.focus_index < static_cast<int>(state.items.size())) {
            auto& item = state.items[state.focus_index];
            if (item.selected_index > 0) {
                item.selected_index--;
                item.value = item.options[item.selected_index];
                if (!item.display_options.empty()) {
                    item.display_value = item.display_options[item.selected_index];
                }
                NotifyChange(state, item);
            }
        }
        return true;

    case 'l':
    case 'C':  // Right arrow (CSI C)
        if (state.focus_index < static_cast<int>(state.items.size())) {
            auto& item = state.items[state.focus_index];
            if (item.selected_index < static_cast<int>(item.options.size()) - 1) {
                item.selected_index++;
                item.value = item.options[item.selected_index];
                if (!item.display_options.empty()) {
                    item.display_value = item.display_options[item.selected_index];
                }
                NotifyChange(state, item);
            }
        }
        return true;

    case '\r':
    case '\n':
        if (state.focus_index >= static_cast<int>(state.items.size())) {
            // Close button
            if (state.on_close) {
                state.on_close();
            }
        } else {
            // Toggle or cycle value
            auto& item = state.items[state.focus_index];
            if (!item.options.empty()) {
                item.selected_index = (item.selected_index + 1) % item.options.size();
                item.value = item.options[item.selected_index];
                if (!item.display_options.empty()) {
                    item.display_value = item.display_options[item.selected_index];
                }
                NotifyChange(state, item);
            }
        }
        return true;

    case 0x1b:  // Escape
    case '\t':  // Tab
        if (state.on_close) {
            state.on_close();
        }
        return true;

    default:
        return false;
    }
}

// ============================================================================
// Settings Initialization
// ============================================================================

void settings_init(SettingsState& state, const AppConfig& config) {
    state.items.clear();

    // UI Language
    SettingsItem ui_lang;
    ui_lang.label = I18n::t("settings.ui_language");
    ui_lang.description = I18n::t("settings.ui_language.desc");
    ui_lang.key = "ui_language";
    ui_lang.options = {"en", "zh-CN"};
    ui_lang.display_options = {"English", "简体中文"};
    ui_lang.value = config.ui_language;
    for (size_t i = 0; i < ui_lang.options.size(); ++i) {
        if (ui_lang.options[i] == config.ui_language) {
            ui_lang.selected_index = i;
            ui_lang.display_value = ui_lang.display_options[i];
            break;
        }
    }
    state.items.push_back(ui_lang);

    // Candidate bar: how many candidates one page may show. The bar still shows
    // fewer on a narrow terminal; this is the upper bound.
    SettingsItem max_candidates;
    max_candidates.label = I18n::t("settings.max_candidates");
    max_candidates.description = I18n::t("settings.max_candidates.desc");
    max_candidates.key = "max_candidates";
    for (int n = 1; n <= 9; ++n) {
        max_candidates.options.push_back(std::to_string(n));
    }
    max_candidates.display_options = max_candidates.options;
    int wanted = config.max_candidates;
    if (wanted < 1)
        wanted = 1;
    if (wanted > 9)
        wanted = 9;
    max_candidates.selected_index = wanted - 1;
    max_candidates.value = max_candidates.options[max_candidates.selected_index];
    max_candidates.display_value = max_candidates.value;
    state.items.push_back(max_candidates);

    // Fuzzy pinyin groups — one toggle per group (平翘舌/n/l/r系/h/f/前后鼻音).
    // Empty selection = precise spelling; the engine maps the group set to a
    // bundled or generated schema (see RimeIme::fuzzy_variant). Each row carries
    // the pair it merges, because the group names alone do not say what stops
    // being distinguished.
    struct {
        const char* id;
        const char* i18n;
    } kFuzzyGroups[] = {
        {"zh_z", "settings.fuzzy.zh_z"}, {"n_l", "settings.fuzzy.n_l"},   {"r", "settings.fuzzy.r"},
        {"hu_f", "settings.fuzzy.hu_f"}, {"nose", "settings.fuzzy.nose"},
    };
    for (size_t n = 0; n < sizeof(kFuzzyGroups) / sizeof(kFuzzyGroups[0]); ++n) {
        const auto& g = kFuzzyGroups[n];
        const bool on =
            std::find(config.fuzzy_groups.begin(), config.fuzzy_groups.end(), g.id) != config.fuzzy_groups.end();
        SettingsItem fuzzy;
        fuzzy.label = I18n::t(g.i18n);
        fuzzy.description = I18n::t(std::string(g.i18n) + ".desc");
        if (n == 0) {
            fuzzy.group_header = I18n::t("settings.group.fuzzy");
        }
        fuzzy.key = std::string("fuzzy_") + g.id;
        fuzzy.options = {"off", "on"};
        fuzzy.display_options = {I18n::t("option.off"), I18n::t("option.on")};
        fuzzy.selected_index = on ? 1 : 0;
        fuzzy.value = fuzzy.options[fuzzy.selected_index];
        fuzzy.display_value = fuzzy.display_options[fuzzy.selected_index];
        state.items.push_back(fuzzy);
    }

    state.focus_index = 0;
}

void settings_apply(SettingsState& state, AppConfig& config) {
    for (const auto& item : state.items) {
        if (item.key == "ui_language") {
            config.ui_language = item.value;
        }
    }
}

}  // namespace ui
