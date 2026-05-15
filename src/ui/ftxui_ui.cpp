#include "ftxui_ui.hpp"

namespace ui {

using namespace ftxui;

Component CreateCandidateBar(
    const std::vector<std::string>* candidates,
    const size_t* selected,
    const std::string* buffer
) {
    return Renderer([candidates, selected, buffer]() {
        if (!candidates || candidates->empty()) {
            // 显示提示信息
            return hbox({
                text(" Ctrl+Space 切换中英文 ") | dim,
                text("│") | color(Color::GrayDark),
                text(" 1-9 选择候选词 ") | dim,
                text("│") | color(Color::GrayDark),
                text(" ←→ 翻页 ") | dim,
                text("│") | color(Color::GrayDark),
                text(" Esc 取消 ") | dim,
                text("│") | color(Color::GrayDark),
                text(" Backspace 删除 ") | dim,
                text("│") | color(Color::GrayDark),
                text(" Ctrl+C 退出 ") | dim,
            }) | size(HEIGHT, EQUAL, 1);
        }

        std::vector<Element> items;

        // 显示拼音缓冲区
        std::string buf = buffer ? *buffer : "";
        items.push_back(text(" 拼音: " + buf + " ") | bold);

        // 显示候选词
        for (size_t i = 0; i < candidates->size(); ++i) {
            std::string label = std::to_string(i + 1) + "." + (*candidates)[i];
            if (selected && i == *selected) {
                items.push_back(text(" [" + label + "] ") | inverted | bold);
            } else {
                items.push_back(text(" " + label + " ") | color(Color::Yellow));
            }
        }

        // 添加翻页提示
        items.push_back(text("  ←→翻页 ") | dim);
        items.push_back(text(" PgUp/PgDn ") | dim);
        items.push_back(text(" Backspace删除 ") | dim);

        return hbox(items) | inverted | size(HEIGHT, EQUAL, 1);
    });
}

Component CreateModeIndicator(const std::string* mode) {
    return Renderer([mode]() {
        std::string m = mode ? *mode : "简体中文";
        // 根据模式显示不同颜色
        Color mode_color = (m.find("简体") != std::string::npos) ? Color::Green :
                           (m.find("繁体") != std::string::npos) ? Color::Yellow :
                           Color::Cyan;
        return hbox({
            text(" 【") | dim,
            text(m) | bold | color(mode_color),
            text("】 ") | dim,
            text("Ctrl+Space切换 ") | dim | color(Color::GrayDark),
            text("│ ") | dim,
            text("Shift 切换简繁 ") | dim | color(Color::GrayDark),
        });
    });
}

Component CreateTerminalScreen(
    int rows,
    int cols,
    const std::vector<std::string>* lines
) {
    return Renderer([rows, lines]() {
        std::vector<Element> elements;
        if (lines) {
            for (const auto& line : *lines) {
                elements.push_back(text(line));
            }
        }
        // 填充到指定行数
        while (static_cast<int>(elements.size()) < rows) {
            elements.push_back(text(""));
        }
        return vbox(elements);
    });
}

Component CreateMainLayout(
    Component screen,
    Component candidates,
    Component mode_indicator
) {
    auto layout = Container::Vertical({
        screen,
    });

    return Renderer(layout, [screen, candidates, mode_indicator]() {
        return vbox({
            screen->Render(),
            filler(),
            hbox({
                mode_indicator->Render(),
                filler(),
            }),
            candidates->Render(),
        });
    });
}

}  // namespace ui
