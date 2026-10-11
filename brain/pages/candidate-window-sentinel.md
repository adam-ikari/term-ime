---
id: candidate-window-sentinel
title: "候选窗口用 size_t::max 当哨兵：换页后未渲染即被 select() 窄化"
category: decision
status: active
tags: [candidate, paging, app, review]
created: "2026-10-10T11:44:52"
updated: "2026-10-10T16:34:56"
---

<!-- compiled_truth -->
## 不变量（2026-10-10 起）

`candidate_window_` 在 `advance_candidate_window()` 返回时**已经**是 rime 当前页的合法下标。
渲染按批次进行（一批字节处理完才 `render_candidates_bar()`），所以任何「等渲染时再收敛」的
哨兵值在批次中间都是活的：`handle_composing_key` 的数字键、空格、Enter 三条 select 路径都会
直接读它。历史上它取 `numeric_limits<size_t>::max()` 表示「渲染到最后一页」，实测后果是
`select(-1)` 返回空 → Enter 分支照样 `cancel()` → 整个组合态丢失且按键被 Consumed 吞掉；
首页再按 `,` 会让候选栏塌成 1 项。

## 翻页语义（三处互相约束）

- 向后跨页：落在**上一页的最后一个满窗口**（`count - step`），不是它的头部。窗口宽度随字形
  变化，`step` 取自上一次绘制，所以返回的窗口与去程只是部分重叠——契约要求的是「不跳过」，
  不是严格镜像。
- rime 拒绝翻页（已在首/末页）时候选列表不变，此时只能停在头部。判断方式是比对翻页前后的
  签名，不是猜。
- 自己按翻页键造成的列表变化必须**认领签名**（`candidate_page_sig_`），否则
  `render_candidates_bar()` 的「列表变了就归零窗口」会把刚算出的尾部窗口抹掉。
  这条是历史上 `,` 实际退回上一页头部的真正原因（哨兵从来没生效过）。


## Timeline

- time: 2026-10-10T11:44:52
  kind: decision
  summary: "Created this page: 候选窗口用 size_t::max 当哨兵：换页后未渲染即被 select() 窄化"
  source: created via brain create-page
  affects: [candidate-window-sentinel]

- time: 2026-10-10T11:45:04
  kind: decision
  summary: Rewrote compiled_truth to the new best understanding
  source: brain update-truth
  affects: [candidate-window-sentinel]

- time: 2026-10-10T16:34:28
  kind: decision
  summary: "记录不变量：candidate_window_ 任何时刻都是当前页的合法下标"
  source: "2026-10-10 修复轮"
  affects: [candidate-window-sentinel]

- time: 2026-10-10T16:34:56
  kind: evidence
  summary: "哨兵已删除：advance_candidate_window 即刻算出合法窗口，向后跨页落到上一页尾窗并认领候选页签名。回归测试 tests/test_candidate_paging.py（已加入 ci.yml 的 e2e 步骤）三项断言：新二进制 3/3 通过，修复前二进制 0/3 失败（Enter 丢组合、候选栏 3→1、逗号键退回页首跳过整页）。"
  source: "2026-10-10 修复轮"
  affects: [candidate-window-sentinel]
