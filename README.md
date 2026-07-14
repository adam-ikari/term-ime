# TTY 中文输入虚拟终端 (term-ime)

在 Linux TTY 环境中运行的虚拟终端，内置多语言输入法支持。

## 特性

- **PTY 虚拟终端**: 完整的终端模拟，支持 VT100 转义序列
- **多语言输入法**: 基于 librime，支持简体中文
- **可扩展架构**: 语言配置化，避免硬编码
- **异步事件驱动**: 基于 libuv 的高性能事件循环
- **UTF-8 支持**: 完整的 UTF-8 编解码，支持 CJK 宽字符
- **FTXUI 渲染**: 函数式终端 UI 组件

## 依赖

### 运行时依赖

**零运行时依赖** —— term-ime 生成完全静态链接的单文件二进制（`ldd` 显示 "not a dynamic executable"），不需要系统安装任何 `.so` 库。预编译版下载即用，源码构建也产出静态二进制。

### 构建依赖

仅构建工具链，无任何第三方系统库：

- `build-essential` / `cmake` / `pkg-config` - 构建工具

yaml-cpp / leveldb / marisa / opencc 都从 `deps/librime/deps/` 内置源码静态编译，**无需安装**它们的 `-dev` 包。Boost 已彻底剥离（librime 的 boost::algorithm/signals2/interprocess/crc/uuid 改用 `<rime/*.hpp>` 极简实现，regex 改用 `std::regex`；唯一保留的 `boost/sml.hpp` 来自内置 `deps/sml` 子模块，不依赖系统 Boost）。

### Git 子模块（均从源码编译为静态库）
- FTXUI - 终端 UI 组件
- spdlog - 日志库
- nlohmann_json - JSON 解析
- googletest - 单元测试框架
- librime - 输入法引擎（其嵌套依赖 yaml-cpp/leveldb/marisa/opencc 亦从源码静态编译）
- libuv - 异步事件循环
- sml / utf8proc - 状态机 / UTF-8 处理

## 构建

```bash
# 安装构建依赖（仅需工具链，无第三方库）
sudo apt-get install -y build-essential cmake pkg-config

# 克隆仓库（包含子模块）
git clone --recursive https://github.com/user/term-ime.git
cd term-ime

# 构建（产出完全静态链接的二进制）
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
| `Ctrl+A` `S` | 打开/关闭设置面板 |
| `1-9` | 选择候选词 |
| `Space` | 选择第一个候选词（候选状态时） |
| `Esc` | 取消输入 |
| `exit` | 退出 shell |

### 操作流程

1. 启动后进入英文模式，状态栏显示 `[EN]`
2. 按 `Ctrl+A` 然后按 `Space` 切换到中文模式
3. 输入拼音（如 `nihao`），显示候选词
4. 按 `1-9` 选择候选词，或按 `Space` 选择第一个
5. 输入 `exit` 退出程序

## 配置

配置文件位于 `~/.config/term-ime/config.json`:

```json
{
  "languages": [
    {"id": "zh-Hans", "name": "简体中文", "schema": "luna_pinyin_simp", "enabled": true}
  ],
  "active_language": "zh-Hans",
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
│   └── kaomoji.hpp/cpp     # 颜文字
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
- [ ] 更多转义序列支持
- [ ] 主题切换

## 许可证

MIT License
