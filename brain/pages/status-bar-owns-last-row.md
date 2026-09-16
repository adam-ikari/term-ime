---
id: status-bar-owns-last-row
title: "状态栏占用终端最后一行：可用区 = rows-1，resize 后必须清 Renderer::last_bar_sig_"
category: decision
status: active
tags: [terminal, layout, resize, renderer]
created: "2026-09-14T15:18:16"
updated: "2026-09-14T15:18:16"
---

<!-- compiled_truth -->
状态栏（Status bar）固定渲染在终端**最后一行**，属于 UI 保留区，不属于 shell/PTY 的绘制区。

## 规则

1. 可用行数 = `rows - 1`。`Pty`（winsize 下发）与 `Screen`（网格分配）**都**按 `rows - 1`，两处必须一致，否则光标行归属错位、内容被状态栏覆盖。
2. `Renderer` 用 `last_bar_sig_` 做状态栏重绘去重。任何尺寸变化（`SIGWINCH` / resize 路径）之后**必须清 `last_bar_sig_`**，否则新宽度下签名未变，状态栏保持旧宽度不再重绘。
3. PTY 的 winsize 必须取真实终端尺寸（`ioctl(TIOCGWINSZ)` 结果），不要用硬编码或上一次缓存的尺寸；尺寸变化后同步给 PTY，shell 才知道真实可用区。

## 为什么难自己读出来

三条规则分散在 `src/terminal/pty.cpp`、`src/terminal/screen.cpp`、`src/ui/renderer.cpp`，任一处的 `rows - 1` 漏改或 `last_bar_sig_` 漏清都只在 resize 场景下才暴露，静态读代码不会提示。

## 影响面

`src/terminal/pty.cpp`、`src/terminal/screen.cpp`、`src/ui/renderer.cpp`、`src/core/app.cpp`（尺寸同步）。相关：[[parser-stream-state-contract]]。


## Timeline

- time: 2026-09-14T15:18:16
  kind: decision
  summary: "Created this page: 状态栏占用终端最后一行：可用区 = rows-1，resize 后必须清 Renderer::last_bar_sig_"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [status-bar-owns-last-row]

- time: 2026-09-14T15:18:16
  kind: decision
  summary: "状态栏固定占最后一行 → shell 可用区为 rows-1，Pty 与 Screen 都按 rows-1；任何 resize 后必须清 Renderer::last_bar_sig_，否则状态栏不会按新宽度重绘"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [status-bar-owns-last-row]
