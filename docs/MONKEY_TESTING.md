# term-ime Monkey Test 报告

本文件记录用 **term-debug-mcp**(`mcp__tui-debug__*` 工具)驱动 term-ime 做 monkey 测试的方法与发现。测试通过 `tui_start` 启动真实 term-ime 进程,用 `tui_key`/`tui_type` 发送按键,用 `tui_grid`(结构化 24×80 字符网格)、`tui_alive`(进程存活/退出码)、`tui_cursor` 做断言。

序列模型与重放在 `tests/monkey_sequences.py`(Action 数据类、加权生成器、P1–P6 定向探测序列、JSONL 日志/replay)。

## 运行方式

1. `cmake --build build -j$(nproc)` 构建 `build/term-ime`。
2. Claude 按"定向探测"逐个执行,或用 `tests/monkey_sequences.py` 的 `weighted_action` 做随机游走。
3. 失败序列可重放:`tui_start` → 按 JSONL 的 `bytes_hex` 逐条 `tui_type`。

## 不变量

- **INV-1 无崩溃**:`tui_alive` → `alive==true && exit_code is null`。
- **INV-2 状态栏可见**(设置面板关闭时):`tui_grid` 末行恰含 `【EN】` 或 `【中文】` 之一。
- **INV-3 可复位**:发 `reset_sequence()`(lone Esc → Ctrl+A+Space 至 EN → Backspace×5 → Esc)后,末行==`【EN】` 且无候选栏。
- **INV-4 设置面板可达且能干净关闭**:Ctrl+A+s 开 → Tab 关(末行重现模式指示)。
- **INV-5 英文模式透传不丢输入**:EN 模式输 `echo hi`+Enter,网格应含 `echo hi`。
- **INV-6 响应不迟滞**:每步 `tui_wait_for(token, timeout=2)`;>1.5s 或超时记 LAG。

---

## 发现(体验不好的部分)

按严重度排序。每条含复现步骤、观测、根因、源码位置。**已修复的标注 ✅。**

### F1 [严重·状态不一致] 切换中英文模式不取消进行中的输入,后续字母被静默吞掉 ✅ 已修复

**复现**(P2):`Ctrl+A+Space`(切中文)→ 输 `ni`(出现候选栏 `拼音: ni [1.你]...`)→ `Ctrl+A+Space`(切英文)→ 输 `hello`。

**观测**(`tui_grid` 末行):
```
 【EN】  拼音: ni [1.你] 2.拟 3.尼 4.泥 5.呢  Esc 取消
```
状态栏已是 `【EN】`,但候选栏 `拼音: ni` **残留**;输入的 `hello` 5 个字母**全部丢失**——既未进 IME(英文模式 rime `input()` 拒绝),也未透传到 shell(composing 拦截分支 `continue` 吞掉)。用户在英文模式打字毫无响应,易误以为程序卡死。Backspace 可恢复(清掉残留 composition)。

**根因**:`App::on_keyboard_data` 的 Ctrl+A+Space 分支直接调 `ime_->toggle_mode()`,**不像 Ctrl+A+A / Ctrl+A+S 分支那样先 `ime_->cancel()`**。模式切到英文但 rime session 仍处 composing,后续字母走 composing 拦截被吞。

**源码**:`src/core/app.cpp:239-244`(未 cancel)对比 `257-259`/`265-267`(A/S 路径会 cancel);`src/ime/rime_engine.cpp:121-123`(`toggle_mode` 不清 composition)。

**修复方向**:Ctrl+A+Space 分支在 `toggle_mode()` 前加 `if (ime_->state() != Inactive) ime_->cancel();`。

---

### F2 [严重·退出挂死] 子 shell 忽略 SIGTERM 时 term-ime 析构无限阻塞 ✅ 已修复

**复现**(P6):用 config 指定 `shell` 为一个 `trap -- '' TERM; exec bash` 包装脚本(`/tmp/term-ime-monkey-shell.sh`)启动 term-ime → `tui_stop`。

**观测**:`tui_stop` 后 term-ime pid **10 秒后仍存活**(后台计时器 `gone=False after 10.02s`),最终被 SIGKILL 收尸,`exit_code=137`。对照基线:普通 zsh(响应 SIGTERM)时 `tui_stop` 秒回 `exit_code=-15`。

**根因**:`Pty::~Pty()` 发 SIGTERM 给子进程后 `waitpid(pid_, 0, 0)` **无超时阻塞**;子进程忽略 SIGTERM 则永久挂起,term-ime 卡在析构。

**源码**:`src/terminal/pty.cpp:11-19`。

**修复方向**:`waitpid` 改为带超时轮询(`WNOHANG` + sleep),超时后升级 SIGKILL;或直接 `kill(pid, SIGKILL)` 后 `waitpid`。

---

### F3 [重要·交互不一致] 设置面板按 ESC 不关闭,需按两次;与 UI 提示不符 ✅ 已修复

**复现**(P3):`Ctrl+A+s` 开设置 → 按一次 `ESC` → `tui_grid` 仍显示完整设置面板(标题 `设置`、`关闭` 按钮等)→ 再按一次 `ESC` → 面板才关闭(末行重现 `【中文】`)。对照:`Tab` 一次即关。

