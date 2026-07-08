#include "settings.hpp"
#include "components.hpp"
#include "../util/i18n.hpp"
#include <ftxui/dom/elements.hpp>

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

    // Settings items
    for (size_t i = 0; i < state.items.size(); ++i) {
        bool focused = (static_cast<int>(i) == state.focus_index);
        content.push_back(SettingsRow(state.items[i], focused));
    }

    // Separator
    content.push_back(Text(""));
    content.push_back(Text("  " + std::string(30, '-') + "  ") | Dim());
    content.push_back(Text(""));

    // Instructions
    content.push_back(Text("  " + I18n::t("hint.select") + ": Up/Down  ") | Dim());
    content.push_back(Text("  " + I18n::t("hint.toggle_mode") + ": Left/Right/Enter  ") | Dim());
    content.push_back(Text("  " + I18n::t("hint.cancel") + ": Esc/Tab  ") | Dim());

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
                if (state.on_change) {
                    state.on_change(item.key, item.value);
                }
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
                if (state.on_change) {
                    state.on_change(item.key, item.value);
                }
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
                if (state.on_change) {
                    state.on_change(item.key, item.value);
                }
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
    ui_lang.key = "ui_language";
    ui_lang.options = {"en", "zh-CN", "zh-TW", "ja"};
    ui_lang.display_options = {"English", "简体中文", "繁體中文", "日本語"};
    ui_lang.value = config.ui_language;
    for (size_t i = 0; i < ui_lang.options.size(); ++i) {
        if (ui_lang.options[i] == config.ui_language) {
            ui_lang.selected_index = i;
            ui_lang.display_value = ui_lang.display_options[i];
            break;
        }
    }
    state.items.push_back(ui_lang);

    // AI Ranking
    SettingsItem ai_ranking;
    ai_ranking.label = I18n::t("settings.ai_ranking");
    ai_ranking.key = "ai_ranking";
    ai_ranking.options = {"off", "on"};
    ai_ranking.display_options = {"Off", "On"};
    ai_ranking.value = config.llama_ranker.enabled ? "on" : "off";
    ai_ranking.selected_index = config.llama_ranker.enabled ? 1 : 0;
    ai_ranking.display_value = ai_ranking.display_options[ai_ranking.selected_index];
    state.items.push_back(ai_ranking);

    // Backend
    SettingsItem backend;
    backend.label = I18n::t("settings.backend");
    backend.key = "backend";
    backend.options = {"cpu", "cuda", "metal", "vulkan"};
    backend.display_options = {"CPU", "CUDA", "Metal", "Vulkan"};
    backend.value = config.llama_ranker.backend;
    for (size_t i = 0; i < backend.options.size(); ++i) {
        if (backend.options[i] == config.llama_ranker.backend) {
            backend.selected_index = i;
            backend.display_value = backend.display_options[i];
            break;
        }
    }
    state.items.push_back(backend);

    // Threads
    SettingsItem threads;
    threads.label = I18n::t("settings.threads");
    threads.key = "threads";
    threads.options = {"1", "2", "4", "8"};
    threads.display_options = {"1", "2", "4", "8"};
    threads.value = std::to_string(config.llama_ranker.n_threads);
    for (size_t i = 0; i < threads.options.size(); ++i) {
        if (threads.options[i] == threads.value) {
            threads.selected_index = i;
            threads.display_value = threads.display_options[i];
            break;
        }
    }
    state.items.push_back(threads);

    state.focus_index = 0;
}

void settings_apply(SettingsState& state, AppConfig& config) {
    for (const auto& item : state.items) {
        if (item.key == "ui_language") {
            config.ui_language = item.value;
        } else if (item.key == "ai_ranking") {
            config.llama_ranker.enabled = (item.value == "on");
        } else if (item.key == "backend") {
            config.llama_ranker.backend = item.value;
        } else if (item.key == "threads") {
            config.llama_ranker.n_threads = std::stoi(item.value);
        }
    }
}

}  // namespace ui
