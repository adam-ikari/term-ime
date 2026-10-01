---
id: e2e-harness-contract
title: "python PTY 端到端脚本契约（tests/*_e2e.py）"
category: concept
status: active
tags: [testing, pty, harness]
created: "2026-09-15T03:17:45"
updated: "2026-10-01T13:58:59"
---

<!-- compiled_truth -->
## 组合态下的转义序列被拆散（同一个吞键家族，第二次）

顺着「未被消费的按键」继续查，找到第二个更严重的：**转义序列在组合态下被逐字节
拆散喂给 IME**。

`on_keyboard_data` 是逐字节循环。状态机在收到前导 ESC 时还没组成任何序列
（`forward == false`），于是组合态分支把 ESC 当「未知键」丢掉，接着 `[`、`3`、
`~` 被**逐个**交给 IME：`[` 是标点 → rime 提交 `〔`；`3` 走进选词分支；`~` 再一次
标点。结果 Delete（`ESC [ 3 ~`）不但把用户正在打的拼音替他提交了，还往 shell 里
打进一个字面量 `[3~`：

    修复前  /bin/sh: 1: ABCDEF你〔[3~: not found
    修复后  /bin/sh: 1: ABCDEF[3~: not found

日志现场是 `IME composing: ignoring key 0x1b` —— 只丢了 ESC，尾巴照单全收。

修法：序列必须当**一个整体**处理。组合态下 `input_processor_.in_escape()` 为真
时直接吞掉该字节（IME 不能看到序列的任何一部分），等序列完成后再决定：方向键 /
PageUp/PageDown 翻页（保持原样），其余（Home/End/Delete/Insert/F 键）原样转发
给 shell。

Esc 取消组合的逻辑在 `on_keyboard_data` **批次末尾**用 `in_escape()` 判断，所以吞
中间字节不影响它：孤立 ESC 时批次末尾仍在 Escape 态 → 照常取消组合。

方向键翻页此前**零覆盖**，而这次动的正是那条路径，所以同一个用例里补了
Down/Up 翻页断言（并且它确实抓到了问题：修复前 Down 把候选翻成了 `「 【 〔`）。

## 又一次空转断言——第四次，这次栽在「猜字形」上

我写 `check("sequence not torn apart", "「" not in out)`，在**未修复**的二进制上
**通过**了。原因：rime 把 `[` 映射成的是 `〔`（乌龟壳括号）不是 `「`，我凭印象
写了个对不上的字形。

改成断言**性质**而不是某个字形：取 shell 报的 `not found` 那一行，检查里面
**没有任何 CJK 字符**。撕裂版会带出 `你`（拼音被提交）和 `〔`，整体版没有。这样
就不依赖具体映射表。

顺带记一条：`bar()` 返回的是**字符串列表**（`re.findall` 两个组时列表元素是
tuple，但函数返回的是 `m[1]`），不是 (序号, 文本) 的二元组。我按二元组写就崩了。
另外断言「两次抓帧的候选列表完全相等」太脆（重绘中途截断会导致长度不同），
应该断言**组合态存活**（首个候选不变）。

修好后 3 条断言在未修复二进制上 FAIL，修复后 31/31 PASS，arm64 上同样 31/31。


## Timeline

- time: 2026-09-15T03:17:45
  kind: decision
  summary: "Created this page: python PTY 端到端脚本契约（tests/*_e2e.py）"
  source: "2026-09-15 e2e 脚本修复"
  affects: [e2e-harness-contract]

- time: 2026-09-15T03:17:45
  kind: decision
  summary: "python PTY 端到端脚本必须：hermetic 环境 + 固定 SHELL=/bin/sh + 就绪门 + 探针式断言（不得匹配提示符文本）"
  source: "2026-09-15 e2e 脚本修复"
  affects: [e2e-harness-contract]

- time: 2026-09-29T00:41:58
  kind: decision
  summary: "脚本不得写死开发机绝对路径：二进制路径与工作目录要由 __file__ 推仓库根（或统一用相对仓库根的 ./build/term-ime）。tests/test_settings_panel_e2e.py 里的 /home/gem/project/term-ime 字面量让 ci.yml 的 e2e job 从接入那天起每次都在第一步 \"ERROR: ... not found\" 退出，master 上连续 4 次红与代码无关 —— 这类缺陷只在 runner 上暴露，本机永远跑不出。"
  source: "2026-09-29 v1.1.3 发布"
  affects: [e2e-harness-contract]

- time: 2026-09-29T03:21:07
  kind: note
  summary: "设置面板的开/关指纹字串从 'Up/Down' 换成 'Esc/Tab'（三行按键提示并成一行后 Up/Down 不再出现）。断言面板是否还开着要靠这个字面量，改提示文案必须同步改 panel_gone。"
  source: "设置面板描述改造会话"
  affects: [e2e-harness-contract]

- time: 2026-09-29T09:59:18
  kind: decision
  summary: "test_fuzzy_pinyin 的 per-combination prism 部署在 CI 上长期失败（词库拆仓前 run 0e222cb 同样 10/13，非回归）：n_l-only/zh_z-only/subset 三个组合 240s 内等不到 prism.bin。90s→240s 未能修复，600s 反而撞 job 超时被 cancel（已回退 240s）。这三条是 pre-existing flaky，不阻塞发布；根因（CI 2 核编译 7 万词条词典的耗时 vs deploy 真不完成）待单独排查。"
  source: "2026-09-29 CI flaky 排查"
  affects: [e2e-harness-contract]

- time: 2026-09-30T03:34:17
  kind: decision
  summary: "e2e 断言卡在一个有界等待上时，等待窗口不是修复手段：先确认那个操作是不是异步的。librime deploy_schema 是同步的，240s 全是白等，把「编译慢」当根因会把一次静默失败伪装成 flaky（连续 8 个 run 因此长期红）。另外：应用自身的失败路径必须在失败时留下证据（返回值 + 目录/日志 dump），因为第三方库（librime）的日志在本项目构建里是关掉的。"
  source: "2026-09-30 CI flaky 排查"
  affects: [e2e-harness-contract]

- time: 2026-09-30T03:52:05
  kind: reversal
  summary: "撤回「这些 3 个组合是 pre-existing flaky、不阻塞发布」的结论：它不是 flaky，是确定性失败（同一批组合每次都红）。判据是同一断言在多个 run 里以完全相同的方式失败 —— 那是 bug 的形状，不是竞态的形状。下次再看到「长期 flaky」先怀疑确定性失败，并优先找「本地绿/runner 红」的工具链差异（缓冲阈值、文件落盘、locale），而不是加超时。"
  source: "2026-09-30 CI flaky 收尾"
  affects: [e2e-harness-contract]

- time: 2026-09-30T03:54:06
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-09-30T05:12:10
  kind: reversal
  summary: "撤回「这些都是 pre-existing flaky」式的跳过倾向的另一个实例：连续 8 个 run 红时，标签是「已知 flaky 不阻塞发布」，实际是确定性失败。判据同前——同一断言以完全相同方式重复失败即非竞态。CI 里给未验证的东西建 job 时，要带负例自检（拿已知不合格的输入跑同一段校验，必须失败），否则断言可能是空转的。"
  source: "2026-09-30 Termux 交叉编译"
  affects: [e2e-harness-contract]

- time: 2026-10-01T05:42:45
  kind: decision
  summary: "「自动发现」听起来比「写死常量」优雅，但当它依赖有配额上限的 API 时可靠性更差。install.sh 曾用 GitHub API 列出 release 再逐个探测 termux 资产：每次安装 1 次 API + 最多 30 次 HEAD，而未认证限额仅 60 次/小时/IP，几个人同时装就耗尽，之后所有手机安装以裸 403 失败。CI 绿灯是虚假信心（额度还没用完），直到真的把 rate_limit 打到 0/60 才暴露。改用已知 tag 常量，零请求。教训：CI 绿灯不等于依赖外部配额的服务可靠；验证方式应选「把依赖整个屏蔽掉仍能工作」而不是「看它成功了」——后者区分不了『真的没依赖』和『额度还够』。同类：自动跳过静默失败（本项目 opencc 用 set() 遮蔽 -D 变量、std::ofstream 未 close 导致 librime 读到空文件）都是被外部环境差异暴露的。"
  source: "2026-09-30/10-01 Termux 安装链路"
  affects: [e2e-harness-contract]

- time: 2026-10-01T10:47:01
  kind: decision
  summary: "写「连续三段拼音各自 Enter 提交」的测试时又写出一版空转断言，暴露两个新形态的坑。(1) **修复合并后 git checkout 不是回退**：Enter 修复早已 commit，用 git checkout src/core/app.cpp 拿到的是修复版，我据此以为「未修复版也通过」并差点得出错误结论；验证旧行为必须显式取父版本 。(2) **drain() 用局部 total 累加**：多次调用只保留最后一次的内容，于是只看到最后一段的提交，前两段的证据被丢掉，看起来像「只提交了一次」。多段测试必须用全局累加长度。修正后的判据：每段 Enter 提交后再补一次 Enter 把行首执行出来，正确版得到 3 条独立 not found（你好/世界/中国 各一条），坏版本得到 0 条 —— 明确可区分。另外发现一种隐蔽的空转：hits 比 segments 少时 zip 循环体根本不执行，逐段断言会全部真空通过；改用按索引取值并对缺失项显式失败。已把多段用例加进 test_fuzzy_pinyin.py，并用 30c0e2f^ 核实 4 条断言全部 FAIL。"
  source: "2026-10-01 多段提交测试：第三次踩空转断言"
  affects: [e2e-harness-contract]

- time: 2026-10-01T10:47:12
  kind: decision
  summary: "上一条 timeline 里的 'git show 30c0e2f^:src/core/app.cpp' 因为在 shell 里未加引号，尖括号被当成重定向，命令名丢失。完整写法：git show '30c0e2f^:src/core/app.cpp' > app_prefix.cpp（路径含冒号，必须整体加引号）。"
  source: "补记：上一条里命令名被 shell 吃掉了"
  affects: [e2e-harness-contract]

- time: 2026-10-01T12:04:21
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-01T13:58:59
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]
