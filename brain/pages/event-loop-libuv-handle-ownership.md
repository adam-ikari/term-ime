---
id: event-loop-libuv-handle-ownership
title: "EventLoop 的 libuv 句柄所有权：uv_close 时 release，析构先排空 close 回调"
category: decision
status: active
tags: [libuv, event-loop, memory-lifetime]
created: "2026-09-14T15:18:05"
updated: "2026-09-28T19:36:10"
---

<!-- compiled_truth -->
EventLoop 持有的 libuv 句柄（stdin/PTY 的 uv_poll/uv_stream 等）在 `uv_close` 调用时**立即把所有权 release 给 libuv**：close 回调是唯一合法的 `delete` 点。谁 wrap 谁 delete 的直觉在这里是错的。

## 规则

1. `uv_close(handle, cb)` 之前或之后，C++ 侧不得再 touch 该 handle；`cb` 里 `delete` 其 wrapper。
2. `EventLoop` 析构顺序固定：停掉所有 poll → `uv_close` 所有剩余句柄 → `uv_run(loop, UV_RUN_DEFAULT)` 排空队列中的 close 回调 → 才 `uv_loop_close(loop)`。
3. 跳过第 2 步的排空会得到 use-after-free（close 回调在 loop_close 之后才执行）或 `uv_loop_close` 返回 `UV_EBUSY`。

## 反面例子

- 在句柄所属对象析构函数里直接 `delete` handle，再 `uv_close` 于已释放内存 → 双重释放 / 段错误。
- `uv_loop_close` 前不 `uv_run`，close 回调被丢弃。

## 影响面

`src/core/event_loop.{hpp,cpp}`、`src/main.cpp` 的退出路径；也约束 stdin EOF 后事件循环空转的修复（EOF 后必须停止 poll，否则忙等）。


## Timeline

- time: 2026-09-14T15:18:05
  kind: decision
  summary: "Created this page: EventLoop 的 libuv 句柄所有权：uv_close 时 release，析构先排空 close 回调"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [event-loop-libuv-handle-ownership]

- time: 2026-09-14T15:18:10
  kind: decision
  summary: "确认 libuv 句柄所有权在 uv_close 时 release 给 libuv，close 回调负责 delete；EventLoop 析构必须先 uv_run 排空 close 回调再 uv_loop_close"
  source: "2026-09-14 终端/输入法缺陷修复"
  affects: [event-loop-libuv-handle-ownership]

- time: 2026-09-28T14:19:48
  kind: decision
  summary: "watch_fd()/watch_signal() 对同一 fd/signal 的第二次注册直接拒绝并记 error，不覆盖 map 条目：覆盖会丢掉指向 libuv 仍拥有句柄的指针（句柄只能由 uv_close 回调释放），是新句柄尚未初始化、旧句柄永不关闭的 use-after-free 来源。"
  source: "2026-09-28 代码评审修复轮"
  affects: [event-loop-libuv-handle-ownership]

- time: 2026-09-28T19:36:10
  kind: decision
  summary: "watch_fd()/watch_signal() 的返回值从 void 改成 bool：没受理就是 false（重复注册、uv_poll/uv_signal init|start 失败），且失败时 wrapper 已被 release() 交给 uv_close 回调释放。调用方必须把它当致命错误——main() 里 pty/stdin/SIGWINCH/SIGINT/SIGTERM 任一注册被拒即写 stderr + on_quit 退出：注册被拒意味着此后永远收不到键盘或 shell 输出，画面只会停在“像卡住了”的样子，静默继续比崩溃更难诊断。"
  source: "2026-09-28 代码评审修复轮（第 4 提交）"
  affects: [event-loop-libuv-handle-ownership]
