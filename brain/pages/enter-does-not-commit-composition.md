---
id: enter-does-not-commit-composition
title: "Enter 键不提交 IME 组合态：真缺陷，非 Termux 特有"
category: decision
status: active
tags: [ime, keyboard, input, bug]
created: "2026-10-01T06:36:20"
updated: "2026-10-01T06:36:33"
---

<!-- compiled_truth -->
## 现象（2026-10-01 实测发现，Linux 与 Android 同样存在）

中文组合状态下按 **Enter 不会提交候选**，而是被原样转发给 PTY 里的子进程。
这是 term-ime 的真缺陷，与平台无关。

实测三种上词方式（Linux，`nihao` 为例）：

| 方式 | 结果 |
|---|---|
| A. `nihao` + Enter | Enter **未提交**，组合仍在；裸 `\r` 泄漏给 shell |
| B. `nihao` + `1` + Enter | `1` 提交了「你好」，但组合态未清；Enter 二次提交 → shell 把「你好」当命令执行 → `/bin/sh: 1: 你好: not found` |
| C. `nihao` + `1`（不按 Enter） | 正常，「你好」进入 shell 输入行 |

**只有 C 可用**。用户按下数字选词后还得再按一次 ESC 或直接继续输入，
「选词 → 回车」这个最自然的手势是坏的。

## 根因

`src/core/app.cpp` 的 `on_keyboard_data` 逐字节分发：

- 字母：`byte >= 'a' && byte <= 'z'` → `ime_->input()`（624 行附近）
- 中文标点：`is_punct_key(byte)` → 走 librime punctuator（`is_punct_key` 的
  switch 列表里**没有** `\r`/`\n`）
- 其余：`input_result.forward` → `queue_for_shell(...)`

`\r` 既不是字母、也不在 `is_punct_key` 里，于是落到第三条被直接转发给 shell，
**从未进入 `ime_feed()`**，组合态自然不会提交。

数字键选词走 `ime_->select_candidate()`，它提交了文本但没有把 rime 的
session 状态从 Composing 复位，于是后续 Enter 又触发一次提交。

## 为什么一直没被发现

`tests/test_fuzzy_pinyin.py` 等 e2e **只断言候选栏显示**（`s.candidates()`
读状态栏正则），从不测试「提交到 shell」。整个 e2e 层没有一条断言走
`take_commit()` → `send_to_shell()` 这条路径。项目自己的测试因此对这个缺陷
完全失明。

## 修的方向（未实施）

Enter 应当在 `ImeMode::Chinese` 且 rime 处于组合态时**先交给 IME 提交**
（`ime_feed('\r')` 或 `select_candidate` 复位），只有在 IME 不接受时才转发给
shell。同时数字选词后应复位组合态，避免二次提交。

配套要补 e2e 断言：选词后回车，断言 shell 收到的是中文且没有 `not found`。

## 教训

发现这个缺陷的路径值得记住：**为了回答「模拟器里到底跑对没有」，我写了
一个逐帧 harness，结果先撞上自己 harness 的 bug（`drain()` 局部累加、
分帧边界错位），再撞上产品 bug。** 关键动作是拿**同一份 harness 在 Linux 上
跑一遍做对照** —— 行为一致才说明是产品问题，否则就是测试程序的问题。这个
对照不做，会把 harness 的 bug 当成产品 bug 报上去（反过来也一样）。


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