**观测**(第一次 ESC 后的 `tui_grid`,第 3–19 行仍完整显示面板):
```
  3|  ╭──────────────────────────────────╮
  5|  │  设置                            │
  7|  │  界面语言: [简体中文] < 2/4      │
 ...
 16|  │  取消: Esc/Tab                   │   ← UI 承诺 Esc 可取消
 18|  │  关闭                            │
 19|  ╰──────────────────────────────────╯
```

**根因**:设置面板可见时,`on_keyboard_data` 走设置分支,每个字节经 `InputProcessor`。lone ESC 被 SML 吞进 `Escape` 态(`start_escape` 不设 `forward`),故 `settings_handle_key` **收不到 0x1b**,设置不关。第二次 ESC 到来时 SM 已在 `Escape` 态,触发 `forward_escape` 把 `{0x1b, 0x1b}` forward 出去,`settings_handle_key` 才收到并关闭。UI 提示 `取消: Esc/Tab` 给人"按一次 ESC"的预期,实际需两次且中间有不可见状态。

**源码**:`src/core/app.cpp:205-230`(设置分支只看 `forward`/`data`,不处理 lone ESC);`src/core/input_processor.hpp:98`(`Normal + ESC → Escape`,无 forward);`src/ui/settings.cpp:173-178`(ESC/Tab/Enter 关闭)。

**修复方向**:设置分支显式处理 lone ESC——当 `process(byte)` 返回 `forward==false` 且 `byte==0x1b` 时直接调 `settings_handle_key(state, 0x1b)`;或给 `InputProcessor` 一个"lone ESC 超时"机制。

---

### F4 [重要·渲染效率] EscapeCSI 态每收到一个非终结字节就完整重绘一次状态栏 ✅ 已修复(buffer cap + scroll region + 状态栏签名去重)

**复现**(P1):`ESC` + `[` + 200 字节 `1234...`(数字)。

**观测**:每个数字字节后 term-ime 都输出一次完整状态栏重绘序列 `\x1b[s\x1b[24;1H\x1b[2K...【EN】...`(200 字节 = 200 次重绘),且末尾 `90` 被拆成 `9`/`0` 并混入控制序列,出现 `\x1b[u90`、`\x1b[u 90` 等残破拼接。进程未崩溃(INV-1 通过),但渲染开销随 buffer 长度线性放大;若喂 5000 字节会造成明显卡顿。

**根因**:SM 在 `EscapeCSI` 态对每个非终结字节执行 `buffer_byte`(`buffer_` 无上限),但渲染层似乎在每个字节事件后都触发一次完整状态栏重绘,且 buffer 拼接的边界处理在长序列下出错。

**源码**:`src/core/input_processor.hpp:107`(`buffer_byte` 无 cap);渲染路径 `src/ui/renderer.cpp`。

**修复方向**:EscapeCSI 态不触发重绘(无可见状态变化);`buffer_` 加容量上限防 OOM。

---

### F5 [重要·AI 排序] 排序回调用过期候选覆盖当前候选栏,导致候选"回退" ✅ 已修复

**复现**(P4):启用 AI 排序(Ctrl+A+A,模型已加载)→ 中文模式快速输 `shijie`。

**观测**(`tui_grid` 末行,输入到 `shi jie` 后):
```
 【中文】  拼音: shi jie [1.是] 2.说 3.上 4.时 5.谁 [AI]
```
候选词变成了 `是/说/上`(这是输入 `s` 时的候选!),而非 `shijie` 对应的 `世界/师姐...`。AI 排序是异步的,`on_ranking_complete` 在 worker 线程触发时,用户已输入到 `shijie`,但回调仍用**旧的** `last_candidates_`(对应 `s`)覆盖候选栏,导致候选回退到更早的状态。

**根因**:`LlamaRanker::rank_async` 的回调 `App::on_ranking_complete` 直接用 ranking 结果重绘候选,没有校验"排序发起时的候选集"是否仍是当前候选集;快速连续输入下旧排序结果会覆盖新候选。

**源码**:`src/core/app.cpp:179-198`(`on_ranking_complete` 跨线程重绘,无版本/快照校验);`src/ime/llama_ranker.cpp:78-93`(`rank_async` 队列)。

**修复方向**:排序任务携带候选集的版本号/快照 id;回调时校验版本,过期则丢弃。

---

### F6 [重要·AI 排序] 首次候选词触发模型懒加载,首次输入卡顿数秒 ✅ 已修复(worker 线程启动即预加载)

**复现**(P4):启用 AI 排序后,中文模式首次输 `nihao`。

**观测**:候选栏显示 `[⠙ AI...]` 旋转指示器,但实际在进行 330MB 模型加载(`llama_model_loader` / `load_tensors` / `repack` 日志持续数秒)。期间候选词虽已由 librime 给出,但 AI 排序承诺的"智能排序"延迟数秒才生效;用户首次输入体验明显卡顿。

