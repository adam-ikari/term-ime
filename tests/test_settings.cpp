#include "ui/settings.hpp"
#include "ui/jsx.hpp"
#include "util/i18n.hpp"
#include "core/config.hpp"
#include <ftxui/screen/screen.hpp>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace ui;

int main() {
    // Initialize i18n
    I18n::init(I18n::Lang::ZH_CN);

    std::cout << "=== Settings Panel Test ===\n\n";

    // Create settings state
    SettingsState state;
    AppConfig config;

    // Test 1: Initialize settings
    std::cout << "Test 1: Initialize settings from config\n";
    settings_init(state, config);
    std::cout << "  Items count: " << state.items.size() << "\n";
    for (const auto& item : state.items) {
        std::cout << "  - " << item.label << ": " << item.value;
        std::cout << " (options: " << item.options.size() << ")\n";
    }
    std::cout << "\n";

    // Test 2: Render settings panel
    std::cout << "Test 2: Render settings panel\n";
    state.visible = true;
    auto element = SettingsPanel(state);

    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, element);
    std::cout << "Panel rendered (60x20)\n";
    std::cout << "--- Render Output ---\n";
    std::cout << screen.ToString() << "\n";
    std::cout << "--- End ---\n\n";

    // Test 3: Navigate settings with arrow keys
    std::cout << "Test 3: Navigate settings (arrow keys and vim keys)\n";
    settings_handle_key(state, 'j');  // Move down (vim)
    std::cout << "  After 'j': focus_index = " << state.focus_index << "\n";
    settings_handle_key(state, 'B');  // Move down (arrow down)
    std::cout << "  After 'B' (down arrow): focus_index = " << state.focus_index << "\n";
    settings_handle_key(state, 'k');  // Move up (vim)
    std::cout << "  After 'k': focus_index = " << state.focus_index << "\n";
    settings_handle_key(state, 'A');  // Move up (arrow up)
    std::cout << "  After 'A' (up arrow): focus_index = " << state.focus_index << "\n\n";

    // Test 4: Change setting value with arrow keys
    std::cout << "Test 4: Change setting value\n";
    state.focus_index = 0;  // Focus on UI language
    std::cout << "  Current UI language: " << state.items[0].value << "\n";
    settings_handle_key(state, 'l');  // Move right (vim)
    std::cout << "  After 'l': " << state.items[0].value << "\n";
    settings_handle_key(state, 'C');  // Move right (arrow right)
    std::cout << "  After 'C' (right arrow): " << state.items[0].value << "\n";
    settings_handle_key(state, 'h');  // Move left (vim)
    std::cout << "  After 'h': " << state.items[0].value << "\n";
    settings_handle_key(state, 'D');  // Move left (arrow left)
    std::cout << "  After 'D' (left arrow): " << state.items[0].value << "\n\n";

    // Test 5: Apply settings
    std::cout << "Test 5: Apply settings to config\n";
    state.items[0].value = "en";
    state.items[0].selected_index = 0;
    settings_apply(state, config);
    std::cout << "  Config ui_language: " << config.ui_language << "\n\n";

    // Test 6: Close settings
    std::cout << "Test 6: Close settings panel\n";
    bool handled = settings_handle_key(state, 0x1b);  // Escape
    std::cout << "  Escape handled: " << (handled ? "true" : "false") << "\n\n";

    // Test 7: 面板高度预算。FTXUI 超预算只裁不折，被裁掉的先是底部的「关闭」——
    // 面板打不开设置就等于功能没了，所以逐尺寸、逐焦点走一遍，钉住「底部边框 + 关闭 +
    // 当前项描述」三样都在，且行数不随焦点变化。60x20 是最窄最小的受支持终端。
    settings_init(state, config);
    int failures = 0;
    const int last_focus = static_cast<int>(state.items.size());  // == Close
    const std::vector<std::pair<int, int>> sizes = {{60, 20}, {80, 20}, {80, 24}};
    for (const auto& size : sizes) {
        int drawn_rows = 0;
        for (int f = 0; f <= last_focus; ++f) {
            state.focus_index = f;
            auto scr =
                ftxui::Screen::Create(ftxui::Dimension::Fixed(size.first), ftxui::Dimension::Fixed(size.second));
            ftxui::Render(scr, SettingsPanel(state));
            const std::string text = scr.ToString();
            const std::vector<std::string> lines = [&text] {
                std::vector<std::string> out;
                std::stringstream ss(text);
                std::string line;
                while (std::getline(ss, line)) {
                    out.push_back(line);
                }
                // ToString() ends with a newline, so the last element is empty.
                if (!out.empty() && out.back().empty()) {
                    out.pop_back();
                }
                return out;
            }();

            // The panel is vertically centred, so the box need not sit on the last
            // row — but if it is taller than the screen FTXUI drops the bottom border
            // without warning, so its presence is what proves nothing was clipped.
            bool bottom_border = false;
            for (const auto& line : lines) {
                if (line.find("╰") != std::string::npos && line.find("╯") != std::string::npos) {
                    bottom_border = true;
                }
            }
            // The Close row's "> <" markers are focus decoration, so match the label.
            const bool close_visible = text.find(I18n::get("settings.close")) != std::string::npos;
            const std::string want_desc =
                (f < last_focus) ? state.items[f].description : I18n::get("settings.close.desc");
            const bool desc_visible = !want_desc.empty() && text.find(want_desc) != std::string::npos;

            // Height must not move with focus: the description slot is filled by
            // whichever row holds focus, never added or removed.
            int box_rows = 0;
            for (const auto& line : lines) {
                if (line.find("│") != std::string::npos || line.find("╭") != std::string::npos ||
                    line.find("╰") != std::string::npos) {
                    ++box_rows;
                }
            }
            const bool same_height = (drawn_rows == 0 || box_rows == drawn_rows);
            drawn_rows = box_rows;

            if (!bottom_border || !close_visible || !desc_visible || !same_height) {
                ++failures;
                std::cout << "  FAIL " << size.first << "x" << size.second << " focus=" << f
                          << " bottom_border=" << bottom_border << " close=" << close_visible
                          << " desc=" << desc_visible << " same_height=" << same_height << " [" << want_desc << "]\n";
            }
        }
        std::cout << "  " << size.first << "x" << size.second << ": box rows " << drawn_rows << ", foci walked "
                  << (last_focus + 1) << "\n";
    }
    std::cout << "Test 7: panel fits every supported size at every focus position — "
              << (failures == 0 ? "OK" : "FAILED") << "\n\n";

    if (failures != 0) {
        std::cout << "=== Settings Tests FAILED (" << failures << " clipped renders) ===\n";
        return 1;
    }

    std::cout << "=== All Settings Tests Passed ===\n";
    return 0;
}