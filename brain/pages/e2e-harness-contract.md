---
id: e2e-harness-contract
title: "python PTY 端到端脚本契约（tests/*_e2e.py）"
category: concept
status: active
tags: [testing, pty, harness]
created: "2026-09-15T03:17:45"
updated: "2026-09-29T09:59:18"
---

<!-- compiled_truth -->
## 约束（违反即脆性测试）

1. **hermetic**：每次 `mkdtemp` 后设 `HOME` / `XDG_CONFIG_HOME` / `XDG_DATA_HOME` / `TERM=xterm-256color`，结束清理；断言失败必须反映到 exit code。
2. **固定 SHELL**：必须显式 `os.environ["SHELL"] = "/bin/sh"`。继承调用者 `$SHELL` 会让断言随开发者环境飘——全新 HOME 下 zsh 会进 `zsh-newuser-install` 向导，永不打印提示符。
3. **就绪门**：断言前轮询目标标记（有界重试）；启动窗口内按键会被丢弃（见 `startup-readiness-window`）。
4. **探针式断言**：不要用提示符/cwd 文本判断 shell 是否可用；用算术探针 `echo SHELLVIEW$((6*7))` → 期望 `SHELLVIEW42`（输入行与求值行不同，只有活着的 shell 才会打印结果）。
5. **读帧收敛**：一帧由多段 `write()` 组成，读侧要等到静默再解析；帧中途采样会得到半帧（不是应用缺陷）。
6. 关闭面板的验证 = 「面板专有标记消失」+「探针可见」，两者都要。


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
