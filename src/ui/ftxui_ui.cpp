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
            return text("") | size(HEIGHT, EQUAL, 1);
        }

        std::vector<Element> items;

        // 显示拼音缓冲区
        items.push_back(text(" 拼音: " + (buffer ? *buffer : "") + " "));

        // 显示候选词
        for (size_t i = 0; i < candidates->size(); ++i) {
            std::string label = std::to_string(i + 1) + "." + (*candidates)[i];
            if (selected && i == *selected) {
                items.push_back(text("[" + label + "]") | inverted);
            } else {
                items.push_back(text(" " + label + " "));
            }
        }

        return hbox(items) | inverted | size(HEIGHT, EQUAL, 1);
    });
}

Component CreateModeIndicator(const std::string* mode) {
    return Renderer([mode]() {
        std::string m = mode ? *mode : "中文";
        return text(" [" + m + "] ") | bold | color(Color::Cyan);
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
