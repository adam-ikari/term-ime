---
id: pty-outbound-write-queue
title: "Pty 出站写永不丢弃：EAGAIN 排队 + 读事件里 flush（IME 上屏不能丢字）"
category: decision
status: active
tags: [pty, libuv, write, ime]
created: "2026-09-28T14:20:03"
updated: "2026-09-28T16:16:45"
---

<!-- compiled_truth -->
> 页面标题里的"永不丢弃"已被推翻（见时间线 reversal），当前结论以下面为准。

## 第一约束：出站字节流必须保序

1. 队列里的字节永远比后到的新，`write()` 把它们插在本次数据之前再发。
2. 队列满（`TxByteQueue::kMaxBytes = 64 KiB`）时只丢**最新**的尾巴，并用 error 日志说明流的这一段已丢。绝不 erase 头部：那是从 UTF-8 序列或转义序列中间剪断，child 的 parser 之后一直在追一个没有上文的尾巴（实测形态：重定向符被当成野命令）。
3. peer 已死（`POLLERR|POLLHUP|POLLNVAL`，或 write 返回硬错误）时**清空**队列并报出丢了多少字节。留一个永远排不空的队列，等于让下一次 `flush()` 继续谎称"稍后会送出"。
4. `~Pty()` 在 `close(master_fd_)` 之前给队列一次有界 drain（`kExitBudgetMs = 50`），然后才杀子进程。
5. `flush(int budget_ms = 0)` 默认单次非阻塞 write()，因此 `on_pty_data()` 热路径上的 `pty_.flush()` 不再同步阻塞 100ms；只有用户刚产生的字节（`write()`，`kWriteBudgetMs = 100`）值得阻塞等待。EINTR 重试同样要重查 deadline —— SIGWINCH/SIGTERM 就投递在这个线程上。

## 边界（不再对外承诺的东西）

"写不完不静默丢字"不是本模块的性质。大块数据能否送达由两件事决定：内核 tty 输入缓冲（~4 KiB）和粘贴路径的逐字节 `write()`（`src/core/app.cpp` 粘贴分支，7000 字节 = 7000 次 syscall）。队列只在 write 返回短计数/EAGAIN 时接手。粘贴整段写在另一条提交里处理。

## 证据

`tests/test_pty.cpp` 5 例锁住保序与覆盖语义；把 `buffer_tail` 改回"丢最旧"，`OverflowDropsNewestBytesNotOldest` 立即变红（已 A/B 验证）。


## Timeline

- time: 2026-09-28T14:20:03
  kind: decision
  summary: "Created this page: Pty 出站写永不丢弃：EAGAIN 排队 + 读事件里 flush（IME 上屏不能丢字）"
  source: "2026-09-28 代码评审修复轮"
  affects: [pty-outbound-write-queue]

- time: 2026-09-28T15:53:44
  kind: reversal
  summary: "推翻本页两处结论：(1) 队列满时丢最旧字节是错的——这是必须保序的字节流，丢头部留下的是没有上文的尾巴，实测形态是尾部的重定向符变成了 shell 的野命令；宁可丢最新或整段放弃并明确报错。(2) '写不完不再静默丢字'在全速粘贴下不成立：新旧二进制同样只送到 4095/7000，约束是内核 tty 输入缓冲与粘贴路径的逐字节 write()（7000 字节=7000 次 syscall），队列根本没拿到那些字节，措辞需降级为'write 返回短计数/EAGAIN 时不再丢弃'。另外 flush() 挂在输出热路径上，队列非空时每个输出事件最多同步阻塞 100ms。"
  source: "2026-09-28 真实终端 A/B 验证（HEAD~1 vs 修复后）"
  affects: [pty-outbound-write-queue]

- time: 2026-09-28T16:16:45
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: "2026-09-28 队列语义返工（A1/A3/A4/A5）"
  affects: [pty-outbound-write-queue]
