# term-ime — 终端上，终于能打中文了

> **term-ime 是一个 Linux TTY 终端输入法（TTY IME / terminal input method）**：不装 X、不装 Wayland、不碰 D-Bus，一个完全静态的单文件二进制，把 librime 拼音带进 SSH、Docker、WSL 和信创机器（麒麟 / UOS）的纯终端。
>
> 名字读作 "term" + "IME"（输入法），**不是 "term-time"**。主命令是短命令 `ti`。

```bash
# 一键安装，免 sudo；网站：https://adam-ikari.github.io/term-ime/
curl -fsSL https://adam-ikari.github.io/term-ime/install.sh | bash
# 装好后运行（短命令 `ti`；`term-ime` 为兼容别名，二者等价）
ti
```

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
git clone --recursive https://github.com/adam-ikari/term-ime.git
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
ti
```

**注意**: 需要在真实 TTY 或支持 alternate screen 的终端中运行。

> 命令名：主命令是短命令 `ti`（terminal input）。从源码构建时产物是 `./build/term-ime`，安装脚本会把它装成 `ti` 并建一个 `term-ime` 软链接，两个名字都能用。

## 使用方法

### 快捷键

| 快捷键 | 功能 |
|--------|------|
| `Ctrl+A` `Space` | 切换中英文模式 |
| `Ctrl+A` `S` | 打开/关闭设置面板 |
| `1-9` | 选择候选词 |
| `Space` | 选择第一个候选词（候选状态时） |
| `Esc` | 取消输入 |
| `,` `.` | 候选词上一组 / 下一组（屏幕外的候选按可见数量成组翻页） |
| `←` `↑` `PgUp` | 候选词上一组（同 `,`） |
| `→` `↓` `PgDn` | 候选词下一组（同 `.`） |
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

### 候选词显示

候选栏按终端宽度自适应：只显示能**完整放下**的候选词（窄终端会自动少显示几个），
不会出现被截断的半个候选。按 `,` / `.`（或 `<` / `>`）成组翻页，
因此窄终端下屏幕外的候选不会被跳过。

`max_candidates`（1-9，默认 `9`）是每页候选词的数量上限；调大它可以让宽终端一次显示更多。
旧配置里的 `page_size` 仍然兼容读取。
设置面板里也有「候选词数量」（1-9）一项，按 `↑`/`↓` 移动、`←`/`→` 或 `Enter` 改值。

### 模糊音

设置面板里模糊音按组独立开关，共 5 组：

| 开关 | 覆盖 |
| --- | --- |
| 平翘舌（zh/z） | zh/ch/sh ↔ z/c/s |
| n/l | n ↔ l |
| r 系 | r → l、r → y |
| h/f | hu ↔ f |
| 前后鼻音 | en/eng、in/ing、an/ang（含 ian/iang、uan/uang、üan/üang） |

默认全部**开启**（与历史行为一致）。全部关闭 = 精确拼音。
例：n/l 开时输入 `la` 会出现「那/拿」；前后鼻音开时 `fan` 会出现「方」、`qian` 会出现「枪」；
对应组关闭后只剩精确读音（`啦/拉`、`饭/反`、`前/钱`）。

配置文件键为 `fuzzy_groups`（数组，如 `["zh_z","n_l","r","hu_f","nose"]`；空数组 = 精确）。
旧配置 `fuzzy_pinyin`（`true`/`false`）仍兼容读取。全开或全关用现成 schema，
部分开启时按选中组合成一份 schema（首次切换时部署约 1 秒），之后即时生效。

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
│   └── parser.hpp/cpp     # 转义序列解析(CSI 光标/SGR 颜色/ED/EL 擦除)
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
- [x] SGR 16 色支持(前景/背景/加亮/反显)
- [x] ED/EL 擦除(清屏/清行)
- [ ] 更多转义序列支持(如 SGR 自查询、OSC 透传)
- [ ] 主题切换

## 许可证

MIT License
