---
id: e2e-harness-contract
title: "python PTY 端到端脚本契约（tests/*_e2e.py）"
category: concept
status: active
tags: [testing, pty, harness]
created: "2026-09-15T03:17:45"
updated: "2026-09-15T03:17:45"
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
