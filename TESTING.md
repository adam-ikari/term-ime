# term-ime 测试规范与发布清单

> 只有在通过**所有测试用例**后才能发布新版本（创建 Release tag）。

## 测试套件

### 1. 单元测试（gtest）— `term-ime-tests`

| 测试文件 | 测试内容 | 用例数 |
|---------|---------|--------|
| `test_utf8.cpp` | UTF-8 编解码、CJK 宽度、RoundTrip | 48 |
| `test_input_processor.cpp` | 输入处理器状态机、Ctrl+A 组合、ESC 序列、非 ASCII 边界 | 23 |
| `test_config.cpp` | 配置加载/保存、语言配置序列化、默认值 | 12 |
| `test_i18n.cpp` | 翻译加载、默认翻译、语言切换、key 完整性 | 11 |
| `test_ime_contract.cpp` | `ImeEngine` 公开契约：状态推进、`input()` 拒绝、`select()`/`cancel()`、`take_commit()` 排空 | 8 |
| `test_pty.cpp` | `TxByteQueue` 出站队列单元行为 | 5 |
| `test_rime_data.cpp` | 词典数据目录解析 | 5 |
| `test_event_loop.cpp` | libuv 事件循环 | 3 |
| `test_pty_stall.cpp` | 出站队列溢出：连续前缀不变量（PTY 集成） | 3 |
| **合计** | | **118** |

**运行**: `./build/term-ime-tests`（或 `ctest --test-dir build`）

> 用例数由 `./build/term-ime-tests --gtest_list_tests` 得出，不要手工维护 —— 
> 之前这张表写的是 6/6/4/8/13，与实际的 48/12/8/11/23 几乎全错，且列了一个
> 已删除的文件。
>
> **同义反复的断言会被删掉，不补测试。** 曾经的 `test_ime_state.cpp` 整个文件都是
> `EXPECT_NE(ImeState::Inactive, ImeState::Composing)` 和「赋值后断言自己等于自己」——
> 测的是 C++ 语言不是 term-ime，永远不会失败。它撑起「114 个测试」这个数字，让人
> 以为 IME 状态被覆盖了。新增断言前先问：**观测量在有 bug 和无 bug 时是否不同？**

### 2. 端到端测试（python + PTY）— **CI 实际跑的就是这一批**

这 7 个套件跑真实二进制：独立 HOME + 一次性 rime 用户目录 + PTY。它们是本项目
最有价值的测试 —— 三个吞键 bug（Enter 不提交、`[7` 丢 7、转义序列被拆散）全是在这里
被 e2e 断言逮住的，不是靠单元测试。

| 套件 | 覆盖 |
|------|------|
| `test_fuzzy_pinyin.py` | 模糊音 5 组独立开关契约；提交、多段提交、**转义序列整体性**、**按键耗尽**、**未吞数字**、**resize** |
| `test_e2e.py` | 基本流程：模式切换、组词、选中 |
| `test_settings_e2e.py` | 设置面板读写与持久化 |
| `test_settings_panel_e2e.py` | 设置面板键盘导航（指纹字串 `Esc/Tab`，改提示文案要同步） |
| `test_simplified_candidates.py` | 候选栏必须给简体形式 |
| `test_punctuation.py` | 中文模式标点交给 librime punctuator，提交全角 |
| `test_paste_delivery.py` | 大段粘贴到达率与**字节序**；`--stall` 让读者先睡，覆盖出站队列溢出 |

**运行**: `python3 tests/<name>.py`

断言用 `check(name, ok, detail)`，不是裸 `assert` —— 因为要打印失败细节并累计
通过数，裸 `assert` 在 `-O` 下还会被优化掉。

> `tests/fuzz_drive.py`（手动，不在 CI）驱动 `tests/monkey_sequences.py` 的动作模型
> 与四个定向探针，检查不变量是**进程必须活着**。它**不检查输出对错、不检查 hang**
> （卡死但没退出的进程读作 ok）、每轮结束就 kill 所以看不见关机崩溃。
> 结论只能读作「没找到崩溃」，不是「没有问题」。

### 3. 端到端测试（plain assert）— `test-input-e2e`

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

