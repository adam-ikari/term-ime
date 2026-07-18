# term-ime 测试规范与发布清单

> 只有在通过**所有测试用例**后才能发布新版本（创建 Release tag）。

## 测试套件

### 1. 单元测试（gtest）— `term-ime-tests`

| 测试文件 | 测试内容 | 用例数 |
|---------|---------|--------|
| `test_utf8.cpp` | UTF-8 编解码、CJK 宽度、RoundTrip | 6 |
| `test_config.cpp` | 配置加载/保存、语言配置序列化、默认值 | 6 |
| `test_ime_state.cpp` | ImeState/ImeMode 枚举、Candidate 结构体 | 4 |
| `test_i18n.cpp` | 翻译加载、默认翻译、语言切换、key 完整性 | 8 |
| `test_input_processor.cpp` | 输入处理器状态机、Ctrl+A 组合、ESC 序列 | 13 |
| `test_input_processor.cpp` | 边界条件：非 ASCII 字节、连续 ESC 序列 | 7 |

**运行**: `./build/term-ime-tests`

### 2. 端到端测试（plain assert）— `test-input-e2e`

| 测试 | 内容 |
|------|------|
| `test_ctrl_a_space` | Ctrl+A + Space 切换模式 |
| `test_ctrl_a_a` | Ctrl+A + A 转发 |
| `test_ctrl_a_s` | Ctrl+A + S 设置面板 |
| `test_ctrl_a_ctrl_a` | Ctrl+A + Ctrl+A 字面量 |
| `test_normal_key` | 普通键转发 |
| `test_escape_sequence` | ESC 序列重组 |
| `test_sequence` | 混合输入序列 |
| `test_compose_select_cycle` | 组词→选中→继续输入 |
| `test_multiple_select_cycles` | 多次组词→选中循环 |

**运行**: `./build/test-input-e2e`

### 3. UI 渲染测试（plain assert）— `test-ui-jsx` / `test-settings`

| 测试 | 内容 |
|------|------|
| `test-ui-jsx` | UI 组件渲染（CandidateItem, MainBar, EmptyBar 等） |
| `test-settings` | 设置面板、键盘导航、焦点切换、属性修改 |

**运行**: `./build/test-ui-jsx` 和 `./build/test-settings`

### 4. 手动 TUI 验收测试（发布前必须通过）

无法自动化（需要真实 TTY），每次发布前手动验证：

```
# 1. 启动
./build/term-ime

# 2. 切换到中文模式
Ctrl+A → Space → 状态栏显示 [中文]

# 3. 基本输入
输入 "ni" → 显示候选词 [你]
按 1 → 输出 "你"，状态栏回到 [中文]

# 4. 连续输入（5次以上）
输入 "ni" → 按 1 → 输入 "hao" → 按 1 → 输入 "shi" → 按 1
→ 输入 "jie" → 按 1 → 输入 "keji" → 按 1
→ 输出 "你好世界科技"

# 5. 部分消费拼音（关键场景）
输入 "nihao" → 候选词有 "你好"
按 1 选中 "你" → 剩余拼音 "hao" 仍然显示，可继续输入
→ 按 "1" 选中 "好" → 输出 "你好"

# 6. 无效拼音
输入 "sm" → 拼音 "sm" 仍然显示，没有候选项
按 Backspace → 回到 "s"，显示候选项
按 Backspace → 回到 [中文] 状态

# 7. 繁体词检查
输入 "ni" → 所有候选词都是简体中文（没有"裏"、"裡"、"妳"等繁体字）
输入 "ma" → 候选词 "吗"、"嘛"、"马"、"妈"、"骂" 都是简体

# 8. 模式切换
Ctrl+A → Space → 切换到 [EN] → 输入 "hello" 直接输出
Ctrl+A → Space → 回到 [中文] → 输入 "ni" 正常组词

# 9. 设置面板
Ctrl+A → S → 显示设置面板，标题 "设置"
按 Esc → 关闭面板

# 10. 输入大量字母（性能测试）
输入 "nihaoma" 或 "asdfghjkl" → 不应卡顿
```

## 发布清单

发布前必须验证以下所有项：

### 构建检查
- [ ] `make build` 或 `cmake --build build -j$(nproc)` 编译成功，无 warning
- [ ] `file build/term-ime` 显示 "statically linked"
- [ ] `ldd build/term-ime` 显示 "not a dynamic executable"

### 自动化测试
- [ ] `./build/term-ime-tests` — 全部通过
- [ ] `./build/test-input-e2e` — 全部通过
- [ ] `./build/test-ui-jsx` — 全部通过
- [ ] `./build/test-settings` — 全部通过

### 手动 TUI 验收
- [ ] 启动正常
- [ ] 中文模式切换正常
- [ ] 基本输入 + 候选词显示
- [ ] 连续输入 5 次以上无卡死
- [ ] 部分消费拼音后剩余拼音仍显示
- [ ] 无效拼音后拼音仍显示
- [ ] 所有候选词为简体中文
- [ ] 模式切换正常
- [ ] 设置面板正常
- [ ] 输入大量字母无卡顿
- [ ] 退出正常（无崩溃）

### 发布流程
- [ ] 所有测试通过
- [ ] 代码已提交并推送到 master
- [ ] 创建 Release tag（`git tag -a vX.Y.Z && git push origin vX.Y.Z`）
- [ ] 等待 GitHub Actions 构建完成
- [ ] 验证构建产物（下载 release 二进制，检查静态链接）