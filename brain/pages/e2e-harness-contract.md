---
id: e2e-harness-contract
title: "python PTY 端到端脚本契约（tests/*_e2e.py）"
category: concept
status: active
tags: [testing, pty, harness]
created: "2026-09-15T03:17:45"
updated: "2026-10-01T17:00:35"
---

<!-- compiled_truth -->
## 极简化：把「隐式」的东西显式化，而不是把代码写短（2026-10-01）

按极简哲学（减少维护负担、潜在问题、运行开销、攻击面）复查了 IME 按键分发，
做了三件事。**每件都先量后改，不预设收益。**

### 1. 按键类别的不变量测试（护栏，必须先做）

我已被空转断言坑过 4 次，所以任何重构之前先建能真的抓到 bug 的网。
`run_key_exhaustion_case` 按**按键类别**而非逐个键穷举。

写第一版时我把前提搞错了，值得记：我断言「组合态下打 7 应该原样到 shell」，
在**正确的**构建上失败。候选栏亮着时 7 的含义是「选第 7 个候选」，任何输入法
都是这个语义 —— 我的断言在要求一个错误行为。数字只有在 IME 无候选可选时才是
「不被认领」，而 `[` 正是制造这个状态的键。

修正后三类各自钉住前提：有组合但无候选槽位 / 无组合态 / 大写字母永不是拼音。
双向核实（修复前 29/34 FAIL，修复后 34/34 PASS）。

### 2. 删掉 send_to_shell，写出只留一条路径

原来两条：`send_to_shell`（先 flush 再立即 write）与 `queue_for_shell`（追加到
批次）。两条都保序 —— **保序是追加本身提供的，不是立即写提供的**，那条注释
（"否则提交会越过它之前的粘贴"）把功劳记错了地方。立即写只是多一次 write
系统调用。

两条路径的真实代价：每个调用点都要知道自己该用哪条，选错的后果是**静默的**
（字节流乱序，不是崩溃）。组合态里两条同时在用。

删掉后：唯一在键盘批次外写字节的是孤立 ESC 定时器，改成 queue + flush。
顺序保真实测（PTY 探针，改前/改后对照）：同批「nihao Z」两版都收到「你好Z」；
8KB 粘贴 8280 字节完整 3ms；孤立 ESC 两版都 387 字节。

### 3. 删掉 render_candidates_bar 的 refresh 开关

那个 bool 是一次缓存优化（shell 输出触发的重绘不重查 rime，defect 17）。代价是
正确性依赖调用方保证「IME 没变」—— 而这个保证**已经被打破过一次**：
`advance_candidate_window` 读 `ime_snapshot_` 却靠别人的 `render()` 刷新它。
把它改成显式 `refresh_ime_snapshot()` 之后，那条隐式耦合就没必要了。

量了那个优化当初要救的场景（40 轮 x 512B 高频 shell 回显）：
**改动前 2ms / 改动后 2ms**。查询 rime 是廉价 menu 查表，且 `render_candidates`
本身在状态栏没变时跳过终端写入 —— 那个优化本来就没买到可测收益。

## 三条通用教训

1. **量了再说**：本轮两次「我以为有收益」都被实测否掉（渲染批量化、
   rime 快照缓存）。报告里必须给数字，不能把「结构更干净」说成「更快」。
2. **极简 ≠ 行数少**：这三处的净变化是 +26/−24、+14/−8，行数几乎没变。真正
   减的是**隐式状态**：两个 bool 参数、一个依赖别人副作用的缓存。
3. **重构之前先建网**：先有能抓到 bug 的断言，再动代码。否则「重构后测试全绿」
   毫无信息量 —— 我前面就是这么把空转断言当成安全网用过的。


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

- time: 2026-10-01T17:00:35
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]