### 4. UI 渲染测试（plain assert）— `test-ui-jsx` / `test-settings`

| 测试 | 内容 |
|------|------|
| `test-ui-jsx` | UI 组件渲染（CandidateItem, MainBar, EmptyBar 等） |
| `test-settings` | 设置面板、键盘导航、焦点切换、属性修改 |

**运行**: `./build/test-ui-jsx` 和 `./build/test-settings`

### 5. TUI 自动化验收测试（agent-browser / tui-debug 驱动，手动）

每次发布前用 agent 驱动 TUI 测试，记录每个步骤的截图和结果。

#### 测试序列

```
# ===== 测试1: 启动与基本功能 =====
# 1.1 启动程序
启动 term-ime
→ 验证: 状态栏显示 [EN]，无崩溃

# 1.2 切换到中文模式
Ctrl+A → Space
→ 验证: 状态栏显示 [中文]

# 1.3 基本输入 + 候选词
输入 "ni"
→ 验证: 显示"拼音: ni"，候选词列表存在
→ 验证: 所有候选词为简体中文（无"裏"、"裡"、"妳"）

# 1.4 选中候选词
按 1
→ 验证: 输出 "你"，状态栏回到 [中文]

# 1.5 空格选词
输入 "ni" → 按 Space
→ 验证: 输出 "你"

# 1.6 数字键选词
输入 "ni" → 按 2
→ 验证: 输出第2个候选词

# 1.7 退格键
输入 "niha" → 按 Backspace
→ 验证: "拼音"变为 "nih"

# 1.8 设置面板
Ctrl+A → S
→ 验证: 显示设置面板，标题"设置"
按 Esc
→ 验证: 面板关闭，恢复正常

# 1.9 退出
Ctrl+A → Ctrl+C 或发信号
→ 验证: 程序正常退出
```

```
# ===== 测试2: 连续输入（5次以上）=====
# 2.1 你好世界科技进步
输入 "ni" → 1 → "hao" → 1 → "shi" → 1 → "jie" → 1 → "keji" → 1
→ 验证: 输出 "你好世界科技"

# 2.2 天气真好开心
输入 "tianqi" → 1 → "zhen" → 1 → "hao" → 1 → "kaixin" → 1
→ 验证: 输出 "天气真好开心"

# 2.3 电脑学习
输入 "diannao" → 1 → "xuexi" → 1
→ 验证: 输出 "电脑学习"
```

```
# ===== 测试3: 部分消费拼音 =====
# 3.1 双词组分段
输入 "nihaoma" → 候选词有"你好吗"、"你好"
按 3 → 选中"你好"
→ 验证: 剩余拼音 "ma" 仍然显示
→ 验证: 候选词为"吗"、"嘛"、"马"、"妈"、"骂"（简体）
按 1 → 选中"吗"
→ 验证: 输出 "你好吗"

# 3.2 三词组分段
输入 "shijie" → 按 1 → 选中"世界"
→ 验证: 输出"世界"

# 3.3 长输入分段
输入 "xuexijinbu" → 按 1 → 选中"学习"
→ 验证: 剩余拼音 "jinbu" 仍然显示
→ 继续选择完成
```

```
# ===== 测试4: 边界条件 =====
# 4.1 无效拼音
输入 "sm" 
→ 验证: 拼音 "s m" 或类似显示，无崩溃

# 4.2 退格到空
输入 "n" → 按 Backspace
→ 验证: 回到 [中文] 无拼音状态

# 4.3 输入大量字母（性能测试）
输入 "asdfghjklzxcvbnm" 
→ 验证: 无卡顿，拼音显示正常

# 4.4 快速输入
输入 "nihaoma" 一次性
→ 验证: 无卡顿，显示正常

# 4.5 长拼音
输入 "zhonghuarenmingongheguo"
→ 验证: 无崩溃，拼音显示正常
```

```
# ===== 测试5: 模式切换 =====
# 5.1 中文→英文→中文
切换到 [中文] → 输入 "ni" → 验证组词正常
切换到 [EN] → 输入 "hello" → 验证直接输出，不组词
切换到 [中文] → 输入 "ni" → 验证组词正常

# 5.2 英文模式下数字
[EN] 模式 → 输入 "123"
→ 验证: 直接输出 "123"

# 5.3 中文模式下数字
[中文] 模式 → 输入 "ni" → 1 → 输出 "你"
→ 输入 "666" → 在组词状态下按数字键 → 验证选中候选词
```

