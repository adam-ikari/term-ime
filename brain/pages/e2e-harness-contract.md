---
id: e2e-harness-contract
title: "python PTY 端到端脚本契约（tests/*_e2e.py）"
category: concept
status: active
tags: [testing, pty, harness]
created: "2026-09-15T03:17:45"
updated: "2026-10-01T05:42:45"
---

<!-- compiled_truth -->
## 约束（违反即脆性测试）

1. **hermetic**：每次 `mkdtemp` 后设 `HOME` / `XDG_CONFIG_HOME` / `XDG_DATA_HOME` / `TERM=xterm-256color`，结束清理；断言失败必须反映到 exit code。
2. **固定 SHELL**：必须显式 `os.environ["SHELL"] = "/bin/sh"`。继承调用者 `$SHELL` 会让断言随开发者环境飘——全新 HOME 下 zsh 会进 `zsh-newuser-install` 向导，永不打印提示符。
3. **就绪门**：断言前轮询目标标记（有界重试）；启动窗口内按键会被丢弃（见 `startup-readiness-window`）。
4. **探针式断言**：不要用提示符/cwd 文本判断 shell 是否可用；用算术探针 `echo SHELLVIEW$((6*7))` → 期望 `SHELLVIEW42`（输入行与求值行不同，只有活着的 shell 才会打印结果）。
5. **读帧收敛**：一帧由多段 `write()` 组成，读侧要等到静默再解析；帧中途采样会得到半帧（不是应用缺陷）。
6. 关闭面板的验证 = 「面板专有标记消失」+「探针可见」，两者都要。

## 有界等待不是修复手段（2026-09-30）

断言卡在有界等待上时，**先确认那个操作到底是不是异步的**，再谈窗口大小。

反例（本项目真实吃过）：`test_fuzzy_pinyin` 的「生成 prism 是否部署」曾从 90s 一路
加到 240s、600s，连续 8 个 run 红。实际是 `deploy_schema` **同步**执行、早已失败返回，
240s 全是白等——把一次确定性失败伪装成了 flaky。

配套判据：

- **同一断言在多个 run 里以完全相同的方式失败 → 那是 bug 的形状，不是竞态的形状。**
  优先按确定性失败查，别挂 flaky 标签放过去。
- **「本机绿 / runner 红」先查工具链差异**：文件缓冲与落盘阈值、libstdc++/glibc 版本、
  locale、路径长度。CI 日志里时间戳的形状（等满整窗口 vs 立刻返回）是关键线索。
- **失败路径必须留下证据**：被测程序的失败分支若不留日志/返回值，测试就只能靠等。
  本项目构建关掉了 librime 自身日志（`ENABLE_LOGGING=OFF`），所以测试侧要在失败时
  dump 应用日志与相关目录，否则下一轮仍然只能猜。

## 对「长期 flaky」的处理

不要因为「不阻塞发布」就长期容忍。master 长期红会掩盖真实回归，也让人对 CI 失去信任。
该修就修；修不动就把已知失败的 run 标成 flaky 并让它可见，而不是让主线一直红。


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
