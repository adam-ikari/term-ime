---
sidebar_position: 6
---

# TUI 输入法组件（term-terminal）

term-ime 的终端层（`term-terminal` 库）把「输入法怎么在 TUI 里展示」抽成了可复用组件：候选栏、状态栏、设置面板。它们不依赖 term-ime 应用本体，任何 TUI 程序都能拿来渲染中文输入界面。

## 组件清单

### 候选栏 `Renderer::render_candidates`

```cpp
void render_candidates(const std::vector<Candidate>& candidates,
                       size_t selected,
                       const std::string& buffer,
                       const std::string& mode = "EN",
                       int max_items = 9);
```

在终端**最后一行**绘制候选栏（`mode` + 拼音串 + 数字编号候选）。核心契约：

- **按宽度自适应**：只显示能完整放下的候选，放不下的不显示；`max_items` 是上限，窄终端自动少显示
- **显示的集合 = 可选的集合**：显示与 `select()` 的索引严格对齐，不会出现"显示了却选不中"
- **去重重绘**：IME 空闲时连续绘制是 no-op（有 `BAR_FORCE_REDRAW_EVERY` 兜底），避免 shell 输出驱动的高频重绘刷屏

### 状态栏与滚动区 `update_scroll_region` / `redraw_shell`

- `update_scroll_region()`：把可滚动区限定为 `rows-1`，状态栏独占最后一行；resize 后必须调用
- `redraw_shell(screen)`：全屏覆盖（如设置面板）关闭后，从 `Screen` 网格恢复 shell 视图
- 状态栏契约：任何 `ESC[2J`/resize 清掉最后一行后，下一次绘制必须强制重画（`last_bar_sig_` 失效），不会留下空白状态栏

### 设置面板 `ui::SettingsState`

```cpp
// 面板状态：items 是行列表，focus_index 是焦点行；on_change/on_close 是回调
struct SettingsState {
    bool visible;
    int focus_index;
    std::vector<SettingsItem> items;              // label/key/options/selected_index
    std::function<void(const std::string&, const std::string&)> on_change;
    std::function<void()> on_close;
};

Element SettingsPanel(SettingsState& state);      // 渲染（ftxui）
bool settings_handle_key(SettingsState& state, int key);  // 键盘处理（jk/↑↓ 移动、hl/←→ 改值）
void settings_init(SettingsState& state, const AppConfig& config);
void settings_apply(SettingsState& state, AppConfig& config);
```

面板是**全屏覆盖层**，打开时保存 shell 视图、关闭时 `redraw_shell` 恢复。模糊音 5 组、候选数量、界面语言都是面板里的行。

## 与 IME 引擎的组合

`term-terminal` 直接依赖 `term-ime-lib`：`Candidate` / `ImeMode` 类型来自引擎层，渲染层只管画。典型用法：

```cpp
// 输入：IME 消费按键后取状态渲染
if (ime.input(key)) {
    renderer.render_candidates(ime.candidates(), /*selected=*/0, ime.buffer(), "拼", 9);
} else {
    // 非 IME 按键：正常转发
}
```

## 渲染能力

- SGR 颜色：16 色、256 色（`38;5`）、24bit 真彩（`38;2`），前景/背景/加亮/反显
- ED/EL 擦除：清屏、清行各 mode
- CJK 宽字符正确对齐：两格宽度按续格标记，重绘/擦除不会拆出半字残影
- 完整 VT/ANSI 解析：CUP/HVP 光标定位、参数上限防护、Tab 制表位

## 构建与链接

```cmake
target_link_libraries(your_tui_app PRIVATE term-terminal)
```

依赖树：`term-terminal` → `term-ime-lib` → librime / utf8proc，全部静态。