```
# ===== 测试6: 设置面板交互 =====
# 6.1 打开设置
Ctrl+A → S
→ 验证: 面板显示，标题"设置"

# 6.2 键盘导航
按 j → 焦点下移
按 k → 焦点上移

# 6.3 修改设置
选中"界面语言" → 按 l → 切换到 English
→ 验证: value 变化

# 6.4 关闭设置
按 Esc
→ 验证: 面板关闭，主界面恢复正常

# 6.5 设置面板中按 Tab 关闭
打开设置 → 按 Tab
→ 验证: 面板关闭

# 6.6 设置面板时输入
打开设置 → 输入 "ni"
→ 验证: 设置面板不响应 IME 输入
```

```
# ===== 测试7: 多音字 =====
# 7.1 行（xing/hang）
输入 "xing" → 验证候选词有"行"
输入 "hang" → 验证候选词有"行"

# 7.2 长（chang/zhang）
输入 "chang" → 验证候选词有"长"
输入 "zhang" → 验证候选词有"长"

# 7.3 乐（le/yue）
输入 "le" → 验证候选词有"乐"
输入 "yue" → 验证候选词有"乐"
```

```
# ===== 测试8: 词组输入 =====
# 8.1 二字词
输入 "xuexi" → 验证候选词有"学习"
输入 "gongzuo" → 验证候选词有"工作"

# 8.2 三字词
输入 "diannao" → 验证候选词有"电脑"

# 8.3 四字词
输入 "yijian" → 验证候选词有"意见"
输入 "gongsi" → 验证候选词有"公司"

# 8.4 常见短语
输入 "ni hao" → 验证候选词有"你好"
输入 "xie xie" → 验证候选词有"谢谢"
```

```
# ===== 测试9: 特殊按键 =====
# 9.1 Ctrl+A + Ctrl+C 退出
→ 验证: 程序退出

# 9.2 Ctrl+A + 其他键
Ctrl+A → 按 'x'
→ 验证: Ctrl+A + x 被转发到 shell

# 9.3 终端 resize
调整终端大小
→ 验证: 状态栏自适应无错位
```

```
# ===== 测试10: 稳定性测试 =====
# 10.1 连续输入 50 个字符
输入一串随机字母（如 "asdfghjklqwertyuiopzxcvbnm" 重复2次）
→ 验证: 无卡顿，无崩溃

# 10.2 快速切换模式
快速重复 Ctrl+A → Space → 中文 → 英文 → 中文
→ 验证: 切换正常，无状态卡死

# 10.3 输入后立即退出
输入 "ni" → 立即 Ctrl+A → Ctrl+C
→ 验证: 程序正常退出，无崩溃

# 10.4 空输入
输入 ""（直接按 Enter 或其他键）
→ 验证: 无崩溃
```

## 发布清单

发布前必须验证以下所有项：

### 构建检查
- [ ] `make build` 或 `cmake --build build -j$(nproc)` 编译成功，无 warning
- [ ] `file build/term-ime` 显示 "statically linked"
- [ ] `ldd build/term-ime` 显示 "not a dynamic executable"

### 自动化测试
- [ ] `./build/term-ime-tests` — 全部通过（118）
- [ ] `./build/test-input-e2e` — 全部通过
- [ ] `./build/test-ui-jsx` — 全部通过
- [ ] `./build/test-settings` — 全部通过
- [ ] 7 套 python e2e 全通过（见第 2 节，与 CI 跑的是同一批）

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
- [ ] 特殊按键（Ctrl+A 组合）正常
- [ ] 终端 resize 正常
- [ ] 退出正常（无崩溃）

### 发布流程
- [ ] 所有测试通过
- [ ] 代码已提交并推送到 master
- [ ] 创建 Release tag（`git tag -a vX.Y.Z && git push origin vX.Y.Z`）
- [ ] 等待 GitHub Actions 构建完成
- [ ] 验证构建产物（下载 release 二进制，检查静态链接）