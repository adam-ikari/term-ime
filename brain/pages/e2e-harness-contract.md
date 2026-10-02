---
id: e2e-harness-contract
title: "python PTY 端到端脚本契约（tests/*_e2e.py）"
category: concept
status: active
tags: [testing, pty, harness]
created: "2026-09-15T03:17:45"
updated: "2026-10-02T02:02:12"
---

<!-- compiled_truth -->
## 仓库里早就有 fuzz 模型，只是没人驱动它（2026-10-02）

`tests/monkey_sequences.py` 一直在建模动作空间 —— 字母、数字、Ctrl+A 组合、
**畸形 CSI**、方向键、**resize**、wait，外加四个定向探针
（`_p1_escapecsi` / `_p2_toggle_mid_composition` / `_p3_settings_esc` /
`_p6_exit_hang`）。但**没有任何东西驱动它**：只通过 MCP 工具交互式用过，
而且 `grep monkey_sequences ci.yml` 为空 —— 不在 CI 里，等于只活在文档中。

新增 `tests/fuzz_drive.py`：import 那个模型（不重新实现），检查不变量是
**进程必须活着**。

### 这个驱动抓不到什么 —— 必须写清楚，否则会被当成「跑过了所以没问题」

- **不检查输出对错**。存活不代表字节到达了 shell；行为不变量在具名 e2e 里。
- **不检查 hang**。卡死但没退出的进程读作 ok（用 `sleep 600` 替身验证过，确实 ok）。
- 每轮结束就 kill，所以关机崩溃、空闲后首个按键崩溃看不见。

不进 CI：一轮约 6s librime 部署 + 每步 20ms，属手动工具。

### 又一次空转断言（第七次），而且是我自己当场看出来的

先只跑 2 轮就看到「8/8 clean」。原因是报告里一旦有 finding 就把
`PROBES` 清空，后续轮次**回落到随机序列**，于是「定向探针通过」变成了
「随机序列通过」。断言空转。删掉重写。

**这已经是我在同一轮工作里第二次因为「看到全绿就信」而差点交出空转断言。**
判断标准只能是：**这个观测量在有 bug 和无 bug 时是否真的不同**，
而不是「跑出来是什么结果」。

### 变异验证（存活检查必须先证明自己抓得住）

- `BIN` 指向 `exit 3` → 报 `exited at step 0 (code 3)`
- `BIN` 指向 `kill -SEGV` → 报 `exited at step 0 (signal 11)`

跑过量：45 轮随机（约 5000 动作，en/zh 交替）+ 8 轮定向探针，全部干净。
**结论是「没找到崩溃」，不是「没有问题」** —— 覆盖面受上面三条局限约束。


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

- time: 2026-10-02T00:13:25
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]

- time: 2026-10-02T02:02:12
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [e2e-harness-contract]
