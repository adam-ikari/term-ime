#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace ui {

using namespace ftxui;

// 创建候选词栏组件
// candidates: 候选词列表
// selected: 当前选中索引
// buffer: 拼音缓冲区
Component CreateCandidateBar(
    const std::vector<std::string>* candidates,
    const size_t* selected,
    const std::string* buffer
);

// 创建模式指示器组件
Component CreateModeIndicator(const std::string* mode);

// 创建终端屏幕组件
Component CreateTerminalScreen(
    int rows,
    int cols,
    const std::vector<std::string>* lines
);

// 创建主界面布局
Component CreateMainLayout(
    Component screen,
    Component candidates,
    Component mode_indicator
);

}  // namespace ui
