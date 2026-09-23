---
sidebar_position: 5
---

# 输入法库（term-ime-lib）

term-ime 的 IME 引擎是一个**独立、可复用的静态库**：`term-ime-lib`。它封装 librime（Rime 输入法框架），对外暴露一个极小的 C++ 接口——任何程序（TUI、游戏、终端模拟器、编辑器插件）都能嵌入它，获得完整的中文拼音输入能力，而不必自己碰 Rime API。

## 库结构

| 库目标 | 内容 | 依赖 |
|--------|------|------|
| `term-ime-lib` | IME 引擎：`ImeEngine` 接口、`RimeIme` 实现、语言管理、颜文字、i18n | librime、utf8proc（全部源码内置，静态链接） |
| `term-terminal` | PTY + VT 解析 + 候选栏/状态栏/设置面板渲染（见 [TUI 输入法组件](/docs/tui-component)） | `term-ime-lib` |
| `term-core` | 应用主逻辑（事件循环、配置、快捷键） | `term-terminal` |

构建产物：`libtermime.a`（`OUTPUT_NAME termime`）。

## 核心接口

一切输入行为都收敛在抽象类 `ImeEngine`（`src/ime/engine.hpp`）上：

```cpp
class ImeEngine {
   public:
    virtual ~ImeEngine() = default;

    virtual bool input(char ch) = 0;                  // 喂一个按键；返回 true = 被 IME 消费
    virtual ImeState state() const = 0;               // Inactive / Composing / Selecting
    virtual ImeMode mode() const = 0;                 // Chinese / English
    virtual void set_mode(ImeMode mode) = 0;
    virtual void toggle_mode() = 0;
    virtual std::string buffer() const = 0;           // 当前拼音串（如 "nihao"）
    virtual std::vector<Candidate> candidates() const = 0;  // 候选列表（text + code）
    virtual std::u32string select(int index) = 0;     // 选中候选，返回上屏文本
    virtual void backspace() = 0;                     // 删一个音节字符
    virtual void cancel() = 0;                        // 清空整个组合
    virtual void page_up() = 0;
    virtual void page_down() = 0;
};
```

`RimeIme`（`src/ime/rime_engine.hpp`）是其 librime 实现。状态机约定：

- **English 模式**下 `input()` 直接返回 `false`——IME 完全旁路，按键原样交给宿主
- 组合态：`state() == Composing`（输拼音中）→ `Selecting`（有候选可选）
- `select(i)` 上屏后若拼音串已空，内部自动清空组合，宿主可直接回到空闲态

## 嵌入示例

```cpp
#include "ime/rime_engine.hpp"

// 1. 构造（可指定数据目录；默认用打包的 rime-data + XDG 用户目录）
RimeIme ime;

// 2. 初始化：部署 schema、建会话。失败则降级为无 IME。
if (!ime.initialize()) {
    // 日志已输出原因；程序可继续运行（英文直通）
}

// 3. 选 schema + 配置模糊音（5 组独立开关）
ime.select_schema("luna_pinyin_simp");
ime.set_fuzzy_groups({"zh_z", "n_l", "r", "hu_f", "nose"});  // 空 = 精确

// 4. 事件循环：把按键喂进去，按状态渲染
ime.set_mode(ImeMode::Chinese);
if (ime.input(c)) {            // 被 IME 消费
    if (ime.state() != ImeState::Inactive) {
        auto buf = ime.buffer();                    // 拼音串
        auto cands = ime.candidates();              // 候选
        // ... 画你的候选 UI，或在终端里用 term-terminal 的 render_candidates
    }
} else {
    // 直通：这是宿主自己的按键，照常处理
}
```

## 构建与链接

```bash
git clone --recursive https://github.com/adam-ikari/term-ime.git
cd term-ime && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

```cmake
# 你的项目里
target_link_libraries(your_app PRIVATE term-ime-lib)
```

库是纯静态的：librime 及其依赖（yaml-cpp、leveldb、marisa、opencc）全部由 vendored 源码编译，无任何运行时共享库。

## Rime 专属能力

- `select_schema(id)` / `get_schema_list()` / `get_current_schema()`——schema 级控制
- `set_fuzzy_groups(...)`——模糊音按组开关；部分开启时自动合成 per-combination schema 并部署
- 简化字保证：OpenCC 链式转换（繁→简 + 日文新字体 + 异体字），候选里不会出现「楽/薬/妳」这类非简体字

## 测试

引擎契约由 `tests/test_input_processor.cpp`、`tests/test_config.cpp` 与 PTY 端到端测试（`tests/test_fuzzy_pinyin.py` 等）覆盖，CI 全量运行。
