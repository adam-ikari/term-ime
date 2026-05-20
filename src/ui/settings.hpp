#pragma once

#include "jsx.hpp"
#include "../core/config.hpp"
#include <functional>
#include <vector>

namespace ui {

// ============================================================================
// Settings Panel Types
// ============================================================================

// Settings menu item
struct SettingsItem {
    std::string label;
    std::string key;
    std::string value;
    std::vector<std::string> options;  // Available options for selection
    int selected_index = 0;
};

// Settings panel state
struct SettingsState {
    bool visible = false;
    int focus_index = 0;
    std::vector<SettingsItem> items;

    // Callbacks
    std::function<void(const std::string& key, const std::string& value)> on_change;
    std::function<void()> on_close;
};

// ============================================================================
// Settings Panel Component
// ============================================================================

Element SettingsPanel(SettingsState& state);

// ============================================================================
// Settings Menu Actions
// ============================================================================

// Handle key press in settings panel
bool settings_handle_key(SettingsState& state, int key);

// Initialize settings from config
void settings_init(SettingsState& state, const AppConfig& config);

// Apply settings to config
void settings_apply(SettingsState& state, AppConfig& config);

}  // namespace ui