**根因**:`LlamaRanker::load_model` 懒加载发生在首次 `rank_async` → `do_rank` 调用时,加载 330MB GGUF + repack 在 worker 线程同步进行,首个候选的排序结果要等加载完成。

**源码**:`src/ime/llama_ranker.cpp:99-140`(`load_model` 懒加载)、`183-185`(首次 do_rank 触发)。

**修复方向**:启用 AI 排序时即异步预加载模型(不等到首次候选);或首次加载时在 UI 明确提示"正在加载模型"。

---

### F7 [次要·流程矛盾] 下载模型后因 config.enabled=false 无法初始化,用户白等下载 ✅ 已修复

**复现**:config `llama_ranker.enabled=false` 时按 Ctrl+A+A → 触发模型下载(~336MB)→ 下载完成后 `toggle_ai_ranking` 调 `initialize(config_.llama_ranker)` → 因 `enabled=false` 返回 false → `Failed to initialize LLM ranker with downloaded model`(见日志)→ 需再按一次 Ctrl+A+A 才用已下载模型初始化。

**观测**(日志):
```
Download progress: 100% - Complete
Model downloaded successfully: ...
Llama ranker disabled in config
Failed to initialize LLM ranker with downloaded model
```

**根因**:下载完成回调仍用原 `config_.llama_ranker`(enabled=false)调 `initialize`,而 `LlamaRanker::initialize` 第一步就 `if (!config_.enabled) return false`。下载流程不修改 config 的 enabled,导致下载与初始化矛盾。

**源码**:`src/core/app.cpp:470-481`(下载完成回调);`src/ime/llama_ranker.cpp:19-24`(enabled gate)。

**修复方向**:下载完成回调里把 `config_.llama_ranker.enabled=true` 再调 `initialize`;或 `toggle_ai_ranking` 下载前就置 enabled=true。

---

### F8 [已修复] 重复排序内存泄漏(`new[]`/`delete[]` 不匹配)

**复现**(P5):模型加载完成后,稳态下连续多轮拼音+选词触发 ranking,采样 `/proc/<pid>/status` VmRSS 20s。

**观测**:RSS 恒定 `610252 kB`,`delta=0 kB`(40 个样本)。**未发现泄漏**。

**根因分析**:`do_rank` 的 `new[]`/`delete[]` 不匹配(`llama_ranker.cpp:285-316` 的若干 `break` 跳过 `delete[]`)在 Qwen2-0.5B + 当前推理路径下未命中,或泄漏量过小被分配器复用掩盖。需更长时间/更高频或 sanitizer 才能确认。标记为未确认。

---

### F10 [重要·显示污染] llama.cpp stderr 日志覆盖终端屏幕 ✅ 已修复

**复现**:启用 AI 排序(config `llama_ranker.enabled=true`)启动 term-ime。

**观测**:启动屏幕被 llama.cpp 的 stderr 日志覆盖——`register_backend`、`llama_model_loader: ...`、`load_tensors: layer N assigned to device CPU`、`repack: repack tensor blk.N....` 等数百行铺满 24 行,状态栏和 shell 提示符被淹没。F6 的后台预加载把日志提前到启动期,使污染更明显。

**根因**:`LlamaRanker::load_model` 调 `llama_backend_init()` 后,llama.cpp/ggml 默认把 INFO 级日志写到 stderr,term-ime 未重定向,直接落到 TTY。

**源码**:`src/ime/llama_ranker.cpp:96-103`(`load_model` 未设日志回调)。

**修复**:`load_model` 首次调用时通过 `llama_log_set` 注册回调,把 llama 日志转发 spdlog(写文件日志);ERROR/WARN 级才记录,INFO/DEBUG 静默丢弃,TTY 保持干净。

---

### F9 [确认·无崩溃] 随机游走 5 个 seed 共 ~300 步均无崩溃

**复现**:用 `tests/monkey_sequences.py` 的 `weighted_action`(加权分布:字母/数字/Space/Backspace/Enter/Tab/ESC/方向键/Ctrl+A 组合/畸形 CSI/Resize/Wait)生成 seed=42,7,123,2024,99 各 40–60 步序列,写 JSONL 后用 `run_replay_to_pty` 逐字节喂入真实 term-ime PTY(含 0x01 控制字节)。

**观测**:5 个 seed 全部 `exit=0`(进程未中途退出)。INV-1(无崩溃)在概率性输入下成立。

**结论**:已知的 F1–F7 均为**确定性**的状态/体验问题(需特定半引导序列触发),纯随机输入未触发崩溃或数据竞争崩溃。monkey test 的价值在于:随机游走守住"无崩溃"底线,定向探测(P1–P6)暴露体验问题。

---

## 优先级建议

1. ~~**先修 F1、F2**~~(已修复:切换取消 composition、Pty 析构超时+SIGKILL)。
2. ~~**再修 F3、F5、F6**~~(已修复:设置 ESC 一次关闭、AI 排序版本去重、模型预加载)。
3. ~~**F4、F7**~~(已修复:buffer cap + scroll region + 状态栏签名去重、下载后置 enabled)。**F8** 未确认(稳态 RSS 无增长)。
