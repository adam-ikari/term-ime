---
id: enter-does-not-commit-composition
title: "Enter 键不提交 IME 组合态：真缺陷，非 Termux 特有"
category: decision
status: active
tags: [ime, keyboard, input, bug]
created: "2026-10-01T06:36:20"
updated: "2026-10-01T07:26:46"
---

<!-- compiled_truth -->
## 现象（2026-10-01 实测发现，Linux 与 Android 同样存在）

中文组合状态下按 **Enter 不会提交候选**，而是被原样转发给 PTY 里的子进程。
这是 term-ime 的真缺陷，与平台无关。

| 方式 | 修复前 | 修复后 |
|---|---|---|
| `nihao` + Enter | Enter **未提交**，组合仍在；裸 `\r` 泄漏给 shell | 「你好」进入 shell 输入行 |
| `nihao` + `1` + Enter | `1` 提交后组合态未清，Enter 二次提交 → shell 执行「你好」→ `not found` | 正常 |

## 根因

`src/core/app.cpp` 的 `on_keyboard_data` 逐字节分发：

- 字母 → `ime_->input()`
- 中文标点 → `is_punct_key()`（其 switch 列表里**没有** `\r`）
- 其余 → `input_result.forward` → `queue_for_shell()`

`\r` 既不是字母、也不在 `is_punct_key` 里，于是落到第三条被直接转发给 shell，
**从未进入 `ime_feed()`**。

## 修法

在分发链里给 `'\r'` / `'\n'` 一个专门分支：仅当 `ime_->state() != Inactive`
（即确有组合）时提交候选 + `ime_->cancel()` 复位 + `render()`；**没有组合时不
拦截**，让 Enter 继续落到转发分支，否则中文模式下就没法执行命令了。

提交语义是「只提交、不执行」——与 fcitx5/ibus + rime 一致：Enter 把候选放到
输入行，用户再按一次 Enter 才执行。

## 为什么长期没被发现

`tests/test_fuzzy_pinyin.py` 等 e2e **只断言候选栏显示**，从不测试「提交到
shell」。整个 e2e 层没有一条断言走 `take_commit()` → `send_to_shell()`，
于是对这个缺陷完全失明。

## e2e 断言怎么写才不是空转的（这里踩了两次）

关键：**不能用「Enter 后读一帧看候选栏还在不在」来断言**。未修复版在 Enter
之后**根本不重绘**（实测 0 字节），旧候选栏就那么留在屏幕上；而新读的一帧
是空的 —— 修复版读到的也是空。两者观测量完全相同，断言通过但抓不到 bug。
我第一版断言就是这样写的，用未修复二进制一跑就发现是空转。

可靠判据是**观察 shell 收到什么**：连按两次 Enter。
- 正确：第一次提交「你好」到输入行（不带 \n），第二次 Enter **执行**它 →
  shell 报 `not found`。
- 坏：第一次是裸 `\r` 什么都没提交，拼音还在 IME 里，第二次 Enter 执行空行 →
  什么都没发生。

所以断言是「第二次 Enter 后输出里出现 `not found`」。已用未修复二进制验证：
**未修复 FAIL、修复 PASS**。

## 教训（第二次了）

写断言时**必须用未修复的版本跑一遍确认它会失败**。这次是我自己批评过的
「CI 绿灯给虚假信心」的重演：断言在两个版本上都 PASS，说明它测的东西和 bug
无关。判据是「这个观测量在修复前后是否真的不同」，而不是「测试跑通了没有」。


## Timeline

- time: 2026-10-01T06:36:20
  kind: decision
  summary: "Created this page: Enter 键不提交 IME 组合态：真缺陷，非 Termux 特有"
  source: created via brain create-page
  affects: [enter-does-not-commit-composition]

- time: 2026-10-01T06:36:33
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [enter-does-not-commit-composition]

- time: 2026-10-01T07:26:46
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [enter-does-not-commit-composition]
