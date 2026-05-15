# TTY 中文输入虚拟终端 (term-ime)

在 Linux TTY 环境中运行的虚拟终端，内置多语言输入法支持。

## 特性

- **PTY 虚拟终端**: 完整的终端模拟，支持 VT100 转义序列
- **多语言输入法**: 基于 librime，支持中文、日文等多种语言
- **可扩展架构**: 语言配置化，避免硬编码
- **异步事件驱动**: 基于 libuv 的高性能事件循环
- **智能候选词**: 可选的神经网络排序（ONNX Runtime）
- **UTF-8 支持**: 完整的 UTF-8 编解码，支持 CJK 宽字符
- **FTXUI 渲染**: 函数式终端 UI 组件

## 依赖

### 系统依赖
- `librime-dev` - 输入法引擎
- `libuv1-dev` - 异步事件循环
- `rime-data-luna-pinyin` - 拼音方案数据

### 构建依赖
- CMake >= 3.14
- C++17 编译器

### Git 子模块
- FTXUI - 终端 UI 组件
- spdlog - 日志库
- nlohmann_json - JSON 解析
- googletest - 单元测试框架

## 构建

```bash
# 安装依赖
sudo apt-get install librime-dev libuv1-dev rime-data-luna-pinyin

# 克隆仓库（包含子模块）
git clone --recursive https://github.com/user/term-ime.git
cd term-ime

# 构建
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

或使用 Makefile:

```bash
make build
```

## 运行

```bash
./build/term-ime
```

**注意**: 需要在真实 TTY 或支持 alternate screen 的终端中运行。

## 使用方法

### 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+A` `Space` | 切换中英文模式 |
| `1-9` | 选择候选词 |
| `Space` | 选择第一个候选词（候选状态时） |
| `Esc` | 取消输入 |
| `exit` | 退出 shell |

### 操作流程

1. 启动后进入英文模式，状态栏显示 `【EN】`
2. 按 `Ctrl+A` 然后按 `Space` 切换到中文模式
3. 输入拼音（如 `nihao`），显示候选词
4. 按 `1-9` 选择候选词，或按 `Space` 选择第一个
5. 输入 `exit` 退出程序

## 配置

配置文件位于 `~/.config/term-ime/config.json`:

```json
{
  "languages": [
    {"id": "zh-Hans", "name": "简体中文", "schema": "luna_pinyin_simp", "enabled": true},
    {"id": "zh-Hant", "name": "繁體中文", "schema": "luna_pinyin", "enabled": true},
    {"id": "zh-Hant-TW", "name": "正體中文（台灣）", "schema": "terra_pinyin", "enabled": false},
    {"id": "ja", "name": "日本語", "schema": "kana", "enabled": false}
  ],
  "active_language": "zh-Hans",
  "neural_ranker": {
    "enabled": false,
    "model_path": "models/ranker.onnx"
  },
  "log_level": "warn"
}
```

## 架构

```
src/
├── core/
│   ├── app.hpp/cpp        # 应用主逻辑
│   ├── config.hpp/cpp     # 配置管理
│   └── event_loop.hpp/cpp # libuv 事件循环
├── ime/
│   ├── engine.hpp         # IME 抽象接口
│   ├── rime_engine.hpp/cpp # librime 封装
│   ├── language.hpp/cpp   # 语言管理器
│   ├── ranker.hpp/cpp     # 候选词排序接口
│   └── neural_ranker.hpp/cpp # 神经网络排序器
├── terminal/
│   ├── pty.hpp/cpp        # PTY 管理
│   ├── screen.hpp/cpp     # 屏幕缓冲
│   └── parser.hpp/cpp     # 转义序列解析
├── ui/
│   └── renderer.hpp/cpp   # FTXUI 终端渲染
└── util/
    └── utf8.hpp/cpp       # UTF-8 工具

tests/
├── test_main.cpp          # 测试入口
├── test_utf8.cpp          # UTF-8 编解码测试
├── test_config.cpp        # 配置测试
└── test_ime_state.cpp     # IME 状态测试
```

## 开发

### 代码格式化

```bash
make format
```

需要安装 `clang-format`。

### 运行测试

```bash
make build
./build/term-ime-tests
```

或使用 CTest:

```bash
cd build && ctest --output-on-failure
```

### 测试覆盖

- **UTF-8 测试**: ASCII/中文/Emoji 编解码
- **配置测试**: 默认配置、JSON 序列化
- **IME 状态测试**: 候选词结构、状态枚举

## CI/CD

项目使用 GitHub Actions 自动构建和测试：

- 每次 push 和 PR 自动触发
- 构建并运行单元测试
- 代码格式检查（clang-format）

## 功能列表

- [x] PTY 创建与子进程管理
- [x] 屏幕缓冲
- [x] VT100 转义序列解析
- [x] UTF-8 输入输出
- [x] librime 多语言输入法
- [x] 终端大小变化处理
- [x] libuv 异步事件循环
- [x] 可配置多语言支持
- [x] FTXUI 候选词渲染
- [x] Ctrl+A+Space 模式切换
- [x] 空格选择第一个候选词
- [x] 进入/退出时清屏
- [x] 单元测试框架
- [x] CI/CD 自动构建
- [ ] 神经网络候选词排序
- [ ] 更多转义序列支持
- [ ] 颜色支持
- [ ] 主题切换

## 许可证

MIT License
